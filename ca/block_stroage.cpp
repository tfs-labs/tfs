#include "ca/block_stroage.h"
#include "utils/vrf.hpp"
#include "utils/tfs_bench_mark.h"
#include "ca/sync_block.h"
#include "proto/block.pb.h"
#include "ca/block_helper.h"
#include "ca/transaction.h"
#include "ca/sync_block.h"
#include "common/global_data.h"
#include "net/peer_node.h"
#include "proto/block.pb.h"
#include "common/task_pool.h"
#include "common/global.h"
#include "ca/algorithm.h"
#include "ca/test.h"

void BlockStroage::_StartTimer()
{
	_blockTimer.AsyncLoop(100, [this](){
		_BlockCheck();
        ExpiredDeleteCheck();
	});
}


int BlockStroage::AddBlock(const BlockMsg &msg)
{
	std::unique_lock<std::shared_mutex> lck(_blockMutex);

    CBlock block;
    block.ParseFromString(msg.block());

	std::vector<BlockMsg> msgVec;
	msgVec.push_back(msg); // The self add does not have its own signature on the block at this time
    //Self-add does not have its own signature on the block at this time
	_blockMap.insert(std::pair<std::string,std::vector<BlockMsg>>(block.hash(),msgVec));
    _blockCnt.insert(std::pair<std::string,std::vector<BlockMsg>>(block.hash(),msgVec));
	DEBUGLOG("add FailedTransactionCache");
	lck.unlock();

    return 0;
}

int BlockStroage::UpdateBlock(const BlockMsg &msg)
{
    std::unique_lock<std::shared_mutex> lck(_blockMutex);

    CBlock block;
    block.ParseFromString(msg.block());
    INFOLOG("recv block sign addr = {}",GetBase58Addr(block.sign(1).pub()));

    if(block.sign_size() != 2)
    {
		ERRORLOG("sign  size != 2");
        return -1;
    }
    bool flag = false;
	for(auto &i : _blockMap)
	{
		
		if(block.hash() != i.first || global::ca::kRecvSignCnt == _blockCnt[block.hash()].size())
		{
			continue;
		}

        auto it = _blockCnt.find(block.hash());
        if (it != _blockCnt.end())
        {
            _blockCnt[block.hash()].push_back(msg);
        }
        
		if( global::ca::kRecvSignCnt == _blockCnt[block.hash()].size())
		{
            DEBUGLOG("Recv sign count = {}",_blockCnt[block.hash()].size());
            
            Cycliclist<BlockMsg> list;
            std::vector<BlockMsg> secondMsg = _blockCnt[block.hash()];
            secondMsg.erase(secondMsg.begin());

            for(auto &msgBlock : secondMsg)
            {
                list.push_back(msgBlock);
            }

            std::string outPut , proof;
            Account defaultAccount;
            if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultAccount) != 0)
            {
                ERRORLOG("Failed to get the default account");
                return -2;
            }

            int ret = MagicSingleton<VRF>::GetInstance()->CreateVRF(defaultAccount.GetKey(), block.hash(), outPut, proof);
            if (ret != 0)
            {
                // std::cout << "error create:" << ret << std::endl;
                ERRORLOG("error create :{} generate VRF info fail",ret);
                return -3;
            }

            double randNum = MagicSingleton<VRF>::GetInstance()->GetRandNum(outPut);
            int randPos = list.size() * randNum;
            const int signMsgcnt = global::ca::kConsensus / 2;
            auto endMsgpos = randPos - signMsgcnt;

            std::vector<BlockMsg> targetMsg;
            for (; targetMsg.size() < (global::ca::kConsensus - 1); endMsgpos++)
            {
                targetMsg.push_back(list[endMsgpos]);
            }

            if(targetMsg.size() != (global::ca::kConsensus - 1))
            {
                DEBUGLOG("target lazy weight, size = {}",targetMsg.size());
                continue;
            }
        
            for(auto & msgTmg : targetMsg)
            {
                i.second.push_back(msgTmg);
            }

            //Combined into BlockMsg
            _ComposeEndBlockmsg(i.second);	
		}
	}
	lck.unlock();

	return 0;
}
bool BlockStroage::IsBlock(const std::string& blockHash)
{
    std::shared_lock<std::shared_mutex> lck(_blockMutex);
    auto result = _blockMap.find(blockHash);
    if(result != _blockMap.end())
    {
        return false;
    }
    return true;
}

