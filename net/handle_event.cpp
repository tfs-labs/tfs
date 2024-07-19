#include "handle_event.h"

#include <cstddef>
#include <exception>
#include <fstream>
#include <unordered_set>
#include <utility>
#include <iomanip>

#include "./pack.h"
#include "./ip_port.h"
#include "./global.h"


#include "../common/config.h"
#include "../common/global.h"
#include "../common/global_data.h"
#include "../db/db_api.h"
#include "../net/dispatcher.h"
#include "../net/peer_node.h"
#include "../net/socket_buf.h"
#include "../net/test.hpp"
#include "../net/unregister_node.h"
#include "../net/interface.h"
#include "../include/logging.h"
#include "../include/scope_guard.h"
#include "../proto/common.pb.h"
#include "../proto/net.pb.h"
#include "../utils/account_manager.h"
#include "../utils/console.h"
#include "../utils/cycliclist.hpp"
#include "../utils/hex_code.h"
#include "../utils/magic_singleton.h"
#include "../utils/tmp_log.h"
#include "../utils/contract_utils.h"

int HandlePrintMsgReq(const std::shared_ptr<PrintMsgReq> &printMsgReq, const MsgData &from)
{
	static int printMsNum = 0;
	int type = printMsgReq->type();
	if (type == 0)
	{
		std::cout << ++printMsNum << " times data:" << printMsgReq->data() << std::endl;
	}
	else
	{
		std::ofstream file("bigdata.txt", std::fstream::out);
		file << printMsgReq->data();
		file.close();
		std::cout << "write bigdata.txt success!!!" << std::endl;
	}
	return 0;
}

