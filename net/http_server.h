/**
 * *****************************************************************************
 * @file        http_server.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
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

#include "./httplib.h"

#include "../utils/json.hpp"

using namespace httplib;

typedef std::function<void(const Request &, Response &)> HttpCallBack;
typedef std::function<nlohmann::json(const nlohmann::json &)> JsonRpcCallBack;


class HttpServer
{
public:
	HttpServer() = default;
	~HttpServer() = default;
	/**
	 * @brief       
	 * 
	 */
	static void Work();
	
	/**
	 * @brief       
	 * 
	 */
	static void Start();

	/**
	 * @brief       
	 * 
	 */
	static void RegisterAllCallback();

	/**
	 * @brief       
	 * 
	 * @param       pattern 
	 * @param       handler 
	 */
	static void RegisterCallback(std::string pattern, HttpCallBack handler)
	{
		HttpServer::_cbs[pattern] = handler;
	}

	/**
	 * @brief       
	 * 
	 * @param       method 
	 * @param       cb 
	 */
	static void registerJsonRpcCallback(std::string method, JsonRpcCallBack cb)
	{
		HttpServer::rpcCbs[method] = cb;
	}

	static std::map<const std::string, JsonRpcCallBack> rpcCbs;
private:
    friend std::string PrintCache(int where);
    static std::thread _listenThread;
	static std::map<const std::string, HttpCallBack> _cbs;
};

/**
 * @brief       
 * 
 * @param       req 
 * @param       res 
 */
void ApiHello(const Request & req, Response & res);
#endif










