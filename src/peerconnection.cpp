#include "peerconnection.h"

#include <stdexcept>
#include <iostream>
#include <chrono>
#include <algorithm>

#include <api/create_peerconnection_factory.h>
#include <rtc_base/ssl_adapter.h>

#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/jsep.h>
#include <api/stats/rtcstats_objects.h>
#include <rtc_base/thread.h>

#include <pc/session_description.h>

#include "fake_adm.h"
#include "tunnel_loggin.h"

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerconnectionMgr::_pcf = nullptr;
std::unique_ptr<rtc::Thread> PeerconnectionMgr::_signaling_th = nullptr;

constexpr int puissance(int v, unsigned int e)
{
  if(e == 0) return 1;
  return puissance(v, e - 1) * v;
}

void PeerconnectionMgr::Transform(std::unique_ptr<webrtc::TransformableFrameInterface> trans_frame)
{
  auto ssrc = trans_frame->GetSsrc();

  if (auto it = _callbacks.find(ssrc); it != _callbacks.end()) {
    auto data_view = trans_frame->GetData();
    auto length = data_view.size();

    constexpr auto DATA_LENGTH = 4;
    constexpr const unsigned char magic_value[] = { 0xca, 0xfe, 0xba, 0xbe };
    bool has_magic_value = true;
    for(int i = 0; has_magic_value && i < 4; ++i) {
      has_magic_value = data_view[i + (length - DATA_LENGTH*2)] == magic_value[i]; 
    }
    
    // Check that the extracted value match the start value
    if (has_magic_value) {
      unsigned int time = 0;
      constexpr int OCTET = puissance(2,8);
      for(int i = 0; i < DATA_LENGTH; ++i) {
	time += data_view[i + (length - DATA_LENGTH)] * puissance(OCTET, i);
      }

      auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
      unsigned int delay = now.count() - time;

      {
	std::unique_lock<std::mutex> lock(_delay_mutex);
	_delay_sum += static_cast<unsigned long>(delay);
	_delay_count++;
      }
      
      // Set the final transformed data.
      trans_frame->SetData(data_view.subview(0, length - 2 * DATA_LENGTH));
    }
     
    it->second->OnTransformedFrame(std::move(trans_frame));
  }
}

void PeerconnectionMgr::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback, uint32_t Ssrc)
{
  _callbacks[Ssrc] = InCallback;
}

void PeerconnectionMgr::UnregisterTransformedFrameSinkCallback(uint32_t ssrc)
{
  _callbacks.erase(ssrc);
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerconnectionMgr::get_pcf()
{
  if(_pcf != nullptr) return _pcf;

  rtc::InitRandom((int)rtc::Time());
  rtc::InitializeSSL();

  _signaling_th = rtc::Thread::Create();
  _signaling_th->SetName("WebRTCSignalingThread", nullptr);
  _signaling_th->Start();

  rtc::scoped_refptr<FakeAudioDeviceModule> adm(new rtc::RefCountedObject<FakeAudioDeviceModule>());
  
  _pcf = webrtc::CreatePeerConnectionFactory(nullptr, nullptr, _signaling_th.get(), adm,
					     webrtc::CreateBuiltinAudioEncoderFactory(),
					     webrtc::CreateBuiltinAudioDecoderFactory(),
					     webrtc::CreateBuiltinVideoEncoderFactory(),
					     webrtc::CreateBuiltinVideoDecoderFactory(),
					     nullptr, nullptr);

  webrtc::PeerConnectionFactoryInterface::Options options;
  options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  _pcf->SetOptions(options);
  
  return _pcf;
}

void PeerconnectionMgr::clean()
{
  _pcf = nullptr;
  rtc::CleanupSSL();
}

PeerconnectionMgr::PeerconnectionMgr() : _pc{nullptr}, _me(this)
{
  AddRef();
}

PeerconnectionMgr::~PeerconnectionMgr()
{
  Release();
}

void PeerconnectionMgr::start()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Start peerconnection";
  
  auto pcf = get_pcf();

  webrtc::PeerConnectionDependencies deps(this);
  webrtc::PeerConnectionInterface::RTCConfiguration config(webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

  config.set_cpu_adaptation(true);
  config.combined_audio_video_bwe.emplace(true);
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

  if(port_range.has_value()) {
    config.set_min_port(port_range->min_port);
    config.set_max_port(port_range->max_port);
  }

  auto res = pcf->CreatePeerConnectionOrError(config, std::move(deps));

  if(!res.ok()) {
    TUNNEL_LOG(TunnelLogging::Severity::ERROR) << "Can't create peerconnection : " << res.error().message();
    throw std::runtime_error("Could not create peerconection");
  }

  _pc = res.value();

  _stats_th_running = false;
  stats.erase(stats.begin(), stats.end());
  _count = 0;
  _prev_ts = 0.;
  _prev_bytes = 0.;
  _key_frame = 0;
  _frames = 0;

  webrtc::RtpTransceiverInit init;

  init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
  init.stream_ids = { "vm" };

  auto expected_transceiver = _pc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, init);
  if(!expected_transceiver.ok()) return;
  auto transceiver = expected_transceiver.value();

  // auto err = transceiver->SetCodecPreferences(receiver_cap.codecs);
  // if(!err.ok()) {
  //   std::cout << "Could not set codec : " << err.message() << "\n";
  // }

  _signaling_th->BlockingCall([this]() {
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "creating offer";
    _pc->CreateOffer(this, {});
  });
}

