#include "dispatchtx.h"
#include "logging.h"

void ContractDispatcher::Process()
{
    _dispatcherThread = std::thread(std::bind(&ContractDispatcher::_DispatcherProcessingFunc, this));
    _dispatcherThread.detach();
}

void ContractDispatcher::_DispatcherProcessingFunc()
{
    uint64_t timeBaseline = 0;
    while (true)
    {
        sleep(1);
        {
            std::unique_lock<std::mutex> locker(_contractInfoCacheMutex);
            if (_contractDependentCache.empty())
            {
                continue;
            }
        }

        //FormatUTCTimestamp
        // 10:06:09
        // 10:06:00
        // uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        // if (timeBaseline == 0)
        // {
        //     timeBaseline = (currentTime / 1000000 / _precedingContractBlockLookupInterval) * 1000000 * _precedingContractBlockLookupInterval;
        //     DEBUGLOG("current baseline {}", timeBaseline);
        // }
        // auto s1 = MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(currentTime);
        // auto s2 = MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(timeBaseline);
        // DEBUGLOG("FFF OOOOOOOO, currentTime:{}, timeBaseline:{}", s1, s2);

        uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        if(timeBaseline == 0)
        {
            timeBaseline = MagicSingleton<TimeUtil>::GetInstance()->GetTheTimestampPerUnitOfTime(currentTime);
            auto s1 = MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(currentTime);
            auto s2 = MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(timeBaseline);
            DEBUGLOG("FFF OOOOOOOO, currentTime:{}, timeBaseline:{}", s1, s2);
        }
        /*
            A = Transaction reception time in 10 seconds (time interval between two dispatchers' packaging) = Switching dispatcher time (10 seconds)
            B = Delay in distribution and packaging after receiving the transaction by the dispatcher in 5 seconds
            C = B (Fastest transaction confirmation time)
            D = A + B (Slowest transaction confirmation time)
        */
        if (currentTime >= timeBaseline + _contractWaitingTime)
        {
            std::unique_lock<std::mutex> lock(_contractHandleMutex);
            DEBUGLOG("************** time out 10s");
   
            std::multimap<std::string, msgInfo> distribution;
            auto ret = DistributionContractTx(distribution);
            if(ret != 0)
            {
                ERRORLOG("++++++++++++++++ DistributionContractTx error ret :{} ++++++++++++++++++", ret);
                continue;;
            }

            if(distribution.empty())
            {
                ERRORLOG("++++++++++++++++ distribution empty ++++++++++++++++++");
            }

            for(auto & item : distribution)
            {
                DEBUGLOG("-0-0-0-0-0-0-0-0- send packager : {}, Number of transactions: {} ", item.first, item.second.txMsgReq.size());
                for(auto & txmsg : item.second.txMsgReq)
                {
                CTransaction contractTx;
                if (!contractTx.ParseFromString(txmsg.txmsginfo().tx()))
                {
                    ERRORLOG("Failed to deserialize transaction body!");
                    continue;
                }
                DEBUGLOG("send packager : {} , tx hash : {}", item.first, contractTx.hash());
                }
                SendTxInfoToPackager(item.first,item.second.info,item.second.txMsgReq,item.second.nodelist);
            }

            

            std::scoped_lock locker(_contractInfoCacheMutex,_contractMsgMutex);
            _contractMsgReqCache.clear();
            _contractDependentCache.clear();
            timeBaseline = 0;

        }

    }
}

void ContractDispatcher::AddContractInfo(const std::string& contractHash, const std::vector<std::string>& dependentContracts)
{
    std::unique_lock<std::mutex> locker(_contractInfoCacheMutex);
    _contractDependentCache.insert(std::make_pair(contractHash, dependentContracts));
    DEBUGLOG("+++++++++++++++++AddContractInfo++++++++++++++");
}


void ContractDispatcher::AddContractMsgReq(const std::string& contractHash, const ContractTxMsgReq &msg)
{
    std::unique_lock<std::mutex> locker(_contractMsgMutex);
    TxMsgReq  txmsg = msg.txmsgreq();

    _contractMsgReqCache.insert(std::make_pair(contractHash, txmsg));
    DEBUGLOG("+++++++++++++++++AddContractMsgReq++++++++++++++");

}

bool ContractDispatcher::HasDuplicate(const std::vector<std::string>& v1, const std::vector<std::string>& v2)
{
	std::unordered_set<std::string> set;

	for (const std::string& s : v1) {
		set.insert(s);
	}

	for (const std::string& s : v2) {
		if (set.count(s) > 0) {
			return true;
		}
	}

	return false;

}

