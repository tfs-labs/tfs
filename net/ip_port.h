
#ifndef _IP_PORT_H_
#define _IP_PORT_H_

#include <ifaddrs.h>
#include <assert.h>
#include <sys/types.h>		/* basic system data types */
#include <arpa/inet.h>		/* inet(3) functions */
#include <cstdlib>
#include <string>
#include <sstream>
#include "./define.h"

extern char g_localhost_ip[16];
extern char g_public_net_ip[16];
extern int g_my_port;

class IpPort
{
public:
	//Get the local IP
	static bool get_localhost_ip(std::string & localhost_ip);


	static u32 ipnum(const char * sz_ip);
	static u32 ipnum(const std::string& sz_ip);
	static const char * ipsz(const u32 num_ip);

	static bool is_valid_ip(std::string const& str_ip);

	static bool is_local_ip(std::string const& str_ip);

	static bool is_local_ip(u32 u32_ip);

	static bool is_public_ip(std::string const& str_ip);

	static bool is_public_ip(u32 u32_ip);

	static bool is_valid_port(u16 u16_port);

	static u32 get_peer_nip(int sockconn);


	static u16 get_peer_port(int sockconn);

	static u16 get_connect_port(int confd);

	static bool isLAN(std::string const& ipString);

};


#endif