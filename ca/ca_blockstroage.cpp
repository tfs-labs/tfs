#include "ca/ca_blockstroage.h"
#include "utils/VRF.hpp"
#include "utils/TFSbenchmark.h"
#include "ca/ca_sync_block.h"
#include "proto/block.pb.h"
#include "ca/ca_blockhelper.h"
#include "ca/ca_transaction.h"
#include "ca/ca_sync_block.h"
#include "common/global_data.h"
#include "net/peer_node.h"
#include "proto/block.pb.h"
#include "common/task_pool.h"
#include "common/global.h"
#include "ca/ca_algorithm.h"
#include "ca/ca_test.h"

void BlockStroage::StartTimer()
{
	_block_timer.AsyncLoop(100, [this](){
		BlockCheck();
        ExpiredDeleteCheck();
	});
}


int BlockStroage::AddBlock(const BlockMsg &msg)
{
	std::unique_lock<std::shared_mutex> lck(_block_mutex_);

    CBlock block;
    block.ParseFromString(msg.block());

	std::vector<BlockMsg> msgVec;
	msgVec.push_back(msg); // The self add does not have its own signature on the block at this time
    //Self-add does not have its own signature on the block at this time
	_BlockMap.insert(std::pair<std::string,std::vector<BlockMsg>>(block.hash(),msgVec));

	DEBUGLOG("add FailedTransactionCache");
	lck.unlock();

    return 0;
}

int BlockStroage::UpdateBlock(const BlockMsg &msg)
{
    std::unique_lock<std::shared_mutex> lck(_block_mutex_);

    CBlock block;
    block.ParseFromString(msg.block());

    if(block.sign_size() != 2)
    {
		ERRORLOG("sign  size != 2");
        return -1;
    }


	for(auto &i : _BlockMap)
	{
		
		if(block.hash() != i.first || i.second.size() == global::ca::kConsensus)
		{
			continue;
		}
        
		i.second.push_back(msg);

		if(i.second.size() == global::ca::kConsensus)
		{

            //Combined into BlockMsg
			composeEndBlockmsg(i.second);	
		}
	}
	lck.unlock();

	return 0;
}

bool BlockStroage::isBlock(const std::string& blockHash)
{
    std::shared_lock<std::shared_mutex> lck(_block_mutex_);
    auto result = _BlockMap.find(blockHash);
    if(result != _BlockMap.end())
    {
        return false;
    }
    return true;
    
}

void BlockStroage::BlockCheck()
{
    std::unique_lock<std::shared_mutex> lck(_block_mutex_);

    int64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    const int64_t kTenSecond = (int64_t)1000000 * 10; // TODO::10s

	std::vector<std::string> hashKey;
	for(auto &i : _BlockMap)
	{
        BlockMsg copyendmsg_ = i.second.at(0); 

        CBlock block;
        block.ParseFromString(copyendmsg_.block());
        
        // if(block.time() == 0 )
        // {
        //     INFOLOG("block.time == 0,time = {}",block.time());
        //     continue;
        // }

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
            std::pair<std::string, std::vector<std::string>> nodes_pair;
            
            MagicSingleton<VRF>::GetInstance()->getVerifyNodes(block.hash(), nodes_pair);

            //Block signature node in cache
            std::vector<std::string> cache_nodes = nodes_pair.second;

            //The signature node in the block flow
            std::vector<std::string> verify_nodes;
            for(auto &item : block.sign())
            {
                verify_nodes.push_back(GetBase58Addr(item.pub()));
                
            }

            //Compare whether the nodes in the two containers are consistent
            for(auto & sign_node : verify_nodes)
            {
                if(std::find(cache_nodes.begin(), cache_nodes.end(), sign_node) == cache_nodes.end())
                {
                    ERRORLOG(" The nodes in the two containers are inconsistent = {}, blockHash:{}",sign_node, block.hash());
                    hashKey.push_back(block.hash());
                    continue;
                }
            }

            //After the verification is passed, the broadcast block is directly built
            if(block.version() >=0)
            {
                if(blockStatusMap.find(block.hash()) == blockStatusMap.end())
                {
                    blockStatusMap[block.hash()] = {block.hash(), block};
                }
                auto NowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
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
			Remove(hash);
		}
	}
	hashKey.clear();    
}

void BlockStroage::composeEndBlockmsg(std::vector<BlockMsg> &msgvec)
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

            msgvec[0].set_block(endBlock.SerializeAsString());

        }    
    }        
        
	
}