void BlockStroage::_BlockCheck()
{
    std::unique_lock<std::shared_mutex> lck(_blockMutex);

    int64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    const int64_t kTenSecond = (int64_t)1000000 * 10;

	std::vector<std::string> hashKey;
	for(auto &i : _blockMap)
	{
        BlockMsg copyendmsg_ = i.second.at(0); 

        CBlock block;
        block.ParseFromString(copyendmsg_.block());
        if(block.hash() != i.first)
        {
            ERRORLOG("block.hash() != i.first blockHash:{}", block.hash());
            hashKey.push_back(block.hash()); 
            continue;
        }
        
        DEBUGLOG("block.sign_size() =  {}",block.sign_size());
        if( abs(nowTime - (int64_t)block.time()) >= kTenSecond)
        {
            ERRORLOG("Add to failure list timeout blockHash:{}", block.hash());
            hashKey.push_back(block.hash());
            copyendmsg_.Clear();

        }
        else if(block.sign_size() == global::ca::kConsensus)
        {
            DEBUGLOG("begin add cache block");

            //Verify Block flow verifies the signature of the node
            std::pair<std::string, std::vector<std::string>> nodesPair;
            
            MagicSingleton<VRF>::GetInstance()->getVerifyNodes(block.hash(), nodesPair);

            //Block signature node in cache
            std::vector<std::string> cacheNodes = nodesPair.second;

            //The signature node in the block flow
            std::vector<std::string> verifyNodes;
            for(auto &item : block.sign())
            {
                verifyNodes.push_back(GetBase58Addr(item.pub()));
                
            }
            //Compare whether the nodes in the two containers are consistent
            for(auto & signNode : verifyNodes)
            {
                if(std::find(cacheNodes.begin(), cacheNodes.end(), signNode) == cacheNodes.end())
                {
                    ERRORLOG(" The nodes in the two containers are inconsistent = {}, blockHash:{}",signNode, block.hash());
                    hashKey.push_back(block.hash());
                    continue;
                }
            }

            //After the verification is passed, the broadcast block is directly built
            if(block.version() >=0)
            {
                if(_blockStatusMap.find(block.hash()) == _blockStatusMap.end())
                {
                    _blockStatusMap[block.hash()] = {block.hash(), block};
                }
                auto NowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
			    MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(block.hash(), &NowTime, 2);
                MagicSingleton<BlockMonitor>::GetInstance()->SendBroadcastAddBlock(copyendmsg_.block(),block.height());
                INFOLOG("Start to broadcast BuildBlockBroadcastMsg...");
            }
            else
            {
                std::cout << "The version is too low. Please update the version!" << std::endl;
            }
            
            hashKey.push_back(block.hash());
            copyendmsg_.Clear();
        }

	}
	
    if(!hashKey.empty())
	{
		for (auto &hash : hashKey)
		{
			_Remove(hash);
		}
	}
	hashKey.clear();    
}

void BlockStroage::_ComposeEndBlockmsg(std::vector<BlockMsg> &msgvec)
{
	for(auto &msg : msgvec)
	{  
        CBlock block;
        block.ParseFromString(msg.block());

        if(block.sign_size() == 1)  
        {
            continue;
        }

        if(block.sign_size() != 2)
        {
            continue;
        }
        else
        {
            CBlock endBlock;
            endBlock.ParseFromString(msgvec[0].block()); 

            CSign * sign  = endBlock.add_sign();
            sign->set_pub(block.sign(1).pub());
            sign->set_sign(block.sign(1).sign());
            INFOLOG("rand block sign = {}",GetBase58Addr(block.sign(1).pub()));

            msgvec[0].set_block(endBlock.SerializeAsString());

        }    
    }        
}