static std::mutex nodeMutex;
int HandleRegisterNodeReq(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from)
{
	INFOLOG("HandleRegisterNodeReq");
	DEBUGLOG("HandleRegisterNodeReq from.ip:{} from.port:{} from.fd:{}", IpPort::IpSz(from.ip),from.port,from.fd);

	Node node;
	int ret = 0;
	ON_SCOPE_EXIT{
		if (ret != 0)
		{
			DEBUGLOG("register node failed and disconnect. ret:{}", ret);
			MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(node);
		}
	};

	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	std::lock_guard<std::mutex> lock(nodeMutex);
	NodeInfo *nodeInfo = registerNode->mutable_mynode();
	
	std::string dest_pub = nodeInfo->pub();
	std::string destAddr = nodeInfo->addr();
	
	node.fd = from.fd;
	node.address = destAddr;
	node.identity = nodeInfo->identity();
	node.name = nodeInfo->name();
	node.logo = nodeInfo->logo();
	node.pub = dest_pub;
	node.sign = nodeInfo->sign();
	node.listenIp = selfNode.listenIp;
	node.listenPort = SERVERMAINPORT;
	node.publicIp = from.ip;
	node.publicPort = from.port;
	node.height = nodeInfo->height();
	node.ver = nodeInfo->version();

	//MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(node);

	//Multiple registration of the same IP address is prohibited
	std::vector<Node> pubNodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	auto result = std::find_if(pubNodeList.begin(), pubNodeList.end(),[&from](auto & node){ return from.ip == node.publicIp;});
	if(result != pubNodeList.end())
	{
		return ret -= 1;
	}
		
	//Prohibit intranet node connection
	if(IpPort::IsLan(IpPort::IpSz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::IpSz(from.ip));
		return ret -= 2;
	}

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeInfo->version()))
    {
        ERRORLOG("Incompatible version!");
        return ret -= 3;
    }

	if(node.address == MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr())
    {
        ERRORLOG("Don't register yourself ");
        return ret -= 4;
    }

	EVP_PKEY* eckey = nullptr;


	std::string node_addr = nodeInfo->addr();
	if (!isValidAddress(node_addr))
	{
		ERRORLOG("address invalid !!");
		return ret -= 5;
	}

	std::string Addr = GenerateAddr(nodeInfo->identity());
	//std::cout <<"get identity"<<std::endl;
	//std::cout << Str2Hex(nodeInfo->identity());
	if (Addr != node_addr)
	{
		 std::cout <<"===========================hhhhhhhhhhhhh"<< std::endl;
		 //std::cout <<std::hex << Str2Hex(nodeInfo->identity()) <<std::endl;
		 std::cout << "Addr is " << Addr;
		 std::cout << "node_addr is " <<node_addr;
		ERRORLOG("address error !!");
		return ret -= 6;
	}

	if(nodeInfo->identity().size() <= 0)
	{
		ERRORLOG("public key is empty");
		return ret -= 7;
	}

	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(nodeInfo->identity(), account) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return ret -= 8;
	}


	std::string nodeInfo_signature = nodeInfo->sign();
	if(account.Verify(Getsha256hash(Addr), nodeInfo_signature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return ret -= 9;
	}

	
	Node temNode;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(node.address, temNode);
	if ((find && temNode.connKind == NOTYET) || !find)
	{
		node.connKind = PASSIV;
	}
	else
	{
		node.connKind = temNode.connKind;
	}

	if (find)
	{
		//Determine whether fd is consistent with 
		if (temNode.fd != from.fd)
		{
			if (MagicSingleton<BufferCrol>::GetInstance()->IsExists(temNode.publicIp, temNode.publicPort) /*&& temNode.is_established()*/)
			{
				// Join to the peernode
				std::shared_ptr<SocketBuf> ptr = MagicSingleton<BufferCrol>::GetInstance()->GetSocketBuf(temNode.publicIp, temNode.publicPort);
				temNode.fd = ptr->fd;
				temNode.connKind = PASSIV;
				MagicSingleton<PeerNode>::GetInstance()->Update(node);
			}

			MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(temNode);
		}
	}
	else
	{
		if (from.ip == node.publicIp && from.port == node.publicPort)
		{
			MagicSingleton<PeerNode>::GetInstance()->Add(node);
		}
	}

	selfNode.publicIp = from.ip;
	selfNode.publicPort = from.port;
	
	
	std::string signature;

	Account acc;
	if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		return ret -= 10;
	}

	if (selfNode.address != acc.GetAddr())
	{
		return ret -= 11;
	}
	if(!acc.Sign(Getsha256hash(acc.GetAddr()), signature))
	{
		return ret -= 12;
	}

	selfNode.identity = acc.GetPubStr();
	selfNode.sign = signature;

	RegisterNodeAck registerNodeAck;
	registerNodeAck.set_msg_id(registerNode->msg_id());
	std::vector<Node> nodeList;

	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	nodeList.push_back(selfNode);
	if(registerNode->is_get_nodelist() == true)
	{
		nodeList.insert(nodeList.end(), tmp.begin(), tmp.end());
	}

	for (auto &node : nodeList)
	{		
		if (node.fd < 0) 
		{
			if (node.address != selfNode.address)
			{
				ERRORLOG("node  addr is same of self node  addr");
				continue;
			}
		}

		NodeInfo *nodeInfo = registerNodeAck.add_nodes();

		nodeInfo->set_addr(node.address);
		nodeInfo->set_name(node.name);
		nodeInfo->set_logo(node.logo);
		nodeInfo->set_listen_ip(node.listenIp);
		nodeInfo->set_listen_port(node.listenPort);
		nodeInfo->set_public_ip(node.publicIp);
		nodeInfo->set_public_port(node.publicPort);
		nodeInfo->set_identity(node.identity);
		nodeInfo->set_sign(node.sign);
		nodeInfo->set_height(node.height);
		nodeInfo->set_version(node.ver);
	}

	net_com::SendMessage(destAddr, registerNodeAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	return ret;
}

int HandleRegisterNodeAck(const std::shared_ptr<RegisterNodeAck> &registerNodeAck, const MsgData &from)
{
	if(IpPort::IsLan(IpPort::IpSz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::IpSz(from.ip));
		return -1;
	}

	DEBUGLOG("from.ip:{}", IpPort::IpSz(from.ip));
	registerNodeAck->set_from_ip(from.ip);
	registerNodeAck->set_from_port(from.port);
	registerNodeAck->set_fd(from.fd);
	std::string ipPort = std::to_string(from.ip) + ":" + std::to_string(from.port);
	GLOBALDATAMGRPTR.AddWaitData(registerNodeAck->msg_id(), ipPort, registerNodeAck->SerializeAsString());
	return 0;
}

