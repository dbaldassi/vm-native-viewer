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

#include "tunnel_loggin.h"

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerconnectionMgr::_pcf = nullptr;
std::unique_ptr<rtc::Thread> PeerconnectionMgr::_signaling_th = nullptr;

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerconnectionMgr::get_pcf()
{
  if(_pcf != nullptr) return _pcf;

  rtc::InitRandom((int)rtc::Time());
  rtc::InitializeSSL();

  _signaling_th = rtc::Thread::Create();
  _signaling_th->SetName("WebRTCSignalingThread", nullptr);
  _signaling_th->Start();
  
  _pcf = webrtc::CreatePeerConnectionFactory(nullptr, nullptr, _signaling_th.get(), nullptr,
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

  _file_bitstream.open("bitstream.264", std::ios::binary);

  // auto receiver_cap = pcf->GetRtpReceiverCapabilities(cricket::MediaType::MEDIA_TYPE_VIDEO);

  // for(auto&& cap : receiver_cap.codecs) std::cout << cap.name << " ";
  // std::cout << std::endl;
  // std::erase_if(receiver_cap.codecs,
  // 		[](auto&& cap) -> bool { return cap.name != cricket::kH264CodecName; });
  // for(auto&& cap : receiver_cap.codecs) std::cout << cap.name << " ";
  // std::cout << std::endl;
    
  webrtc::RtpTransceiverInit init;

  init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
  init.stream_ids = { "tunnel" };

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

  _file_bitstream.close();
  
  _pc->Close();
  _pc = nullptr;

  TUNNEL_LOG(TunnelLogging::Severity::INFO) << "Received transformable frame : " << _frames;
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

      /*std::cout << "jitter : " << s->jitter_buffer_delay.ValueOrDefault(0.) << "\n"
		<< "jitter target : " << s->jitter_buffer_target_delay.ValueOrDefault(0.) << "\n"
		<< "jitter min : " << s->jitter_buffer_minimum_delay.ValueOrDefault(0.) << "\n"
		<< "jitter flush : " << s->jitter_buffer_flushes.ValueOrDefault(0.) << "\n"
		<< "packet fec : " << s->fec_packets_received.ValueOrDefault(0.) << "\n"
	// << "packet rtx : " << s->retransmitted_bytes_received.ValueOrDefault(0.) << "\n"
		<< "frame dropped : " << s->frames_dropped.ValueOrDefault(0.) << "\n"
		<< "\n";*/
      
      _prev_ts = ts_ms;
      _prev_bytes = *s->bytes_received;
    }
  }
  
  stats.push_back(std::move(rtc_stats));
}

void PeerconnectionMgr::Transform(std::unique_ptr<webrtc::TransformableFrameInterface> transformable_frame)
{
  auto ssrc = transformable_frame->GetSsrc();

  if(auto it = _callbacks.find(ssrc); it != _callbacks.end()) {
    auto video_frame = static_cast<webrtc::TransformableVideoFrameInterface*>(transformable_frame.get());
    if(video_frame->IsKeyFrame()) {
      // std::cout << "Received new key frame: " << ++_key_frame << " num " << _frames << " size " << transformable_frame->GetData().size() << "\n";
    }
    else {
      // std::cout << "Received p frame: " << " size " << transformable_frame->GetData().size() << "\n";
    }


    ++_frames;

    // std::cout << "Frame num " << _frames << " : " << video_frame->GetData().size() << "\n";

    /*std::cout << video_frame->GetMetadata().GetFrameId().value_or(-1) << " " << video_frame->GetMetadata().GetCodec() << " "
      << video_frame->GetTimestamp() << " " << webrtc::ToString(video_frame->GetCaptureTimeIdentifier().value_or(webrtc::Timestamp::Seconds(0))) << "\n";*/
    
    if(_file_bitstream.is_open() && _key_frame >= 1) {
      _file_bitstream.write((const char*)video_frame->GetData().data(), video_frame->GetData().size());
    }
    
    it->second->OnTransformedFrame(std::move(transformable_frame));
  }
}

void PeerconnectionMgr::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback, uint32_t ssrc)
{
  _callbacks[ssrc] = callback;
}

void PeerconnectionMgr::UnregisterTransformedFrameSinkCallback(uint32_t ssrc)
{
  _callbacks.erase(ssrc);
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
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++_count;
    }
  });

  transceiver->receiver()->SetDepacketizerToDecoderFrameTransformer(_me);

  if(video_sink) {
    auto track = static_cast<webrtc::VideoTrackInterface*>(transceiver->receiver()->track().get());
    track->AddOrUpdateSink(video_sink, rtc::VideoSinkWants{});
  }
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

