#include "monitor_manager.h"

MonitorMgr::MonitorMgr() : _alive{false}
{
  _ws.onopen = [this]() {
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "monitor ws opened";
    _alive = true;
    _cv.notify_all();
  };
  _ws.onclose = [this]() {
    TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "monitor ws closed";
    _alive = false;
    if(onclose) onclose();
  };
}

void MonitorMgr::connect()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Connect to VM monitor";
  _ws.connect(host, port, "viewer");
}

void MonitorMgr::start()
{
  TUNNEL_LOG(TunnelLogging::Severity::VERBOSE) << "Start VM monitor connection with name " << name;

  if(!_alive) {
    std::unique_lock<std::mutex> lock(_cv_mutex);
    _cv.wait(lock);
  }
  
  json data = {
    { "cmd", "id" },
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
    data.emplace("cmd", "target");
    data.emplace("target", *report.target);
    data.emplace("name", name);
  }
  else if(report.bitrate.has_value() || report.fps.has_value()) {
    data.emplace("cmd", "bitrate");
    data.emplace("bitrate", report.bitrate.value_or(0));
    data.emplace("fps", report.fps.value_or(0));
    data.emplace("name", name);
  }

  _ws.send(data.dump());
}