int VerifyRegisterNode(const NodeInfo &nodeInfo, uint32_t &fromIp, uint32_t &fromPort)
{
	if(IpPort::IsLan(IpPort::IpSz(fromIp)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::IpSz(fromIp));
		return -1;
	}

	INFOLOG("HandleRegisterNodeAck");

	auto self_node = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	Node node;
	node.address = nodeInfo.addr();
	node.identity = nodeInfo.identity();
	node.name = nodeInfo.name();
	node.logo = nodeInfo.logo();
	node.pub = nodeInfo.pub();
	node.sign = nodeInfo.sign();
	node.listenIp = self_node.listenIp;
	node.listenPort = SERVERMAINPORT;
	node.publicIp = fromIp;
	node.publicPort = fromPort;
	node.height = nodeInfo.height();
	node.ver = nodeInfo.version();

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeInfo.version()))
    {
        ERRORLOG("Incompatible version!");
        return -2;
    }

	if(!isValidAddress(nodeInfo.addr()))
	{
		return -3;
	}

	std::string Addr = GenerateAddr(node.identity);
	if (Addr != node.address)
	{
		// std::cout << "===========================vvvvvvvv"<<std::endl;
		// std::cout <<std::hex << Str2Hex(node.pub) <<std::endl;
		// std::cout << "vAddr is " << Addr;
		// std::cout << "vnode_addr is " <<node.address;
		return -4;
	}
	
	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(node.identity, account) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -5;
	}

	if(account.Verify(Getsha256hash(Addr), node.sign) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -6;
	}

	if (node.address == MagicSingleton<PeerNode>::GetInstance()->GetSelfId())
	{
		return -7;
	}

	Node tempNode;
	bool find_result = MagicSingleton<PeerNode>::GetInstance()->FindNode(node.address, tempNode);
	if (find_result)
	{
		return -8;
	}
	else
	{
		DEBUGLOG("HandleRegisterNodeAck node.id: {}", node.address);
		//Join to the peernode
		std::shared_ptr<SocketBuf> ptr = MagicSingleton<BufferCrol>::GetInstance()->GetSocketBuf(node.publicIp, node.publicPort);
		
		DEBUGLOG("from ip : {}, from port: {}", IpPort::IpSz(node.publicIp), node.publicPort);

		if(ptr != NULL)
		{
			node.fd = ptr->fd;
		}
		else 
		{
			return -9;
		}

		net_com::AnalysisConnectionKind(node);
		MagicSingleton<PeerNode>::GetInstance()->Add(node);
	}
	return 0;
}

int HandlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from)
{
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node))
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}, ip:{}", pingReq->id(), node.address, IpPort::IpSz(from.ip));
        return -1;
    }
    if(pingReq->id() != node.address)
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}, ip:{}", pingReq->id(), node.address, IpPort::IpSz(from.ip));
        return -2;
    }

	node.ResetHeart();
	MagicSingleton<PeerNode>::GetInstance()->AddOrUpdate(node);
	net_com::SendPongReq(node);
	
	return 0;

}

int HandlePongReq(const std::shared_ptr<PongReq> &pongReq, const MsgData &from)
{
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node))
    {
		ERRORLOG("Invalid message peerNode_id:{}, node.address:{}, ip:{}", pongReq->id(), node.address, IpPort::IpSz(from.ip));
        return -1;
    }
    if(pongReq->id() != node.address)
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}, ip:{}", pongReq->id(), node.address, IpPort::IpSz(from.ip));
        return -2;
    }

	node.ResetHeart();
	MagicSingleton<PeerNode>::GetInstance()->AddOrUpdate(node);
	DEBUGLOG("recv addr:{}, node.address:{}, ip:{}, pulse:{}", pongReq->id(), node.address, IpPort::IpSz(from.ip), node.pulse);
	return 0;

}

