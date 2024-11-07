#ifndef MONITOR_MANAGER_H
#define MONITOR_MANAGER_H

#include <optional>
#include "websocket.h"

class MonitorMgr
{
  WebSocketSecure _ws;
 
public:
  using json = nlohmann::json;

  struct Report
  {
    std::optional<int> target;
    std::optional<int> bitrate;
    std::optional<int> fps;
  };
  
  std::string host;
  int         port;

  std::string name;
  
public:
  MonitorMgr();

  void connect();
  
  void start();
  void stop();

  void send_report(Report report);
};


#endif /* MONITOR_MANAGER_H */
