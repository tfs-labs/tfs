#include "./peer_node.h"

#include <cstdint>
#include <sys/time.h>
#include <chrono>
#include <bitset>

#include "./ip_port.h"
#include "./api.h"
#include "./epoll_mode.h"
#include "./global.h"
#include "./unregister_node.h"

#include "../include/logging.h"
#include "../utils/base58.h"
#include "../utils/console.h"
#include "../net/unregister_node.h"
#include "../net/global.h"
#include "../common/config.h"

bool PeerNode::Add(const Node& node)
{
	std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
	if(node.base58Address.size() == 0)
	{
		return false;
	} 
	if(MagicSingleton<Config>::GetInstance()->_Verify(node) != 0)
	{
		return false;
	}
	auto itr = _nodeMap.find(node.base58Address);
	if (itr != _nodeMap.end())
	{
		return false;
	}
	this->_nodeMap[node.base58Address] = node;

	return true;
}

bool PeerNode::Update(const Node & node)
{
	{
		std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
		auto itr = this->_nodeMap.find(node.base58Address);
		if (itr == this->_nodeMap.end())
		{
			return false;
		}
		this->_nodeMap[node.base58Address] = node;
	}

	return true;
}

bool PeerNode::AddOrUpdate(Node node)
{
	std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
	this->_nodeMap[node.base58Address] = node;
	
	return true;
}

void PeerNode::DeleteNode(std::string base58Addr)
{
	std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
	auto nodeIt = _nodeMap.find(base58Addr);
	
	if (nodeIt != _nodeMap.end())
	{
		int fd = nodeIt->second.fd;
		if(fd > 0)
		{
			MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
			close(fd);
		}	
		u32 ip = nodeIt->second.publicIp;
		u16 port = nodeIt->second.publicPort;
		if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(ip, port))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({}) " RESET, IpPort::IpSz(ip), port);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->DeleteSpiltNodeList(nodeIt->first);			
		nodeIt = _nodeMap.erase(nodeIt);
	}
	else
	{
		DEBUGLOG("Not found base58 {} in _nodeMap", base58Addr);
	}
}

void PeerNode::CloseFd(int fd)
{
	if(fd <= 0)
	{
		return;
	}

	MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(fd); 
	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
	int ret = close(fd);
	if(ret != 0)
	{
		DEBUGLOG("CloseFd close error, fd = {}, errno = {}, ret = ",fd , errno, ret);
	}
}

void PeerNode::DeleteByFd(int fd)
{
	std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
	auto nodeIt = _nodeMap.begin();
	for(; nodeIt != _nodeMap.end(); ++nodeIt)
	{
        if(nodeIt->second.fd == fd)
		{
			break;
		}	
    }

	u32 ip = nodeIt->second.publicIp;
	u16 port = nodeIt->second.publicPort;

	if (nodeIt != _nodeMap.end())
	{
		if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(ip, port))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(ip), port);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->DeleteSpiltNodeList(nodeIt->first);		
		nodeIt = _nodeMap.erase(nodeIt);
	}
	else
	{
		if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(fd))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(ip), port);
		}
	}

	if(fd > 0)
	{
		MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
		close(fd);
	}	
}


void PeerNode::Print(std::vector<Node> & nodeList)
{
	std::cout << std::endl;
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodeList)
	{
		i.Print();
	}
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	std::cout << "PeerNode size is: " << nodeList.size() << std::endl;
}

void PeerNode::Print(const Node & node)
{
	std::cout << "---------------------------------- node info --------------------------------------------------------------------------" << std::endl;
	
	std::cout << "publicIp: " << IpPort::IpSz(node.publicIp) << std::endl;
	std::cout << "localIp: " << IpPort::IpSz(node.listenIp) << std::endl;
	std::cout << "publicPort: " << node.publicPort << std::endl;
	std::cout << "localPort: " << node.listenPort << std::endl;
	std::cout << "connKind: " << node.connKind << std::endl;
	std::cout << "fd: " << node.fd << std::endl;
	std::cout << "heartProbes: " << node.heartProbes << std::endl;
	std::cout << "base58Address: " << node.base58Address << std::endl;
	std::cout << "chainHeight: " << node.height << std::endl;
	std::cout << "publicNodeId: " << node.publicBase58Addr << std::endl;

	std::cout << "---------------------------------- end --------------------------------------------------------------------------" << std::endl;
}

