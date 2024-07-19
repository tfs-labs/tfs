#include "unregister_node.h"

#include "./handle_event.h"
#include "./api.h"

#include "../utils/time_util.h"
#include "../net/peer_node.h"
#include "../utils/magic_singleton.h"
#include "../common/global_data.h"
#include "../common/config.h"
#include "../proto/net.pb.h"
#include "../utils/account_manager.h"

UnregisterNode::UnregisterNode()
{
}
UnregisterNode::~UnregisterNode()
{
}

int UnregisterNode::Add(const Node & node)
{
    std::unique_lock<std::shared_mutex> lck(_mutexForNodes);
    std::string key = std::to_string(node.publicIp) + std::to_string(node.publicPort);

    if(key.size() == 0)
	{
		return -1;
	} 
	auto itr = _nodes.find(key);
	if (itr != _nodes.end())
	{
		return -2;
	}
	this->_nodes[key] = node;


    return 0;
}

bool UnregisterNode::Register(std::map<uint32_t, Node> nodeMap)
{
    std::string msgId;
    uint32 sendNum = nodeMap.size();
    if (!GLOBALDATAMGRPTR.CreateWait(5, sendNum, msgId))
    {
        return false;
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::map<uint32_t, int> nodeIPAndFd;
    
    for (auto & unconnectNode : nodeMap)
    {
        bool isFind = false;
        for (auto & node : nodelist)
        {
            if (unconnectNode.second.publicIp == node.publicIp)
            {
                isFind = true;
                break;
            }
        }
        
        if (isFind)
        {
            continue;
        }

        int ret = net_com::SendRegisterNodeReq(unconnectNode.second, msgId, false);
        if(ret != 0)
        {
            ERRORLOG("SendRegisterNodeReq fail ret = {}", ret);
            continue;
        }
        nodeIPAndFd.insert(std::make_pair(unconnectNode.second.publicIp, unconnectNode.second.fd));
    }

    std::vector<std::string> returnDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait Register time out send:{} recv:{}", sendNum, returnDatas.size());
            for(auto& t: nodeIPAndFd)
            {
                DEBUGLOG("wait Register time out, 111 close fd:{}, ip = {}",t.second , IpPort::IpSz(t.first));
                MagicSingleton<PeerNode>::GetInstance()->CloseFd(t.second);
            }
            return false;
        }
    }

    RegisterNodeAck registerNodeAck;
    for (auto &retData : returnDatas)
    {
        registerNodeAck.Clear();
        if (!registerNodeAck.ParseFromString(retData))
        {
            continue;
        }
        uint32_t ip = registerNodeAck.from_ip();
        uint32_t port = registerNodeAck.from_port();

        auto find = nodeIPAndFd.find(ip);
        if(find != nodeIPAndFd.end())
        {
            nodeIPAndFd.erase(find);
        }

        std::cout << "registerNodeAck.nodes_size(): " << registerNodeAck.nodes_size() <<std::endl;
        if(registerNodeAck.nodes_size() <= 1)
	    {
            const NodeInfo &nodeinfo = registerNodeAck.nodes(0);
			if (MagicSingleton<BufferCrol>::GetInstance()->IsExists(ip, port) /* && node.is_established()*/)
			{
                DEBUGLOG("HandleRegisterNodeAck--FALSE from.ip: {}", IpPort::IpSz(ip));
                auto ret = VerifyRegisterNode(nodeinfo, ip, port);
                if(ret < 0)
                {
                    DEBUGLOG("VerifyRegisterNode error ret:{}", ret);
                }
			}
        }
    }

    for(auto& t: nodeIPAndFd)
    {
        DEBUGLOG("nodeIPAndFd, 111 close fd:{}, ip = {}, fd:{}",t.second , IpPort::IpSz(t.first), t.second);
        MagicSingleton<PeerNode>::GetInstance()->CloseFd(t.second);
    }

    return true;
}
bool UnregisterNode::StartRegisterNode(std::map<std::string, int> &serverList)
{
    std::string msgId;
    uint32 sendNum = serverList.size();
    if (!GLOBALDATAMGRPTR.CreateWait(5, sendNum, msgId))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    for (auto & item : serverList)
	{
        //The party actively establishing the connection
		Node node;
		node.publicIp = IpPort::IpNum(item.first);
		node.listenIp = selfNode.listenIp;
		node.listenPort = SERVERMAINPORT;

		if (item.first == global::g_localIp)
		{
			continue;
		}

		int ret = net_com::SendRegisterNodeReq(node, msgId, true);
        if(ret != 0)
        {
            ERRORLOG("StartRegisterNode error ret : {}", ret);
        }
	}

    std::vector<std::string> returnDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait StartRegisterNode time out send:{} recv:{}", sendNum, returnDatas.size());
            return false;
        }
    }
    RegisterNodeAck registerNodeAck;
    std::map<uint32_t, Node> nodeMap;


    for (auto &retData : returnDatas)
    {
        registerNodeAck.Clear();
        if (!registerNodeAck.ParseFromString(retData))
        {
            continue;
        }

        uint32_t ip = registerNodeAck.from_ip();
        uint32_t port = registerNodeAck.from_port();
        uint32_t fd = registerNodeAck.fd();
        for (int i = 0; i < registerNodeAck.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = registerNodeAck.nodes(i);
            {
                Node node;
                node.listenIp = selfNode.listenIp;
	            node.listenPort = SERVERMAINPORT;
                node.publicIp = nodeinfo.public_ip();
                node.address = nodeinfo.addr();
            }
            if(nodeinfo.addr() == selfNode.address)
            {
                continue;
            }
            if(i == 0)
            {
                //Determine if TCP is connected
                if (MagicSingleton<BufferCrol>::GetInstance()->IsExists(ip, port))
                {
                    DEBUGLOG("HandleRegisterNodeAck--TRUE from.ip: {}", IpPort::IpSz(ip));
                    auto ret = VerifyRegisterNode(nodeinfo, ip, port);
                    if(ret < 0)
                    {
                        DEBUGLOG("VerifyRegisterNode error ret:{}", ret);
                        MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(ip, port, fd);
                        continue;
                    }
                }
            }
            else
            {
                Node node;
                node.listenIp = selfNode.listenIp;
	            node.listenPort = SERVERMAINPORT;
                node.publicIp = nodeinfo.public_ip();
                DEBUGLOG("Add NodeList--TRUE ip: {}", IpPort::IpSz(node.publicIp));
                if(nodeMap.find(node.publicIp) == nodeMap.end())
                {
                    nodeMap[nodeinfo.public_ip()] = node;
                }
            } 
        }
    }
    Register(nodeMap);
    return true;
}

