#ifndef _NET_API_H_
#define _NET_API_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <netinet/tcp.h>

#include <iostream>
#include <string>
#include <random>

#include "./peer_node.h"
#include "./pack.h"
#include "../../common/config.h"
#include "common/global.h"
#include "./ip_port.h"

#include "../include/logging.h"
#include "common.pb.h"
#include "./socket_buf.h"
#include "./global.h"
#include "handle_event.h"
#include "../utils/util.h"
#include "../proto/ca_protomsg.pb.h"

namespace net_tcp
{
	int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
	int Bind(int fd, const struct sockaddr *sa, socklen_t salen);
	int Connect(int fd, const struct sockaddr *sa, socklen_t salen);
	int Listen(int fd, int backlog);
	int Socket(int family, int type, int protocol);
	int Send(int sockfd, const void *buf, size_t len, int flags);
	int Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);
	int listen_server_init(int port, int listen_num);
	int set_fd_noblocking(int sockfd);
}

namespace net_data
{
	uint64_t pack_port_and_ip(uint16_t por, uint32_t ip);
	uint64_t pack_port_and_ip(int port, std::string ip);
	std::pair<uint16_t, uint32_t> apack_port_and_ip_to_int(uint64_t port_and_ip);
	std::pair<int, std::string> apack_port_and_ip_to_str(uint64_t port_and_ip);

}

namespace net_com
{
	using namespace net_tcp;
	using namespace net_data;

	enum class Compress : uint8_t
	{
		kCompress_False = 0,
		kCompress_True = 1
	};

	enum class Encrypt : uint8_t
	{
		kEncrypt_False = 0,
		kEncrypt_True = 1,
	};

	enum class Priority : uint8_t
	{
		kPriority_Low_0 = 0,
		kPriority_Low_1 = 2,
		kPriority_Low_2 = 4,

		kPriority_Middle_0 = 5,
		kPriority_Middle_1 = 8,
		kPriority_Middle_2 = 10,

		kPriority_High_0 = 11,
		kPriority_High_1 = 14,
		kPriority_High_2 = 15,
	};

	int connect_init(u32 u32_ip, u16 u16_port, u16 &connect_port);
	bool send_one_message(const Node &to, const net_pack &pack);
	bool send_one_message(const Node &to, const std::string &msg, const int8_t priority);
	bool send_one_message(const MsgData &to, const net_pack &pack);

	template <typename T>
	bool send_message(const std::string id,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	template <typename T>
	bool send_message(const Node &dest,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	template <typename T>
	bool send_message(const MsgData &from,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	template <typename T>
	bool broadcast_message(T &msg,
						   const net_com::Compress isCompress = net_com::Compress::kCompress_True,
						   const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
						   const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	bool BlockBroadcast_message( BuildBlockBroadcastMsg &BuildBlockMsg,
								const net_com::Compress isCompress = net_com::Compress::kCompress_True,
								const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
								const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	int parse_conn_kind(Node &to);

	bool is_need_send_trans_message(const Node &to);

	bool net_init();
	int input_send_one_message();
	bool test_send_big_data();
	int test_broadcast_message();

	bool SendPrintMsgReq(Node &to, const std::string data, int type = 0);
	bool SendPrintMsgReq(const std::string &id, const std::string data, int type = 0);
	int SendRegisterNodeReq(Node &dest, std::string &msg_id, bool get_nodelist);
	void SendPingReq(const Node &dest);
	void SendPongReq(const Node &dest);
	void DealHeart();
	bool SendSyncNodeReq(const Node &dest, std::string &msg_id);
	void SendNodeHeightChanged();
}

namespace net_callback
{
	void register_chain_height_callback(std::function<int(uint32_t &)> callback);
	extern std::function<int(uint32_t &)> chain_height_callback;
}

template <typename T>
bool net_com::send_message(const Node &dest, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	CommonMsg comm_msg;
	Pack::InitCommonMsg(comm_msg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);

	net_pack pack;
	Pack::common_msg_to_pack(comm_msg, (uint8_t)priority, pack);

	return net_com::send_one_message(dest, pack);
}

template <typename T>
bool net_com::send_message(const std::string id, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->find_node(id, node);
	if (find)
	{
		return net_com::send_message(node, msg, isCompress, isEncrypt, priority);
	}
	else
	{
		Node transNode;
		transNode.base58address = id;
		return net_com::send_message(transNode, msg, isCompress, isEncrypt, priority);
	}
}

template <typename T>
bool net_com::send_message(const MsgData &from, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->find_node_by_fd(from.fd, node);
	if (find)
	{
		return net_com::send_message(node, msg, isCompress, isEncrypt, priority);
	}
	else
	{
		CommonMsg comm_msg;
		Pack::InitCommonMsg(comm_msg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);

		net_pack pack;
		Pack::common_msg_to_pack(comm_msg, (uint8_t)priority, pack);
		return net_com::send_one_message(from, pack);
	}
}

template <typename T>
bool net_com::broadcast_message(T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	BroadcastMsgReq req;
	req.set_priority(static_cast<uint32_t>(priority));

	CommonMsg comm_msg;
	Pack::InitCommonMsg(comm_msg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);
	req.set_data(comm_msg.SerializeAsString());

	NodeInfo *pNodeInfo = req.mutable_from();
	const Node &selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	pNodeInfo->set_pub(selfNode.pub);
	pNodeInfo->set_base58addr(selfNode.base58address);

	const std::vector<Node> &&publicNodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	if (global::kBuildType == global::BuildType::kBuildType_Dev)
	{
		std::cout << "Total number of public nodelists:" << publicNodeList.size() << std::endl;
	}
	if (publicNodeList.empty())
	{
		ERRORLOG("publicNodeList is empty!");
		return false;
	}

	const double threshold = 0.25;
	const std::size_t cntUnConnected = std::count_if(publicNodeList.cbegin(), publicNodeList.cend(), [](const Node &node)
													 { return node.fd == -1; });
	double percent = static_cast<double>(cntUnConnected) / publicNodeList.size();
	if (percent > threshold)
	{
		ERRORLOG("Unconnected nodes are {},accounting for {}%", cntUnConnected, percent * 100);
		return false;
	}
	std::cout << "Verification passed, start broadcasting!" << std::endl;

	// Send to public nodelist
	for (auto &item : publicNodeList)
	{
		if (req.from().base58addr() != item.base58address)
		{
			net_com::send_message(item, req);
		}
	}
	return true;
}

#endif
