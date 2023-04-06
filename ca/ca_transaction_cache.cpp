#include <unordered_map>
#include <future>

#include "ca_transaction_cache.h"
#include "ca_transaction.h"
#include "utils/json.hpp"
#include "db/db_api.h"

#include "ca/ca_txhelper.h"
#include "utils/MagicSingleton.h"
#include "ca_algorithm.h"
#include "../utils/time_util.h"
#include "ca_blockhelper.h"
#include "utils/AccountManager.h"
#include "ca_checker.h"
#include "utils/TFSbenchmark.h"
#include "common/time_report.h"
#include "utils/console.h"
#include "utils/tmplog.h"


const int CtransactionCache::build_interval_ = 3 * 1000;
const time_t CtransactionCache::tx_expire_interval_  = 10;
const int CtransactionCache::build_threshold_ = 1000000;


int CreateBlock(std::vector<TransactionEntity>& txs, CBlock& cblock)
{
	cblock.Clear();

	// Fill version
	cblock.set_version(0);

	// Fill time
	uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
	cblock.set_time(time);

	// Fill height
	uint64_t prevBlockHeight = txs.front().get_txmsg().txmsginfo().height();
	uint64_t cblockHeight = prevBlockHeight + 1;
	cblock.set_height(cblockHeight);

	// Fill tx
	for(auto& tx : txs)
	{
		// Add major transaction
		CTransaction * major_tx = cblock.add_txs();
		*major_tx = tx.get_transaction();
		
		
		auto tx_hash = major_tx->hash();
	}

    // Fill preblockhash
    uint64_t seekPrehashTime = 0;
    std::future_status status;
    auto future_Prehash = MagicSingleton<BlockStroage>::GetInstance()->GetPrehash(prevBlockHeight);
    if(!future_Prehash.valid())
    {
        ERRORLOG("future_Prehash invalid,hight:{}",prevBlockHeight);
        return -3;
    }
    status = future_Prehash.wait_for(std::chrono::seconds(6));
    if (status == std::future_status::timeout) 
    {
        ERRORLOG("seek prehash timeout, hight:{}",prevBlockHeight);
        return -4;
    }
    else if(status == std::future_status::ready) 
    {
        std::string pre_block_hash = future_Prehash.get().first;
        if(pre_block_hash.empty())
        {
            ERRORLOG("seek prehash <fail>!!!,hight:{},prehash:{}",prevBlockHeight, pre_block_hash);
            return -5;
        }
        DEBUGLOG("seek prehash <success>!!!,hight:{},prehash:{},blockHeight:{}",prevBlockHeight, pre_block_hash,cblockHeight);
        seekPrehashTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        cblock.set_prevhash(pre_block_hash);
    }
    
	// Fill merkleroot
	cblock.set_merkleroot(ca_algorithm::CalcBlockMerkle(cblock));
	// Fill hash
	cblock.set_hash(getsha256hash(cblock.SerializeAsString()));
    MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(cblock.hash(), &seekPrehashTime, 8);
    MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockContainsTransactionAmountMap(cblock.hash(), txs.size());
	return 0;
}

int BuildBlock(std::vector<TransactionEntity>& txs, bool build_first)
{
	if(txs.empty())
	{
		ERRORLOG("Txs is empty!");
		return -1;
	}

	CBlock cblock;
    auto S = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
	int ret = CreateBlock(txs,cblock);
    if(ret != 0)
    {
        if(ret == -3 || ret == -4 || ret == -5)
        {
            MagicSingleton<BlockStroage>::GetInstance()->Force_commit_seek_task(cblock.height() - 1);
        }
        ERRORLOG("Create block failed!");
		return ret - 100;
    }
	auto S1 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
	std::string serBlock = cblock.SerializeAsString();

	ca_algorithm::PrintBlock(cblock);
    auto start_t4 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    auto end_t4 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    auto blockTime = cblock.time();
    auto t4 = end_t4 - start_t4;
    auto txSize = txs.size();
    auto BlockHight = cblock.height();
    MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(cblock.hash(), &blockTime, 1 , &t4, &txSize, &BlockHight);

    BlockMsg blockmsg;
    blockmsg.set_version(global::kVersion);
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
    blockmsg.set_block(serBlock);


    for(auto &tx : cblock.txs())
    {
        if(GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }

        CTransaction copyTx = tx;
        copyTx.clear_hash();
        copyTx.clear_verifysign();
        std::string tx_hash = getsha256hash(copyTx.SerializeAsString());
        MagicSingleton<TFSbenchmark>::GetInstance()->SetTxHashByBlockHash(cblock.hash(), tx_hash);

        uint64_t handleTxHeight =  cblock.height() - 1;
        TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
        if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
        {
            continue;
        }

        std::pair<std::string,Vrf>  vrf;
        if(!MagicSingleton<VRF>::GetInstance()->getVrfInfo(tx_hash, vrf))
        {
            ERRORLOG("getVrfInfo failed!");
            return -3000;
        }
        Vrf *vrfinfo  = blockmsg.add_vrfinfo();
        vrfinfo ->CopyFrom(vrf.second);

    }
    
    auto msg = make_shared<BlockMsg>(blockmsg);
	ret = DoHandleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("DoHandleBlock failed The error code is {}",ret);
        return ret -4000;
    }

	return 0;
}

