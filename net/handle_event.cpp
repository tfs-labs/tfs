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


#include "../ca/global.h"
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
#include "../utils/base58.h"
#include "../utils/console.h"
#include "../utils/cycliclist.hpp"
#include "../utils/hex_code.h"
#include "../utils/magic_singleton.h"
#include "../utils/tmp_log.h"

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
		ofstream file("bigdata.txt", fstream::out);
		file << printMsgReq->data();
		file.close();
		cout << "write bigdata.txt success!!!" << endl;
	}
	return 0;
}

static std::mutex nodeMutex;
int HandleRegisterNodeReq(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from)
{
	INFOLOG("HandleRegisterNodeReq");
	DEBUGLOG("HandleRegisterNodeReq from.ip:{} from.port:{} from.fd:{}", IpPort::IpSz(from.ip),from.port,from.fd);

	int ret = 0;

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

	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	
	std::lock_guard<std::mutex> lock(nodeMutex);
	NodeInfo *nodeInfo = registerNode->mutable_mynode();
	
	std::string dest_pub = nodeInfo->pub();
	std::string destBase58Addr = nodeInfo->base58addr();
	Node node;
	node.fd = from.fd;
	node.base58Address = destBase58Addr;
	node.identity = nodeInfo->identity();
	node.name = nodeInfo->name();
	node.logo = nodeInfo->logo();
	node.timeStamp = nodeInfo->time_stamp();
	node.pub = dest_pub;
	node.sign = nodeInfo->sign();
	node.listenIp = selfNode.listenIp;
	node.listenPort = SERVERMAINPORT;
	node.publicIp = from.ip;
	node.publicPort = from.port;
	node.height = nodeInfo->height();
	node.publicBase58Addr = "";
	node.ver = nodeInfo->version();

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeInfo->version()))
    {
        ERRORLOG("Incompatible version!");
        return ret -= 3;
    }

	if(node.base58Address == MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr())
    {
        ERRORLOG("Don't register yourself ");
        return ret -= 4;
    }

	EVP_PKEY* eckey = nullptr;
	ON_SCOPE_EXIT{
		if (ret != 0)
		{
			DEBUGLOG("register node failed and disconnect. ret:{}", ret);
			MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(node);
		}
	};

	std::string node_base58addr = nodeInfo->base58addr();
	if (!CheckBase58Addr(node_base58addr, Base58Ver::kBase58Ver_Normal))
	{
		ERRORLOG("base58Address invalid !!");
		return ret -= 5;
	}

	std::string base58Addr = GetBase58Addr(nodeInfo->identity(), Base58Ver::kBase58Ver_Normal);
	if (base58Addr != node_base58addr)
	{
		ERRORLOG("base58Address error !!");
		return ret -= 6;
	}

	if(nodeInfo->identity().size() <= 0)
	{
		ERRORLOG("public key is empty");
		return ret -= 7;
	}
	//TODO
	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(nodeInfo->identity(), account) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return ret -= 8;
	}

	std::string nodeInfo_signature = nodeInfo->sign();
	if(account.Verify(Getsha256hash(base58Addr), nodeInfo_signature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return ret -= 9;
	}

	// if(GetEDPubKeyByBytes(nodeInfo->identity(), eckey) == false)
	// {
	// 	ERRORLOG(RED "Get public key from bytes failed!" RESET);
	// 	return ret -= 8;
	// }

	// if(ED25519VerifyMessage(Getsha256hash(base58Addr), eckey, nodeInfo->sign()) == false)
	// {
	// 	ERRORLOG(RED "Public key verify sign failed!" RESET);
	// 	return ret -= 9;
	// }
	
	Node temNode;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(node.base58Address, temNode);
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

	if (selfNode.base58Address != acc.GetBase58())
	{
		return ret -= 11;
	}
	if(!acc.Sign(Getsha256hash(acc.GetBase58()), signature))
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
			if (node.base58Address != selfNode.base58Address)
			{
				ERRORLOG("node base58 addr is same of self node base58 addr");
				continue;
			}
		}

		NodeInfo *nodeInfo = registerNodeAck.add_nodes();

		nodeInfo->set_base58addr(node.base58Address);
		nodeInfo->set_name(node.name);
		nodeInfo->set_logo(node.logo);
		nodeInfo->set_time_stamp(node.timeStamp);
		nodeInfo->set_listen_ip(node.listenIp);
		nodeInfo->set_listen_port(node.listenPort);
		nodeInfo->set_public_ip(node.publicIp);
		nodeInfo->set_public_port(node.publicPort);
		nodeInfo->set_identity(node.identity);
		nodeInfo->set_sign(node.sign);
		nodeInfo->set_height(node.height);
		nodeInfo->set_public_base58addr(node.publicBase58Addr);
		nodeInfo->set_version(node.ver);
	}

	net_com::SendMessage(destBase58Addr, registerNodeAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
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
	GLOBALDATAMGRPTR2.AddWaitData(registerNodeAck->msg_id(), registerNodeAck->SerializeAsString());
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
	node.base58Address = nodeInfo.base58addr();
	node.identity = nodeInfo.identity();
	node.name = nodeInfo.name();
	node.logo = nodeInfo.logo();
	node.timeStamp = nodeInfo.time_stamp();
	node.pub = nodeInfo.pub();
	node.sign = nodeInfo.sign();
	node.listenIp = self_node.listenIp;
	node.listenPort = SERVERMAINPORT;
	node.publicIp = fromIp;
	node.publicPort = fromPort;
	node.height = nodeInfo.height();
	node.publicBase58Addr = nodeInfo.public_base58addr();
	node.ver = nodeInfo.version();

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeInfo.version()))
    {
        ERRORLOG("Incompatible version!");
        return -9;
    }

	if(!CheckBase58Addr(nodeInfo.base58addr(), Base58Ver::kBase58Ver_Normal))
	{
		return -2;
	}

	std::string base58Addr = GetBase58Addr(node.identity, Base58Ver::kBase58Ver_Normal);
	if (base58Addr != node.base58Address)
	{
		return -3;
	}
	
	//TODO
	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(node.identity, account) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -4;
	}

	if(account.Verify(Getsha256hash(base58Addr), node.sign) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -5;
	}


	// EVP_PKEY* eckey = nullptr;
	// if(GetEDPubKeyByBytes(node.identity, eckey) == false)
	// {
	// 	EVP_PKEY_free(eckey);
	// 	ERRORLOG(RED "Get public key from bytes failed!" RESET);
	// 	return -4;
	// }

	// if(ED25519VerifyMessage(Getsha256hash(base58Addr), eckey, node.sign) == false)
	// {
	// 	EVP_PKEY_free(eckey);
	// 	ERRORLOG(RED "Public key verify sign failed!" RESET);
	// 	return -5;
	// }
	// EVP_PKEY_free(eckey);

	if (node.base58Address == MagicSingleton<PeerNode>::GetInstance()->GetSelfId())
	{
		return -6;
	}

	Node tempNode;
	bool find_result = MagicSingleton<PeerNode>::GetInstance()->FindNode(node.base58Address, tempNode);
	if (find_result)
	{
		return -7;
	}
	else
	{
		DEBUGLOG("HandleRegisterNodeAck node.id: {}", node.base58Address);
		//Join to the peernode
		std::shared_ptr<SocketBuf> ptr = MagicSingleton<BufferCrol>::GetInstance()->GetSocketBuf(node.publicIp, node.publicPort);
		
		DEBUGLOG("from ip : {}, from port: {}", IpPort::IpSz(node.publicIp), node.publicPort);

		if(ptr != NULL)
		{
			node.fd = ptr->fd;
		}
		else 
		{
			return -8;
		}

		net_com::AnalysisConnectionKind(node);
		MagicSingleton<PeerNode>::GetInstance()->Add(node);
	}
	return 0;
}
int HandleBroadcastMsgReq(const std::shared_ptr<BroadcastMsgReq> &broadcastMsgReq, const MsgData &from)
{
	// Send processing to its own node
	MsgData toSelfMsgData = from;
	toSelfMsgData.pack.data = broadcastMsgReq->data();
	toSelfMsgData.pack.flag = broadcastMsgReq->priority();
	auto ret = MagicSingleton<ProtobufDispatcher>::GetInstance()->Handle(toSelfMsgData);
	if (ret != 0)
	{
		return ret;
	}
	return 0;
}

int HandlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from)
{
	std::string id = pingReq->id();
	Node node;

	if (MagicSingleton<PeerNode>::GetInstance()->FindNode(id, node))
	{
		node.ResetHeart();
		MagicSingleton<PeerNode>::GetInstance()->AddOrUpdate(node);
		net_com::SendPongReq(node);
	}
	return 0;
}

int HandlePongReq(const std::shared_ptr<PongReq> &pongReq, const MsgData &from)
{
	std::string id = pongReq->id();
	Node node;

	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(id, node);
	if (find)
	{
		node.ResetHeart();
		MagicSingleton<PeerNode>::GetInstance()->AddOrUpdate(node);
	}
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
	auto self_base58addr = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
	auto self_node = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	SyncNodeAck syncNodeAck;
	//Get all the intranet nodes connected to you
	vector<Node> &&nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	if (nodeList.size() == 0)
	{
		ERRORLOG("nodeList size is 0");
		return 0;
	}
	//Put your own id into syncNodeAck->ids
	syncNodeAck.set_ids(std::move(self_base58addr));
	syncNodeAck.set_msg_id(syncNodeReq->msg_id());
	//Place the nodes in the nodeList into syncNodeAck->nodes
	for (auto &node : nodeList)
	{
		if (node.fd < 0)
		{
			ERRORLOG("node fd < 0");
			continue;
		}

		NodeInfo *nodeInfo = syncNodeAck.add_nodes();
		nodeInfo->set_base58addr(node.base58Address);
		nodeInfo->set_listen_ip(node.listenIp);
		nodeInfo->set_listen_port(node.listenPort);
		nodeInfo->set_public_ip(node.publicIp);
		nodeInfo->set_public_port(node.publicPort);
		nodeInfo->set_identity(node.identity);
		nodeInfo->set_height(node.height);
		nodeInfo->set_public_base58addr(node.publicBase58Addr);
		nodeInfo->set_version(node.ver);
	}

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
	GLOBALDATAMGRPTR3.AddWaitData(syncNodeAck->msg_id(), syncNodeAck->SerializeAsString());
	return 0;
}

int HandleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from)
{
	EchoAck echoAck;
	echoAck.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
	echoAck.set_message(echoReq->message());
	net_com::SendMessage(echoReq->id(), echoAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
	return 0;
}

int HandleEchoAck(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from)
{
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
	
	auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	double t = ms.count() - testAck->time();
	double speed = 20 / (t/1000);
	MagicSingleton<netTest>::GetInstance()->SetTime(speed);
	return 0;
}

int HandleNodeHeightChangedReq(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from)
{
	std::string id = req->id();
	uint32 height = req->height();
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(id, node);
	if (find)
	{
		node.SetHeight(height);
		MagicSingleton<PeerNode>::GetInstance()->Update(node);
		DEBUGLOG("success Update {}  to height {}", id, height);
	}	
	return 0;
}

int HandleCheckTxReq(const std::shared_ptr<CheckTxReq>& req, const MsgData& from)
{
	if (0 != Util::IsVersionCompatible(req->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
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
		}
		else
		{
			correHash->set_flag(0);
		}
	}

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
	GLOBALDATAMGRPTR.AddWaitData(ack->msg_id(), ack->SerializeAsString());

	return 0;
}

int HandleNodeBase58AddrChangedReq(const std::shared_ptr<NodeBase58AddrChangedReq>& req, const MsgData& from)
{
	INFOLOG("HandleNodeBase58AddrChangedReq");

	const NodeSign &oldSign = req->oldsign();
	const NodeSign &newSign = req->newsign();
	//TODO
	Account oldAccount;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(oldSign.pub(), oldAccount) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -1;
	}

	std::string oldSignature = oldSign.sign();
	if(oldAccount.Verify(Getsha256hash(GetBase58Addr(newSign.pub())), oldSignature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -2;
	}

	// EVP_PKEY* oldPublicKey = nullptr;
	// if(GetEDPubKeyByBytes(oldSign.pub(), oldPublicKey) == false)
	// {
	// 	ERRORLOG(RED "Get public key from bytes failed!" RESET);
	// 	return -1;
	// }

	// if(ED25519VerifyMessage(Getsha256hash(GetBase58Addr(newSign.pub(), Base58Ver::kBase58Ver_Normal)), oldPublicKey, oldSign.sign()) == false)
	// {
	// 	ERRORLOG(RED "Public key verify sign failed!" RESET);
	// 	return -2;
	// }
	
	//TODO
	Account newAccount;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(newSign.pub(), newAccount) == false){
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -3;
	}

	std::string newSignature = newSign.sign();
	if(newAccount.Verify(Getsha256hash(GetBase58Addr(newSign.pub())), newSignature) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -4;
	}

	// EVP_PKEY* newPublicKey = nullptr;
	// if(GetEDPubKeyByBytes(newSign.pub(), newPublicKey) == false)
	// {
	// 	ERRORLOG(RED "Get public key from bytes failed!" RESET);
	// 	return -3;
	// }

	// if(ED25519VerifyMessage(Getsha256hash(GetBase58Addr(newSign.pub(), Base58Ver::kBase58Ver_Normal)), newPublicKey, newSign.sign()) == false)
	// {
	// 	ERRORLOG(RED "Public key verify sign failed!" RESET);
	// 	return -4;
	// }

	// ON_SCOPE_EXIT
	// {
	// 	EVP_PKEY_free(oldPublicKey);
	// 	EVP_PKEY_free(newPublicKey);
	// };

	int ret = MagicSingleton<PeerNode>::GetInstance()->UpdateBase58Address(oldSign.pub(), newSign.pub());
	return ret;
}

int HandleBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgData)
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

	std::vector<Cycliclist<std::string>::iterator> upAddr;
	std::vector<Cycliclist<std::string>::iterator> downAddr;
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
             
				upAddr.push_back(up);
				downAddr.push_back(down);
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
	auto getUpIter= [&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
        res.second = 0;

        try {
            if (times >= upAddr.size() || times >= downAddr.size()) 
			{
                ERRORLOG("find times is over! "
                         "[times:{}],[upAddr.size:{}],[downAddr.size:{}]",
                         times, upAddr.size(), downAddr.size());
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
                if(upAddr[times] == Cycliclist<std::string>::iterator(nullptr))
				{
                    res.second=-1;
                    return res;
                }
                if (upAddr[times]->data == iter->data) 
				{
                    res.first = iter;
                    res.second = 0;
                }
            }
             if(upAddr[times] == Cycliclist<std::string>::iterator(nullptr))
			 {      
                    res.second = -1;
                    return res;
            }
            if (upAddr[times]->data == iter->data) 
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
	auto getDownIter = [&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
        res.second = 0;

		if(times >= upAddr.size() || times >= downAddr.size())
		{
			ERRORLOG("find times is over! [times:{}],[upAddr.size:{}],[downAddr.size:{}]",times,upAddr.size(),downAddr.size());
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
                 if(downAddr[times] == Cycliclist<std::string>::iterator(nullptr))
				 {
                    res.second = -1;
                    return res;
                 }
                if (downAddr[times] -> data == iter-> data) {
                    res.first = iter;

                    res.second = 0;
                }
            }
            if(downAddr[times] == Cycliclist<std::string>::iterator(nullptr))
			{       
                    res.second = -1;
                    return res;
            }
            if (downAddr[times]->data == iter->data) 
			{
                res.first = iter;
                res.second = 0;
            }
        } catch (std::exception &e) {
            res.second=-3;
        }
        return res;
	};
	int findTimes = 0;
	//Determine broadcast type
	if(msg->type() == 1)
	{
		std::vector<std::string> nodeListAddrs;
		std::vector<Node> nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
      
		for(auto &n:nodeList)
		{
			nodeListAddrs.push_back(n.base58Address);
		}
      
		std::sort(nodeListAddrs.begin(),nodeListAddrs.end());
		std::vector<std::string> allAddrs;
		auto castAddrs = msg->castaddrs();
		if(msg->castaddrs_size() == 0)
		{
			std::cout <<" error castaddr empty" << std::endl;
		}

		for(auto & addr : castAddrs)
		{
			allAddrs.push_back(addr);
		}
       
		std::sort(allAddrs.begin(),allAddrs.end());
		Cycliclist<std::string> cycList;
		for(auto & addr : allAddrs)
		{
			cycList.push_back(addr);
		}

		std::string defalutAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
		bool isFindd=InitFindeTarget(defalutAddr,cycList);

        if(isFindd==false)
		{
            return -1;
        }

		//nodeList Add to circular linked list
		Cycliclist<std::string> cycNodeList;
		for(auto &addr : nodeListAddrs)
		{
			cycNodeList.push_back(addr);
		}
		msg->set_type(2);
		for(;findTimes < upAddr.size() && findTimes < downAddr.size(); findTimes++)
		{
			auto upIter = getUpIter(findTimes,cycNodeList);
            if(upIter.second!=0)
			{   
                return -1;
            }
			auto downIter = getDownIter(findTimes,cycNodeList);
            if(downIter.second!=0)
			{
                return -1;
            }
          
			Cycliclist<std::string>::iterator startPosition=upIter.first;
			Cycliclist<std::string>::iterator endPosition=downIter.first;
			
			if(startPosition == Cycliclist<std::string>::iterator(nullptr) || endPosition == Cycliclist<std::string>::iterator(nullptr))
			{
				continue;
			}
			else
			{
				for(;startPosition!=endPosition;startPosition++)
				{
                  //  debugL("__line:%s",__LINE__);
					if(startPosition->data != defalutAddr)
					{
						net_com::SendMessage(startPosition->data, *msg);
					}
					//debugL("__line:%s",__LINE__);
				}
				if(startPosition->data!=defalutAddr)
				{
					net_com::SendMessage(startPosition->data, *msg);
                   // debugL("__line:%s",__LINE__);
				}
				break;
			}
		}
	}
	return 0;
}
