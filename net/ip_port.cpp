#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <regex>
#include <netdb.h> 
#include <stdlib.h>
#include <unistd.h>
#include "../utils/util.h"
#include "./ip_port.h"
#include "./peer_node.h"
#include "../../include/logging.h"



//Ip /Get Local IP
bool IpPort::GetLocalHostIp(std::string & localHostIp)
{
	struct ifaddrs* ifAddrPtr = NULL;
	struct ifaddrs* ifAddrPtrCopy = NULL;
	if(getifaddrs(&ifAddrPtr) != 0)
	{
		std::cout << "Please fill in the IP address in the configuration file" << std::endl;
		return false;
	}
	ifAddrPtrCopy = ifAddrPtr;

	void* tmpAddrPtr = NULL;
	char localIpStr[IP_LEN] = {0};

	while (ifAddrPtr != NULL)
	{
		if (ifAddrPtr->ifa_addr != NULL && ifAddrPtr->ifa_addr->sa_family == AF_INET)
		{
			tmpAddrPtr = &((struct sockaddr_in*)ifAddrPtr->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, tmpAddrPtr, localIpStr, IP_LEN);
			INFOLOG("Get local ip");
			INFOLOG("==========={}==============", localIpStr);
			
			u32 local_ip = IpPort::IpNum(localIpStr);
			if (htonl(INADDR_LOOPBACK) != local_ip 
				&& htonl(INADDR_BROADCAST) != local_ip 
				&& IpPort::IsLocalIp(local_ip)
				)
			{
				INFOLOG("==========={}==============", localIpStr);
				INFOLOG("Belong local IP");

				localHostIp = localIpStr;
				freeifaddrs(ifAddrPtrCopy);
				return true;
			}
		}
		ifAddrPtr = ifAddrPtr->ifa_next;
	}
	freeifaddrs(ifAddrPtrCopy);
	return false;
}

u32 IpPort::IpNum(const char* szIp)
{
	u32 numIp = inet_addr(szIp);
	return numIp;
}
u32 IpPort::IpNum(const std::string& szIp)
{
	u32 numIp = inet_addr(szIp.c_str());
	return numIp;
}

const char* IpPort::IpSz(const u32 numIp)
{
	struct in_addr addr = {0};
	memcpy(&addr, &numIp, sizeof(u32));
	const char* szIp = inet_ntoa(addr);
	return szIp;
}

bool IpPort::IsValidIp(std::string const& strIp)
{
	try
	{
		std::string pattern = "^(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|[1-9])\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)$";
		std::regex re(pattern);
		bool rst = std::regex_search(strIp, re);
		return rst;
	}
	catch (std::regex_error const& e)
	{
		DEBUGLOG("IsValidIp e{})", e.what());
	}
	return false;
}

bool IpPort::IsLocalIp(std::string const& strIp)
{
	if (false == IsValidIp(strIp))
		return false;

	try
	{
		std::string pattern{ "^(127\\.0\\.0\\.1)|(localhost)|(10\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})|(172\\.((1[6-9])|(2\\d)|(3[01]))\\.\\d{1,3}\\.\\d{1,3})|(192\\.168\\.\\d{1,3}\\.\\d{1,3})$" };
		std::regex re(pattern);
		bool rst = std::regex_search(strIp, re);
		return rst;
	}
	catch (std::regex_error const& e)
	{
		DEBUGLOG("IsValidIp e({})", e.code());
	}
	return false;
}

bool IpPort::IsLocalIp(u32 u32_ip)
{
	std::string strIp = IpSz(u32_ip);
	return IsLocalIp(strIp);
}

bool IpPort::IsPublicIp(std::string const& strIp)
{
	if (false == IsValidIp(strIp))
		return false;

	return false == IsLocalIp(strIp);
}

bool IpPort::IsPublicIp(u32 u32_ip)
{
	std::string strIp = IpSz(u32_ip);
	return IsPublicIp(strIp);
}

bool IpPort::IsValidPort(u16 u16_port)
{
	return u16_port > 0 && u16_port <= 65535;
}

u32 IpPort::GetPeerNip(int sockConn)
{
	struct sockaddr_in sa { 0 };
	socklen_t len = sizeof(sa);
	if (!getpeername(sockConn, (struct sockaddr*) & sa, &len))
		return IpNum(inet_ntoa(sa.sin_addr));
	else
		DEBUGLOG("GetPeerNip getpeername fail.");

	return 0;
}

u16 IpPort::GetPeerPort(int sockConn)
{
	struct sockaddr_in sa { 0 };
	socklen_t len;
	if (!getpeername(sockConn, (struct sockaddr*) & sa, &len))
		return ntohs(sa.sin_port);
	else
		DEBUGLOG("GetPeerPort getpeername fail.");

	return 0;
}

//Gets the local address on the connection represented by sockfd
u16 IpPort::GetConnectPort(int conFd)
{
	struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
	if(!getsockname(conFd, (struct sockaddr*)&clientAddr, &clientAddrLen))
	{
		return ntohs(clientAddr.sin_port);
	}
	else
	{
		DEBUGLOG("GetConnectPort getsockname fail.");
	} 
	return 0;
}

bool IpPort::IsLan(std::string const& ipString)
{
	#if PRIMARYCHAIN || TESTCHAIN
	std::istringstream st(ipString);
	int ip[2];
	for(int i = 0; i < 2; i++)
	{
		std::string temp;
		getline(st,temp,'.');
		std::istringstream a(temp);
		a >> ip[i];
	}
	if((ip[0]==10) || (ip[0]==172 && ip[1]>=16 && ip[1]<=31) || (ip[0]==192 && ip[1]==168))
	{
		return true;
	}
	return false;
	# endif

	return false;
}
