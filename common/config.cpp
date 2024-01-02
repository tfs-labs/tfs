#include "./config.h"

#include <boost/asio/deadline_timer.hpp>
#include <boost/bind.hpp>
#include <boost/asio/io_context.hpp>

#include <iostream>
#include <fstream>
#include <tuple>
#include <regex>
#include <filesystem>

#include "../utils/console.h"
#include "../net/peer_node.h"
#include "../common/global.h"
#include "../net/node.hpp"
#include "../net/ip_port.h"
#include "../include/logging.h"
#include "../net/peer_node.h"
#include "../ca/global.h"


int Config::InitFile()
{
 
    std::ifstream f(kConfigFilename);
    
    if(f.good())
    {
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        tmpJson = nlohmann::json::parse(str);
        if(_Parse(tmpJson) != 0)
        {   
            return -1;
        }
        if(_version != kNewConfigName)
        {
            std::cout<<"Please make sure that you've delete the config file,and reboot the program." << std::endl;
            return -3;
        }
    }
    f.close();

    std::ifstream configFile(Config::kConfigFilename);

    if (!configFile)
    {
        std::ofstream outFile(Config::kConfigFilename);
        tmpJson = nlohmann::json::parse(global::ca::kConfigJson);

        if(_Parse(tmpJson) != 0)
        {
            return -2;
        }
        if(_version != kNewConfigName)
        {
            std::cout<<"Please make sure that you've delete the config file,and reboot the program." << std::endl;
            return -3;
        }
        outFile << tmpJson.dump(4);
        outFile.close();    
        
    }
    configFile.close();

    if(_Filter() != 0 || _Verify() != 0)
    {
        std::filesystem::remove(kConfigFilename);
        std::ofstream outFile(Config::kConfigFilename);
        auto json = nlohmann::json::parse(global::ca::kConfigJson);
        outFile << json.dump(4);
        outFile.close();  
      return 0;
    }

    //GetAllServerAddress();
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
        _ip = json[kCfgIp].get<std::string>();
        _httpCallback.ip = json[kCfgHttpCallback][kCfgHttpCallbackIp].get<std::string>();
        _httpCallback.port = json[kCfgHttpCallback][kCfgHttpCallbackPort].get<uint32_t>();
        _httpCallback.path = json[kCfgHttpCallback][kCfgHttpCallbackPath].get<std::string>();
        _httpCallback.path = json[kCfgHttpCallback][kCfgHttpCallbackPath].get<std::string>();
        _httpPort = json[kCfgHttpPort].get<uint32_t>();
        _rpc = json[KRpcApi].get<bool>();
        _info.logo = json[kCfgInfo][kCfgInfoLogo].get<std::string>();
        _info.name = json[kCfgInfo][kCfgInfoName].get<std::string>();
        _log.console = json[kCfgLog][kCfgLogConsole].get<bool>();
        _log.level = json[kCfgLog][kCfgLogLevel].get<std::string>();
        _log.path = json[kCfgLog][kCfgLogPath].get<std::string>();
        for(auto server : json["server"])
        {
            _server.insert(server.get<std::string>());
        }
       _serverPort = json[kCfgServerPort].get<uint32_t>();

       _version = json[kCfgKeyVersion].get<std::string>();  
        if(_version != kNewConfigName)
        {
            std::cout<<"Please make sure that you've delete the config file,and reboot the program." << std::endl;
            return -2;
        }

        }

        catch(const std::exception& e) 
        {
         std::cout << e.what() << std::endl; 
        }
                
       return 0;
        }

    //_Check whether the nickname is legal
#define Dverify(Info,param,minnum,maxnum,ip,regex_var)  \
    if(Info.param.length() >= minnum && Info.param.length() <= maxnum )  \
    {   \
        if(!Info.param.empty() && !std::regex_match(Info.param,std::regex(regex_var)))  \
        {   \
            std::cout << RED << "Config error in " << #param << " _Verify " ; \
            if(ip)  std::cout << " ip: " << IpPort::IpSz(ip);    \
            std::cout << RESET << std::endl;    \
            return -1;  \
        }   \
    }   \
    else    \
    {   \
        std::cout << RED << "Config error in "<< #param << " length! " << #param << " : " <<  Info.param  \
        << " length is: " << Info.param.length() << " The range should be:" << minnum << " - " << maxnum ;  \
        if(ip)  std::cout << " ip: " << IpPort::IpSz(ip);    \
        std::cout << RESET << std::endl;    \
        return -2;  \
    }   \


int Config::_Verify(const Node& node)
{
    Dverify(node,name,0,45,node.publicIp,"^[\u4E00-\u9FA5A-Za-z0-9_]+$");
    Dverify(node,logo,0,512,node.publicIp,"^(http|https|ftp)\\://([a-zA-Z0-9\\.\\-]+(\\:[a-zA-Z0-9\\.&%\\$\\-]+)*@)?((25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.[a-zA-Z]{2,4})(\\:[0-9]+)?(/[^/][a-zA-Z0-9\\.\\,\?\'\\/\\+&%\\$#\\=~_\\-@]*)*$"); 
    return 0;
}

