
#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <map>
#include <list>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include "httplib.h"
#include "../utils/json.hpp"

using namespace httplib;

typedef std::function<void(const Request &, Response &)> HttpCallBack;
typedef std::function<nlohmann::json(const nlohmann::json &)> JsonRpcCallBack;


class HttpServer
{
public:
	HttpServer() = default;
	~HttpServer() = default;


	static void work();
	static void start();
	static void registerAllCallback();
	static void registerCallback(std::string pattern, HttpCallBack handler)
	{
		HttpServer::cbs[pattern] = handler;
	}
	static void registerJsonRpcCallback(std::string method, JsonRpcCallBack cb)
	{
		HttpServer::rpc_cbs[method] = cb;
	}


	
	static std::map<const std::string, JsonRpcCallBack> rpc_cbs;
private:
    static std::thread listen_thread;
	static std::map<const std::string, HttpCallBack> cbs;
};


void api_hello(const Request & req, Response & res);
void api_print_block(const Request & req, Response & res);


#endif










