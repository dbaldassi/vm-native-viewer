#include <iostream>
#include <csignal>
#include <thread>
#include <cstdlib>
#include <unistd.h>

#include "medooze_mgr.h"
#include "peerconnection.h"
#include "monitor_manager.h"

#include "tunnel_loggin.h"
#include "main_wnd.h"

#ifndef FAKE_RENDERER
#include <gtk/gtk.h>
#endif

namespace config
{
  constexpr int MEDOOZE_PORT = 8084;
  constexpr const char * MEDOOZE_HOST = "134.59.133.76";

  constexpr int MONITOR_PORT = 9000;
  constexpr const char * MONITOR_HOST = "134.59.133.57";
}

int run(std::optional<PeerconnectionMgr::PortRange> range)
{
  MedoozeMgr        medooze;
  PeerconnectionMgr pc;
  MonitorMgr        monitor;
  
  std::condition_variable cv;
  std::mutex              mutex_cv;
  
  medooze.host = config::MEDOOZE_HOST;
  medooze.port = config::MEDOOZE_PORT;

  monitor.host = config::MONITOR_HOST;
  monitor.port = config::MONITOR_PORT;
  
  WindowRenderer window;
  auto res = window.create();

  if(!res) {
    TUNNEL_LOG(TunnelLogging::Severity::ERROR) << "Could not create window";
    std::exit(EXIT_FAILURE);
  }

  pc.video_sink = &window;
  pc.port_range = std::move(range);

#ifndef FAKE_RENDERER
  std::thread([](){ gtk_main(); }).detach();
#endif

  monitor.connect();
  medooze.start();

  pc.onlocaldesc = [&medooze](auto&& desc) -> void { medooze.view(desc); };
  pc.onstats = [&monitor](auto&& stats) -> void {
    MonitorMgr::Report report;
    report.bitrate = stats.bitrate;
    report.fps = stats.fps;

    monitor.send_report(std::move(report));
  };

  medooze.onname = [&monitor](const std::string& name) {
    monitor.name = name;
    monitor.start();
  };
  medooze.onanswer = [&pc](auto&& sdp) -> void { pc.set_remote_description(sdp); };
  medooze.ontarget = [&monitor](int target) {
    MonitorMgr::Report report;
    report.target = target;
    monitor.send_report(std::move(report));
  };
  
  medooze.onclose = [&cv]() -> void { cv.notify_all(); };
  monitor.onclose = [&cv]() -> void { cv.notify_all(); };
  
  pc.start();

  {
    std::unique_lock<std::mutex> lock(mutex_cv);
    cv.wait(lock);
  }

  pc.stop();
  monitor.stop();
  medooze.stop();
  window.destroy();
  
  return 0;
}

int main(int argc, char *argv[])
{
#ifndef FAKE_RENDERER
  gtk_init(&argc, &argv);

#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
  // g_thread_init API is deprecated since glib 2.31.0, see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !GLIB_CHECK_VERSION(2, 31, 0)
  g_thread_init(NULL);
#endif
#endif
  
  TunnelLogging::set_min_severity(TunnelLogging::Severity::VERBOSE);

  std::optional<PeerconnectionMgr::PortRange> range;
  if(argc == 3) {
    range = PeerconnectionMgr::PortRange{};
    range->min_port = std::atoi(argv[1]);
    range->max_port = std::atoi(argv[2]);
  }
  
  run(std::move(range));
  
  PeerconnectionMgr::clean();

#ifndef FAKE_RENDERER
  gtk_main_quit();
#endif
    
  return 0;
}