void BlockStroage::_Remove(const std::string &hash)
{
	for(auto iter = _blockMap.begin(); iter != _blockMap.end();)
	{
		if (iter->first == hash)
		{
			iter = _blockMap.erase(iter);
			DEBUGLOG("BlockStroage::Remove hash:{}", hash);
		}
		else
		{
			iter++;
		}
	}

    for(auto iter = _blockCnt.begin(); iter != _blockCnt.end();)
	{
		if (iter->first == hash)
		{
			iter = _blockCnt.erase(iter);
			DEBUGLOG("BlockStroage::Remove  _blockCnt hash:{}", hash);
		}
		else
		{
			iter++;
		}
	}
}

std::shared_future<RetType> BlockStroage::GetPrehash(const uint64_t height)
{
    std::shared_lock<std::shared_mutex> lck(_prehashMutex);
    auto result = _preHashMap.find(height);
    if(result != _preHashMap.end())
    {
       return result->second;
    }
    return {};
}

bool BlockStroage::IsSeekTask(uint64_t seekHeight)
{
    std::shared_lock<std::shared_mutex> lck(_prehashMutex);
    if(_preHashMap.find(seekHeight) != _preHashMap.end())
    {
        DEBUGLOG("seek_prehash_task repeat");
        return true;
    }
    return false;
}


void BlockStroage::CommitSeekTask(uint64_t seekHeight)
{
    if(IsSeekTask(seekHeight))
    {
        return;
    }
    std::unique_lock<std::shared_mutex> lck(_prehashMutex);
    if(_preHashMap.size() > 100)
    {
        auto endHeight = _preHashMap.end()->first;
        std::map<uint64_t, std::shared_future<RetType>> PreHashTemp(_preHashMap.find(endHeight - 10), _preHashMap.end());
        _preHashMap.clear();
        _preHashMap.swap(PreHashTemp);
    }
    DEBUGLOG("CommitSeekTask, height:{}", seekHeight);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStroage::_SeekPreHashThread, this, seekHeight));
    try
    {
        _preHashMap[seekHeight] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockTask([task](){(*task)();});
    return;
}

void BlockStroage::ForceCommitSeekTask(uint64_t seekHeight)
{
    std::unique_lock<std::shared_mutex> lck(_prehashMutex);
    DEBUGLOG("ForceCommitSeekTask, height:{}", seekHeight);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStroage::_SeekPreHashThread, this, seekHeight));
    try
    {
        _preHashMap[seekHeight] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockTask([task](){(*task)();});
    return;
}

int GetPrehashFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds)
{
    int ret = 0;
    if ((ret = SyncBlock::GetSyncNodeSimplify(num, selfNodeHeight, pledgeAddr,
                                                                             sendNodeIds)) != 0)
    {
        ERRORLOG("get seek node fail, ret:{}", ret);
        return -1;
    }
    return 0;
}
RetType BlockStroage::_SeekPreHashThread(uint64_t seekHeight)
{
    DEBUGLOG("_SeekPreHashThread Start");
    uint64_t chainHeight = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->ObtainChainHeight(chainHeight))
    {
        DEBUGLOG("ObtainChainHeight fail!!!");
        return {"",0};
    }
    uint64_t selfNodeHeight = 0;
    std::vector<std::string> pledgeAddr; // stake and invested addr
    {
        DBReader dbReader;
        auto status = dbReader.GetBlockTop(selfNodeHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            DEBUGLOG("GetBlockTop fail!!!");
            return {"",0};
        }
        std::vector<std::string> stakeAddr;
        status = dbReader.GetStakeAddress(stakeAddr);
        if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
        {
            DEBUGLOG("GetStakeAddress fail!!!");
            return {"",0};
        }

        for(const auto& addr : stakeAddr)
        {
            if(VerifyBonusAddr(addr) != 0)
            {
                DEBUGLOG("{} doesn't get invested, skip", addr);
                continue;
            }
            pledgeAddr.push_back(addr);
        }
    }
    std::vector<std::string> sendNodeIds;
    if (GetPrehashFindNode(10, chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
        return {"",0};
    }
    if(seekHeight == 0 || seekHeight > selfNodeHeight)
    {
        DEBUGLOG("seekHeight:{}, selfNodeHeight:{}", seekHeight, selfNodeHeight);
    }
    return _SeekPreHashByNode(sendNodeIds, seekHeight, selfNodeHeight, chainHeight);
}

