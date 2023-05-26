#include <fstream>
#include "handle_event.h"
#include "../include/logging.h"
#include "./pack.h"
#include "./ip_port.h"
#include "peer_node.h"
#include "../include/net_interface.h"
#include "./global.h"
#include "net.pb.h"
#include "common.pb.h"
#include "dispatcher.h"
#include "../common/config.h"
#include "socket_buf.h"
#include <unordered_set>
#include <utility>
#include "../common/global.h"
#include "common/global_data.h"
#include "global.h"
#include "utils/MagicSingleton.h"
#include "../include/ScopeGuard.h"
#include "../ca/ca_global.h"
#include "utils/hexcode.h"
#include "utils/base58.h"
#include "net/unregister_node.h"
#include "utils/console.h"
#include "utils/AccountManager.h"
#include "utils/tmplog.h"
#include "utils/Cycliclist.hpp"
#include "db/db_api.h"
#include <iomanip>

int handlePrintMsgReq(const std::shared_ptr<PrintMsgReq> &printMsgReq, const MsgData &from)
{
	static int PrintMsNum = 0;
	int type = printMsgReq->type();
	if (type == 0)
	{
		std::cout << ++PrintMsNum << " times data:" << printMsgReq->data() << std::endl;
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

static std::mutex node_mutex;
int handleRegisterNodeReq(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from)
{
	INFOLOG("handleRegisterNodeReq");
	DEBUGLOG("handleRegisterNodeReq from.ip:{} from.port:{} from.fd:{}", IpPort::ipsz(from.ip),from.port,from.fd);

	int ret = 0;

	//Multiple registration of the same IP address is prohibited
	std::vector<Node> pubNodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	auto result = std::find_if(pubNodeList.begin(), pubNodeList.end(),[&from](auto & node){ return from.ip == node.public_ip;});
	if(result != pubNodeList.end())
	{
		return ret -= 1;
	}
		
	//Prohibit intranet node connection
	if(IpPort::isLAN(IpPort::ipsz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::ipsz(from.ip));
		return ret -= 2;
	}

	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	
	std::lock_guard<std::mutex> lock(node_mutex);
	NodeInfo *nodeinfo = registerNode->mutable_mynode();
	
	std::string dest_pub = nodeinfo->pub();
	std::string dest_base58addr = nodeinfo->base58addr();
	Node node;
	node.fd = from.fd;
	node.base58address = dest_base58addr;
	node.identity = nodeinfo->identity();
	node.name = nodeinfo->name();
	node.logo = nodeinfo->logo();
	node.time_stamp = nodeinfo->time_stamp();
	node.pub = dest_pub;
	node.sign = nodeinfo->sign();
	node.listen_ip = selfNode.listen_ip;
	node.listen_port = SERVERMAINPORT;
	node.public_ip = from.ip;
	node.public_port = from.port;
	node.height = nodeinfo->height();
	node.public_base58addr = "";
	node.ver = nodeinfo->version();

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeinfo->version()))
    {
        ERRORLOG("Incompatible version!");
        return ret -= 3;
    }

	if(node.base58address == MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr())
    {
        ERRORLOG("Don't register yourself ");
        return ret -= 4;
    }

	EVP_PKEY* eckey = nullptr;
	ON_SCOPE_EXIT{
		if (ret != 0)
		{
			DEBUGLOG("register node failed and disconnect. ret:{}", ret);
			MagicSingleton<PeerNode>::GetInstance()->disconnect_node(node);
		}
		EVP_PKEY_free(eckey);
	};

	std::string node_base58addr = nodeinfo->base58addr();
	if (!CheckBase58Addr(node_base58addr, Base58Ver::kBase58Ver_Normal))
	{
		ERRORLOG("base58address invalid !!");
		return ret -= 5;
	}

	std::string base58Addr = GetBase58Addr(nodeinfo->identity(), Base58Ver::kBase58Ver_Normal);
	if (base58Addr != node_base58addr)
	{
		ERRORLOG("base58address error !!");
		return ret -= 6;
	}

	if(nodeinfo->identity().size() <= 0)
	{
		ERRORLOG("public key is empty");
		return ret -= 7;
	}

	if(GetEDPubKeyByBytes(nodeinfo->identity(), eckey) == false)
	{
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return ret -= 8;
	}

	if(ED25519VerifyMessage(getsha256hash(base58Addr), eckey, nodeinfo->sign()) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return ret -= 9;
	}
	
	Node tem_node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->find_node(node.base58address, tem_node);
	if ((find && tem_node.conn_kind == NOTYET) || !find)
	{
		node.conn_kind = PASSIV;
	}
	else
	{
		node.conn_kind = tem_node.conn_kind;
	}

	if (find)
	{
		//Determine whether fd is consistent with 
		if (tem_node.fd != from.fd)
		{
			if (MagicSingleton<BufferCrol>::GetInstance()->is_exists(tem_node.public_ip, tem_node.public_port) /*&& tem_node.is_established()*/)
			{
				// Join to the peernode
				std::shared_ptr<SocketBuf> ptr = MagicSingleton<BufferCrol>::GetInstance()->get_socket_buf(tem_node.public_ip, tem_node.public_port);
				tem_node.fd = ptr->fd;
				tem_node.conn_kind = PASSIV;
				MagicSingleton<PeerNode>::GetInstance()->update(node);
			}

			MagicSingleton<PeerNode>::GetInstance()->disconnect_node(tem_node);
		}
	}
	else
	{
		if (from.ip == node.public_ip && from.port == node.public_port)
		{
			MagicSingleton<PeerNode>::GetInstance()->add(node);
		}
	}

	selfNode.public_ip = from.ip;
	selfNode.public_port = from.port;
	
	std::string signature;

	Account acc;
	EVP_PKEY_free(acc.GetKey());
	if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		return ret -= 10;
	}

	if (selfNode.base58address != acc.GetBase58())
	{
		return ret -= 11;
	}
	if(!acc.Sign(getsha256hash(acc.GetBase58()), signature))
	{
		return ret -= 12;
	}

	selfNode.identity = acc.GetPubStr();
	selfNode.sign = signature;

	RegisterNodeAck registerNodeAck;
	registerNodeAck.set_msg_id(registerNode->msg_id());
	std::vector<Node> nodelist;

	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	nodelist.push_back(selfNode);
	if(registerNode->is_get_nodelist() == true)
	{
		nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	}

	for (auto &node : nodelist)
	{		
		if (node.fd < 0) 
		{
			if (node.base58address != selfNode.base58address)
			{
				ERRORLOG("node base58 addr is same of self node base58 addr");
				continue;
			}
		}

		NodeInfo *nodeinfo = registerNodeAck.add_nodes();

		nodeinfo->set_base58addr(node.base58address);
		nodeinfo->set_name(node.name);
		nodeinfo->set_logo(node.logo);
		nodeinfo->set_time_stamp(node.time_stamp);
		nodeinfo->set_listen_ip(node.listen_ip);
		nodeinfo->set_listen_port(node.listen_port);
		nodeinfo->set_public_ip(node.public_ip);
		nodeinfo->set_public_port(node.public_port);
		nodeinfo->set_identity(node.identity);
		nodeinfo->set_sign(node.sign);
		nodeinfo->set_height(node.height);
		nodeinfo->set_public_base58addr(node.public_base58addr);
		nodeinfo->set_version(node.ver);
	}

	net_com::send_message(dest_base58addr, registerNodeAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	return ret;
}

