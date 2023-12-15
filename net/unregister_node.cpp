
#include "unregister_node.h"

#include "./handle_event.h"
#include "./api.h"

#include "../utils/time_util.h"
#include "../net/peer_node.h"
#include "../utils/magic_singleton.h"
#include "../common/global_data.h"
#include "../common/config.h"
#include "../proto/net.pb.h"

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
        }
    }

    std::vector<std::string> returnDatas;
    if (!GLOBALDATAMGRPTR2.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait Register time out send:{} recv:{}", sendNum, returnDatas.size());
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
    for (auto &retData : returnDatas)
    {
        syncNodeAck.Clear();
        if (!syncNodeAck.ParseFromString(retData))
        {
            continue;
        }
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
            //Add the nodes brought back by synchronizing the nodes to the array
            syncNodes.push_back(node);

        }
    }

    //Count the number of IPs and the number of times they correspond to IPs
    {
        std::map<Node,int, NodeCompare> syncNodeCount;
        for(auto it = syncNodes.begin(); it != syncNodes.end(); ++it)
        {
            syncNodeCount[*it]++;
        }
        AddConsensusNode(syncNodeCount);
        syncNodes.clear();
        syncNodeCount.clear();
    }

    //Only the latest elements are stored in the maintenance map map
    if(_consensusNodeList.size() == 2)
    {
        ClearConsensusNodeList();
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

void UnregisterNode::GetIpMap(std::map<uint64_t, std::map<Node, int, NodeCompare>> & m1)
{
    std::shared_lock<std::shared_mutex> lck(_mutexConsensusNodes);
    m1 = _consensusNodeList;
}

void UnregisterNode::AddConsensusNode(const std::map<Node, int, NodeCompare>  syncNodeCount)
{
    std::unique_lock<std::shared_mutex> lck(_mutexConsensusNodes);
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    _consensusNodeList[currentTime] = syncNodeCount;
}

void UnregisterNode::DeleteConsensusNode(const std::string & base58)
{
    std::unique_lock<std::shared_mutex> lck(_mutexConsensusNodes);
    auto iter1 = _consensusNodeList.begin();
    if(_consensusNodeList.empty() || iter1->second.empty() )
    {
        ERRORLOG("_consensusNodeList is empty");
        return;
    }

    for(auto & [_ , iter]: _consensusNodeList)
    {
        for(auto iter2 = iter.begin(); iter2 != iter.end(); ++iter2)
        {
            if(iter2->first.base58Address == base58)
            {
                iter2 = iter.erase(iter2);
                return;
            }
        }
    }
}

void UnregisterNode::ClearConsensusNodeList()
{
    std::unique_lock<std::shared_mutex> lck(_mutexConsensusNodes);
    auto it = _consensusNodeList.begin();
    _consensusNodeList.erase(it);
}

std::vector<Node> UnregisterNode::GetConsensusNodeList(std::vector<Node> & nodeList)
{
    std::shared_lock<std::shared_mutex> lck(_mutexConsensusNodes);
    std::vector<Node> nodes;

    if(_consensusNodeList.empty())
    {
        return nodes;
    }

    auto item = _consensusNodeList.begin();

    std::map<int,int> mapCnts; 
    std::vector<uint64_t> counts;
    for(auto iter : item->second)
    {
        counts.push_back(iter.second);
        ++mapCnts[iter.second];
    }

    int maxElem = 0, maxCnt = 0;
    for(auto iter : mapCnts)
    {
        if(maxCnt <  iter.second)
        {
            maxElem = iter.first;
            maxCnt = iter.second;
        }
    }

    DEBUGLOG("GetConsensusNodeList : maxCnt = {} , item->second.size = {}, maxElem = {}", maxCnt, item->second.size(), maxElem);

    counts.erase(std::remove_if(counts.begin(), counts.end(),[maxElem](int x){ return x == maxElem;}), counts.end());
    if(counts.size() < 2)
    {
        for(auto i : item->second)
        {
            if(i.second == maxElem)
            {
                nodes.push_back(i.first);
            }
        }
    }
    else
    {

        uint64_t quarterNum = counts.size() * 0.25;
        uint64_t threeQuarterNum = counts.size() * 0.75;
        if (quarterNum == threeQuarterNum)
        {
            ERRORLOG("Number of exceptions quarterNum = {}, threeQuarterNum = {}", quarterNum, threeQuarterNum);
            return nodes;
        }

        std::sort(counts.begin(), counts.end());

        uint64_t quarterNumValue = counts.at(quarterNum);
        uint64_t threeQuarterNumValue = counts.at(threeQuarterNum);
        int64_t slowerLimitValue = quarterNumValue -
                                            ((threeQuarterNumValue - quarterNumValue) * 1.5);

        if(slowerLimitValue > maxElem)
        {
            DEBUGLOG("Modify the value of the number of exceptions : {} , slowerLimitValue: {}", maxElem, slowerLimitValue);
            slowerLimitValue = maxElem;
        }

        if(slowerLimitValue >= 0)
        {
            for (auto iter = item->second.begin(); iter != item->second.end(); ++iter)
            {
                if (iter->second < slowerLimitValue)
                {
                    DEBUGLOG("slowerLimitValue : {}, iter->second", slowerLimitValue, iter->second);
                    continue;
                }

                nodes.push_back(iter->first);
            }
        }
    }


    for(auto & node : nodeList)
    {
        for(auto & iter : nodes)
        {
            if(node.base58Address == iter.base58Address)
            {
                iter.height = node.height;
            }
        }
    }

    return nodes;
}