int Config::_Verify()
{
    Dverify(_info,name,0,45,0,"^[\u4E00-\u9FA5A-Za-z0-9_]+$");
    Dverify(_info,logo,0,512,0,"^(http|https|ftp)\\://([a-zA-Z0-9\\.\\-]+(\\:[a-zA-Z0-9\\.&%\\$\\-]+)*@)?((25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.[a-zA-Z]{2,4})(\\:[0-9]+)?(/[^/][a-zA-Z0-9\\.\\,\?\'\\/\\+&%\\$#\\=~_\\-@]*)*$"); 
    return 0;
}

int Config::_Filter()
{
    if(FilterIp(_ip) != 0)
    {
        return -1;
    }
    
    if(FilterServerPort(_serverPort) != 0)
    {
        return -2;
    }

    if(FilterHttpPort(_httpPort) != 0)
    {
        return -3;
    }

    for(std::string  server : _server)
    {
        if(FilterServerIp(server)  != 0)
        {
        return -4;
        }
    }
 
    if(FilterLog(_log) != 0)
    {
        return -5;
    }
    // if(!FilterBool(_rpc))
    // {
    //     return -6;
    // }
    // if(FilterVersion(_version) != 0)
    // {
    //     return -7;
    // }
    
    if(FilterHttpCallback(_httpCallback) != 0)
    {
        return -8;
    }
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
    if(_rpc == false)
    return false;
    else
    return true;
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
    std::ifstream configFile(Config::kConfigFilename);
    if (!configFile.good())
    {
        return -1;
    }
    tmpJson["ip"] = ip;
    
    std::ofstream outFile(Config::kConfigFilename);
    if (outFile.fail())
    {
        return -2;
    }
    outFile << tmpJson.dump(4);
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
template <typename T> 
int Config::FilterHttpPort(T & t)
{
    std::regex userPortRegex("^([1-9][0-9]{1,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
    std::string portStr = std::to_string(t);
   
    if (std::regex_match(portStr, userPortRegex) || portStr.empty())
    {
        return 0;
    }
    else 
    {
    cerr << RED << "http port format error" <<  RESET << std::endl;
    return -1;
    }
    
}

template <typename T> 
int Config::FilterServerPort(T & t)
{
    std::regex userPortRegex("^([1-9][0-9]{1,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
    std::string portStr = std::to_string(t);
   
    if (std::regex_match(portStr, userPortRegex) || portStr.empty() || portStr == "0")
    {
        return 0;
    }
    else 
    {
    cerr << RED << "server port format error" <<  RESET << std::endl;
    return -1;
    }
    
}


template <typename T> 
int Config::FilterIp(T & t)
{
    std::regex ipRegex("(\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b)");
    std::string ip = t;
    
    if(std::regex_match(ip, ipRegex) || ip.empty())
    {
         
        return 0;
    }
    else 
    {
    std::cerr << RED <<"ip address format error and has set default" << RESET << std::endl ;
    return -1;
    }
}

template <typename T> 
int Config::FilterServerIp(T & t)
{
    std::regex ipRegex("(\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b)");
    std::string ip = t;
    
    if(std::regex_match(ip, ipRegex) || ip.empty())
    {
        return 0;
    }
    else 
    {
    std::cerr << RED <<"server ip address format error and has set default" << RESET << std::endl ;
    return -1;
    }
}

int Config::FilterLog(Log &log)
{    
    std::string level = tmpJson["log"]["level"];
    std::regex regex_level("(debug|info|warn|error|trace|critical|off)", std::regex::icase);
    if (!std::regex_match(level, regex_level)) 
    {
        std::cerr << RED <<"log level input error " << RESET <<std::endl;
        return -1;
    }

    std::string path = tmpJson["log"]["path"];
    std::regex regex_path("^\\./logs$");
    if(!std::regex_match(path,regex_path))
    {
        std::cerr << RED <<"log path input error " << RESET << std::endl;
        return -2;
    }

    if(!FilterBool(log.console))
    {
        std::cerr << RED <<"log console is not bool type" << RESET << std::endl;
        return -3;
    }
    return 0;
  
}
int Config::FilterHttpCallback(HttpCallback &httpcallback)
{
    if(FilterIp(httpcallback.ip) != 0 )
    {
        return -1;
    }
    if(FilterServerPort(httpcallback.port) != 0)
    {
        return -2;
    }
    return 0;
}
template <typename T>
bool Config::FilterBool(T  &t)
{
    if(std::is_same<bool, T>::value)
    {
        return true;
    }
    else
    {
        return false;
    }
    
}

void Config::GetAllServerAddress()
{
    std::vector<Node> nodeList =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        tmpJson["server"].clear();
   	for (auto node : nodeList)
	{
        std::string ip = std::string(IpPort::IpSz(node.publicIp));
        std::vector<std::string> a = tmpJson["server"];
        if (std::find(a.begin(), a.end(), ip) != a.end())
        {
            continue;
        }
        else
        {
            tmpJson["server"].push_back(ip);
        }
    }

    std::ifstream f(kConfigFilename);
    
    if(f.good())
    {
        std::filesystem::remove(kConfigFilename);
        std::ofstream outFile(Config::kConfigFilename);
        outFile << tmpJson.dump(4);
        outFile.close();  
    }
    
}
void Config::_Check() 
{
    while(!_exitThread)  
    {
    std::this_thread::sleep_for(std::chrono::hours(1));
    GetAllServerAddress(); 
    }
    return ;
}