bool UnregisterNode::StartSyncNode()
{
    std::string msgId;
    std::vector<Node> node_list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    uint32 sendNum = node_list.size();
    if (!GLOBALDATAMGRPTR.CreateWait(5, sendNum, msgId))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    
    for (auto & node : node_list)
    {
        //Determine if TCP is connected
        if (MagicSingleton<BufferCrol>::GetInstance()->IsExists(node.publicIp, node.publicPort) /* && node.is_established()*/)
        {
            net_com::SendSyncNodeReq(node, msgId);
        }
        else
        {
            DEBUGLOG("SendSyncNodeReq error id:{} ip:{} port:{}", node.address, IpPort::IpSz(node.publicIp), node.publicPort);
        }
    }

    std::vector<std::string> returnDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait StartRegisterNode time out send:{} recv:{}", sendNum, returnDatas.size());
            return false;
        }
    }
    SyncNodeAck syncNodeAck;
    std::map<uint32_t, Node> nodeMap;
    std::vector<Node> syncNodes;
    std::map<std::string, std::vector<Node>> _vrfNodelist;

    
    auto VerifySign = [&](const CSign & sign, const std::string & serHash) -> int{
        if (sign.sign().size() == 0 || sign.pub().size() == 0)
        {
            return -1;
        }
        if (serHash.size() == 0)
        {
            return -2;
        }

        EVP_PKEY* eckey = nullptr;
        if(GetEDPubKeyByBytes(sign.pub(), eckey) == false)
        {
            EVP_PKEY_free(eckey);
            ERRORLOG("Get public key from bytes failed!");
            return -3;
        }

        if(ED25519VerifyMessage(serHash, eckey, sign.sign()) == false)
        {
            EVP_PKEY_free(eckey);
            ERRORLOG("Public key verify sign failed!");
            return -4;
        }
        EVP_PKEY_free(eckey);
        return 0;
    };

    for (auto &retData : returnDatas)
    {
        syncNodeAck.Clear();
        if (!syncNodeAck.ParseFromString(retData))
        {
            continue;
        }
        auto copySyncNodeAck = syncNodeAck;
        copySyncNodeAck.clear_sign();
        std::string serVinHash = Getsha256hash(copySyncNodeAck.SerializeAsString());

        int verifySignRet = VerifySign(syncNodeAck.sign(), serVinHash);
        if (verifySignRet != 0)
        {
            ERRORLOG("targetNodelist VerifySign fail!!!");
            continue;
        }

        std::vector<Node> targetAddrList;
        for (int i = 0; i < syncNodeAck.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = syncNodeAck.nodes(i);
            if(nodeinfo.addr() == selfNode.address)
            {
                continue;
            }
            Node node;
            node.listenIp = selfNode.listenIp;
            node.listenPort = SERVERMAINPORT;
            node.publicIp = nodeinfo.public_ip();
            node.address = nodeinfo.addr();
            node.height = nodeinfo.height();

            if(nodeMap.find(node.publicIp) == nodeMap.end())
            {
                
                nodeMap[nodeinfo.public_ip()] = node;
            }
            targetAddrList.push_back(node);

        }
        _vrfNodelist[syncNodeAck.ids()] = targetAddrList;
    }

    for(auto & item : _vrfNodelist)
    {
        std::sort(item.second.begin(), item.second.end(), compareStructs);
        auto last = std::unique(item.second.begin(), item.second.end(), compareStructs);
        item.second.erase(last, item.second.end());
        DEBUGLOG(" sort and unique @@@@@@ ");
        for(auto & i : item.second)
        {
            syncNodes.push_back(i);
        }
    }

    //Count the number of IPs and the number of times they correspond to IPs
    {
        std::map<Node,int, NodeCompare> syncNodeCount;
        for(auto it = syncNodes.begin(); it != syncNodes.end(); ++it)
        {
            syncNodeCount[*it]++;
        }
        splitAndInsertData(syncNodeCount);
        syncNodes.clear();
        syncNodeCount.clear();
    }

    //Only the latest elements are stored in the maintenance map map
    if(stakeNodelist.size() == 2 || unStakeNodelist.size() == 2)
    {
        ClearSplitNodeListData();
    }

    if(nodeMap.empty())
    {
        auto configServerList = MagicSingleton<Config>::GetInstance()->GetServer();
        int port = MagicSingleton<Config>::GetInstance()->GetServerPort();
        
        std::map<std::string, int> serverList;
        for (auto & configServerIp: configServerList)
        {
            serverList.insert(std::make_pair(configServerIp, port));
        }

        MagicSingleton<UnregisterNode>::GetInstance()->StartRegisterNode(serverList);
    }
    else
    {
        Register(nodeMap);
    }

    return true;
}



