/**
 * *****************************************************************************
 * @file        api.h
 * @brief       
 * @author  ()
 * @date        2023-09-25
 * @copyright   tfsc
 * *****************************************************************************
 */
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
#include "./ip_port.h"
#include "./pack.h"
#include "./socket_buf.h"
#include "./global.h"
#include "./handle_event.h"

#include "../common/config.h"
#include "../common/global.h"
#include "../include/logging.h"
#include "../proto/common.pb.h"
#include "../utils/util.h"
#include "../proto/ca_protomsg.pb.h"
/**
 * @brief       
 * 
 */
namespace net_tcp
{
	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salenptr 
	 * @return      int 
	 */
	int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
	
	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salen 
	 * @return      int 
	 */
	int Bind(int fd, const struct sockaddr *sa, socklen_t salen);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salen 
	 * @return      int 
	 */
	int Connect(int fd, const struct sockaddr *sa, socklen_t salen);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       backLog 
	 * @return      int 
	 */
	int Listen(int fd, int backLog);

	/**
	 * @brief       
	 * 
	 * @param       family 
	 * @param       type 
	 * @param       protocol 
	 * @return      int 
	 */
	int Socket(int family, int type, int protocol);

	/**
	 * @brief       
	 * 
	 * @param       sockfd 
	 * @param       buf 
	 * @param       len 
	 * @param       flags 
	 * @return      int 
	 */
	int Send(int sockfd, const void *buf, size_t len, int flags);

	/**
	 * @brief       Set the Socket Option object
	 * 
	 * @param       fd 
	 * @param       level 
	 * @param       optName 
	 * @param       optVal 
	 * @param       optLen 
	 * @return      int 
	 */
	int SetSocketOption(int fd, int level, int optName, const void *optVal, socklen_t optLen);

	/**
	 * @brief       
	 * 
	 * @param       port 
	 * @param       listenNum 
	 * @return      int 
	 */
	int ListenServerInit(int port, int listenNum);

	/**
	 * @brief       Set the Fd No Blocking object
	 * 
	 * @param       sockfd 
	 * @return      int 
	 */
	int SetFdNoBlocking(int sockfd);
}
namespace net_data
{
	/**
	 * @brief       
	 * 
	 * @param       por 
	 * @param       ip 
	 * @return      uint64_t 
	 */
	uint64_t DataPackPortAndIp(uint16_t por, uint32_t ip);

	/**
	 * @brief       
	 * 
	 * @param       port 
	 * @param       ip 
	 * @return      uint64_t 
	 */
	uint64_t DataPackPortAndIp(int port, std::string ip);

	/**
	 * @brief       
	 * 
	 * @param       portAndIp 
	 * @return      std::pair<uint16_t, uint32_t> 
	 */
	std::pair<uint16_t, uint32_t> DataPackPortAndIpToInt(uint64_t portAndIp);
	
