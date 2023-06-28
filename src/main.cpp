#include <iostream>

#include <gtk/gtk.h>

#include "tunnel_mgr.h"
#include "main_wnd.h"

#define T(TIME,BITRATE,DELAY,LOSS) std::make_tuple(TIME,BITRATE,DELAY,LOSS)

namespace config
{

constexpr int medooze_port = 8084;
constexpr const char * medooze_host = "192.168.1.33";

constexpr int quic_server_port = 8888;
constexpr const char * quic_server_host = "192.168.1.47";

constexpr bool enable_medooze_bwe = true;
constexpr int medooze_probing = 2000;

constexpr const char * WS_CLIENT_HOST = "192.168.1.33";
constexpr const int WS_CLIENT_PORT = 3333;

constexpr const char * WS_SERVER_HOST = "192.168.1.47";
constexpr const int WS_SERVER_PORT = 3334;

}

int main(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
  // g_thread_init API is deprecated since glib 2.31.0, see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !GLIB_CHECK_VERSION(2, 31, 0)
  g_thread_init(NULL);
#endif

  MedoozeMgr        medooze;
  PeerconnectionMgr pc;
  
  medooze.host = config::medooze_host;
  medooze.port = config::medooze_port;
  medooze.probing = config::enable_medooze_bwe;
  medooze.probing_bitrate = config::medooze_probing;
  
  TunnelMgr tunnel(medooze, pc);
  tunnel.config.impl = "mvfst";
  tunnel.config.cc = "newreno";
  tunnel.config.datagrams = false;
  tunnel.config.quic_port = config::quic_server_port;
  tunnel.config.quic_host = config::quic_server_host;
  tunnel.config.external_file_transfer = false;

  tunnel.client.host = config::WS_CLIENT_HOST;
  tunnel.client.port = config::WS_CLIENT_PORT;

  tunnel.server.host = config::WS_SERVER_HOST;
  tunnel.server.port = config::WS_SERVER_PORT;
  
  tunnel.connect();

  WindowRenderer window;
  auto res = window.create();

  if(!res) {
    std::cout << "Could not create window" << "\n";
    std::exit(EXIT_FAILURE);
  }

  pc.video_sink = &window;
  
  std::deque<TunnelMgr::Constraints> constraints_init{T(5, 2500, 1, 0), {}, T(5, 2500, 1, 0)};
  std::queue<TunnelMgr::Constraints> constraints(constraints_init);

  std::thread([](){ gtk_main(); }).detach();
  
  while(!constraints.empty()) {
    tunnel.start();
    tunnel.run(constraints);
  }

  tunnel.disconnect();

  PeerconnectionMgr::clean();

  gtk_main_quit();
  window.destroy();
  
  return 0;
}