RetType BlockStroage::_SeekPreHashByNode(
		const std::vector<std::string> &sendNodeIds, uint64_t seekHeight, const uint64_t &selfNodeHeight, const uint64_t &chainHeight)
{
    std::string msgId;
    uint64_t succentCount = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(10, sendNodeIds.size() * 0.8, msgId))
    {
        ERRORLOG("CreateWait fail!!!");
        return {"", 0};
    }
    for (auto &nodeId : sendNodeIds)
    {
        DEBUGLOG("new seek get block hash from {}", nodeId);
        SendSeekGetPreHashReq(nodeId, msgId, seekHeight);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < sendNodeIds.size() * 0.5)
        {
            ERRORLOG("wait seek block hash time out send:{} recv:{}", sendNodeIds.size(), retDatas.size());
            return {"", 0};
        }
    }

    std::map<std::string, bool> nodeAddrs;
    MagicSingleton<PeerNode>::GetInstance()->GetNodelist(nodeAddrs);
    
    SeekPreHashByHightAck ack;
    std::map<uint64_t, std::map<std::string, uint64_t>> seekPreHashes;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        succentCount++;
        uint64_t seekHeight = ack.seek_height();
        for(auto& prehash : ack.prehashes())
        {
            if(seekPreHashes[seekHeight].find(prehash) == seekPreHashes[seekHeight].end())
            {
                seekPreHashes[seekHeight][prehash] = 1;
            }
            else
            {
                seekPreHashes[seekHeight][prehash]++;
            }
        } 
    }

    std::set<std::string> verifyHashes;
    size_t verifyNum = succentCount / 5 * 3;

    for (auto &iter : seekPreHashes)
    {
        uint16_t maxPercentage = 0;
        std::string maxPercentagePrehash;
        for(auto &prehash : iter.second)
        {
            if (prehash.second >= verifyNum)
            {
                uint16_t percentage = prehash.second / succentCount * 100;
                if(maxPercentage < percentage)
                {
                    maxPercentage = percentage;
                    maxPercentagePrehash = prehash.first;
                }
            }
        }
        if(maxPercentage >= 80 || maxPercentage >= 70)
        {
            DEBUGLOG("_SeekPreHashByNode <success> !!! ,seekHeight:{}, maxPercentage:{} > 70% , maxPercentagePrehash:{}", iter.first, maxPercentage, maxPercentagePrehash);
            return {maxPercentagePrehash, maxPercentage};
        }
        else
        {
            DEBUGLOG("_SeekPreHashByNode <fail> !!! ,seekHeight:{}, maxPercentage:{} < 70% , maxPercentagePrehash:{}", iter.first, maxPercentage, maxPercentagePrehash);
        }
    }
    return {"", 0};
}