	/**
	 * @brief       
	 * 
	 * @param       portAndIp 
	 * @return      std::pair<int, std::string> 
	 */
	std::pair<int, std::string> DataPackPortAndIpToString(uint64_t portAndIp);

}
/**
 * @brief       
 * 
 */
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
	/**
	 * @brief       
	 * 
	 * @param       u32_ip 
	 * @param       u16_port 
	 * @param       connectedPort 
	 * @return      int 
	 */
	int InitConnection(u32 u32_ip, u16 u16_port, u16 &connectedPort);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       pack 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const Node &to, const NetPack &pack);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       msg 
	 * @param       priority 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const Node &to, const std::string &msg, const int8_t priority);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       pack 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const MsgData &to, const NetPack &pack);

	template <typename T>
	/**
	 * @brief       
	 * 
	 */
	bool SendMessage(const std::string id,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool SendMessage(const Node &dest,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool SendMessage(const MsgData &from,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::kCompress_True,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
					  const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool BroadCastMessage(T &msg,
						   const net_com::Compress isCompress = net_com::Compress::kCompress_True,
						   const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
						   const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	/**
	 * @brief       
	 * 
	 */
	bool BroadBroadcastMessage( BuildBlockBroadcastMsg &BuildBlockMsg,
								const net_com::Compress isCompress = net_com::Compress::kCompress_True,
								const net_com::Encrypt isEncrypt = net_com::Encrypt::kEncrypt_False,
								const net_com::Priority priority = net_com::Priority::kPriority_Low_0);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @return      int 
	 */
	int AnalysisConnectionKind(Node &to);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @return      true 
	 * @return      false 
	 */
	bool IsNeedSendTransactionMessage(const Node &to);

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool InitializeNetwork();

	/**
	 * @brief       
	 * 
	 * @return      int 
	 */
	int SendOneMessageByInput();

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool SendBigDataByTest();

	/**
	 * @brief       
	 * 
	 * @return      int 
	 */
	int BroadCastMessageByTest();

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       data 
	 * @param       type 
	 * @return      true 
	 * @return      false 
	 */
	bool SendPrintMsgReq(Node &to, const std::string data, int type = 0);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 * @param       data 
	 * @param       type 
	 * @return      true 
	 * @return      false 
	 */
	bool SendPrintMsgReq(const std::string &id, const std::string data, int type = 0);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 * @param       msgId 
	 * @param       isGetNodeList 
	 * @return      int 
	 */
	int SendRegisterNodeReq(Node &dest, std::string &msgId, bool isGetNodeList);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 */
	void SendPingReq(const Node &dest);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 */
	void SendPongReq(const Node &dest);

	/**
	 * @brief       
	 * 
	 */
	void DealHeart();

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 * @param       msgId 
	 * @return      true 
	 * @return      false 
	 */
	bool SendSyncNodeReq(const Node &dest, std::string &msgId);

	/**
	 * @brief       
	 * 
	 */
	void SendNodeHeightChanged();
}

namespace net_callback
{
	/**
	 * @brief       
	 * 
	 * @param       callback 
	 */
	void RegisterChainHeightCallback(std::function<int(uint32_t &)> callback);
	extern std::function<int(uint32_t &)> chainHeightCallback;
}


template <typename T>
/**
 * @brief       
 * 
 * @param       dest 
 * @param       msg 
 * @param       isCompress 
 * @param       isEncrypt 
 * @param       priority 
 * @return      true 
 * @return      false 
 */
bool net_com::SendMessage(const Node &dest, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	CommonMsg commonMsg;

	Pack::InitCommonMsg(commonMsg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);
	NetPack pack;

	Pack::PackCommonMsg(commonMsg, (uint8_t)priority, pack);
	return net_com::SendOneMessage(dest, pack);
}

template <typename T>
bool net_com::SendMessage(const std::string id, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(id, node);
	if (find)
	{
		return net_com::SendMessage(node, msg, isCompress, isEncrypt, priority);
	}
	else
	{
		Node transNode;
		transNode.base58Address = id;
		return net_com::SendMessage(transNode, msg, isCompress, isEncrypt, priority);
	}
}

template <typename T>
bool net_com::SendMessage(const MsgData &from, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node);
	if (find)
	{
		return net_com::SendMessage(node, msg, isCompress, isEncrypt, priority);
	}
	else
	{
		CommonMsg comm_msg;
		Pack::InitCommonMsg(comm_msg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);

		NetPack pack;
		Pack::PackCommonMsg(comm_msg, (uint8_t)priority, pack);
		return net_com::SendOneMessage(from, pack);
	}
}

/**
 * @brief       
 * 
 */
template <typename T>
bool net_com::BroadCastMessage(T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	BroadcastMsgReq broadcastMsgReq;
	broadcastMsgReq.set_priority(static_cast<uint32_t>(priority));

	CommonMsg commonMsg;
	Pack::InitCommonMsg(commonMsg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);
	broadcastMsgReq.set_data(commonMsg.SerializeAsString());

	NodeInfo *pNodeInfo = broadcastMsgReq.mutable_from();
	const Node &selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	pNodeInfo->set_pub(selfNode.pub);
	pNodeInfo->set_base58addr(selfNode.base58Address);

	const std::vector<Node> &&publicNodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	if (global::kBuildType == global::BuildType::kBuildType_Dev)
	{
		// std::cout << "Total number of public nodelists :" << publicNodeList.size() << std::endl;
		INFOLOG("Total number of public nodelists: {}",  publicNodeList.size());
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
	// std::cout << "Verification passed, start broadcasting!" << std::endl;
	INFOLOG("Verification passed, start broadcasting!");

	// Send to public nodelist
	for (auto &item : publicNodeList)
	{
		if (broadcastMsgReq.from().base58addr() != item.base58Address)
		{
			net_com::SendMessage(item, broadcastMsgReq);
		}
	}
	return true;
}

#endif
