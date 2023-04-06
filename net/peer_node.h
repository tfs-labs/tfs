#ifndef _PEER_NODE_H_
#define _PEER_NODE_H_

#include <map>
#include <list>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include "./define.h"
#include "./ip_port.h"
#include <shared_mutex>
#include "node.hpp"

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
	bool find_node(std::string const& base58addr, Node& x);
	bool find_node_by_fd(int fd, Node& node_);
	
	std::vector<Node> get_nodelist(NodeType type = NODE_ALL, bool mustAlive = false);
	void get_nodelist(std::map<std::string, bool>& NodeAddrs, NodeType type = NODE_ALL, bool mustAlive = false);

	void delete_node(std::string base58addr);
	void delete_by_fd(int fd);

	bool add(const Node &_node);
	bool update(const Node &_node);
	bool add_or_update(Node _node);
	void print(std::vector<Node> &nodelist);
	void print(const Node &node);
	std::string nodelist_info(std::vector<Node> &nodelist);


	//Refresh threads
	bool nodelist_refresh_thread_init();
	void nodelist_switch_thread_fun();

	//Thread functions
	void nodelist_refresh_thread_fun();
	bool nodelist_switch_thread();

	int connect_node(Node & node);
	int disconnect_node(Node & node);
	int disconnect_node(uint32_t ip, uint16_t port, int fd);


	// Get the ID
	const std::string get_self_id();

	//Get the PUB
	const std::string get_self_pub();
	void set_self_id(const std::string &base58addr);
	void set_self_ip_p(const u32 public_ip);
	void set_self_ip_l(const u32 listen_ip);
	void set_self_port_p(const u16 port_p);
	void set_self_port_l(const u16 port_l);
	void set_self_height(u32 height);
	void set_self_height();
	void set_self_ver(const std::string & ver);

	void set_self_identity(const std::string & identity );
	void set_self_name(const std::string &name_);
	void set_self_logo(const std::string &logo_);

	u32 get_self_chain_height_newest();
	const Node get_self_node();
	const std::string get_base58addr();
    int update_base58Addr(const string &oldpub, const std::string & newpub);

	
private:

	//List of public network nodes
	std::shared_mutex mutex_for_nodes_;
	std::map<std::string, Node> node_map_;

	std::mutex mutex_for_curr_;
	Node curr_node_;

	std::thread refresh_thread;
	std::thread node_switch_thread;
};

#endif