void PeerconnectionMgr::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "On create offer success";

  _pc->SetLocalDescription(std::unique_ptr<webrtc::SessionDescriptionInterface>(desc), _me);
  
}

void PeerconnectionMgr::OnFailure(webrtc::RTCError error)
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "On create offer failure";
}

void PeerconnectionMgr::OnSetLocalDescriptionComplete(webrtc::RTCError error)
{
  if(error.ok()) {
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "On set local desc success";
    
    std::string sdp;
    auto desc = _pc->local_description();
    desc->ToString(&sdp);

    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << sdp;

    if(onlocaldesc) onlocaldesc(sdp);
  }
  else {
    TUNNEL_LOG(TunnelLogging::Severity::ERROR) << "On set local desc failure";
  }
}

void PeerconnectionMgr::OnSetRemoteDescriptionComplete(webrtc::RTCError error)
{
  if(error.ok()) {
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "On set remote desc success";
  }
  else {
    TUNNEL_LOG(TunnelLogging::Severity::ERROR) << "On set remote desc failure";
  }
}

void PeerconnectionMgr::stop()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "PeerConnection::Stop" << "\n";
  _stats_th_running = false;
  if(_stats_th.joinable()) _stats_th.join();
  
  _pc->Close();
  _pc = nullptr;
}

void PeerconnectionMgr::set_remote_description(const std::string &sdp)
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Set remote desc";
  auto desc = webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp);

  if(!desc) {
    TUNNEL_LOG(TunnelLogging::Severity::ERROR) << "Error parsing remote sdp" << "\n";
    throw std::runtime_error("Could not set remote description");
  }

  _pc->SetRemoteDescription(std::move(desc), _me);
}

void PeerconnectionMgr::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{  
  RTCStats rtc_stats;

  auto inbound_stats = report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  
  for(const auto& s : inbound_stats) {
    if(*s->kind == webrtc::RTCMediaStreamTrackKind::kVideo) {
      auto ts = s->timestamp();
      auto ts_ms = ts.ms();
      auto delta = ts_ms - _prev_ts;
      auto bytes = *s->bytes_received - _prev_bytes;
      
      rtc_stats.x = _count;
      rtc_stats.link = link;
      rtc_stats.bitrate = static_cast<int>(8. * bytes / delta);
      rtc_stats.fps = s->frames_per_second.ValueOrDefault(0.);
      rtc_stats.frame_dropped = s->frames_dropped.ValueOrDefault(0.);
      rtc_stats.frame_decoded = s->frames_decoded.ValueOrDefault(0.);
      rtc_stats.frame_key_decoded = s->key_frames_decoded.ValueOrDefault(0.);
      rtc_stats.frame_width = s->frame_width.ValueOrDefault(0);
      rtc_stats.frame_height = s->frame_height.ValueOrDefault(0);
      
      TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << s->frame_width.ValueOrDefault(0.) << "x" << s->frame_height.ValueOrDefault(0.);
            
      _prev_ts = ts_ms;
      _prev_bytes = *s->bytes_received;
    }
  }
  
  auto remote_outbound_stats = report->GetStatsOfType<webrtc::RTCRemoteOutboundRtpStreamStats>();
  for(const auto& s : remote_outbound_stats) {
    if(*s->kind == webrtc::RTCMediaStreamTrackKind::kVideo) {
      rtc_stats.rtt = static_cast<int>(s->round_trip_time.ValueOrDefault(0.) * 1000);
    }
  }
  
  if(_delay_count > 0) {
    std::unique_lock<std::mutex> lock(_delay_mutex);
    rtc_stats.delay = _delay_sum / _delay_count;
    _delay_count = 0; _delay_sum = 0;
  }

  if(onstats) onstats(std::move(rtc_stats));
}

void PeerconnectionMgr::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) 
{
  switch(new_state) {
  case webrtc::PeerConnectionInterface::SignalingState::kClosed:
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "pc on closed";
    break;
  case webrtc::PeerConnectionInterface::SignalingState::kStable:
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "pc on stable";
  default:
    break;
  }
}

void PeerconnectionMgr::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{}

void PeerconnectionMgr::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{}

void PeerconnectionMgr::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) 
{}

void PeerconnectionMgr::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) 
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "PeerconnectionMgr::OnTrack";
  
  _stats_th = std::thread([this, transceiver]() {
    _stats_th_running = true;
    _count = 0;
    
    while(_stats_th_running) {
      _pc->GetStats(transceiver->receiver(), _me);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ++_count;
    }
  });

  if(video_sink) {
    auto track = static_cast<webrtc::VideoTrackInterface*>(transceiver->receiver()->track().get());
    track->AddOrUpdateSink(video_sink, rtc::VideoSinkWants{});
  }

  transceiver->receiver()->SetDepacketizerToDecoderFrameTransformer(_me);
}

void PeerconnectionMgr::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) 
{}

void PeerconnectionMgr::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)  
{}

void PeerconnectionMgr::OnRenegotiationNeeded() 
{}

void PeerconnectionMgr::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) 
{}

void PeerconnectionMgr::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) 
{}

void PeerconnectionMgr::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{}

void PeerconnectionMgr::OnIceConnectionReceivingChange(bool receiving) 
{}

