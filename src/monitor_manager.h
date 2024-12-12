#ifndef MONITOR_MANAGER_H
#define MONITOR_MANAGER_H

#include <optional>
#include <condition_variable>
#include <mutex>

#include "websocket.h"

class MonitorMgr
{
  WebSocketSecure _ws;
  
  std::condition_variable _cv;
  std::mutex              _cv_mutex;

  std::atomic_bool _alive;
 
public:
  using json = nlohmann::json;

  struct Report
  {
    std::optional<int> target;
    std::optional<int> bitrate;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> fps;
  };
  
  std::string host;
  int         port;

  std::string name;

  std::function<void()> onclose;
  
public:
  MonitorMgr();

  void connect();
  
  void start();
  void stop();

  void send_report(Report report);
};


#endif /* MONITOR_MANAGER_H */