std::string PeerNode::NodelistInfo(std::vector<Node> & nodeList)
{
	std::ostringstream oss;	
	oss << std::endl;
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodeList)
	{
		oss << i.InfoStr();
	}
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	oss << "PeerNode size is: " << nodeList.size() << std::endl;
	return oss.str();
}

bool PeerNode::FindNodeByFd(int fd, Node &node)
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);
	for (auto x : _nodeMap)
	{
		if (x.second.fd == fd)
		{
			node = x.second;
			return true;
		}
	}
	return false;
}

// find node
bool PeerNode::FindNode(std::string const &base58Addr, Node &x)
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);

	string strId = base58Addr;
	auto it = _nodeMap.find(strId);
	if (it != _nodeMap.end())
	{
		x = it->second;
		return true;
	}
	return false;
}

vector<Node> PeerNode::GetNodelist(NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);
	vector<Node> rst;
	auto cb = _nodeMap.cbegin(), ce = _nodeMap.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.IsConnected())
				{
					rst.push_back(cb->second);
				}
			}else{
				rst.push_back(cb->second);
			}
		}
	}
	return rst;
}
std::vector<Node> PeerNode::GetNodelistByHeartBeat(NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);
	vector<Node> rst;
	auto cb = _nodeMap.cbegin(), ce = _nodeMap.cend();
	uint32_t continueNum = 0;
	for (; cb != ce; ++cb)
	{
		if(cb->second.heartProbes < HEART_PROBES)
		{
			++continueNum;
			continue;
		}

		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.IsConnected())
				{
					rst.push_back(cb->second);
				}
			}else{
				rst.push_back(cb->second);
			}
		}
	}
	DEBUGLOG("GetNodelistByHeartBeat continueNum: {}", continueNum);
	return rst;
}

void PeerNode::GetNodelist(std::map<std::string, bool>& nodeAddrs, NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);
	auto cb = _nodeMap.cbegin(), ce = _nodeMap.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.IsConnected())
				{
					nodeAddrs[cb->first] = false;
				}
			}else{
				nodeAddrs[cb->first] = false;
			}
		}
	}
	return;
}

uint64_t PeerNode::GetNodelistSize()
{
	std::shared_lock<std::shared_mutex> lck(_mutexForNodes);
	return _nodeMap.size();
}

// Refresh threads
extern atomic<int> g_nodelistRefreshTime;
bool PeerNode::NodelistRefreshThreadInit()
{
	_refreshThread = std::thread(std::bind(&PeerNode::NodelistRefreshThreadFun, this));
	_refreshThread.detach();
	return true;
}

//Network nodes swap threads
bool PeerNode::NodelistSwitchThread()
{
	_nodeSwitchThread = std::thread(std::bind(&PeerNode::NodelistSwitchThreadFun, this));
	_nodeSwitchThread.detach();
	return true;
}

void PeerNode::NodelistSwitchThreadFun()
{
	do
	{
		sleep(global::g_nodelistRefreshTime);
		if(!_nodesSwapEnd)
		{
			return;
		}
		// std::vector<Node> NodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
		// if(NodeList.size() == 0)
		// {
		// 	PeerNode::NodelistRefreshThreadFun();
		// }
		// else
		// {
			MagicSingleton<UnregisterNode>::GetInstance()->StartSyncNode();
		// }
	} while (true);
}

//Thread functions
void PeerNode::NodelistRefreshThreadFun()
{
	std::lock_guard<std::mutex> lck(global::g_mutexListenThread);
	while(!global::g_ListenThreadInited)
	{
		global::g_condListenThread.wait(global::g_mutexListenThread);
	}
	auto configServerList = MagicSingleton<Config>::GetInstance()->GetServer();
	int port = MagicSingleton<Config>::GetInstance()->GetServerPort();
	
	std::map<std::string, int> server_list;
	for (auto & configServerIp: configServerList)
	{
		server_list.insert(std::make_pair(configServerIp, port));
	}

	MagicSingleton<UnregisterNode>::GetInstance()->StartRegisterNode(server_list);
}

int PeerNode::ConnectNode(Node & node)
{
	if (node.fd > 0)
	{
		return -1;
	}

	net_com::AnalysisConnectionKind(node);
	INFOLOG("node.connKind: {}", node.connKind);

	uint32_t u32_ip = node.publicIp;
	uint16_t port = node.listenPort;

	u16 connectPort;
	int cfd = net_com::InitConnection(u32_ip, port, connectPort);
	if (cfd <= 0)
	{
		return -2;
	}

	DEBUGLOG("||||ConnectNode: ip:({}) port:({}) ",IpPort::IpSz(u32_ip),connectPort);
	node.fd = cfd;
	node.publicPort = connectPort;
	MagicSingleton<BufferCrol>::GetInstance()->AddBuffer(u32_ip, connectPort, cfd); 
	MagicSingleton<EpollMode>::GetInstance()->EpollLoop(cfd, EPOLLIN | EPOLLET | EPOLLOUT);
	
	return 0;
}

