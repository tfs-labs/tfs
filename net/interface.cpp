
#include "../net/interface.h"

#include <string.h>

#include "./ip_port.h"
#include "./peer_node.h"
#include "./pack.h"
#include "./api.h"


#include "../include/logging.h"
#include "../proto/net.pb.h"

//Obtaining the ID of your own node returns 0 successfully
std::string NetGetSelfNodeId()
{
	return MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
}

Node NetGetSelfNode() 
{
    return MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
}

//Returns all IDs of the K bucket
std::vector<std::string> NetGetNodeIds()
{
	std::vector<std::string> ids;
	auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	for(auto& node:nodeList)
	{
		ids.push_back(node.base58Address);
	}
	return ids;
}

double NetGetConnectedPercent()
{
	auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	int num = 0;
	for(auto& node:nodeList)
	{
		if(node.fd > 0 || node.fd == -2)
		{
			num++;
		}	
	}
	int totalNum = nodeList.size();
	return totalNum == 0 ? 0 : num/totalNum;
}

//Returns the public network node
std::vector<Node> NetGetPublicNode()
{
	std::vector<Node> vnodes;
	auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist(NODE_PUBLIC);
	for(auto& node: nodeList)
	{
		if(node.fd > 0)
		{
			vnodes.push_back(node);
		}
	}
	return vnodes;
}

//Returns all public network nodes, regardless of whether they are connected or not
std::vector<Node> NetGetAllPublicNode()
{
	std::vector<Node> vnodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist(NODE_PUBLIC, false);
	return vnodes;
}

//Returns all node IDs and base58Address
std::map<std::string, string> NetGetNodeIdsAndBase58Address()
{
	std::vector<Node> nodeList;
	nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

	std::map<std::string, string> res;
	for(auto& node:nodeList)
	{
		res[node.base58Address] = node.base58Address;
	}
	return res;
}

//Find the ID by IP
std::string NetGetIdByIp(std::string ip)
{
	auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist(NODE_PUBLIC);
	for(auto& node:nodeList)
	{	
		
		if(std::string(IpPort::IpSz(node.publicIp)) == ip)
		{
			return node.base58Address;
		}
	}

	return std::string();
}


void NetSendNodeHeightChanged()
{
	net_com::SendNodeHeightChanged();
}
