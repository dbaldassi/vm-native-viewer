#ifndef MEDOOZE_MGR_H
#define MEDOOZE_MGR_H

#include <condition_variable>

#include "websocket.h"

class MedoozeMgr
{
  WebSocketSecure _ws;

  std::condition_variable _cv;
  std::mutex              _cv_mutex;
  
public:

  using json = nlohmann::json;
  
  std::string host;

  int port;
  std::function<void(const json&)> onanswer;
  std::function<void(int)> ontarget;
  std::function<void(const std::string&)> onname;
  
public:
  
  MedoozeMgr();

  void start();
  void stop();
  void view(const std::string& sdp);
};

#endif /* MEDOOZE_MGR_H */