int BlockStroage::CheckData(const BlockStatus& blockStatus)
{
    if(blockStatus.status() >= 0)
    {
        DEBUGLOG("AAAC block status invalid");
        return -1;
    }
    auto& hash = blockStatus.blockhash();
    auto destNode = blockStatus.id();
    auto found = _blockStatusMap.find(hash);
    if(found == _blockStatusMap.end())
    {
        return -2;
    }
    else if(_blockStatusMap[hash].broadcastType == BroadcastType::verifyBroadcast)
    {
        if(found->second.verifyNodes.empty() || 
        found->second.verifyNodes.find(destNode) == found->second.verifyNodes.end())
        {
            return -3;
        }
        found->second.verifyNodes.erase(destNode);
        if(found->second.blockStatusList.size() > found->second.verifyNodesNumber)
        {
            return -4;
        }
    }
    else if(_blockStatusMap[hash].broadcastType == BroadcastType::level1Broadcast)
    {
        if(found->second.level1Nodes.empty() || 
        found->second.level1Nodes.find(destNode) == found->second.level1Nodes.end())
        {
            return -5;
        }
        found->second.level1Nodes.erase(destNode);
        if(_blockStatusMap[hash].blockStatusList.size() > found->second.level1NodesNumber * _failureRate)
        {
            return -6;
        }
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    if(nowTime > _blockStatusMap[hash].block.time() + 5 * 1000000ull)
    {
        DEBUGLOG("AAAC,blockStatus nowTime:{},blockTime:{}, timeout:{}",nowTime, (_blockStatusMap[hash].block.time() + 5 * 1000000ull), (nowTime - (_blockStatusMap[hash].block.time() + 5 * 1000000ull))/1000000);
        return -7;
    }
    return 0;
}


void BlockStroage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes, const Vrf& vrf)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    if(_blockStatusMap.find(blockHash) == _blockStatusMap.end())
    {
        BlockStatusWrapper blockStatusWrapper;
        blockStatusWrapper.blockHash = blockHash;
        blockStatusWrapper.block = Block;
        blockStatusWrapper.broadcastType = BroadcastType::verifyBroadcast;
        blockStatusWrapper.verifyNodesNumber = global::ca::KSend_node_threshold - global::ca::kConsensus + 2;
        blockStatusWrapper.verifyNodes.insert(verifyNodes.begin(), verifyNodes.end());
        for(auto &tx : Block.txs())
        {
            if(GetTransactionType(tx) != kTransactionType_Tx)
            {
                continue;
            }

            uint64_t handleTxHeight =  Block.height() - 1;
            TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
            if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
            {
                continue;
            }

            std::pair<std::string,Vrf>  vrf;
            if(!MagicSingleton<VRF>::GetInstance()->getVrfInfo(tx.hash(), vrf))
            {
                ERRORLOG("getVrfInfo failed! txhash:{}", tx.hash());
                continue;
            }
            blockStatusWrapper.vrfMap[tx.hash()] = vrf.second;

            if(!MagicSingleton<VRF>::GetInstance()->getTxVrfInfo(tx.hash(), vrf))
            {
                ERRORLOG("getTxVrfInfo failed! txhash:{}", tx.hash());
                continue;
            }
            blockStatusWrapper.txvrfMap[tx.hash()] = vrf.second;

        }
        blockStatusWrapper.vrfMap[blockHash] = vrf;
        _blockStatusMap[blockHash] = blockStatusWrapper;
    }
}

