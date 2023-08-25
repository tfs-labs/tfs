#include "./peer_node.h"
#include "../include/logging.h"
#include "./ip_port.h"
#include "./net_api.h"
#include <chrono>
#include "./epoll_mode.h"
#include <sys/time.h>
#include <bitset>
#include "./global.h"
#include "utils/base58.h"
#include "utils/console.h"
#include "net/unregister_node.h"
#include "net/global.h"
#include "common/config.h"
#include "unregister_node.h"

bool PeerNode::add(const Node& _node)
{
	std::unique_lock<std::shared_mutex> lck(mutex_for_nodes_);
	if(_node.base58address.size() == 0)
	{
		return false;
	} 
	if(MagicSingleton<Config>::GetInstance()->verify(_node) != 0)
	{
		return false;
	}
	auto itr = node_map_.find(_node.base58address);
	if (itr != node_map_.end())
	{
		return false;
	}
	this->node_map_[_node.base58address] = _node;

	return true;
}

bool PeerNode::update(const Node & _node)
{
	{
		std::unique_lock<std::shared_mutex> lck(mutex_for_nodes_);

		auto itr = this->node_map_.find(_node.base58address);
		if (itr == this->node_map_.end())
		{
			return false;
		}
		this->node_map_[_node.base58address] = _node;
	}

	return true;
}

bool PeerNode::add_or_update(Node _node)
{
	std::unique_lock<std::shared_mutex> lck(mutex_for_nodes_);
	this->node_map_[_node.base58address] = _node;
	
	return true;
}

void PeerNode::delete_node(std::string base58addr)
{
	std::unique_lock<std::shared_mutex> lck(mutex_for_nodes_);
	auto nodeIt = node_map_.find(base58addr);
	
	if (nodeIt != node_map_.end())
	{
		int fd = nodeIt->second.fd;
		if(fd > 0)
		{
			MagicSingleton<EpollMode>::GetInstance()->delete_epoll_event(fd);
			close(fd);
		}	
		u32 ip = nodeIt->second.public_ip;
		u16 port = nodeIt->second.public_port;
		if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(ip, port))
		{
			ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({}) " RESET, IpPort::ipsz(ip), port);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->deleteConsensusNode(nodeIt->first);			
		nodeIt = node_map_.erase(nodeIt);
	}
	else
	{
		DEBUGLOG("Not found base58 {} in node_map_", base58addr);
	}
}

void PeerNode::delete_by_fd(int fd)
{
	std::unique_lock<std::shared_mutex> lck(mutex_for_nodes_);
	auto nodeIt = node_map_.begin();
	for(; nodeIt != node_map_.end(); ++nodeIt)
	{
        if(nodeIt->second.fd == fd)
		{
			break;
		}	
    }

	u32 ip = nodeIt->second.public_ip;
	u16 port = nodeIt->second.public_port;

	if (nodeIt != node_map_.end())
	{
		if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(ip, port))
		{
			ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({})" RESET, IpPort::ipsz(ip), port);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->deleteConsensusNode(nodeIt->first);		
		nodeIt = node_map_.erase(nodeIt);
	}
	else
	{
		if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(fd))
		{
			ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({})" RESET, IpPort::ipsz(ip), port);
		}
	}

	if(fd > 0)
	{
		MagicSingleton<EpollMode>::GetInstance()->delete_epoll_event(fd);
		close(fd);
	}	
}


void PeerNode::print(std::vector<Node> & nodelist)
{
	std::cout << std::endl;
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodelist)
	{
		i.print();
	}
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	std::cout << "PeerNode size is: " << nodelist.size() << std::endl;
}

void PeerNode::print(const Node & node)
{
	std::cout << "---------------------------------- node info --------------------------------------------------------------------------" << std::endl;
	
	std::cout << "public_ip: " << IpPort::ipsz(node.public_ip) << std::endl;
	std::cout << "local_ip: " << IpPort::ipsz(node.listen_ip) << std::endl;
	std::cout << "public_port: " << node.public_port << std::endl;
	std::cout << "local_port: " << node.listen_port << std::endl;
	std::cout << "conn_kind: " << node.conn_kind << std::endl;
	std::cout << "fd: " << node.fd << std::endl;
	std::cout << "heart_probes: " << node.heart_probes << std::endl;
	std::cout << "base58address: " << node.base58address << std::endl;
	std::cout << "chain_height: " << node.height << std::endl;
	std::cout << "public_node_id: " << node.public_base58addr << std::endl;

	std::cout << "---------------------------------- end --------------------------------------------------------------------------" << std::endl;
}

