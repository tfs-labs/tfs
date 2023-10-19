/**
 * *****************************************************************************
 * @file        ip_port.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
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

extern char g_localHostIp[16];
extern char g_public_net_ip[16];
extern int g_my_port;

class IpPort
{
public:
	/**
	 * @brief       Get the Local Host Ip object
	 * 
	 * @param       localHostIp 
	 * @return      true 
	 * @return      false 
	 */
	static bool GetLocalHostIp(std::string & localHostIp);
	
	/**
	 * @brief       
	 * 
	 * @param       szIp 
	 * @return      u32 
	 */
	static u32 IpNum(const char * szIp);

	/**
	 * @brief       
	 * 
	 * @param       szIp 
	 * @return      u32 
	 */
	static u32 IpNum(const std::string& szIp);

	/**
	 * @brief       
	 * 
	 * @param       numIp 
	 * @return      const char* 
	 */
	static const char * IpSz(const u32 numIp);

	/**
	 * @brief       
	 * 
	 * @param       strIp 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsValidIp(std::string const& strIp);

	/**
	 * @brief       
	 * 
	 * @param       strIp 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsLocalIp(std::string const& strIp);

	/**
	 * @brief       
	 * 
	 * @param       u32_ip 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsLocalIp(u32 u32_ip);

	/**
	 * @brief       
	 * 
	 * @param       strIp 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsPublicIp(std::string const& strIp);

	/**
	 * @brief       
	 * 
	 * @param       u32_ip 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsPublicIp(u32 u32_ip);

	/**
	 * @brief       
	 * 
	 * @param       u16_port 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsValidPort(u16 u16_port);

	/**
	 * @brief       Get the Peer Nip object
	 * 
	 * @param       sockConn 
	 * @return      u32 
	 */
	static u32 GetPeerNip(int sockConn);

	/**
	 * @brief       Get the Peer Port object
	 * 
	 * @param       sockConn 
	 * @return      u16 
	 */
	static u16 GetPeerPort(int sockConn);

	/**
	 * @brief       Get the Connect Port object
	 * 
	 * @param       conFd 
	 * @return      u16 
	 */
	static u16 GetConnectPort(int conFd);

	/**
	 * @brief       
	 * 
	 * @param       ipString 
	 * @return      true 
	 * @return      false 
	 */
	static bool IsLan(std::string const& ipString);

};


#endif