void BlockStroage::Remove(const std::string &hash)
{
	for(auto iter = _BlockMap.begin(); iter != _BlockMap.end();)
	{
		if (iter->first == hash)
		{
			iter = _BlockMap.erase(iter);
			DEBUGLOG("BlockStroage::Remove hash:{}", hash);
		}
		else
		{
			iter++;
		}
	}
}


std::shared_future<RetType> BlockStroage::GetPrehash(const uint64_t height)
{
    std::shared_lock<std::shared_mutex> lck(prehash_mutex);
    auto result = PreHashMap.find(height);
    if(result != PreHashMap.end())
    {
       return result->second;
    }
    return {};
}

bool BlockStroage::is_seek_task(uint64_t seek_height)
{
    std::shared_lock<std::shared_mutex> lck(prehash_mutex);
    if(PreHashMap.find(seek_height) != PreHashMap.end())
    {
        DEBUGLOG("seek_prehash_task repeat");
        return true;
    }
    return false;
}


void BlockStroage::commit_seek_task(uint64_t seek_height)
{
    if(is_seek_task(seek_height))
    {
        return;
    }
    std::unique_lock<std::shared_mutex> lck(prehash_mutex);
    if(PreHashMap.size() > 10000)
    {
        auto endHeight = PreHashMap.end()->first;
        std::map<uint64_t, std::shared_future<RetType>> PreHashTemp(PreHashMap.find(endHeight - 100), PreHashMap.end());
        PreHashMap.clear();
        PreHashMap.swap(PreHashTemp);
    }
    DEBUGLOG("commit_seek_task, height:{}", seek_height);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStroage::SeekPreHashThread, this, seek_height));
    try
    {
        /* code */
        PreHashMap[seek_height] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    MagicSingleton<taskPool>::GetInstance()->commit_syncBlock_task([task](){(*task)();});
    return;
}

void BlockStroage::Force_commit_seek_task(uint64_t seek_height)
{
    std::unique_lock<std::shared_mutex> lck(prehash_mutex);
    DEBUGLOG("Force_commit_seek_task, height:{}", seek_height);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStroage::SeekPreHashThread, this, seek_height));
    try
    {
        /* code */
        PreHashMap[seek_height] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    MagicSingleton<taskPool>::GetInstance()->commit_syncBlock_task([task](){(*task)();});
    return;
}

int GetPrehashFindNode(uint32_t num, uint64_t self_node_height, const std::vector<std::string> &pledge_addr,
                            std::vector<std::string> &send_node_ids)
{
    int ret = 0;
    if ((ret = SyncBlock::GetSyncNodeSimplify(num, self_node_height, pledge_addr,
                                                                             send_node_ids)) != 0)
    {
        ERRORLOG("get seek node fail, ret:{}", ret);
        return -1;
    }
    return 0;
}
RetType BlockStroage::SeekPreHashThread(uint64_t seek_height)
{
    DEBUGLOG("SeekPreHashThread Start");
    uint64_t chain_height = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->obtain_chain_height(chain_height))
    {
        DEBUGLOG("obtain_chain_height fail!!!");
        return {"",0};
    }
    uint64_t self_node_height = 0;
    std::vector<std::string> pledge_addr; // stake and invested addr
    {
        DBReader db_reader;
        auto status = db_reader.GetBlockTop(self_node_height);
        if (DBStatus::DB_SUCCESS != status)
        {
            DEBUGLOG("GetBlockTop fail!!!");
            return {"",0};
        }
        std::vector<std::string> stake_addr;
        status = db_reader.GetStakeAddress(stake_addr);
        if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
        {
            DEBUGLOG("GetStakeAddress fail!!!");
            return {"",0};
        }

        for(const auto& addr : stake_addr)
        {
            if(VerifyBonusAddr(addr) != 0)
            {
                DEBUGLOG("{} doesn't get invested, skip", addr);
                continue;
            }
            pledge_addr.push_back(addr);
        }
    }
    std::vector<std::string> send_node_ids;
    if (GetPrehashFindNode(10, chain_height, pledge_addr, send_node_ids) != 0)
    {
        ERRORLOG("get sync node fail");
        return {"",0};
    }
    if(seek_height == 0 || seek_height > self_node_height)
    {
        DEBUGLOG("seek_height:{}, self_node_height:{}", seek_height, self_node_height);
    }
    return SeekPreHashByNode(send_node_ids, seek_height, self_node_height, chain_height);
}