std::string PeerNode::nodelist_info(std::vector<Node> & nodelist)
{
	std::ostringstream oss;	
	oss << std::endl;
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodelist)
	{
		oss << i.info_str();
	}
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	oss << "PeerNode size is: " << nodelist.size() << std::endl;
	return oss.str();
}

bool PeerNode::find_node_by_fd(int fd, Node &node_)
{
	std::shared_lock<std::shared_mutex> lck(mutex_for_nodes_);
	for (auto node : node_map_)
	{
		if (node.second.fd == fd)
		{
			node_ = node.second;
			return true;
		}
	}
	return false;
}

// find node
bool PeerNode::find_node(std::string const &base58addr, Node &x)
{
	std::shared_lock<std::shared_mutex> lck(mutex_for_nodes_);

	string str_id = base58addr;
	auto it = node_map_.find(str_id);
	if (it != node_map_.end())
	{
		x = it->second;
		return true;
	}
	return false;
}

vector<Node> PeerNode::get_nodelist(NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(mutex_for_nodes_);
	vector<Node> rst;
	auto cb = node_map_.cbegin(), ce = node_map_.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.is_connected())
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
void PeerNode::get_nodelist(std::map<std::string, bool>& NodeAddrs, NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(mutex_for_nodes_);
	auto cb = node_map_.cbegin(), ce = node_map_.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.is_connected())
				{
					NodeAddrs[cb->first] = false;
				}
			}else{
				NodeAddrs[cb->first] = false;
			}
		}
	}
	return;
}

uint64_t PeerNode::get_nodelist_size()
{
	std::shared_lock<std::shared_mutex> lck(mutex_for_nodes_);
	return node_map_.size();
}

// Refresh threads
extern atomic<int> nodelist_refresh_time;
bool PeerNode::nodelist_refresh_thread_init()
{
	refresh_thread = std::thread(std::bind(&PeerNode::nodelist_refresh_thread_fun, this));
	refresh_thread.detach();
	return true;
}

//Network nodes swap threads
bool PeerNode::nodelist_switch_thread()
{
	node_switch_thread = std::thread(std::bind(&PeerNode::nodelist_switch_thread_fun, this));
	node_switch_thread.detach();
	return true;
}

void PeerNode::nodelist_switch_thread_fun()
{
	do
	{
		sleep(global::nodelist_refresh_time);
		if(!nodes_swap_end)
		{
			return;
		}
		// std::vector<Node> NodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
		// if(NodeList.size() == 0)
		// {
		// 	PeerNode::nodelist_refresh_thread_fun();
		// }
		// else
		// {
			MagicSingleton<UnregisterNode>::GetInstance()->StartSyncNode();
		// }	

	} while (true);
}

//Thread functions
void PeerNode::nodelist_refresh_thread_fun()
{
	std::lock_guard<std::mutex> lck(global::mutex_listen_thread);
	while(!global::listen_thread_inited)
	{
		global::cond_listen_thread.wait(global::mutex_listen_thread);
	}
	auto config_server_list = MagicSingleton<Config>::GetInstance()->GetServer();
	int port = MagicSingleton<Config>::GetInstance()->GetServerPort();
	
	std::map<std::string, int> server_list;
	for (auto & config_server_ip: config_server_list)
	{
		server_list.insert(std::make_pair(config_server_ip, port));
	}

	MagicSingleton<UnregisterNode>::GetInstance()->StartRegisterNode(server_list);
}