int handleRegisterNodeAck(const std::shared_ptr<RegisterNodeAck> &registerNodeAck, const MsgData &from)
{
	if(IpPort::isLAN(IpPort::ipsz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::ipsz(from.ip));
		return -1;
	}

	DEBUGLOG("from.ip:{}", IpPort::ipsz(from.ip));
	registerNodeAck->set_from_ip(from.ip);
	registerNodeAck->set_from_port(from.port);
	registerNodeAck->set_fd(from.fd);
	GLOBALDATAMGRPTR2.AddWaitData(registerNodeAck->msg_id(), registerNodeAck->SerializeAsString());
	return 0;
}

int VerifyRegisterNode(const NodeInfo &nodeinfo, uint32_t &from_ip, uint32_t &from_port)
{
	if(IpPort::isLAN(IpPort::ipsz(from_ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::ipsz(from_ip));
		return -1;
	}

	INFOLOG("handleRegisterNodeAck");

	auto self_node = MagicSingleton<PeerNode>::GetInstance()->get_self_node();

	Node node;
	node.base58address = nodeinfo.base58addr();
	node.identity = nodeinfo.identity();
	node.name = nodeinfo.name();
	node.logo = nodeinfo.logo();
	node.time_stamp = nodeinfo.time_stamp();
	node.pub = nodeinfo.pub();
	node.sign = nodeinfo.sign();
	node.listen_ip = self_node.listen_ip;
	node.listen_port = SERVERMAINPORT;
	node.public_ip = from_ip;
	node.public_port = from_port;
	node.height = nodeinfo.height();
	node.public_base58addr = nodeinfo.public_base58addr();
	node.ver = nodeinfo.version();

	// Judge whether the version is compatible
    if (0 != Util::IsVersionCompatible(nodeinfo.version()))
    {
        ERRORLOG("Incompatible version!");
        return -9;
    }

	if(!CheckBase58Addr(nodeinfo.base58addr(), Base58Ver::kBase58Ver_Normal))
	{
		return -2;
	}

	std::string base58Addr = GetBase58Addr(node.identity, Base58Ver::kBase58Ver_Normal);
	if (base58Addr != node.base58address)
	{
		return -3;
	}
	
	EVP_PKEY* eckey = nullptr;
	if(GetEDPubKeyByBytes(node.identity, eckey) == false)
	{
		EVP_PKEY_free(eckey);
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -4;
	}

	if(ED25519VerifyMessage(getsha256hash(base58Addr), eckey, node.sign) == false)
	{
		EVP_PKEY_free(eckey);
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -5;
	}
	EVP_PKEY_free(eckey);

	if (node.base58address == MagicSingleton<PeerNode>::GetInstance()->get_self_id())
	{
		return -6;
	}

	Node temp_node;
	bool find_result = MagicSingleton<PeerNode>::GetInstance()->find_node(node.base58address, temp_node);
	if (find_result)
	{
		return -7;
	}
	else
	{
		DEBUGLOG("handleRegisterNodeAck node.id: {}", node.base58address);
		//Join to the peernode
		std::shared_ptr<SocketBuf> ptr = MagicSingleton<BufferCrol>::GetInstance()->get_socket_buf(node.public_ip, node.public_port);
		
		DEBUGLOG("from ip : {}, from port: {}", IpPort::ipsz(node.public_ip), node.public_port);

		if(ptr != NULL)
		{
			node.fd = ptr->fd;
		}
		else 
		{
			return -8;
		}

		net_com::parse_conn_kind(node);
		MagicSingleton<PeerNode>::GetInstance()->add(node);
	}
	return 0;
}
int handleBroadcastMsgReq(const std::shared_ptr<BroadcastMsgReq> &broadcastMsgReq, const MsgData &from)
{
	// Send processing to its own node
	MsgData toSelfMsgData = from;
	toSelfMsgData.pack.data = broadcastMsgReq->data();
	toSelfMsgData.pack.flag = broadcastMsgReq->priority();
	auto ret = MagicSingleton<ProtobufDispatcher>::GetInstance()->handle(toSelfMsgData);
	if (ret != 0)
	{
		return ret;
	}
	return 0;
}

int handlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from)
{
	std::string id = pingReq->id();
	Node node;

	if (MagicSingleton<PeerNode>::GetInstance()->find_node(id, node))
	{
		node.ResetHeart();
		MagicSingleton<PeerNode>::GetInstance()->add_or_update(node);
		net_com::SendPongReq(node);
	}
	return 0;
}

