/**
 * *****************************************************************************
 * @file        config.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <set>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>
#include <chrono>
#include <boost/asio/deadline_timer.hpp>
#include <thread>
#include <functional>

#include "../utils/timer.hpp"
#include "../utils/json.hpp"
#include "../utils/magic_singleton.h"
#define SERVERMAINPORT (MagicSingleton<Config>::GetInstance()->GetServerPort())

class Node;
class Config
{
public:
    struct Info
    {
        std::string logo;
        std::string name;
    };

    struct Log
    {
        bool console;
        std::string level;
        std::string path;
    };

    struct HttpCallback
    {
        std::string ip;
        std::string path;
        uint32_t port;
    };

    const std::string kConfigFilename = "config.json";
    const std::string kCfgHttpCallback = "http_callback";
    const std::string kCfgHttpCallbackIp = "ip";
    const std::string kCfgHttpCallbackPort = "port";
    const std::string kCfgHttpCallbackPath = "path";
    const std::string kCfgHttpPort = "http_port";
    const std::string KRpcApi = "rpc";
    const std::string kCfgInfo = "info";
    const std::string kCfgInfoLogo = "logo";
    const std::string kCfgInfoName = "name";
    const std::string kCfgIp = "ip";
    const std::string kCfgLog = "log";
    const std::string kCfgLogLevel = "level";
    const std::string kCfgLogPath = "path";
    const std::string kCfgLogConsole = "console";
    const std::string kCfgServerPort = "server_port";
    const std::string kCfgKeyVersion = "version";
    const std::string kNewConfigName = "0.34";
    nlohmann::json tmpJson ;
    int count = 0;

public:
    Config()
    {
        InitFile();
        _thread = std::thread(std::bind(&Config::_Check, this));
    };

    Config(bool & flag)
    {
        int ret = InitFile();
        if ( ret == 0 ) flag = true;
    };
    
    ~Config()
    {  
        _exitThread = true;
        _thread.~thread();
    };

    /**
     * @brief       
     * 
     * @return      int 
     */
    int InitFile();

    /**
     * @brief       
     * 
     * @param       _node 
     * @return      int 
     */
    int _Verify(const Node& _node);

    /**
     * @brief       Get the Server Port object
     * 
     * @return      int 
     */
    int GetServerPort();

    /**
     * @brief       Get the Http Port object
     * 
     * @return      int 
     */
    int GetHttpPort();

    /**
     * @brief       Get the Info object
     * 
     * @param       info 
     * @return      int 
     */
    int GetInfo(Config::Info & info);

    /**
     * @brief       Get the Log object
     * 
     * @param       log 
     * @return      int 
     */
    int GetLog(Config::Log & log);

    /**
     * @brief       Get the Http Callback object
     * 
     * @param       httpCallback 
     * @return      int 
     */
    int GetHttpCallback(Config::HttpCallback & httpCallback);

    /**
     * @brief       Get the Rpc object
     * 
     * @return      true 
     * @return      false 
     */
    bool GetRpc();

    /**
     * @brief       
     * 
     * @return      std::string 
     */
    std::string GetIP();
    
    /**
     * @brief       
     * 
     * @param       ip 
     * @return      int 
     */
    int SetIP(const std::string & ip);

    /**
     * @brief       Get the Server object
     * 
     * @return      std::set<std::string> 
     */
    std::set<std::string> GetServer();

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T>
    int FilterServerPort(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T>
    int FilterHttpPort(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T> 
    int FilterIp(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T> 
    int FilterServerIp(T & t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      true 
     * @return      false 
     */
    template <typename T>
    bool FilterBool(T  &t);

    /**
     * @brief       
     * 
     * @param       log 
     * @return      int 
     */
    int FilterLog(Log &log);

    /**
     * @brief       
     * 
     * @param       httpcallback 
     * @return      int 
     */
    int FilterHttpCallback(HttpCallback &httpcallback);

    /**
     * @brief       Get the All Server Address object
     * 
     */
    void GetAllServerAddress();

//    int FilterVersion(std::string  & Version);
   


private:
    /**
     * @brief       
     * 
     * @param       json 
     * @return      int 
     */
    int _Parse(nlohmann::json json);

    /**
     * @brief       
     * 
     * @return      int 
     */
    int _Verify();

    /**
     * @brief       
     * 
     * @return      int 
     */
    int _Filter();

    /**
     * @brief       
     * 
     */
    void _Check();

 
private:

    Info _info;
    std::string _ip;
    Log _log;
    bool _rpc;
    HttpCallback _httpCallback;
    uint32_t _httpPort;
    std::set<std::string> _server;
    uint32_t _serverPort;
    std::string _version;
    std::thread _thread;
    std::atomic<bool> _exitThread{false};

};

#endif