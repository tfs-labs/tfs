#include <string.h>
#include "../include/net_interface.h"
#include "../include/logging.h"
#include "./ip_port.h"

#include "./peer_node.h"
#include "./pack.h"
#include "./net_api.h"
#include "net.pb.h"

//Obtaining the ID of your own node returns 0 successfully
std::string net_get_self_node_id()
{
	return MagicSingleton<PeerNode>::GetInstance()->get_self_id();
}

Node net_get_self_node() 
{
    return MagicSingleton<PeerNode>::GetInstance()->get_self_node();
}

//Returns all IDs of the K bucket
std::vector<std::string> net_get_node_ids()
{
	std::vector<std::string> ids;
	auto nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	for(auto& node:nodelist)
	{
		ids.push_back(node.base58address);
	}
	return ids;
}

double net_get_connected_percent()
{
	auto nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	int num = 0;
	for(auto& node:nodelist)
	{
		if(node.fd > 0 || node.fd == -2)
		{
			num++;
		}	
	}
	int total_num = nodelist.size();
	return total_num == 0 ? 0 : num/total_num;
}

//Returns the public network node
std::vector<Node> net_get_public_node()
{
	std::vector<Node> vnodes;
	auto nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist(NODE_PUBLIC);
	for(auto& node: nodelist)
	{
		if(node.fd > 0)
		{
			vnodes.push_back(node);
		}
	}
	return vnodes;
}

//Returns all public network nodes, regardless of whether they are connected or not
std::vector<Node> net_get_all_public_node()
{
	std::vector<Node> vnodes = MagicSingleton<PeerNode>::GetInstance()->get_nodelist(NODE_PUBLIC, false);
	return vnodes;
}

//Returns all node IDs and base58address
std::map<std::string, string> net_get_node_ids_and_base58address()
{
	std::vector<Node> nodelist;
	nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();

	std::map<std::string, string> res;
	for(auto& node:nodelist)
	{
		res[node.base58address] = node.base58address;
	}
	return res;
}

//Find the ID by IP
std::string net_get_ID_by_ip(std::string ip)
{
	auto nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist(NODE_PUBLIC);
	for(auto& node:nodelist)
	{	
		
		if(std::string(IpPort::ipsz(node.public_ip)) == ip)
		{
			return node.base58address;
		}
	}

	return std::string();
}


void net_send_node_height_changed()
{
	net_com::SendNodeHeightChanged();
}