int handlePongReq(const std::shared_ptr<PongReq> &pongReq, const MsgData &from)
{
	std::string id = pongReq->id();
	Node node;

	auto find = MagicSingleton<PeerNode>::GetInstance()->find_node(id, node);
	if (find)
	{
		node.ResetHeart();
		MagicSingleton<PeerNode>::GetInstance()->add_or_update(node);
	}
	return 0;
}

int handleSyncNodeReq(const std::shared_ptr<SyncNodeReq> &syncNodeReq, const MsgData &from)
{
	if(IpPort::isLAN(IpPort::ipsz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::ipsz(from.ip));
		return -1;
	}

	DEBUGLOG("handleSyncNodeReq from.ip:{}", IpPort::ipsz(from.ip));
	auto self_base58addr = MagicSingleton<PeerNode>::GetInstance()->get_self_id();
	auto self_node = MagicSingleton<PeerNode>::GetInstance()->get_self_node();

	SyncNodeAck syncNodeAck;
	//Get all the intranet nodes connected to you
	vector<Node> &&nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	if (nodelist.size() == 0)
	{
		ERRORLOG("nodelist size is 0");
		return 0;
	}
	//Put your own id into syncNodeAck->ids
	syncNodeAck.set_ids(std::move(self_base58addr));
	syncNodeAck.set_msg_id(syncNodeReq->msg_id());
	//Place the nodes in the nodelist into syncNodeAck->nodes
	for (auto &node : nodelist)
	{
		if (node.fd < 0)
		{
			ERRORLOG("node fd < 0");
			continue;
		}

		NodeInfo *nodeinfo = syncNodeAck.add_nodes();
		nodeinfo->set_base58addr(node.base58address);
		nodeinfo->set_listen_ip(node.listen_ip);
		nodeinfo->set_listen_port(node.listen_port);
		nodeinfo->set_public_ip(node.public_ip);
		nodeinfo->set_public_port(node.public_port);
		nodeinfo->set_identity(node.identity);
		nodeinfo->set_height(node.height);
		nodeinfo->set_public_base58addr(node.public_base58addr);
		nodeinfo->set_version(node.ver);
	}

	net_com::send_message(from, syncNodeAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	return 0;
}

int handleSyncNodeAck(const std::shared_ptr<SyncNodeAck> &syncNodeAck, const MsgData &from)
{
	if(IpPort::isLAN(IpPort::ipsz(from.ip)) == true)
	{
		ERRORLOG("{} is an intranet node", IpPort::ipsz(from.ip));
		return -1;
	}

	DEBUGLOG("handleSyncNodeAck from.ip:{}", IpPort::ipsz(from.ip));
	GLOBALDATAMGRPTR3.AddWaitData(syncNodeAck->msg_id(), syncNodeAck->SerializeAsString());
	return 0;
}

int handleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from)
{
	EchoAck echoAck;
	echoAck.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
	echoAck.set_message(echoReq->message());
	net_com::send_message(echoReq->id(), echoAck, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
	return 0;
}

int handleEchoAck(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from)
{
	std::cout << "echo from id:" << echoAck->id() << endl;
	return 0;
}


int handleNetTestReq(const std::shared_ptr<TestNetReq> &testReq, const MsgData &from)
{
	TestNetAck testAck;
	testAck.set_data(testReq->data());
	std::string h =testReq->data();
	if(testReq->hash() == getsha256hash(h))
	{
		testAck.set_hash(h);
	}
	else
	{
		testAck.set_data("-1");
	}
	testAck.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
	testAck.set_time(testReq->time());
	net_com::send_message(testReq->id(), testAck, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
	return 0;
}



int handleNetTestAck(const std::shared_ptr<TestNetAck> &testAck, const MsgData &from)
{
	
	auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	double t = ms.count() - testAck->time();
	double speed = 20 / (t/1000);
	MagicSingleton<netTest>::GetInstance()->setTime(speed);
	return 0;
}


// Create: handle node height, 20211129  Liu
int handleNodeHeightChangedReq(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from)
{
	std::string id = req->id();
	uint32 height = req->height();
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->find_node(id, node);
	if (find)
	{
		node.set_height(height);
		MagicSingleton<PeerNode>::GetInstance()->update(node);
		DEBUGLOG("success update {}  to height {}", id, height);
	}

	
	return 0;
}



int handleCheckTxReq(const std::shared_ptr<CheckTxReq>& req, const MsgData& from)
{
	DEBUGLOG("handleCheckTxReq ------------------------------>");
	if (0 != Util::IsVersionCompatible(req->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
	}

	DBReader db_reader;
	std::string txRaw;

	CheckTxAck ack;
	ack.set_version(global::kVersion);
	ack.set_msg_id(req->msg_id());
	for(auto & hash : req->txhash())
	{
		CorresHash * correhash =  ack.add_flaghash();
		correhash->set_hash(hash);
		if(DBStatus::DB_SUCCESS == db_reader.GetTransactionByHash(hash, txRaw))
		{
			correhash->set_flag(1);
		}
		else
		{
			correhash->set_flag(0);
		}
	}

	net_com::send_message(from, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);

	return 0;
}


int handleCheckTxAck(const std::shared_ptr<CheckTxAck>& ack, const MsgData& from)
{
	DEBUGLOG("handleCheckTxAck ------------------------------>");

	if (0 != Util::IsVersionCompatible(ack->version()))
	{
		ERRORLOG("HandleBuildBlockBroadcastMsg IsVersionCompatible");
		return -1;
	}
	GLOBALDATAMGRPTR.AddWaitData(ack->msg_id(), ack->SerializeAsString());

	return 0;
}







int handleNodeBase58AddrChangedReq(const std::shared_ptr<NodeBase58AddrChangedReq>& req, const MsgData& from)
{
	INFOLOG("handleNodeBase58AddrChangedReq");

	const NodeSign &oldSign = req->oldsign();
	const NodeSign &newSign = req->newsign();

	EVP_PKEY* oldPublicKey = nullptr;
	if(GetEDPubKeyByBytes(oldSign.pub(), oldPublicKey) == false)
	{
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -1;
	}

	if(ED25519VerifyMessage(getsha256hash(GetBase58Addr(newSign.pub(), Base58Ver::kBase58Ver_Normal)), oldPublicKey, oldSign.sign()) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -2;
	}

	EVP_PKEY* newPublicKey = nullptr;
	if(GetEDPubKeyByBytes(newSign.pub(), newPublicKey) == false)
	{
		ERRORLOG(RED "Get public key from bytes failed!" RESET);
		return -3;
	}

	if(ED25519VerifyMessage(getsha256hash(GetBase58Addr(newSign.pub(), Base58Ver::kBase58Ver_Normal)), newPublicKey, newSign.sign()) == false)
	{
		ERRORLOG(RED "Public key verify sign failed!" RESET);
		return -4;
	}

	ON_SCOPE_EXIT
	{
		EVP_PKEY_free(oldPublicKey);
		EVP_PKEY_free(newPublicKey);
	};

	int ret = MagicSingleton<PeerNode>::GetInstance()->update_base58Addr(oldSign.pub(), newSign.pub());
	return ret;
}

int handleBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgdata){

	
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
	auto InitFindeTarget=[&](const std::string & addr,Cycliclist<std::string> &clist)->void
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

		//Split the linked list with the target position as the midpoint
		Cycliclist<std::string>::iterator up;
		Cycliclist<std::string>::iterator down;

		up=targetPosition-1;
		down=targetPosition+1;
		int times=0;
		while(times < clist.size() /2){	
			if(up != down && up-1 != down && down+1 != up)
			{
				upAddr.push_back(up);
				downAddr.push_back(down);
				up--;
				down++;
			}else{
				break;
			}
			times++;
		}
	};

	//Find adjacent nodes up
	auto getUpIter=[&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
		
		if(times >= upAddr.size() || times >= downAddr.size()){
			ERRORLOG("find times is over! [times:{}],[upAddr.size:{}],[downAddr.size:{}]",times,upAddr.size(),downAddr.size());
			res.second=-1033;
			return res;
		}
		Cycliclist<std::string>::iterator iter = nodeList.begin();
		for(;iter!=nodeList.end();iter++){
			
			if(upAddr[times]->data==iter->data){
				
				res.first = iter;
				res.second=0;
			}
		}
		
		if(upAddr[times]->data==iter->data){
				res.first = iter;
				res.second=0;
		}
		return res;
	};

	//Find adjacent nodes down
	auto getDownIter=[&](int times, Cycliclist<std::string> & nodeList)->std::pair<Cycliclist<std::string>::iterator,int>
	{
		std::pair<Cycliclist<std::string>::iterator,int> res;
		
		if(times >= upAddr.size() || times >= downAddr.size()){
			ERRORLOG("find times is over! [times:{}],[upAddr.size:{}],[downAddr.size:{}]",times,upAddr.size(),downAddr.size());
			res.second=-1033;
			return res;
		}
		Cycliclist<std::string>::iterator iter = nodeList.begin();
		for(;iter != nodeList.end(); iter++){
			
			if(downAddr[times]->data == iter->data){
				res.first = iter;
				res.second=0;
			}
		}
		if(downAddr[times]->data==iter->data){
			res.first=iter;
			res.second=0;
		}
		return res;
	};

	int FindTimes=0;
	//Determine broadcast type
	if(msg->type()==1)
	{
		//std::cout << "cast coming 1....." << std::endl;


		std::vector<std::string> nodeListAddrs;
		std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
		for(auto &n:nodelist){
			nodeListAddrs.push_back(n.base58address);
		}
		std::sort(nodeListAddrs.begin(),nodeListAddrs.end());

		std::vector<std::string> allAddrs;
		auto castAddrs = msg->castaddrs();
		if(msg->castaddrs_size() == 0){
			std::cout <<" error castaddr empty" << std::endl;
		}

		for(auto & addr : castAddrs){
			allAddrs.push_back(addr);
		}
		std::sort(allAddrs.begin(),allAddrs.end());

		Cycliclist<std::string> cyc_list;
		for(auto & addr : allAddrs){
			cyc_list.push_back(addr);
		}

		std::string defalutAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
		InitFindeTarget(defalutAddr,cyc_list);
	
		//nodelist Add to circular linked list
		Cycliclist<std::string> cyc_node_list;
		for(auto &addr : nodeListAddrs){
			cyc_node_list.push_back(addr);
		}
		msg->set_type(2);
		//msg->clear_castaddrs();
		for(;FindTimes < upAddr.size() && FindTimes < downAddr.size(); FindTimes++){
			auto upIter = getUpIter(FindTimes,cyc_node_list);
			auto downIter = getDownIter(FindTimes,cyc_node_list);
			Cycliclist<std::string>::iterator startPosition=upIter.first;
			Cycliclist<std::string>::iterator endPosition=downIter.first;
			if(startPosition == nullptr || endPosition == nullptr){
				continue;
			}else{
				
				for(;startPosition!=endPosition;startPosition++){
					if(startPosition->data != defalutAddr){
						net_com::send_message(startPosition->data, *msg);
					}
					
				}
				if(startPosition->data!=defalutAddr){
					net_com::send_message(startPosition->data, *msg);
				}
				break;
			}
		}

	}else if(msg->type()==2){
		//std::cout << "cast coming 2....." << std::endl;
	}

	return 0;


}


void netTest::netTestRegister()
{
	mutex_time.lock();
	signal = false;
	mutex_time.unlock();
	MagicSingleton<taskPool>::GetInstance()->commit_ca_task(std::bind(&netTest::isValue, this));
}