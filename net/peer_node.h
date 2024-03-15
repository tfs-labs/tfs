/**
 * *****************************************************************************
 * @file        peer_node.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _PEER_NODE_H_
#define _PEER_NODE_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <list>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <shared_mutex>

#include "./define.h"
#include "./ip_port.h"

#include "../net/node.hpp"

using namespace std;

enum NodeType
{
	NODE_ALL,
	NODE_PUBLIC
};


class PeerNode
{
public:
	PeerNode() = default;
	~PeerNode() = default;

public:
	/**
	 * @brief       
	 * 
	 * @param       base58Addr 
	 * @param       x 
	 * @return      true 
	 * @return      false 
	 */
	bool FindNode(std::string const& base58Addr, Node& x);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       node 
	 * @return      true 
	 * @return      false 
	 */
	bool FindNodeByFd(int fd, Node& node);
	
	/**
	 * @brief       Get the Nodelist object
	 * 
	 * @param       type 
	 * @param       mustAlive 
	 * @return      std::vector<Node> 
	 */
	std::vector<Node> GetNodelist(NodeType type = NODE_ALL, bool mustAlive = false);
	std::vector<Node> GetNodelistByHeartBeat(NodeType type = NODE_ALL, bool mustAlive = false);

	/**
	 * @brief       Get the Nodelist object
	 * 
	 * @param       nodeAddrs 
	 * @param       type 
	 * @param       mustAlive 
	 */
	void GetNodelist(std::map<std::string, bool>& nodeAddrs, NodeType type = NODE_ALL, bool mustAlive = false);

	/**
	 * @brief       Get the Nodelist Size object
	 * 
	 * @return      uint64_t 
	 */
	uint64_t GetNodelistSize();

	/**
	 * @brief       
	 * 
	 * @param       base58Addr 
	 */
	void DeleteNode(std::string base58Addr);

	/**
	 * @brief       When adding to the node list fails close fd
	 * 
	 * @param       fd  
	 */
	void CloseFd(int fd);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 */
	void DeleteByFd(int fd);

	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @return      true 
	 * @return      false 
	 */
	bool Add(const Node &node);

	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @return      true 
	 * @return      false 
	 */
	bool Update(const Node &node);

	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @return      true 
	 * @return      false 
	 */
	bool AddOrUpdate(Node node);

	/**
	 * @brief       
	 * 
	 * @param       nodeList 
	 */
	void Print(std::vector<Node> &nodeList);

	/**
	 * @brief       
	 * 
	 * @param       node 
	 */
	void Print(const Node &node);

	/**
	 * @brief       
	 * 
	 * @param       nodeList 
	 * @return      std::string 
	 */
	std::string NodelistInfo(std::vector<Node> &nodeList);

	//Refresh threads
	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool NodelistRefreshThreadInit();

	/**
	 * @brief       
	 * 
	 */
	void NodelistSwitchThreadFun();

	//Thread functions
	/**
	 * @brief       
	 * 
	 */
	void NodelistRefreshThreadFun();

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool NodelistSwitchThread();

	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @return      int 
	 */
	int ConnectNode(Node & node);

	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @return      int 
	 */
	int DisconnectNode(Node & node);

	/**
	 * @brief       
	 * 
	 * @return      int 
	 */
	int DeleteOldVersionNode(); 

	/**
	 * @brief       
	 * 
	 * @param       ip 
	 * @param       port 
	 * @param       fd 
	 * @return      int 
	 */
	int DisconnectNode(uint32_t ip, uint16_t port, int fd);

	// Get the ID
	/**
	 * @brief       Get the Self Id object
	 * 
	 * @return      const std::string 
	 */
	const std::string GetSelfId();

	//Get the PUB
	/**
	 * @brief       Get the Self Pub object
	 * 
	 * @return      const std::string 
	 */
	const std::string GetSelfPub();

	/**
	 * @brief       Set the Self Id object
	 * 
	 * @param       base58Addr 
	 */
	void SetSelfId(const std::string &base58Addr);

	/**
	 * @brief       Set the Self Ip Public object
	 * 
	 * @param       publicIp 
	 */
	void SetSelfIpPublic(const u32 publicIp);

	/**
	 * @brief       Set the Self Ip Listen object
	 * 
	 * @param       listenIp 
	 */
	void SetSelfIpListen(const u32 listenIp);

	/**
	 * @brief       Set the Self Port Public object
	 * 
	 * @param       portPublic 
	 */
	void SetSelfPortPublic(const u16 portPublic);

	/**
	 * @brief       Set the Self Port Listen object
	 * 
	 * @param       portListen 
	 */

	/**
	 * @brief       Set the Self Port Listen object
	 * 
	 * @param       portListen 
	 */
	void SetSelfPortListen(const u16 portListen);

	/**
	 * @brief       Set the Self Height object
	 * 
	 * @param       height 
	 */
	void SetSelfHeight(u32 height);

	/**
	 * @brief       Set the Self Height object
	 * 
	 */
	void SetSelfHeight();

	/**
	 * @brief       Set the Self Ver object
	 * 
	 * @param       ver 
	 */
	void SetSelfVer(const std::string & ver);

	/**
	 * @brief       Set the Self Identity object
	 * 
	 * @param       identity 
	 */
	void SetSelfIdentity(const std::string & identity );

	/**
	 * @brief       Set the Self Name object
	 * 
	 * @param       name 
	 */
	void SetSelfName(const std::string &name);

	/**
	 * @brief       Set the Self Logo object
	 * 
	 * @param       logo 
	 */
	void SetSelfLogo(const std::string &logo);

	/**
	 * @brief       Get the Self Chain Height Newest object
	 * 
	 * @return      u32 
	 */
	u32 GetSelfChainHeightNewest();

	/**
	 * @brief       Get the Self Node object
	 * 
	 * @return      const Node 
	 */
	const Node GetSelfNode();

	/**
	 * @brief       Get the Base 5 8addr object
	 * 
	 * @return      const std::string 
	 */
	const std::string GetBase58Address();

	/**
	 * @brief       
	 * 
	 * @param       oldPub 
	 * @param       newPub 
	 * @return      int 
	 */
    int UpdateBase58Address(const string &oldPub, const std::string & newPub);

	/**
	 * @brief       
	 * 
	 */
	void StopNodesSwap() {_nodesSwapEnd = false;}

	
private:
    friend std::string PrintCache(int where);
	//List of public network nodes
	std::shared_mutex _mutexForNodes;
	std::map<std::string, Node> _nodeMap;

	std::mutex _mutexForCurr;
	Node _currNode;

	std::thread _refreshThread;
	std::thread _nodeSwitchThread;

	std::atomic<bool> _nodesSwapEnd = true;
};

#endif