void BlockStroage::ExpiredDeleteCheck()
{
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::unique_lock<std::mutex> lck(_statusMutex);
    for(auto iter = _blockStatusMap.begin(); iter != _blockStatusMap.end();)
    {
        if(nowTime >= iter->second.block.time() + 15ull * 1000000)
        {
            DEBUGLOG("_blockStatusMap deleteBlockHash:{}", iter->second.block.hash().substr(0,6));
            _blockStatusMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

void BlockStroage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    auto found = _blockStatusMap.find(blockHash);
    if(found != _blockStatusMap.end())
    {
        found->second.block = Block;
        found->second.broadcastType = BroadcastType::level1Broadcast;
        found->second.level1NodesNumber = level1Nodes.size();
        found->second.blockStatusList.clear();
        found->second.level1Nodes.insert(level1Nodes.begin(), level1Nodes.end());
    }
}

void BlockStroage::AddBlockStatus(const BlockStatus& blockStatus)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    auto& hash = blockStatus.blockhash();
    auto ret = CheckData(blockStatus);
    if(ret != 0)
    {
        DEBUGLOG("AAAC Valid data, ret:{}, blockHash:{}, destNode:{}", ret, hash.substr(0,6), blockStatus.id());
        return;
    }

    _blockStatusMap[hash].blockStatusList.push_back(blockStatus);
    DEBUGLOG("AAAC destNode:{}, blockhash:{}, blockStatus:{}, level_1_broadcast_nodes:{}, broadcastType:{}",blockStatus.id(), hash.substr(0,6), blockStatus.status(), _blockStatusMap[hash].level1Nodes.size(), _blockStatusMap[hash].broadcastType);

    if(_blockStatusMap[hash].broadcastType == BroadcastType::level1Broadcast && _blockStatusMap[hash].blockStatusList.size() == (uint32_t)(_blockStatusMap[hash].level1NodesNumber * _failureRate))
    {
        MagicSingleton<TaskPool>::GetInstance()->CommitBlockTask(std::bind(&BlockStroage::NewbuildBlockByBlockStatus,this,hash));
    }
    else if(_blockStatusMap[hash].broadcastType == BroadcastType::verifyBroadcast && _blockStatusMap[hash].blockStatusList.size() == _blockStatusMap[hash].verifyNodesNumber)
    {
        MagicSingleton<TaskPool>::GetInstance()->CommitBlockTask(std::bind(&BlockStroage::NewbuildBlockByBlockStatus,this,hash));
    }
}


void BlockStroage::NewbuildBlockByBlockStatus(const std::string blockHash)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    CBlock& oldBlock = _blockStatusMap[blockHash].block;
    CBlock newBlock = oldBlock;
    uint32_t FailThreshold = 0;

    if(_blockStatusMap[blockHash].broadcastType == BroadcastType::verifyBroadcast)
    {
        FailThreshold = _blockStatusMap[blockHash].verifyNodesNumber * _failureRate;
    }
    else if(_blockStatusMap[blockHash].broadcastType == BroadcastType::level1Broadcast)
    {
        FailThreshold = _blockStatusMap[blockHash].level1NodesNumber * _failureRate;
    }

    DEBUGLOG("AAAC oldBlockHash:{}, broadcastType:{}, FailThreshold:{}", blockHash.substr(0,6), _blockStatusMap[blockHash].broadcastType, FailThreshold);

    newBlock.clear_txs();
    newBlock.clear_sign();
    newBlock.clear_hash();

    std::map<std::string, uint32> txsStatus;
    std::multimap<std::string, int> testMap;
    
    for(auto& iter : _blockStatusMap[blockHash].blockStatusList)
    {
        for(auto& tx : iter.txstatus())
        {
            if(txsStatus.find(tx.txhash()) == txsStatus.end())
            {
                txsStatus[tx.txhash()] = 1;
            }
            else
            {
                txsStatus[tx.txhash()]++;
            }
            testMap.insert({tx.txhash(), tx.status()});
        }
    }

    for(auto &it : testMap)
    {
        DEBUGLOG("AAAC txstatus , txHash:{}, err:{}",it.first, it.second);
    }

    BlockMsg blockmsg;
    for(auto &tx : oldBlock.txs())
    {
        if(GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }

        if(txsStatus.find(tx.hash()) != txsStatus.end() && txsStatus[tx.hash()] >= FailThreshold)
        {
            DEBUGLOG("AAAC delete tx txHash:{} ,poll:{}", tx.hash(), txsStatus[tx.hash()]);
            continue;
        }
        
        if(VerifyTxTimeOut(tx) != 0)
        {
            DEBUGLOG("AAAC time out tx hash = {}, blockHash:{}",tx.hash(), oldBlock.hash().substr(0,6));
            continue;
        }

        uint64_t handleTxHeight =  oldBlock.height() - 1;
        TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
        if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
        {
            *newBlock.add_txs() = tx;
            continue;
        }
        auto foundVrf = _blockStatusMap[blockHash].vrfMap.find(tx.hash());
        if(foundVrf != _blockStatusMap[blockHash].vrfMap.end())
        {
            blockmsg.add_vrfinfo()->CopyFrom(foundVrf->second);
            
        } 
        else
        {
            continue;
        }
        auto foundTxVrf = _blockStatusMap[blockHash].txvrfMap.find(tx.hash());
        if(foundTxVrf != _blockStatusMap[blockHash].txvrfMap.end())
        {
            auto vrfJson = nlohmann::json::parse(foundTxVrf->second.data());
            vrfJson["txhash"] = tx.hash();
            foundTxVrf->second.set_data(vrfJson.dump());

            blockmsg.add_txvrfinfo()->CopyFrom(foundTxVrf->second);
            *newBlock.add_txs() = tx;
        } 
    }
    lck.unlock();
    if(newBlock.txs_size() == 0)
    {
        DEBUGLOG("newBlock.txs_size() == 0");
        return;
    }

    InitNewBlock(oldBlock, newBlock);

    std::ostringstream filestream;
    PrintBlock(newBlock, true, filestream);

    std::string test_str = filestream.str();
    DEBUGLOG("AAAC newBuildBlock --> {}", test_str);

    blockmsg.set_version(global::kVersion);
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    blockmsg.set_block(newBlock.SerializeAsString());

    auto msg = make_shared<BlockMsg>(blockmsg);
	auto ret = DoHandleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("AAAC DoHandleBlock failed The error code is {}",ret);
        CBlock cblock;
	    if (!cblock.ParseFromString(msg->block()))
	    {
		    ERRORLOG("fail to serialization!!");
		    return;
	    }
        ClearVRF(cblock);
        return;
    }

    DEBUGLOG("AAAC newBuildBlock success oldBlockHash:{}, newBlockHash:{}",oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
    return;
}

