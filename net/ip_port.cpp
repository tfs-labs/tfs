#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <regex>
#include "./ip_port.h"
#include "./peer_node.h"
#include "../../include/logging.h"
#include <netdb.h> 
#include <stdlib.h>
#include <unistd.h>
#include "../utils/util.h"

char g_localhost_ip[16];
char g_public_net_ip[16];
char g_publicIP[30];             //IP Store the public IP address
int g_my_port;

//Ip /Get Local IP
bool IpPort::get_localhost_ip(std::string & localhost_ip)
{
	struct ifaddrs* if_addr_ptr = NULL;
	struct ifaddrs* if_addr_ptr_copy = NULL;
	getifaddrs(&if_addr_ptr);
	if_addr_ptr_copy = if_addr_ptr;

	void* tmp_addr_ptr = NULL;
	char local_ip_str[IP_LEN] = {0};

	while (if_addr_ptr != NULL)
	{
		if (if_addr_ptr->ifa_addr->sa_family == AF_INET)
		{
			tmp_addr_ptr = &((struct sockaddr_in*)if_addr_ptr->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, tmp_addr_ptr, local_ip_str, IP_LEN);
			INFOLOG("Get local ip");
			INFOLOG("==========={}==============", local_ip_str);
			
			u32 local_ip = IpPort::ipnum(local_ip_str);
			if (htonl(INADDR_LOOPBACK) != local_ip 
				&& htonl(INADDR_BROADCAST) != local_ip 
				&& IpPort::is_local_ip(local_ip)
				)
			{
				INFOLOG("==========={}==============", local_ip_str);
				INFOLOG("Belong local IP");

				localhost_ip = local_ip_str;
				freeifaddrs(if_addr_ptr_copy);
				return true;
			}
		}
		if_addr_ptr = if_addr_ptr->ifa_next;
	}
	freeifaddrs(if_addr_ptr_copy);
	return false;
}

u32 IpPort::ipnum(const char* sz_ip)
{
	u32 num_ip = inet_addr(sz_ip);
	return num_ip;
}
u32 IpPort::ipnum(const string& sz_ip)
{
	u32 num_ip = inet_addr(sz_ip.c_str());
	return num_ip;
}

const char* IpPort::ipsz(const u32 num_ip)
{
	struct in_addr addr = {0};
	memcpy(&addr, &num_ip, sizeof(u32));
	const char* sz_ip = inet_ntoa(addr);
	return sz_ip;
}

bool IpPort::is_valid_ip(std::string const& str_ip)
{
	try
	{
		string pattern = "^(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|[1-9])\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\.(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)$";
		std::regex re(pattern);
		bool rst = std::regex_search(str_ip, re);
		return rst;
	}
	catch (std::regex_error const& e)
	{
		DEBUGLOG("is_valid_ip e{})", e.what());
	}
	return false;
}

bool IpPort::is_local_ip(std::string const& str_ip)
{
	if (false == is_valid_ip(str_ip))
		return false;

	try
	{
		std::string pattern{ "^(127\\.0\\.0\\.1)|(localhost)|(10\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})|(172\\.((1[6-9])|(2\\d)|(3[01]))\\.\\d{1,3}\\.\\d{1,3})|(192\\.168\\.\\d{1,3}\\.\\d{1,3})$" };
		std::regex re(pattern);
		bool rst = std::regex_search(str_ip, re);
		return rst;
	}
	catch (std::regex_error const& e)
	{
		DEBUGLOG("is_valid_ip e({})", e.code());
	}
	return false;
}

bool IpPort::is_local_ip(u32 u32_ip)
{
	string str_ip = ipsz(u32_ip);
	return is_local_ip(str_ip);
}

bool IpPort::is_public_ip(std::string const& str_ip)
{
	if (false == is_valid_ip(str_ip))
		return false;

	return false == is_local_ip(str_ip);
}

bool IpPort::is_public_ip(u32 u32_ip)
{
	string str_ip = ipsz(u32_ip);
	return is_public_ip(str_ip);
}

bool IpPort::is_valid_port(u16 u16_port)
{
	return u16_port > 0 && u16_port <= 65535;
}

u32 IpPort::get_peer_nip(int sockconn)
{
	struct sockaddr_in sa { 0 };
	socklen_t len = sizeof(sa);
	if (!getpeername(sockconn, (struct sockaddr*) & sa, &len))
		return ipnum(inet_ntoa(sa.sin_addr));
	else
		DEBUGLOG("get_peer_nip getpeername fail.");

	return 0;
}

u16 IpPort::get_peer_port(int sockconn)
{
	struct sockaddr_in sa { 0 };
	socklen_t len;
	if (!getpeername(sockconn, (struct sockaddr*) & sa, &len))
		return ntohs(sa.sin_port);
	else
		DEBUGLOG("get_peer_port getpeername fail.");

	return 0;
}

//Gets the local address on the connection represented by sockfd
u16 IpPort::get_connect_port(int confd)
{
	struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
	if(!getsockname(confd, (struct sockaddr*)&clientAddr, &clientAddrLen))
	{
		return ntohs(clientAddr.sin_port);
	}
	else
	{
		DEBUGLOG("get_connect_port getsockname fail.");
	} 
	return 0;
}

bool IpPort::isLAN(std::string const& ipString)
{
	#if PRIMARYCHAIN || TESTCHAIN
	istringstream st(ipString);
	int ip[2];
	for(int i = 0; i < 2; i++)
	{
		string temp;
		getline(st,temp,'.');
		istringstream a(temp);
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