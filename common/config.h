#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <set>
#include "../utils/json.hpp"
#include "../utils/MagicSingleton.h"

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

public:
    Config()
    {
        InitFile();
    };
    Config(bool & flag)
    {
        int ret = InitFile();
        if ( ret == 0 ) flag = true;
    };
    ~Config(){};

    int InitFile();
    int verify(const Node& _node);
    int GetServerPort();
    int GetHttpPort();
    bool GetRpc();
    std::string GetIP();
    int GetInfo(Config::Info & info);
    int SetIP(const std::string & ip);

    std::set<std::string> GetServer();

    int GetLog(Config::Log & log);
    int GetHttpCallback(Config::HttpCallback & httpCallback);


private:
    int _Parse(nlohmann::json json);
    int verify();
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
};

#endif