void UnregisterNode::GetIpMap(std::map<uint64_t, std::map<std::string, int>> & m1,std::map<uint64_t, std::map<std::string, int>> & m2)
{
    std::unique_lock<std::mutex> locker(_mutexStakelist);
    m1 = stakeNodelist;
    m2 = unStakeNodelist;
}


void UnregisterNode::DeleteSpiltNodeList(const std::string & addr)
{
    std::unique_lock<std::mutex> locker(_mutexStakelist);
    if(stakeNodelist.empty() || unStakeNodelist.empty())
    {
        ERRORLOG("list is empty!");
        return;
    }

    for(auto & [_,iter] : stakeNodelist)
    {
        for(auto iter2 = iter.begin();iter2 != iter.end(); ++iter2)
        {
            if(iter2->first == addr)
            {
                iter2 = iter.erase(iter2);
                return;
            }
        }
    }
    
    for(auto & [_,iter] : unStakeNodelist)
    {
        for(auto iter2 = iter.begin();iter2 != iter.end(); ++iter2)
        {
            if(iter2->first == addr)
            {
                iter2 = iter.erase(iter2);
                return;
            }
        }
    }
}

void UnregisterNode::GetConsensusStakeNodelist(std::map<std::string,int>& consensusStakeNodeMap)
{
    std::unique_lock<std::mutex> lck(_mutexStakelist);
    if(stakeNodelist.empty())
    {
        return;
    }
    consensusStakeNodeMap.insert(stakeNodelist.rbegin()->second.begin(), stakeNodelist.rbegin()->second.end());
    return;
}

