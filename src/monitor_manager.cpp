#include "monitor_manager.h"

MonitorMgr::MonitorMgr()
{
  _ws.onopen = []() { TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "monitor ws opened"; };
  _ws.onclose = []() { TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "monitor ws closed"; };
}

void MonitorMgr::connect()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Connect to VM monitor";
  _ws.connect(host, port);
}

void MonitorMgr::start()
{
  json data = {
    { "cmd", "new_viewer" },
    { "name", name }
  };

  _ws.send(data.dump());
}

void MonitorMgr::stop()
{
  _ws.disconnect();
}

void MonitorMgr::send_report(Report report)
{
  json data{};
  
  if(report.target.has_value()) {
    data.emplace("cmd", "viewertarget");
    data.emplace("target", *report.target);
    data.emplace("name", name);
  }
  else if(report.bitrate.has_value() || report.fps.has_value()) {
    data.emplace("cmd", "viewerbitrate");
    data.emplace("bitrate", report.bitrate.value_or(0));
    data.emplace("fps", report.fps.value_or(0));
    data.emplace("name", name);
  }

  _ws.send(data.dump());
}