CtransactionCache::CtransactionCache()
{
    build_timer_.AsyncLoop(
        build_interval_, 
        [=](){ blockbuilder_.notify_one(); }
        );
}

int CtransactionCache::add_cache(const CTransaction& transaction, const TxMsgReq& sendTxMsg)
{
    std::unique_lock<mutex> locker(cache_mutex_);
    uint64_t height = sendTxMsg.txmsginfo().height() + 1;

    auto find = cache_.find(height); 
    if(find == cache_.end()) 
    {
        cache_[height] = std::list<TransactionEntity>{}; 
    }

    time_t add_time = time(NULL);
    cache_.at(height).push_back(TransactionEntity(transaction, sendTxMsg, add_time)) ;
    for(auto tx_entity: cache_)
    {
        if (tx_entity.second.size() >= build_threshold_)
        {
            blockbuilder_.notify_one();
        }
    }
    return 0;
}

bool CtransactionCache::process()
{
    build_thread_ = std::thread(std::bind(&CtransactionCache::processing_func, this));
    build_thread_.detach();
    return true;
}


void CtransactionCache::processing_func()
{
    while (true)
    {
        std::unique_lock<mutex> locker(cache_mutex_);
        auto S_Pending = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        blockbuilder_.wait(locker);
        auto E_Pending = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        std::vector<cache_iter> empty_height_cache;
        for(auto cache_entity = cache_.begin(); cache_entity != cache_.end(); ++cache_entity)
        {
            if(cache_entity == cache_.end())
            {
                break;
            }
            
            MagicSingleton<TFSbenchmark>::GetInstance()->SetBlockPendingTime(E_Pending);

            std::list<tx_entities_iter> build_txs = get_needed_cache(cache_entity->second);
            std::vector<TransactionEntity> build_caches;
            for(auto iter : build_txs)
            {
                build_caches.push_back(*iter);
            }
            auto ret = BuildBlock(build_caches, false);
            if(ret != 0)
            {
                ERRORLOG("{} build block fail", ret);
                if(ret == -103 || ret == -104 || ret == -105 || ret == -2)
                { 
                    continue;
                }
                tear_down(build_txs, false, empty_height_cache, cache_entity);
                continue;
            }
            tear_down(build_txs, true, empty_height_cache, cache_entity);
        }
        for(auto cache: empty_height_cache)
        {
            cache_.erase(cache);
        }
        locker.unlock();
        
    }
    
}

std::list<CtransactionCache::tx_entities_iter> CtransactionCache::get_needed_cache(const std::list<TransactionEntity>& txs)
{
    std::list<tx_entities_iter> build_caches;

    if(txs.empty())
    {
        return build_caches;
    }

    tx_entities_iter iter = txs.begin();
    tx_entities_iter end = txs.end();

    for(int i = 0; i < build_threshold_ && iter != end; ++i, ++iter) 
    {
        build_caches.push_back(iter);
    }        


    return build_caches;
}

bool CtransactionCache::remove_processed_transaction(const  std::list<tx_entities_iter>& tx_entities_iter, const bool build_success, std::list<TransactionEntity>& tx_entities)
{

    for(auto iter : tx_entities_iter)
    {
        std::string hash = iter->get_transaction().hash();
        tx_entities.erase(iter);
        std::string message;
        if(build_success)
        {
            message = " successfully packaged";
        }
        else
        {
            message = " packaging fail";
        }
        std::cout << "transaction " << hash << message << std::endl;
    }
    

    for(auto tx_entity = tx_entities.begin(); tx_entity != tx_entities.end(); ++tx_entity)
    {
        time_t current_time = time(NULL);
        if((current_time - tx_entity->get_timestamp()) > tx_expire_interval_)
        {
            TRACELOG("transaction {} has expired", tx_entity->get_transaction().hash());
            std::cout << "transaction expired: " << tx_entity->get_transaction().hash() << std::endl;
        }
    }

    if(tx_entities.empty())
    {
        return false;
    }            
    return true;
}

void CtransactionCache::get_cache(std::map<uint64_t, std::list<TransactionEntity>>& cache)
{
    cache = cache_;
}

void CtransactionCache::tear_down(const  std::list<tx_entities_iter>& tx_entities_iters, const bool build_success, std::vector<cache_iter>& empty_height_cache , cache_iter cache_entity)
{
    if(!remove_processed_transaction(tx_entities_iters, build_success, cache_entity->second))
    {
        empty_height_cache.push_back(cache_entity);         
    }
}
