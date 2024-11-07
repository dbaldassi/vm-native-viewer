#include <iostream>
#include "medooze_mgr.h"

#include "tunnel_loggin.h"

MedoozeMgr::MedoozeMgr()
{
  _ws.onopen = []() { TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "medooze ws opened"; };
  _ws.onclose = []() { TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "medooze ws closed"; };
}

void MedoozeMgr::start()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Start connection medooze manager";
  
  _ws.connect(host, port , "vm-relay");
  _ws.onmessage = [this](auto&& msg) {
    if(auto answer = msg.find("answer"); answer != msg.end()) {
      if(onanswer) onanswer(*answer);
      if(onname) onname(msg["name"].template get<std::string>());
    }
    else if(auto answer = msg.find("target"); answer != msg.end()) {
      if(ontarget) ontarget(answer->template get<int>());
    }
  };
}

void MedoozeMgr::stop()
{
  _ws.disconnect();
}

void MedoozeMgr::view(const std::string& sdp)
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "MedoozeManager::view";
  
  json cmd = {
    { "cmd", "view" },
    { "offer", sdp }
  };

  _ws.send(cmd.dump());
}