void UnregisterNode::GetConsensusNodelist(std::map<std::string,int>& consensusNodeMap)
{
    std::unique_lock<std::mutex> lck(_mutexStakelist);
    if(stakeNodelist.empty() || unStakeNodelist.empty())
    {
        return;
    }
    consensusNodeMap.insert(stakeNodelist.rbegin()->second.begin(), stakeNodelist.rbegin()->second.end());
    
    for(const auto& it : unStakeNodelist.rbegin()->second)
    {
        consensusNodeMap[it.first] = it.second;
    }
    return;
}

void UnregisterNode::ClearSplitNodeListData()
{
    std::unique_lock<std::mutex> lck(_mutexStakelist);
    auto it = stakeNodelist.begin();
    stakeNodelist.erase(it);

    auto _it = unStakeNodelist.begin();
    unStakeNodelist.erase(_it);
    DEBUGLOG("ClearSplitNodeListData @@@@@ ");
}

static int calculateAverage(const std::vector<int>& vec)
{
    if (vec.empty()) {
        std::cout << "Error: Vector is empty." << std::endl;
        return 0;
    }

    int sum = 0;
    
    for (int num : vec)
    {
        sum += num;
    }
    
    int average = static_cast<double>(sum) / vec.size();
    
    return average;
}

void UnregisterNode::splitAndInsertData(const std::map<Node, int, NodeCompare>  syncNodeCount)
{
    std::unique_lock<std::mutex> locker(_mutexStakelist);
    std::map<std::string, int>  stakeSyncNodeCount;
    std::map<std::string, int>  UnstakeSyncNodeCount;

    auto VerifyBonusAddr = [](const std::string& bonusAddr) -> int
    {
        uint64_t investAmount;
        auto ret = MagicSingleton<BonusAddrCache>::GetInstance()->getAmount(bonusAddr, investAmount);
        if (ret < 0)
        {
            return -99;
        }
        return investAmount >= global::ca::kMinInvestAmt ? 0 : -99;
    };

    DBReader dbReader;
    DEBUGLOG("splitAndInsertData @@@@@ ");
    for(auto & item : syncNodeCount)
    {
        //Verification of investment and pledge
        int ret = VerifyBonusAddr(item.first.address);
        std::vector<std::string> pledgeUtxoHashs;
        int retValue = dbReader.GetStakeAddressUtxo(item.first.address, pledgeUtxoHashs);
        bool HasPledged = DBStatus::DB_SUCCESS == retValue || !pledgeUtxoHashs.empty();
        if (HasPledged && ret == 0)
        {
            stakeSyncNodeCount.insert(std::make_pair(item.first.address,item.second));
        }
        else
        {
            UnstakeSyncNodeCount.insert(std::make_pair(item.first.address,item.second));
        }
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    stakeNodelist[nowTime] = stakeSyncNodeCount;
    unStakeNodelist[nowTime] = UnstakeSyncNodeCount;
}