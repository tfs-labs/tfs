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
	std::string     address     = "";
	std::string     pub               = "";
	std::string     sign              = "";
	std::string     identity          = "";
	std::string     name			  = "";
	std::string 	logo			  = "";
	std::string		ver			      = "";
	uint32_t        listenIp                = 0;
	uint32_t        listenPort              = 0;
	uint32_t        publicIp                = 0;
	uint32_t        publicPort              = 0;
	uint32_t        height                   = 0;
	ConnKind 	    connKind                = NOTYET;
	int32_t         fd                       = -1;
	int32_t         pulse             = HEART_PROBES;

	Node(){}
	Node(std::string nodeAddress)
	{
		address = nodeAddress;
	}

	bool operator==(const Node &obj) const
	{
		return address == obj.address;
	}

	bool operator>(const Node &obj)
	{
		return (*this).address > obj.address;
	}

	void ResetHeart()
	{
		pulse = HEART_PROBES;
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
			<< "  addr(" << "0x"+ address << ")"
			<< "  pulse(" << pulse << ")"
			<< "  height( " << height << " )"
			<< "  name(" << name << ")"
			<< "  version(" << ver << ")"
			<< "  logo(" << logo << ")"
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