int BlockStroage::InitNewBlock(const CBlock& oldBlock, CBlock& newBlock)
{
	newBlock.set_version(0);
	newBlock.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	newBlock.set_height(oldBlock.height());
    newBlock.set_prevhash(oldBlock.prevhash());
	newBlock.set_merkleroot(ca_algorithm::CalcBlockMerkle(newBlock));
	newBlock.set_hash(Getsha256hash(newBlock.SerializeAsString()));
	return 0;
}

void SendSeekGetPreHashReq(const std::string &nodeId, const std::string &msgId, uint64_t seekHeight)
{
    SeekPreHashByHightReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    req.set_seek_height(seekHeight);
    NetSendMessage<SeekPreHashByHightReq>(nodeId, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return;
}

void SendSeekGetPreHashAck(SeekPreHashByHightAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t seekHeight)
{
    DEBUGLOG("SendSeekGetPreHashAck, id:{}, height:{}",  nodeId, seekHeight);
    ack.set_self_node_id(NetGetSelfNodeId());
    DBReader dbReader;
    uint64_t selfNodeHeight = 0;
    if (0 != dbReader.GetBlockTop(selfNodeHeight))
    {
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msgId);
    std::vector<std::string> blockHashes;
    if(seekHeight > selfNodeHeight)
    {
        return;
    }

    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(seekHeight, blockHashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail !!!");
        return;
    }
    ack.set_seek_height(seekHeight);
    for(auto &hash : blockHashes)
    {
        ack.add_prehashes(hash);
    }
    
    return;
}

int HandleSeekGetPreHashReq(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgdata)
{
    SeekPreHashByHightAck ack;
    SendSeekGetPreHashAck(ack,msg->self_node_id(), msg->msg_id(), msg->seek_height());
    NetSendMessage<SeekPreHashByHightAck>(msg->self_node_id(), ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}
int HandleSeekGetPreHashAck(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int DoProtoBlockStatus(const BlockStatus& blockStatus, const std::string destNode)
{
    NetSendMessage<BlockStatus>(destNode, blockStatus, net_com::Priority::kPriority_High_1);
    return 0;
}

int HandleBlockStatusMsg(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgdata)
{
    MagicSingleton<BlockStroage>::GetInstance()->AddBlockStatus(*msg);
    return 0;
}



