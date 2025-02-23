#ifndef PEERCONNECTION_H
#define PEERCONNECTION_H

#include <memory>
#include <string>
#include <thread>
#include <list>
#include <unordered_map>
#include <fstream>

#include <api/peer_connection_interface.h>
#include <api/scoped_refptr.h>
#include <api/media_stream_interface.h>
#include <api/set_local_description_observer_interface.h>
#include <api/set_remote_description_observer_interface.h>

class PeerconnectionMgr : public webrtc::PeerConnectionObserver,
			  public webrtc::CreateSessionDescriptionObserver,
			  public webrtc::SetLocalDescriptionObserverInterface,
			  public webrtc::SetRemoteDescriptionObserverInterface,
			  public webrtc::RTCStatsCollectorCallback,
			  public webrtc::FrameTransformerInterface
{
  static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcf;
  static std::unique_ptr<rtc::Thread> _signaling_th;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _pc;
  rtc::scoped_refptr<PeerconnectionMgr> _me;

  std::atomic_bool _stats_th_running = false; // std::jthread where are you ? :'(
  std::thread      _stats_th;
  
  int    _count;
  double _prev_ts = 0.;
  double _prev_bytes = 0.;

  int _key_frame;
  int _frames;

  using MetadataHeader = uint32_t;

  std::unordered_map <uint64_t, rtc::scoped_refptr<webrtc::TransformedFrameCallback>> _callbacks;

  std::mutex _delay_mutex;
  unsigned long _delay_sum = 0;
  unsigned long _delay_count = 0;
  
public:
  rtc::VideoSinkInterface<webrtc::VideoFrame> * video_sink = nullptr;

  struct RTCStats
  {
    int x;
    int bitrate;
    int link;
    int fps;    
    int frame_dropped = 0;
    int frame_decoded = 0;
    int frame_key_decoded = 0;
    int frame_rendered = 0;
    int frame_width = 0;
    int frame_height = 0;
    int delay = 0;
    int rtt = 0;
  };

  std::function<void(RTCStats)> onstats; 
  
  static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> get_pcf();
  static void clean();

  std::function<void(const std::string&)> onlocaldesc;
  std::list<RTCStats> stats;
  int link;

  struct PortRange
  {
    int min_port;
    int max_port;
  };
  
  std::optional<PortRange> port_range;
  
  PeerconnectionMgr();
  ~PeerconnectionMgr();
  
  void start();
  void stop();
  void set_remote_description(const std::string& sdp);

  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
  void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)  override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override;

  void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
  
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override;
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

  void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> TransformableFrame) override;
  void RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback) override {}
  void RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback, uint32_t Ssrc) override;
  void UnregisterTransformedFrameCallback() override {}
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

public:
  void AddRef() const override { ref_count_.IncRef(); }

  webrtc::RefCountReleaseStatus Release() const override {
    const auto status = ref_count_.DecRef();
    if (status == webrtc::RefCountReleaseStatus::kDroppedLastRef) {
      // don't want to delete it
      // delete this;
    }
    return status;
  }

  virtual bool HasOneRef() const { return ref_count_.HasOneRef(); }

protected:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};

};

#endif /* PEERCONNECTION_H */
