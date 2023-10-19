
#include "http_server.h"

#include <unistd.h>

#include <functional>

#include "../common/config.h"
#include "../utils/magic_singleton.h"


std::thread HttpServer::_listenThread;
std::map<const std::string, HttpCallBack> HttpServer::_cbs;
std::map<const std::string, JsonRpcCallBack> HttpServer::rpcCbs;

void HttpServer::Work()
{
  using namespace httplib;

  Server svr;

  for(auto item: _cbs)
  {
    svr.Get(item.first.c_str(), item.second);
    svr.Post(item.first.c_str(), item.second);
  }

  int port = 8080;
  port = MagicSingleton<Config>::GetInstance()->GetHttpPort();
  svr.listen("0.0.0.0", port);
}


void HttpServer::Start()
{
    RegisterAllCallback();
    _listenThread = std::thread(HttpServer::Work);
    _listenThread.detach();	
}

void HttpServer::RegisterAllCallback()
{
  	RegisterCallback("/hello",ApiHello);
}


void ApiHello(const Request & req, Response & res)
{
  res.set_content("Hello World!!!", "text/plain");
}















