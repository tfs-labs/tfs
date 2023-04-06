#ifndef _NODE_H_
#define _NODE_H_

#include <string>
#include <iostream>
#include <sstream>
#include "define.h"
#include "ip_port.h"


enum ConnKind
{
	NOTYET, //Not yet connected
	DRTO2O, //Outer and external direct connection
	PASSIV	//Passively accept connections
};

class Node
{
public:
	std::string     base58address     = "";
	std::string     pub               = "";
	std::string     sign              = "";
	std::string 	public_base58addr = "";
	std::string     identity          = "";
	std::string     name			  = "";
	std::string 	logo			  = "";
	std::string		ver			      = "";
	uint32_t      listen_ip                = 0;
	uint32_t      listen_port              = 0;
	uint32_t      public_ip                = 0;
	uint32_t      public_port              = 0;
	uint32_t      height                   = 0;
	uint64_t      time_stamp				  = 0;
	ConnKind conn_kind                = NOTYET;
	int32_t      fd                       = -1;
	int32_t      heart_probes             = HEART_PROBES;

	Node()
	{
	}
	Node(std::string node_base58address)
	{
		base58address = node_base58address;
	}

	bool operator==(const Node &obj) const
	{
		return base58address == obj.base58address;
	}

	bool operator>(const Node &obj) //
	{
		return (*this).base58address > obj.base58address;
	}

	void ResetHeart()
	{
		heart_probes = HEART_PROBES;
	}
	void print()
	{
		std::cout << info_str() << std::endl;
	}

	std::string info_str()
	{
		
		std::ostringstream oss;
		oss
			<< "  ip(" << std::string(IpPort::ipsz(public_ip)) << ")"
			<< "  port(" << public_port << ")"
			<< "  ip_l(" << std::string(IpPort::ipsz(listen_ip)) << ")"
			<< "  port_l(" << listen_port << ")"
			<< "  kind(" << conn_kind << ")"
			<< "  fd(" << fd << ")"
			<< "  base58(" << base58address << ")"
			<< "  heart_probes(" << heart_probes << ")"
			<< "  height( " << height << " )"
			<< "  name(" << name << ")"
			<< "  version(" << ver << ")"
			<< std::endl;
		return oss.str();
	}
	bool is_connected() const
	{
		return fd > 0 || fd == -2;
	}
	void set_height(uint32_t height)
	{
		this->height = height;
	}
};

#endif 