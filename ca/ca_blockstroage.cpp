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

void BlockStroage::StartTimer()
{
	_block_timer.AsyncLoop(100, [this](){
		BlockCheck();
	});

}


int BlockStroage::AddBlock(const BlockMsg &msg)
{
	std::unique_lock<std::mutex> lck(_block_mutex_);

    CBlock block;
    block.ParseFromString(msg.block());

	std::vector<BlockMsg> msgVec;
	msgVec.push_back(msg); // The self add does not have its own signature on the block at this time
    //Self-add does not have its own signature on the block at this time
	_BlockMap.insert(std::pair<uint64_t,std::vector<BlockMsg>>(block.time(),msgVec));

	DEBUGLOG("add TranStroage");
	lck.unlock();

    return 0;
}

int BlockStroage::UpdateBlock(const BlockMsg &msg)
{
    std::unique_lock<std::mutex> lck(_block_mutex_);

    CBlock block;
    block.ParseFromString(msg.block());

    if(block.sign_size() != 2)
    {
		ERRORLOG("sign  size != 2");
        return -1;
    }


	for(auto &i : _BlockMap)
	{
		
		if(block.time() != i.first || i.second.size() == global::ca::kConsensus)
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


void BlockStroage::BlockCheck()
{
    std::unique_lock<std::mutex> lck(_block_mutex_);

	std::vector<uint64_t> timeKey;
	for(auto &i : _BlockMap)
	{
        BlockMsg copyendmsg_ = i.second.at(0); 

        CBlock block;
        block.ParseFromString(copyendmsg_.block());
        
        if(block.time() == 0 )
        {
            INFOLOG("block.time == 0,time = {}",block.time());
            continue;
        }

        if(block.time() != i.first)
        {
            timeKey.push_back(block.time()); 
            continue;
        }
        
        int64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        const int64_t kTenSecond = (int64_t)1000000 * 10; // TODO::10s

        DEBUGLOG("block.sign_size() =  {}",block.sign_size());
        if( abs(nowTime - (int64_t)block.time()) >= kTenSecond)
        {
            ERRORLOG("Add to failure list");
            timeKey.push_back(block.time());
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
                    ERRORLOG(" The nodes in the two containers are inconsistent = {}",sign_node);
                    timeKey.push_back(block.time());
                    continue;
                }
            }

            //After the verification is passed, the broadcast block is directly built
            if(block.version() >=0)
            {
                auto NowTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
			    MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(block.hash(), &NowTime, 2);
                MagicSingleton<BlockMonitor>::GetInstance()->SendBroadcastAddBlock(copyendmsg_.block(),block.height());
                INFOLOG("Start to broadcast BuildBlockBroadcastMsg...");
            }
            else
            {
                std::cout << "The version is too low. Please update the version!" << std::endl;
            }
            
            timeKey.push_back(block.time());
            copyendmsg_.Clear();
        }

	}
	if(!timeKey.empty())
	{
		for (auto &time : timeKey)
		{
			Remove(time);
		}
	}
	timeKey.clear();    
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


void BlockStroage::Remove(const uint64_t &time)
{
	for(auto iter = _BlockMap.begin(); iter != _BlockMap.end();)
	{
		if (iter->first == time)
		{
			iter = _BlockMap.erase(iter);
			DEBUGLOG("BlockStroage::Remove");
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
    if ((ret = MagicSingleton<SyncBlock>::GetInstance()->GetFastSyncNode(num, self_node_height, pledge_addr, send_node_ids)) != 0)
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