int PeerNode::connect_node(Node & node)
{
	if (node.fd > 0)
	{
		return -1;
	}

	net_com::parse_conn_kind(node);
	INFOLOG("node.conn_kind: {}", node.conn_kind);

	uint32_t u32_ip = node.public_ip;
	uint16_t port = node.listen_port;

	u16 connect_port;
	int cfd = net_com::connect_init(u32_ip, port, connect_port);
	if (cfd <= 0)
	{
		return -2;
	}

	DEBUGLOG("||||connect_node: ip:({}) port:({}) ",IpPort::ipsz(u32_ip),connect_port);
	node.fd = cfd;
	node.public_port = connect_port;
	MagicSingleton<BufferCrol>::GetInstance()->add_buffer(u32_ip, connect_port, cfd); 
	MagicSingleton<EpollMode>::GetInstance()->add_epoll_event(cfd, EPOLLIN | EPOLLET | EPOLLOUT);
	
	return 0;
}

int PeerNode::disconnect_node(Node & node)
{
	if (node.fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->delete_epoll_event(node.fd);
	close(node.fd);
	if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(node.public_ip, node.public_port))
	{
		ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({})" RESET, IpPort::ipsz(node.public_ip), node.public_port);
	}			
	return 0;
}

int PeerNode::disconnect_node(uint32_t ip, uint16_t port, int fd)
{
	if (fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->delete_epoll_event(fd);
	close(fd);
	if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(ip, port))
	{
		ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({})" RESET, IpPort::ipsz(ip), port);
	}			
	return 0;
}

//Get the ID
const std::string PeerNode::get_self_id()
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	return curr_node_.base58address;
}

//Get pub
const std::string PeerNode::get_self_pub()
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	return curr_node_.pub;
}

// Set the ID
void PeerNode::set_self_id(const std::string &base58addr)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.base58address = base58addr;
}

// Set the node ID
void PeerNode::set_self_identity(const std::string & identity)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.identity = identity;
}

//Set the node name
void PeerNode::set_self_name(const std::string &name_)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.name = name_;
}

//Set the node logo
void PeerNode::set_self_logo(const std::string &logo_)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.logo = logo_;
}

// Set the IP
void PeerNode::set_self_ip_p(const u32 public_ip)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.public_ip = public_ip;
}

// Set the IP
void PeerNode::set_self_ip_l(const u32 listen_ip)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.listen_ip = listen_ip;
}


//Set the port
void PeerNode::set_self_port_p(const u16 port_p)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.public_port = port_p;
}


// Set the port
void PeerNode::set_self_port_l(const u16 port_l)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.listen_port = port_l;
}

void PeerNode::set_self_height(u32 height)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
    curr_node_.set_height(height);
}

void PeerNode::set_self_height()
{
	if (net_callback::chain_height_callback)
	{
		uint32_t height = 0;
		int ret = net_callback::chain_height_callback(height);
		if (ret >= 0)
		{
            set_self_height(height);
			net_com::SendNodeHeightChanged();
		}
	}
	else
	{
		INFOLOG("Set self chain height: callback empty!!!");
	}
}

void PeerNode::set_self_ver(const std::string & ver)
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	curr_node_.ver = ver;
}

u32 PeerNode::get_self_chain_height_newest()
{
	// Update to newest chain height
    set_self_height();
	return curr_node_.height;
}


// Own node
const Node PeerNode::get_self_node()
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	return curr_node_;
}


// Get the base58 address
const std::string PeerNode::get_base58addr()
{
	std::lock_guard<std::mutex> lck(mutex_for_curr_);
	return curr_node_.base58address;
}

int PeerNode::update_base58Addr(const string &oldpub, const std::string & newpub)
{
	if (oldpub.size() == 0 || newpub.size() == 0)
	{
		return -1;
	}
	
	std::string oldBase58Addr = GetBase58Addr(oldpub, Base58Ver::kBase58Ver_Normal);
	std::string newBase58Addr = GetBase58Addr(newpub, Base58Ver::kBase58Ver_Normal);
	
	Node node;
	if (!find_node(oldBase58Addr, node))
	{
		return -2;
	}
	
	node.base58address = newBase58Addr;
	node.identity = newpub;

	delete_node(oldBase58Addr);
	if (!add(node))
	{
		return -3;
	}
	return 0;
}