int PeerNode::DisconnectNode(Node & node)
{
	if (node.fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(node.fd);
	close(node.fd);
	if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(node.publicIp, node.publicPort))
	{
		ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(node.publicIp), node.publicPort);
	}			
	return 0;
}

int PeerNode::DeleteOldVersionNode()
{
	// std::vector<Node> nodeList = this->GetNodelist();
	// for(auto& t : nodeList)
	// {
	// 	std::vector<std::string> vRecvVersion;
	// 	StringUtil::SplitString(t.ver, "_", vRecvVersion);
	// 	if (vRecvVersion.size() < 1 || vRecvVersion.size() > 3 )
	// 	{
	// 		ERRORLOG(" version error: -2");
	// 		return -1;
	// 	}
	// 	if(vRecvVersion[1] < global::kLinuxCompatible)
	// 	{
	// 		int ret = this->DisconnectNode(t);
	// 		if(ret)
	// 		{	
	// 			ERRORLOG("DisconnectNode error, error num:{}", ret);
	// 			return ret;
	// 		}
	// 	}
	// }
	return 0;
}

int PeerNode::DisconnectNode(uint32_t ip, uint16_t port, int fd)
{
	if (fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
	close(fd);
	if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(ip, port))
	{
		ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(ip), port);
	}			
	return 0;
}

//Get the ID
const std::string PeerNode::GetSelfId()
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	return _currNode.base58Address;
}

//Get pub
const std::string PeerNode::GetSelfPub()
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	return _currNode.pub;
}

// Set the ID
void PeerNode::SetSelfId(const std::string &base58Addr)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.base58Address = base58Addr;
}

// Set the node ID
void PeerNode::SetSelfIdentity(const std::string & identity)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.identity = identity;
}

//Set the node name
void PeerNode::SetSelfName(const std::string &name)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.name = name;
}

//Set the node logo
void PeerNode::SetSelfLogo(const std::string &logo)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.logo = logo;
}

// Set the IP
void PeerNode::SetSelfIpPublic(const u32 publicIp)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.publicIp = publicIp;
}

// Set the IP
void PeerNode::SetSelfIpListen(const u32 listenIp)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.listenIp = listenIp;
}


//Set the port
void PeerNode::SetSelfPortPublic(const u16 portPublic)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.publicPort = portPublic;
}


// Set the port
void PeerNode::SetSelfPortListen(const u16 portListen)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.listenPort = portListen;
}

void PeerNode::SetSelfHeight(u32 height)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
    _currNode.SetHeight(height);
}

void PeerNode::SetSelfHeight()
{
	if (net_callback::chainHeightCallback)
	{
		uint32_t height = 0;
		int ret = net_callback::chainHeightCallback(height);
		if (ret >= 0)
		{
            SetSelfHeight(height);
			net_com::SendNodeHeightChanged();
		}
	}
	else
	{
		INFOLOG("Set self chain height: callback empty!!!");
	}
}

void PeerNode::SetSelfVer(const std::string & ver)
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	_currNode.ver = ver;
}

u32 PeerNode::GetSelfChainHeightNewest()
{
	// Update to newest chain height
    SetSelfHeight();
	return _currNode.height;
}


// Own node
const Node PeerNode::GetSelfNode()
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	return _currNode;
}


// Get the base58 address
const std::string PeerNode::GetBase58Address()
{
	std::lock_guard<std::mutex> lck(_mutexForCurr);
	return _currNode.base58Address;
}

int PeerNode::UpdateBase58Address(const string &oldPub, const std::string & newPub)
{
	if (oldPub.size() == 0 || newPub.size() == 0)
	{
		return -1;
	}
	
	std::string oldBase58Addr = GetBase58Addr(oldPub, Base58Ver::kBase58Ver_Normal);
	std::string newBase58Addr = GetBase58Addr(newPub, Base58Ver::kBase58Ver_Normal);
	
	Node node;
	if (!FindNode(oldBase58Addr, node))
	{
		return -2;
	}
	
	node.base58Address = newBase58Addr;
	node.identity = newPub;

	DeleteNode(oldBase58Addr);
	if (!Add(node))
	{
		return -3;
	}
	return 0;
}