
#include "http_server.h"
#include <unistd.h>
#include <functional>
#include "../common/config.h"
#include "../utils/MagicSingleton.h"


std::thread HttpServer::listen_thread;
std::map<const std::string, HttpCallBack> HttpServer::cbs;
std::map<const std::string, JsonRpcCallBack> HttpServer::rpc_cbs;

void HttpServer::work()
{
  using namespace httplib;

  Server svr;

  for(auto item: cbs)
  {
    svr.Get(item.first.c_str(), item.second);
    svr.Post(item.first.c_str(), item.second);
  }

  int port = 8080;
  port = MagicSingleton<Config>::GetInstance()->GetHttpPort();
  svr.listen("0.0.0.0", port);
}


void HttpServer::start()
{
    registerAllCallback();
    listen_thread = std::thread(HttpServer::work);
    listen_thread.detach();	
}

void HttpServer::registerAllCallback()
{
  	registerCallback("/hello",api_hello);
}


void api_hello(const Request & req, Response & res)
{
  res.set_content("Hello World!!!", "text/plain");
}