int HandleSyncNodeReq(const std::shared_ptr<SyncNodeReq> &syncNodeReq, const MsgData &from)
{
	if(IpPort::IsLan(IpPort::IpSz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::IpSz(from.ip));
		return -1;
	}

	DEBUGLOG("HandleSyncNodeReq from.ip:{}", IpPort::IpSz(from.ip));
	auto self_addr = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
	auto self_node = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	SyncNodeAck syncNodeAck;
	//Get all the intranet nodes connected to you
	std::vector<Node> &&nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	if (nodeList.size() == 0)
	{
		ERRORLOG("nodeList size is 0");
		return 0;
	}
	syncNodeAck.set_ids(self_addr);
	syncNodeAck.set_msg_id(syncNodeReq->msg_id());
	for (auto &node : nodeList)
	{
		if (node.fd < 0)
		{
			ERRORLOG("node fd < 0");
			continue;
		}

		NodeInfo *nodeInfo = syncNodeAck.add_nodes();
		nodeInfo->set_addr(node.address);
		nodeInfo->set_listen_ip(node.listenIp);
		nodeInfo->set_listen_port(node.listenPort);
		nodeInfo->set_public_ip(node.publicIp);
		nodeInfo->set_public_port(node.publicPort);
		nodeInfo->set_identity(node.identity);
		nodeInfo->set_height(node.height);
		nodeInfo->set_version(node.ver);
	}

	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(self_addr, account) != 0)
	{
		ERRORLOG("account {} doesn't exist", self_addr);
		return -2;
	}

	std::string signature;
	std::string serVinHash = Getsha256hash(syncNodeAck.SerializeAsString());
	if(!account.Sign(serVinHash, signature))
	{
		return -3;
	}

	CSign * sign = syncNodeAck.mutable_sign();
	sign->set_sign(signature);
	sign->set_pub(account.GetPubStr());
	net_com::SendMessage(from, syncNodeAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	return 0;
}

int HandleSyncNodeAck(const std::shared_ptr<SyncNodeAck> &syncNodeAck, const MsgData &from)
{
	if(IpPort::IsLan(IpPort::IpSz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::IpSz(from.ip));
		return -1;
	}

	DEBUGLOG("HandleSyncNodeAck from.ip:{}", IpPort::IpSz(from.ip));
    if(!PeerNode::PeerNodeVerifyNodeId(from.fd, syncNodeAck->ids()))
    {
        return -2;
    }

	GLOBALDATAMGRPTR.AddWaitData(syncNodeAck->msg_id(), syncNodeAck->ids(), syncNodeAck->SerializeAsString());
	return 0;
}

int HandleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from)
{
	if(!PeerNode::PeerNodeVerifyNodeId(from.fd, echoReq->id()))
    {
        return -1;
    }
	EchoAck echoAck;
	echoAck.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
	echoAck.set_message(echoReq->message());
	net_com::SendMessage(echoReq->id(), echoAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
	return 0;
}

int HandleEchoAck(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from)
{
	if(!PeerNode::PeerNodeVerifyNodeId(from.fd, echoAck->id()))
    {
        return -1;
    }
	std::string echo_ack =  echoAck->message();
	if(-1 == echo_ack.find("_"))
	{
		MagicSingleton<echoTest>::GetInstance()->AddEchoCatch(echo_ack, echoAck->id());
		return 0;
	}

	printf("echo from id:%s\tmessage:%s\n",echoAck->id().c_str(), echo_ack.c_str());
	return 0;
}

int HandleNetTestReq(const std::shared_ptr<TestNetReq> &testReq, const MsgData &from)
{
	if(!PeerNode::PeerNodeVerifyNodeId(from.fd, testReq->id()))
    {
        return -1;
    }
	TestNetAck testAck;
	testAck.set_data(testReq->data());
	std::string h =testReq->data();
	if(testReq->hash() == Getsha256hash(h))
	{
		testAck.set_hash(h);
	}
	else
	{
		testAck.set_data("-1");
	}
	testAck.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
	testAck.set_time(testReq->time());
	net_com::SendMessage(testReq->id(), testAck, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
	return 0;
}

int HandleNetTestAck(const std::shared_ptr<TestNetAck> &testAck, const MsgData &from)
{
	if(!PeerNode::PeerNodeVerifyNodeId(from.fd, testAck->id()))
    {
        return -1;
    }

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	double t = ms.count() - testAck->time();
	double speed = 20 / (t/1000);
	MagicSingleton<netTest>::GetInstance()->SetTime(speed);
	return 0;
}

int HandleNodeHeightChangedReq(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from)
{
	DEBUGLOG("ip:{}", IpPort::IpSz(from.ip));
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node))
    {
        ERRORLOG("Invalid message ip:{}", IpPort::IpSz(from.ip));
        return -1;
    }
    if(req->id() != node.address)
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}", req->id(), node.address);
        return -2;
    }

	std::string id = req->id();
	uint32 height = req->height();

	node.SetHeight(height);
	MagicSingleton<PeerNode>::GetInstance()->Update(node);
	DEBUGLOG("success update {}  to height {}", id, height);
		
	return 0;
}

