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
	std::string     base58Address     = "";
	std::string     pub               = "";
	std::string     sign              = "";
	std::string 	publicBase58Addr = "";
	std::string     identity          = "";
	std::string     name			  = "";
	std::string 	logo			  = "";
	std::string		ver			      = "";
	uint32_t        listenIp                = 0;
	uint32_t        listenPort              = 0;
	uint32_t        publicIp                = 0;
	uint32_t        publicPort              = 0;
	uint32_t        height                   = 0;
	uint64_t        timeStamp				  = 0;
	ConnKind 	    connKind                = NOTYET;
	int32_t         fd                       = -1;
	int32_t         heartProbes             = HEART_PROBES;

	Node(){}
	Node(std::string nodeBase58Address)
	{
		base58Address = nodeBase58Address;
	}

	bool operator==(const Node &obj) const
	{
		return base58Address == obj.base58Address;
	}

	bool operator>(const Node &obj) //
	{
		return (*this).base58Address > obj.base58Address;
	}

	void ResetHeart()
	{
		heartProbes = HEART_PROBES;
	}
	void Print()
	{
		std::cout << InfoStr() << std::endl;
	}

	std::string InfoStr()
	{
		
		std::ostringstream oss;
		oss
			<< "  ip(" << std::string(IpPort::IpSz(publicIp)) << ")"
			<< "  port(" << publicPort << ")"
			<< "  ip_l(" << std::string(IpPort::IpSz(listenIp)) << ")"
			<< "  port_l(" << listenPort << ")"
			<< "  kind(" << connKind << ")"
			<< "  fd(" << fd << ")"
			<< "  base58(" << base58Address << ")"
			<< "  heartProbes(" << heartProbes << ")"
			<< "  height( " << height << " )"
			<< "  name(" << name << ")"
			<< "  version(" << ver << ")"
			<< std::endl;
		return oss.str();
	}
	bool IsConnected() const
	{
		return fd > 0 || fd == -2;
	}
	void SetHeight(uint32_t height)
	{
		this->height = height;
	}
};

#endif 