int ContractDispatcher::SendTxInfoToPackager(const std::string &packager, const Vrf &info, std::vector<TxMsgReq> &txsmsg, const std::set<std::string> nodelist)
{
    DEBUGLOG("***** SendTxInfoToPackager *****");
    ContractPackagerMsg msg;
    msg.set_version(global::kVersion);

    Vrf * vrfinfo = msg.mutable_vrfinfo();
    vrfinfo->CopyFrom(info);
    

    for(auto & msgreq : txsmsg)
    {
        TxMsgReq * _Msg = msg.add_txmsgreq();
        _Msg -> CopyFrom(msgreq);
    }

    VrfDataSource *data = msg.mutable_vrfdatasource();
    for (const auto& addr : nodelist)
    {
        data->add_vrfnodelist(addr);
        DEBUGLOG("add addr = {}",addr);
    }
    
    std::string owner = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    std::string serVinHash = Getsha256hash(msg.SerializeAsString());
    std::string signature;
    std::string pub;
    if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
    {
        ERRORLOG("sign fail");
        return -1;
    }
    
    CSign * Sign = msg.mutable_sign();
    Sign->set_sign(signature);
    Sign->set_pub(pub);

	NetSendMessage<ContractPackagerMsg>(packager, msg, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);

    if(owner == packager)
    {
        auto sendTxmsg = std::make_shared<ContractPackagerMsg>(msg);
        MsgData msgdata;
        HandleContractPackagerMsg(sendTxmsg,msgdata);
    }
    return 0;

}

std::vector<std::vector<TxMsgReq>> ContractDispatcher::GetDependentData()
{
    DEBUGLOG("DependencyGrouping");
    std::vector<std::set<std::string>> res;

	for (const auto& [key, values] : _contractDependentCache) {
		std::set<std::string> commonKeys;
		commonKeys.insert(key);

		for (const auto& [otherKey, otherValues] : _contractDependentCache) {
			if (key == otherKey)  continue;

			if (HasDuplicate(values, otherValues) == true)
			{
				commonKeys.insert(otherKey);
			}
		}

		if (!commonKeys.empty()) {
			bool foundDuplicate = false;
			for (auto& itemSet : res) {
                
				std::set<std::string> intersection;
				std::set_intersection(
					itemSet.begin(), itemSet.end(),
					commonKeys.begin(), commonKeys.end(),
					std::inserter(intersection, intersection.begin())
				);

				if (!intersection.empty()) {
					foundDuplicate = true;
					itemSet.insert(commonKeys.begin(), commonKeys.end());
					break;
				}
			}
			if (!foundDuplicate) {
				res.push_back(commonKeys);
			}
		}
	}

    std::vector<std::vector<TxMsgReq>> txMsgVec;
    for(const auto & hashContainer : res)
    {
        std::vector<TxMsgReq> messageVec;
        for(const auto & hash : hashContainer)
        {
            messageVec.emplace_back(_contractMsgReqCache.at(hash));
        }
        txMsgVec.push_back(messageVec);
    }
    return txMsgVec;
}

std::vector<std::vector<TxMsgReq>> ContractDispatcher::GroupDependentData(const std::vector<std::vector<TxMsgReq>> & txMsgVec)
{
    int size = txMsgVec.size();
    int parts = 8;

    std::vector<std::vector<TxMsgReq>> groupTxMsg;
    if(size <= parts)
    {
        for(const auto & hashContainer : txMsgVec)
        {
            groupTxMsg.emplace_back(hashContainer);
        }
    }
    else
    {
        int perPart = size / parts;
        int remain = size % parts;

        int index = 0;

        for(int i = 0; i < parts; i++) {
            std::vector<TxMsgReq> part;
            for(int j = index; j < index + perPart; j++) 
            {
                for(auto& txMsg : txMsgVec[j])
                {
                    part.push_back(txMsg);
                }
            }

            index += perPart;
            if(remain > 0) 
            {
                for(int j = index; j < index + remain; j++)
                {
                    for(auto& txMsg : txMsgVec[j])
                    {
                        part.push_back(txMsg);
                    }
                }
                index += remain;
                remain = 0;
            }
            groupTxMsg.push_back(part);
        }
    }
    return groupTxMsg;
}

int ContractDispatcher::DistributionContractTx(std::multimap<std::string, msgInfo>& distribution)
{

    std::vector<std::vector<TxMsgReq>> txMsgVec = GetDependentData();
    if(txMsgVec.empty())
    {
        ERRORLOG("DependencyGrouping txMsgVec is empty");
        return -1;
    }
    std::vector<std::vector<TxMsgReq>> groupTxMsg = GroupDependentData(txMsgVec);
    if(groupTxMsg.empty())
    {
        ERRORLOG("GroupDependentData groupTxMsg is empty");
        return -2;
    }
    for(const auto & txMsgContainer : groupTxMsg)
    {
        
        std::string contractHash;
        std::vector<TxMsgReq> txMsgData;

        for(const auto & txMsg : txMsgContainer)
        {
            CTransaction ContractTx;
            if (!ContractTx.ParseFromString(txMsg.txmsginfo().tx()))
            {
                ERRORLOG("Failed to deserialize transaction body!");
                return -2;
            }
            contractHash += ContractTx.hash();
            txMsgData.push_back(txMsg);
        }
    
        std::string vrfInput = Getsha256hash(contractHash);

        std::set<std::string>  out_nodelist;
        std::string packAddr;
        Vrf vrf;
        auto ret = FindContractPackNode(vrfInput ,packAddr,vrf,out_nodelist);
        if(ret != 0)
        {
            ERRORLOG("DistributionContractTx ret :{}", ret);
            continue;
        }

        msgInfo msg;
        msg.info = vrf;
        msg.txMsgReq = txMsgData;
        msg.nodelist = out_nodelist;
        distribution.insert(std::make_pair(packAddr, msg));
    }
    
    return 0;
}