int HandleCheckTxReq(const std::shared_ptr<CheckTxReq>& req, const MsgData& from)
{
	if (0 != Util::IsVersionCompatible(req->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
	}

	if(req->isresponse() != 0 && req->isresponse() != 1)
	{
		ERRORLOG("The field is incorrect");
		return -2;
	}

	DBReader dbReader;
	std::string txRaw;

	CheckTxAck ack;
	ack.set_version(global::kVersion);
	ack.set_msg_id(req->msg_id());
	for(auto & hash : req->txhash())
	{
		CorresHash * correHash =  ack.add_flaghash();
		correHash->set_hash(hash);
		if(DBStatus::DB_SUCCESS == dbReader.GetTransactionByHash(hash, txRaw))
		{
			correHash->set_flag(1);
			if(req->isresponse())
			{
				ack.set_tx(txRaw);
			}
		}
		else
		{
			correHash->set_flag(0);
			ack.set_tx("");
		}
	}
	ack.set_addr(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
	net_com::SendMessage(from, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);

	return 0;
}

int HandleCheckTxAck(const std::shared_ptr<CheckTxAck>& ack, const MsgData& from)
{
	if (0 != Util::IsVersionCompatible(ack->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
	}
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node))
    {
        ERRORLOG("Invalid message ");
        return -2;
    }
	GLOBALDATAMGRPTR.AddWaitData(ack->msg_id(), node.address, ack->SerializeAsString());

	return 0;
}

int HandleNodeAddrChangedReq(const std::shared_ptr<NodeAddrChangedReq>& req, const MsgData& from)
{
	INFOLOG("HandleNodeAddrChangedReq");

	const NodeSign &oldSign = req->oldsign();
	const NodeSign &newSign = req->newsign();

	std::string oldAddr = GenerateAddr(oldSign.pub());
	if(!PeerNode::PeerNodeVerifyNodeId(from.fd, oldAddr))
    {
        return -1;
    }

	Account oldAccount;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(oldSign.pub(), oldAccount) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -2;
	}

	std::string oldSignature = oldSign.sign();
	if(oldAccount.Verify(Getsha256hash(GenerateAddr(newSign.pub())), oldSignature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -3;
	}

	Account newAccount;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(newSign.pub(), newAccount) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -4;
	}

	std::string newSignature = newSign.sign();
	if(newAccount.Verify(Getsha256hash(GenerateAddr(newSign.pub())), newSignature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -5;
	}

	int ret = MagicSingleton<PeerNode>::GetInstance()->UpdateAddress(oldSign.pub(), newSign.pub());
	return ret;
}

void initCyclicTargetAddresses(const std::shared_ptr<BuildBlockBroadcastMsg>& msg, Cycliclist<std::string> & targetAddressCycleList)
{
	std::vector<std::string> targetAddresses;
	auto castAddrs = msg->castaddrs();
	if(msg->castaddrs_size() == 0)
	{
		std::cout <<" error castaddr empty" << std::endl;
	}

	for(auto & addr : castAddrs)
	{
		targetAddresses.push_back(addr);
	}
	
	std::sort(targetAddresses.begin(), targetAddresses.end());
	for(auto & addr : targetAddresses)
	{
		DEBUGLOG("initCyclicTargetAddresses addr : {}", addr);
		targetAddressCycleList.push_back(addr);
	}
}

void initCyclicNodeList(Cycliclist<std::string>& cyclicNodeList) 
{
	std::vector<std::string> nodeListAddrs;
	std::vector<Node> nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	
	for(auto &node : nodeList)
	{
		nodeListAddrs.push_back(node.address);
	}
	std::sort(nodeListAddrs.begin(),nodeListAddrs.end());

	for(auto & addr : nodeListAddrs)
	{
		DEBUGLOG("broadcast cyclic list addr : {}", addr);
		cyclicNodeList.push_back(addr);
	}
}

int HandleBroadcastMsg(const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgData)
{	
	DEBUGLOG("HandleBuildBlockBroadcastMsg begin");

	//Determine if the version is compatible
	if (0 != Util::IsVersionCompatible(msg->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
	}

	std::string serBlock = msg->blockraw();
	CBlock block;
   
	if (!block.ParseFromString(serBlock))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg block ParseFromString failed");
		return -2;
	}

	std::vector<Cycliclist<std::string>::iterator> upperBoundary;
	std::vector<Cycliclist<std::string>::iterator> lowerBoundary;
	Cycliclist<std::string>::iterator targetIter;

	//Initialize lookup list
	auto InitFindeTarget=[&](const std::string & addr,Cycliclist<std::string> &clist)->bool
	{
       
		Cycliclist<std::string>::iterator targetPosition;
		//Find target location
       
		clist.filter([&](Cycliclist<std::string>::iterator iter){
			if(iter->data == addr){
				targetPosition = iter;
				targetIter = iter;
			}
			return false;
		});

        if(targetPosition==Cycliclist<std::string>::iterator(nullptr)){
           
		    
            return false;
        }
		Cycliclist<std::string>::iterator up;
		Cycliclist<std::string>::iterator down;
    
		up=targetPosition - 1;
		down=targetPosition + 1;
     
		int times = 0;
		while(times < clist.size() / 2){	
			if(up != down && up-1 != down && down+1 != up)
			{
             
				upperBoundary.push_back(up);
				lowerBoundary.push_back(down);
				up--;
				down++;
			}
			else
			{
				break;
			}
			times++;
		}
        return true;
	};

	//Find adjacent nodes up
	auto getUpperIterator= [&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
        res.second = 0;

        try {
            if (times >= upperBoundary.size() || times >= lowerBoundary.size()) 
			{
                ERRORLOG("find times is over! "
                         "[times:{}],[upperBoundary.size:{}],[lowerBoundary.size:{}]",
                         times, upperBoundary.size(), lowerBoundary.size());
                res.second = -1033;
                return res;
            }

            Cycliclist<std::string>::iterator iter = nodeList.begin();
            if(iter == Cycliclist<std::string>::iterator(nullptr))
			{
                 res.second=-1;
                 return res;
            }
            for (; iter != nodeList.end(); iter++) {
                if(upperBoundary[times] == Cycliclist<std::string>::iterator(nullptr))
				{
                    res.second=-1;
                    return res;
                }
                if (upperBoundary[times]->data == iter->data) 
				{
                    res.first = iter;
                    res.second = 0;
                }
            }
             if(upperBoundary[times] == Cycliclist<std::string>::iterator(nullptr))
			 {      
                    res.second = -1;
                    return res;
            }
            if (upperBoundary[times]->data == iter->data) 
			{
                res.first = iter;
                res.second = 0;
            }

        } catch (std::exception &e) {
            res.second = -3;
           // errorL("nul")
        }
        return res;
	};

	//Find adjacent nodes down
	auto getLowerIterator = [&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
        res.second = 0;

		if(times >= upperBoundary.size() || times >= lowerBoundary.size())
		{
			ERRORLOG("find times is over! [times:{}],[upperBoundary.size:{}],[lowerBoundary.size:{}]",times,upperBoundary.size(),lowerBoundary.size());
			res.second=-1033;
			return res;
		}
        try{
            Cycliclist<std::string>::iterator iter = nodeList.begin();
            if(iter == Cycliclist<std::string>::iterator(nullptr))
			{
               //  errorL("node list is empty!");
                 res.second = -1;
                 return res;
            }
            for (; iter != nodeList.end(); iter++) {
                 if(lowerBoundary[times] == Cycliclist<std::string>::iterator(nullptr))
				 {
                    res.second = -1;
                    return res;
                 }
                if (lowerBoundary[times] -> data == iter-> data) {
                    res.first = iter;

                    res.second = 0;
                }
            }
            if(lowerBoundary[times] == Cycliclist<std::string>::iterator(nullptr))
			{       
                    res.second = -1;
                    return res;
            }
            if (lowerBoundary[times]->data == iter->data) 
			{
                res.first = iter;
                res.second = 0;
            }
        } catch (std::exception &e) {
            res.second = -3;
        }
        return res;
	};
	int findTimes = 0;
	if(msg->type() == 1)
	{
		Cycliclist<std::string> targetAddressCycleList;
		initCyclicTargetAddresses(msg, targetAddressCycleList);

		std::string defaultAddress = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
		bool isTargetFound = InitFindeTarget(defaultAddress, targetAddressCycleList);

        if(isTargetFound == false)
		{
            return -1;
        }

		Cycliclist<std::string> cyclicNodeList;
		initCyclicNodeList(cyclicNodeList);

		msg->set_type(2);
		for(;findTimes < upperBoundary.size() && findTimes < lowerBoundary.size(); findTimes++)
		{
			auto upIter = getUpperIterator(findTimes, cyclicNodeList);
            if(upIter.second != 0)
			{   
                return -1;
            }
			auto downIter = getLowerIterator(findTimes, cyclicNodeList);
            if(downIter.second != 0)
			{
                return -1;
            }
          
			Cycliclist<std::string>::iterator startIterator = upIter.first;
			Cycliclist<std::string>::iterator endIterator = downIter.first;

			if(startIterator == Cycliclist<std::string>::iterator(nullptr) || endIterator == Cycliclist<std::string>::iterator(nullptr))
			{
				continue;
			}
			else
			{
				for(; startIterator != endIterator; startIterator++)
				{
					if(startIterator->data != defaultAddress)
					{
						MagicSingleton<TaskPool>::GetInstance()->CommitBroadcastTask(std::bind(&net_com::SendMessageTask, startIterator->data, *msg));
					}
				}
				if(startIterator->data != defaultAddress)
				{
					MagicSingleton<TaskPool>::GetInstance()->CommitBroadcastTask(std::bind(&net_com::SendMessageTask, startIterator->data, *msg));
				}
				break;
			}
		}
	}
	return 0;
}


int HandleGetTxUtxoHashReq(const std::shared_ptr<GetUtxoHashReq>& req, const MsgData& from){
	if (0 != Util::IsVersionCompatible(req->version()))
	{
		ERRORLOG("HandleGetTxUtxoHashReq IsVersionCompatible");
		return -1;
	}

	DBReader dbReader;
	std::string txRaw;

	GetUtxoHashAck ack;
	ack.set_version(global::kVersion);
	ack.set_msg_id(req->msg_id());
	for(auto & hash : req->utxohash())
	{
		CorresHash * correHash =  ack.add_flaghash();
		correHash->set_hash(hash);
		if(DBStatus::DB_SUCCESS == dbReader.GetTransactionByHash(hash, txRaw))
		{
			correHash->set_flag(1);
		}
		else
		{
			correHash->set_flag(0);
		}
	}

	net_com::SendMessage(from, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);

	return 0;
}

int HandleGetTxUtxoHashAck(const std::shared_ptr<GetUtxoHashAck>& ack, const MsgData& from){
	if (0 != Util::IsVersionCompatible(ack->version()))
	{
		ERRORLOG("HandleGetTxUtxoHashAck IsVersionCompatible");
		return -1;
	}
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(from.fd, node))
    {
        ERRORLOG("Invalid message ");
        return -2;
    }
	GLOBALDATAMGRPTR.AddWaitData(ack->msg_id(), node.address, ack->SerializeAsString());

	return 0;

}