RetType BlockStroage::SeekPreHashByNode(
		const std::vector<std::string> &send_node_ids, uint64_t seek_height, const uint64_t &self_node_height, const uint64_t &chain_height)
{
    std::string msg_id;
    uint64_t succent_count = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(10, send_node_ids.size() * 0.8, msg_id))
    {
        ERRORLOG("CreateWait fail!!!");
        return {"", 0};
    }
    for (auto &node_id : send_node_ids)
    {
        DEBUGLOG("new seek get block hash from {}", node_id);
        SendSeekGetPreHashReq(node_id, msg_id, seek_height);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(ret_datas.size() < send_node_ids.size() * 0.5)
        {
            ERRORLOG("wait seek block hash time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
            return {"", 0};
        }
    }

    std::map<std::string, bool> NodeAddrs;
    MagicSingleton<PeerNode>::GetInstance()->get_nodelist(NodeAddrs);
    
    SeekPreHashByHightAck ack;
    std::map<uint64_t, std::map<std::string, uint64_t>> seek_pre_hashes;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        succent_count++;
        uint64_t seek_height = ack.seek_height();
        for(auto& prehash : ack.prehashes())
        {
            if(seek_pre_hashes[seek_height].find(prehash) == seek_pre_hashes[seek_height].end())
            {
                seek_pre_hashes[seek_height][prehash] = 1;
            }
            else
            {
                seek_pre_hashes[seek_height][prehash]++;
            }
        }
            
        
    }

    std::set<std::string> verify_hashes;
    size_t verify_num = succent_count / 5 * 3;

    for (auto &iter : seek_pre_hashes)
    {
        uint16_t max_percentage = 0;
        std::string max_percentage_prehash;
        for(auto &prehash : iter.second)
        {
            if (prehash.second >= verify_num)
            {
                uint16_t percentage = prehash.second / succent_count * 100;
                if(max_percentage < percentage)
                {
                    max_percentage = percentage;
                    max_percentage_prehash = prehash.first;
                }
            }
        }
        if(max_percentage >= 80 || max_percentage >= 70)
        {
            DEBUGLOG("SeekPreHashByNode <success> !!! ,seek_height:{}, max_percentage:{} > 70% , max_percentage_prehash:{}", iter.first, max_percentage, max_percentage_prehash);
            return {max_percentage_prehash, max_percentage};
        }
        else
        {
            DEBUGLOG("SeekPreHashByNode <fail> !!! ,seek_height:{}, max_percentage:{} < 70% , max_percentage_prehash:{}", iter.first, max_percentage, max_percentage_prehash);
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
    auto found = blockStatusMap.find(hash);
    if(found == blockStatusMap.end())
    {
        return -2;
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::VerifyBroadcast)
    {
        if(found->second.VerifyNodes.empty() || 
        found->second.VerifyNodes.find(destNode) == found->second.VerifyNodes.end())
        {
            return -3;
        }
        found->second.VerifyNodes.erase(destNode);
        if(found->second.BlockStatusList.size() > found->second.VerifyNodesNumber)
        {
            return -4;
        }
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::level1Broadcast)
    {
        if(found->second.level1Nodes.empty() || 
        found->second.level1Nodes.find(destNode) == found->second.level1Nodes.end())
        {
            return -5;
        }
        found->second.level1Nodes.erase(destNode);
        if(blockStatusMap[hash].BlockStatusList.size() > found->second.level1NodesNumber * FailureRate)
        {
            return -6;
        }
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    if(nowTime > blockStatusMap[hash].block.time() + 5 * 1000000ull)
    {
        DEBUGLOG("AAAC,blockStatus nowTime:{},blockTime:{}, timeout:{}",nowTime, (blockStatusMap[hash].block.time() + 5 * 1000000ull), (nowTime - (blockStatusMap[hash].block.time() + 5 * 1000000ull))/1000000);
        return -7;
    }
    return 0;
}

void BlockStroage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes, const Vrf& vrf)
{
    std::unique_lock<std::mutex> lck(status_mutex_);
    if(blockStatusMap.find(blockHash) == blockStatusMap.end())
    {
        BlockStatusWrapper blockStatusWrapper;
        blockStatusWrapper.blockHash = blockHash;
        blockStatusWrapper.block = Block;
        blockStatusWrapper.broadcastType = BroadcastType::VerifyBroadcast;
        blockStatusWrapper.VerifyNodesNumber = global::ca::KSign_node_threshold - global::ca::kConsensus + 2;
        blockStatusWrapper.VerifyNodes.insert(verifyNodes.begin(), verifyNodes.end());
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
        blockStatusMap[blockHash] = blockStatusWrapper;
    }
}

void BlockStroage::ExpiredDeleteCheck()
{
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    std::unique_lock<std::mutex> lck(status_mutex_);
    for(auto iter = blockStatusMap.begin(); iter != blockStatusMap.end();)
    {
        if(nowTime >= iter->second.block.time() + 15ull * 1000000)
        {
            DEBUGLOG("blockStatusMap deleteBlockHash:{}", iter->second.block.hash().substr(0,6));
            blockStatusMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

void BlockStroage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes)
{
    std::unique_lock<std::mutex> lck(status_mutex_);
    auto found = blockStatusMap.find(blockHash);
    if(found != blockStatusMap.end())
    {
        found->second.block = Block;
        found->second.broadcastType = BroadcastType::level1Broadcast;
        found->second.level1NodesNumber = level1Nodes.size();
        found->second.BlockStatusList.clear();
        found->second.level1Nodes.insert(level1Nodes.begin(), level1Nodes.end());
    }
}

void BlockStroage::AddBlockStatus(const BlockStatus& blockStatus)
{
    std::unique_lock<std::mutex> lck(status_mutex_);
    auto& hash = blockStatus.blockhash();
    auto ret = CheckData(blockStatus);
    if(ret != 0)
    {
        DEBUGLOG("AAAC Valid data, ret:{}, blockHash:{}, destNode:{}", ret, hash.substr(0,6), blockStatus.id());
        return;
    }

    blockStatusMap[hash].BlockStatusList.push_back(blockStatus);
    DEBUGLOG("AAAC destNode:{}, blockhash:{}, blockStatus:{}, level_1_broadcast_nodes:{}, broadcastType:{}",blockStatus.id(), hash.substr(0,6), blockStatus.status(), blockStatusMap[hash].level1Nodes.size(), blockStatusMap[hash].broadcastType);

    if(blockStatusMap[hash].broadcastType == BroadcastType::level1Broadcast && blockStatusMap[hash].BlockStatusList.size() == (uint32_t)(blockStatusMap[hash].level1NodesNumber * FailureRate))
    {
        MagicSingleton<taskPool>::GetInstance()->commit_block_task(std::bind(&BlockStroage::newbuildBlockByBlockStatus,this,hash));
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::VerifyBroadcast && blockStatusMap[hash].BlockStatusList.size() == blockStatusMap[hash].VerifyNodesNumber)
    {
        MagicSingleton<taskPool>::GetInstance()->commit_block_task(std::bind(&BlockStroage::newbuildBlockByBlockStatus,this,hash));
    }
}


void BlockStroage::newbuildBlockByBlockStatus(const std::string blockHash)
{
    std::unique_lock<std::mutex> lck(status_mutex_);
    CBlock& oldBlock = blockStatusMap[blockHash].block;
    CBlock newBlock = oldBlock;
    uint32_t FailThreshold = 0;

    if(blockStatusMap[blockHash].broadcastType == BroadcastType::VerifyBroadcast)
    {
        FailThreshold = blockStatusMap[blockHash].VerifyNodesNumber * FailureRate;
    }
    else if(blockStatusMap[blockHash].broadcastType == BroadcastType::level1Broadcast)
    {
        FailThreshold = blockStatusMap[blockHash].level1NodesNumber * FailureRate;
    }

    DEBUGLOG("AAAC oldBlockHash:{}, broadcastType:{}, FailThreshold:{}", blockHash.substr(0,6), blockStatusMap[blockHash].broadcastType, FailThreshold);

    newBlock.clear_txs();
    newBlock.clear_sign();
    newBlock.clear_hash();

    std::map<std::string, uint32> TxsStatus;
    //std::map<int32, uint32> errorStatus;
    std::multimap<std::string, int> testMap;
    
    for(auto& iter : blockStatusMap[blockHash].BlockStatusList)
    {
        for(auto& tx : iter.txstatus())
        {
            if(TxsStatus.find(tx.txhash()) == TxsStatus.end())
            {
                TxsStatus[tx.txhash()] = 1;
            }
            else
            {
                TxsStatus[tx.txhash()]++;
            }
            testMap.insert({tx.txhash(), tx.status()});
        }
        //errorStatus[iter.status()]++;
    }

    for(auto &it : testMap)
    {
        DEBUGLOG("AAAC txstatus , txHash:{}, err:{}",it.first, it.second);
    }

    // for(auto& iter : errorStatus)
    // {
    //     if(iter.first < 0 && iter.second >= FailThreshold)
    //     {
    //         DEBUGLOG("AAAC block fail, ret:{}", iter.first);
    //         break;
    //     }
    // }

    BlockMsg blockmsg;
    for(auto &tx : oldBlock.txs())
    {
        if(GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }

        if(TxsStatus.find(tx.hash()) != TxsStatus.end() && TxsStatus[tx.hash()] >= FailThreshold)
        {
            DEBUGLOG("AAAC delete tx txHash:{} ,poll:{}", tx.hash(), TxsStatus[tx.hash()]);
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
        auto foundVrf = blockStatusMap[blockHash].vrfMap.find(tx.hash());
        if(foundVrf != blockStatusMap[blockHash].vrfMap.end())
        {
            blockmsg.add_vrfinfo()->CopyFrom(foundVrf->second);
            
        } 
        else
        {
            continue;
        }
        auto foundTxVrf = blockStatusMap[blockHash].txvrfMap.find(tx.hash());
        if(foundTxVrf != blockStatusMap[blockHash].txvrfMap.end())
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

    initNewBlock(oldBlock, newBlock);

    std::ostringstream filestream;
    printBlock(newBlock, true, filestream);

    std::string test_str = filestream.str();
    DEBUGLOG("AAAC newBuildBlock --> {}", test_str);

    blockmsg.set_version(global::kVersion);
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
    blockmsg.set_block(newBlock.SerializeAsString());

    auto msg = make_shared<BlockMsg>(blockmsg);
	auto ret = DoHandleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("AAAC DoHandleBlock failed The error code is {}",ret);
        return;
    }

    DEBUGLOG("AAAC newBuildBlock success oldBlockHash:{}, newBlockHash:{}",oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
    return;
}

int BlockStroage::initNewBlock(const CBlock& oldBlock, CBlock& newBlock)
{
	newBlock.set_version(0);
	newBlock.set_time(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
	newBlock.set_height(oldBlock.height());
    newBlock.set_prevhash(oldBlock.prevhash());
	newBlock.set_merkleroot(ca_algorithm::CalcBlockMerkle(newBlock));
	newBlock.set_hash(getsha256hash(newBlock.SerializeAsString()));
	return 0;
}

void SendSeekGetPreHashReq(const std::string &node_id, const std::string &msg_id, uint64_t seek_height)
{
    SeekPreHashByHightReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    req.set_seek_height(seek_height);
    net_send_message<SeekPreHashByHightReq>(node_id, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return;
}

void SendSeekGetPreHashAck(SeekPreHashByHightAck& ack,const std::string &node_id, const std::string &msg_id, uint64_t seek_height)
{
    DEBUGLOG("SendSeekGetPreHashAck, id:{}, height:{}",  node_id, seek_height);
    ack.set_self_node_id(net_get_self_node_id());
    DBReader db_reader;
    uint64_t self_node_height = 0;
    if (0 != db_reader.GetBlockTop(self_node_height))
    {
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msg_id);
    std::vector<std::string> block_hashes;
    if(seek_height > self_node_height)
    {
        return;
    }

    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(seek_height, block_hashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail !!!");
        return;
    }
    ack.set_seek_height(seek_height);
    for(auto &hash : block_hashes)
    {
        ack.add_prehashes(hash);
    }
    
    return;
}

int HandleSeekGetPreHashReq(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgdata)
{
    SeekPreHashByHightAck ack;
    SendSeekGetPreHashAck(ack,msg->self_node_id(), msg->msg_id(), msg->seek_height());
    net_send_message<SeekPreHashByHightAck>(msg->self_node_id(), ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}
int HandleSeekGetPreHashAck(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int doProtoBlockStatus(const BlockStatus& blockStatus, const std::string destNode)
{
    net_send_message<BlockStatus>(destNode, blockStatus, net_com::Priority::kPriority_High_1);
    return 0;
}

int HandleBlockStatusMsg(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgdata)
{
    MagicSingleton<BlockStroage>::GetInstance()->AddBlockStatus(*msg);
    return 0;
}



