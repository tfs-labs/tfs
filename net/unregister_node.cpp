#include "unregister_node.h"

#include "./handle_event.h"
#include "./api.h"

#include "../utils/time_util.h"
#include "../net/peer_node.h"
#include "../utils/magic_singleton.h"
#include "../common/global_data.h"
#include "../common/config.h"
#include "../proto/net.pb.h"
#include "../ca/algorithm.h"
#include "../utils/account_manager.h"
#include "../ca/transaction.h"

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
    if (!GLOBALDATAMGRPTR2.CreateWait(5, sendNum, msgId))
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
    if (!GLOBALDATAMGRPTR2.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
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
        DEBUGLOG("nodeIPAndFd, 111 close fd:{}, ip = {}",t.second , IpPort::IpSz(t.first));
        MagicSingleton<PeerNode>::GetInstance()->CloseFd(t.second);
    }

    return true;
}
bool UnregisterNode::StartRegisterNode(std::map<std::string, int> &serverList)
{
    std::string msgId;
    uint32 sendNum = serverList.size();
    if (!GLOBALDATAMGRPTR2.CreateWait(5, sendNum, msgId))
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
    if (!GLOBALDATAMGRPTR2.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
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
                node.base58Address = nodeinfo.base58addr();
            }
            if(nodeinfo.base58addr() == selfNode.base58Address)
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
    if (!GLOBALDATAMGRPTR3.CreateWait(5, sendNum, msgId))
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
            DEBUGLOG("SendSyncNodeReq error id:{} ip:{} port:{}", node.base58Address, IpPort::IpSz(node.publicIp), node.publicPort);
        }
    }

    std::vector<std::string> returnDatas;
    if (!GLOBALDATAMGRPTR3.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
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

        int verifySignRet = ca_algorithm::VerifySign(syncNodeAck.sign(), serVinHash);
        if (verifySignRet != 0)
        {
            ERRORLOG("targetNodelist VerifySign fail!!!");
            continue;
        }
        std::vector<Node> targetAddrList;
        for (int i = 0; i < syncNodeAck.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = syncNodeAck.nodes(i);
            if(nodeinfo.base58addr() == selfNode.base58Address)
            {
                continue;
            }
            Node node;
            node.listenIp = selfNode.listenIp;
            node.listenPort = SERVERMAINPORT;
            node.publicIp = nodeinfo.public_ip();
            node.base58Address = nodeinfo.base58addr();
            node.timeStamp = nodeinfo.time_stamp();
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
        
        for(auto & i : item.second)
        {
            syncNodes.push_back(i);
        }
    }

    //Count the number of IPs and the number of times they correspond to IPs
    {
        std::unique_lock<std::mutex> locker(_mutexStakelist);

        std::map<Node,int, NodeCompare> syncNodeCount;
        for(auto it = syncNodes.begin(); it != syncNodes.end(); ++it)
        {
            syncNodeCount[*it]++;
        }
        splitAndInsertData(syncNodeCount);
        syncNodes.clear();
        syncNodeCount.clear(); 

        //Only the latest elements are stored in the maintenance map map
        if(stakeNodelist.size() == 2 && unStakeNodelist.size() == 2)
        {
            ClearSplitNodeListData();
        }
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


void UnregisterNode::DeleteSpiltNodeList(const std::string & base58)
{
    std::unique_lock<std::mutex> locker(_mutexStakelist);
    if(stakeNodelist.empty() || unStakeNodelist.empty())
    {
        ERRORLOG("stakeNodelist size = {}, unStakeNodelist size = {}",stakeNodelist.size(),unStakeNodelist.size());
        return;
    }

    for(auto & [_,iter] : stakeNodelist)
    {
        for(auto iter2 = iter.begin();iter2 != iter.end(); ++iter2)
        {
            if(iter2->first == base58)
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
            if(iter2->first == base58)
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
    DEBUGLOG("stakeNodelist size = {}",stakeNodelist.size());
    if(stakeNodelist.empty())
    {
        ERRORLOG("stakeNodelist size = {}",stakeNodelist.size());
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
        ERRORLOG("stakeNodelist empty");
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
    DEBUGLOG("ClearSplitNodeListData stakeNodelist size = {} , unStakeNodelist size = {}",stakeNodelist.size(),unStakeNodelist.size());
    auto it = stakeNodelist.begin();
    stakeNodelist.erase(it);

    auto _it = unStakeNodelist.begin();
    unStakeNodelist.erase(_it);
    DEBUGLOG("2ClearSplitNodeListData stakeNodelist size = {} , unStakeNodelist size = {}",stakeNodelist.size(),unStakeNodelist.size());
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



void UnregisterNode::splitAndInsertData(const std::map<Node, int, NodeCompare> & syncNodeCount)
{
    std::map<std::string, int>  stakeSyncNodeCount;
    std::map<std::string, int>  UnstakeSyncNodeCount;
    DEBUGLOG("splitAndInsertData @@@@@ ");
    for(auto & item : syncNodeCount)
    {
        //Verification of investment and pledge
        int ret = VerifyBonusAddr(item.first.base58Address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(item.first.base58Address, global::ca::StakeType::kStakeType_Node);
        if (stakeTime > 0 && ret == 0)
        {
            stakeSyncNodeCount.insert(std::make_pair(item.first.base58Address,item.second));
        }
        else
        {
            UnstakeSyncNodeCount.insert(std::make_pair(item.first.base58Address,item.second));
        }
    }
    DEBUGLOG("stakeNodelist size = {} , unStakeNodelist size = {}",stakeNodelist.size(),unStakeNodelist.size());
    DEBUGLOG("stakeSyncNodeCount size = {} , UnstakeSyncNodeCount size = {}",stakeSyncNodeCount.size(),UnstakeSyncNodeCount.size());
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    stakeNodelist[nowTime] = stakeSyncNodeCount;
    unStakeNodelist[nowTime] = UnstakeSyncNodeCount;
    DEBUGLOG("stakeNodelist size = {} , unStakeNodelist size = {}",stakeNodelist.size(),unStakeNodelist.size());
}

int UnregisterNode::verifyVrfDataSource(const std::vector<Node>& vrfNodelist, const uint64_t& vrfTxHeight)
{
    if(vrfNodelist.empty())
    {
        return -1;
    }
    std::set<std::string> vrfStakeNodelist;
    std::set<std::string> vrfUnStakeNodelist;
    for(const auto& node : vrfNodelist)
    {
        int ret = VerifyBonusAddr(node.base58Address);
		int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.base58Address, global::ca::StakeType::kStakeType_Node);
		if (stakeTime > 0 && ret == 0)
		{
			vrfStakeNodelist.insert(node.base58Address);
		}
        else
        {
            vrfUnStakeNodelist.insert(node.base58Address);
        }
    }

    std::unique_lock<std::mutex> locker(_mutexStakelist);
    if(vrfStakeNodelist.size() >= global::ca::kNeed_node_threshold)
    {
        if(stakeNodelist.empty())
        {
            return -2;
        }
        std::set<std::string> consensusStakeNodelist;
        for(const auto& it : stakeNodelist.rbegin()->second)
        {
            Node node;
            if(MagicSingleton<PeerNode>::GetInstance()->FindNode(it.first, node))
            {
                if(node.height >= vrfTxHeight)
                {
                    consensusStakeNodelist.insert(it.first);
                }
                continue;
            }
            consensusStakeNodelist.insert(it.first);
        }
        if(consensusStakeNodelist.empty())
        {
            ERRORLOG("consensusStakeNodelist.empty() == true");
            return -3;
        }
        std::set<std::string> difference;
        std::set_difference(consensusStakeNodelist.begin(), consensusStakeNodelist.end(),
                            vrfStakeNodelist.begin(), vrfStakeNodelist.end(),
                            std::inserter(difference, difference.begin()));

        for(auto& id : difference)
        {
            DEBUGLOG("difference, id:{}", id);
        }

        double differenceRatio = static_cast<double>(difference.size()) / consensusStakeNodelist.size();

        DEBUGLOG("difference size:{}, vrfStakeNodelist size:{}, consensusStakeNodelist size:{}, differenceRatio:{}", difference.size(), vrfStakeNodelist.size(), consensusStakeNodelist.size(), differenceRatio);
        if (differenceRatio <= 0.25)
        {
            return 0;
        }
        else
        {
            return -4;
        }

    }
    else if(!vrfUnStakeNodelist.empty())
    {
        if(unStakeNodelist.empty())
        {
            return -5;
        }
        std::set<std::string> consensusUnStakeNodelist;
        for(const auto& it : unStakeNodelist.rbegin()->second)
        {
            Node node;
            if(MagicSingleton<PeerNode>::GetInstance()->FindNode(it.first, node))
            {
                if(node.height >= vrfTxHeight)
                {
                    consensusUnStakeNodelist.insert(it.first);
                }
                continue;
            }
            consensusUnStakeNodelist.insert(it.first);
        }

        if(consensusUnStakeNodelist.empty())
        {
            ERRORLOG("consensusUnStakeNodelist.empty() == true");
            return -6;
        }

        std::set<std::string> difference;
        std::set_difference(consensusUnStakeNodelist.begin(), consensusUnStakeNodelist.end(),
                            vrfUnStakeNodelist.begin(), vrfUnStakeNodelist.end(),
                            std::inserter(difference, difference.begin()));

        for(auto& id : difference)
        {
            DEBUGLOG("difference, id:{}", id);
        }

        double differenceRatio = static_cast<double>(difference.size()) / consensusUnStakeNodelist.size();

        DEBUGLOG("difference size:{}, vrfUnStakeNodelist size:{}, consensusUnStakeNodelist size:{}, differenceRatio:{}",difference.size(), vrfUnStakeNodelist.size(), consensusUnStakeNodelist.size(), differenceRatio);
        if (differenceRatio <= 0.25)
        {
            return 0;
        }
        else
        {
            return -7;
        }
    }
    return -8;
}
