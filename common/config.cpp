#include <iostream>
#include <fstream>
#include <tuple>
#include <regex>

#include "./config.h"
#include "../net/ip_port.h"
#include "../include/logging.h"
#include "../net/peer_node.h"
#include "../ca/ca_global.h"
#include "utils/console.h"
#include "net/peer_node.h"

int Config::InitFile()
{
    nlohmann::json _json;

    std::ifstream configFile(Config::kConfigFilename);
    if (!configFile.good())
    {
        std::ofstream outFile(Config::kConfigFilename);
        if (outFile.fail())
        {
            return -1;
        }
        auto json = nlohmann::json::parse(global::ca::kConfigJson);
        outFile << json.dump(4);
        _json = json;
    }
    else
    {
        configFile >> _json;
    }

    int ret = _Parse(_json);
    if (ret != 0)
    {
        return ret -= 100;
    }

    ret = verify();
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

int Config::_Parse(nlohmann::json json)
{
    if (json.is_null())
    {
        return -1;
    }

    try
    {
        _httpCallback.ip = json[kCfgHttpCallback][kCfgHttpCallbackIp].get<std::string>();
        _httpCallback.port = json[kCfgHttpCallback][kCfgHttpCallbackPort].get<uint32_t>();
        _httpCallback.path = json[kCfgHttpCallback][kCfgHttpCallbackPath].get<std::string>();

        _httpPort = json[kCfgHttpPort].get<uint32_t>();
        _rpc = json[KRpcApi].get<bool>();

        _info.logo = json[kCfgInfo][kCfgInfoLogo].get<std::string>();
        _info.name = json[kCfgInfo][kCfgInfoName].get<std::string>();

        _ip = json[kCfgIp].get<std::string>();

        _log.console = json[kCfgLog][kCfgLogConsole].get<bool>();
        _log.level = json[kCfgLog][kCfgLogLevel].get<std::string>();
        _log.path = json[kCfgLog][kCfgLogPath].get<std::string>();

        for(auto server : json["server"])
        {
            _server.insert(server.get<std::string>());
        }

        _serverPort = json[kCfgServerPort].get<uint32_t>();
        _version = json[kCfgKeyVersion].get<std::string>();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -2;
    }

    return 0;
}

    //Check whether the nickname is legal
#define _verify_(Info,param,minnum,maxnum,ip,regex_var)  \
    if(Info.param.length() >= minnum && Info.param.length() <= maxnum )  \
    {   \
        if(!Info.param.empty() && !std::regex_match(Info.param,std::regex(regex_var)))  \
        {   \
            std::cout << RED << "Config error in " << #param << " verify " ; \
            if(ip)  std::cout << " ip: " << IpPort::ipsz(ip);    \
            std::cout << RESET << std::endl;    \
            return -1;  \
        }   \
    }   \
    else    \
    {   \
        std::cout << RED << "Config error in "<< #param << " length! " << #param << " : " <<  Info.param  \
        << " length is: " << Info.param.length() << " The range should be:" << minnum << " - " << maxnum ;  \
        if(ip)  std::cout << " ip: " << IpPort::ipsz(ip);    \
        std::cout << RESET << std::endl;    \
        return -2;  \
    }   \


int Config::verify(const Node& _node)
{
    _verify_(_node,name,0,45,_node.public_ip,"^[\u4E00-\u9FA5A-Za-z0-9_]+$");
    _verify_(_node,logo,0,512,_node.public_ip,"^(http|https|ftp)\\://([a-zA-Z0-9\\.\\-]+(\\:[a-zA-Z0-9\\.&%\\$\\-]+)*@)?((25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.[a-zA-Z]{2,4})(\\:[0-9]+)?(/[^/][a-zA-Z0-9\\.\\,\?\'\\/\\+&%\\$#\\=~_\\-@]*)*$"); 
    return 0;
}

int Config::verify()
{
    _verify_(_info,name,0,45,0,"^[\u4E00-\u9FA5A-Za-z0-9_]+$");
    _verify_(_info,logo,0,512,0,"^(http|https|ftp)\\://([a-zA-Z0-9\\.\\-]+(\\:[a-zA-Z0-9\\.&%\\$\\-]+)*@)?((25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.[a-zA-Z]{2,4})(\\:[0-9]+)?(/[^/][a-zA-Z0-9\\.\\,\?\'\\/\\+&%\\$#\\=~_\\-@]*)*$"); 
    return 0;
}

int Config::GetServerPort()
{
    return _serverPort;
}

int Config::GetHttpPort()
{
    return _httpPort;
}

bool Config::GetRpc()
{
    return _rpc;
}

std::string Config::GetIP()
{
    return _ip;
}

int Config::GetInfo(Config::Info & info)
{
    info = _info;
    return 0;
}

int Config::SetIP(const std::string & ip)
{
    nlohmann::json _json;

    std::ifstream configFile(Config::kConfigFilename);
    if (!configFile.good())
    {
        return -1;
    }

    configFile >> _json;
    configFile.close();

    _json["ip"] = ip;

    std::ofstream outFile(Config::kConfigFilename);
    if (outFile.fail())
    {
        return -2;
    }
    outFile << _json.dump(4);
    outFile.close();

    _ip = ip;

    return 0;
}

std::set<std::string> Config::GetServer()
{
    return _server;
}

int Config::GetLog(Config::Log & log)
{
    log = _log;

    return 0;
}

int Config::GetHttpCallback(Config::HttpCallback & httpCallback)
{
    httpCallback = _httpCallback;
    return 0;
}
