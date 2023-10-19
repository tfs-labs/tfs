#include <unordered_map>
#include <future>

#include "transaction_cache.h"
#include "transaction.h"
#include "utils/json.hpp"
#include "db/db_api.h"

#include "ca/txhelper.h"
#include "utils/magic_singleton.h"
#include "algorithm.h"
#include "../utils/time_util.h"
#include "block_helper.h"
#include "utils/account_manager.h"
#include "checker.h"
#include "utils/tfs_bench_mark.h"
#include "common/time_report.h"
#include "utils/console.h"
#include "utils/tmp_log.h"


const int TransactionCache::_kBuildInterval = 3 * 1000;
const time_t TransactionCache::_kTxExpireInterval  = 10;
const int TransactionCache::_kBuildThreshold = 1000000;


int CreateBlock(std::vector<TransactionEntity>& txs, CBlock& cblock)
{
	cblock.Clear();

	// Fill version
	cblock.set_version(0);

	// Fill time
	uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	cblock.set_time(time);
    DEBUGLOG("block set time ======");

	// Fill height
	uint64_t prevBlockHeight = txs.front().GetTxmsg().txmsginfo().height();
	uint64_t cblockHeight = prevBlockHeight + 1;
	cblock.set_height(cblockHeight);

	// Fill tx
	for(auto& tx : txs)
	{
		// Add major transaction
		CTransaction * majorTx = cblock.add_txs();
		*majorTx = tx.GetTransaction();
		
		
		auto txHash = majorTx->hash();
	}

    // Fill preblockhash
    uint64_t seekPrehashTime = 0;
    std::future_status status;
    auto futurePrehash = MagicSingleton<BlockStroage>::GetInstance()->GetPrehash(prevBlockHeight);
    if(!futurePrehash.valid())
    {
        ERRORLOG("futurePrehash invalid,hight:{}",prevBlockHeight);
        return -3;
    }
    status = futurePrehash.wait_for(std::chrono::seconds(6));
    if (status == std::future_status::timeout) 
    {
        ERRORLOG("seek prehash timeout, hight:{}",prevBlockHeight);
        return -4;
    }
    else if(status == std::future_status::ready) 
    {
        std::string preBlockHash = futurePrehash.get().first;
        if(preBlockHash.empty())
        {
            ERRORLOG("seek prehash <fail>!!!,hight:{},prehash:{}",prevBlockHeight, preBlockHash);
            return -5;
        }
        DEBUGLOG("seek prehash <success>!!!,hight:{},prehash:{},blockHeight:{}",prevBlockHeight, preBlockHash,cblockHeight);
        seekPrehashTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        cblock.set_prevhash(preBlockHash);
    }
    
	// Fill merkleroot
	cblock.set_merkleroot(ca_algorithm::CalcBlockMerkle(cblock));
	// Fill hash
	cblock.set_hash(Getsha256hash(cblock.SerializeAsString()));
    DEBUGLOG("block hash = {} set time ",cblock.hash());
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
    auto S = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	int ret = CreateBlock(txs,cblock);
    if(ret != 0)
    {
        if(ret == -3 || ret == -4 || ret == -5)
        {
            MagicSingleton<BlockStroage>::GetInstance()->ForceCommitSeekTask(cblock.height() - 1);
        }
        ERRORLOG("Create block failed!");
		return ret - 100;
    }
	auto S1 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	std::string serBlock = cblock.SerializeAsString();

	ca_algorithm::PrintBlock(cblock);
    auto startT4 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    auto endT4 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    auto blockTime = cblock.time();
    auto t4 = endT4 - startT4;
    auto txSize = txs.size();
    auto BlockHight = cblock.height();
    MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(cblock.hash(), &blockTime, 1 , &t4, &txSize, &BlockHight);

    BlockMsg blockmsg;
    blockmsg.set_version(global::kVersion);
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
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
        std::string txHash = Getsha256hash(copyTx.SerializeAsString());
        MagicSingleton<TFSbenchmark>::GetInstance()->SetTxHashByBlockHash(cblock.hash(), txHash);

        uint64_t handleTxHeight =  cblock.height() - 1;
        TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
        if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
        {
            continue;
        }

        std::pair<std::string,Vrf>  vrf;
        if(!MagicSingleton<VRF>::GetInstance()->getVrfInfo(txHash, vrf))
        {
            ERRORLOG("getVrfInfo failed!");
            return -3000;
        }
        Vrf *vrfinfo  = blockmsg.add_vrfinfo();
        vrfinfo ->CopyFrom(vrf.second);

        if(!MagicSingleton<VRF>::GetInstance()->getTxVrfInfo(txHash, vrf))
        {
            ERRORLOG("getTxVrfInfo failed!");
            return -4000;
        }

        auto vrfJson = nlohmann::json::parse(vrf.second.data());
		vrfJson["txhash"] = txHash;
        vrf.second.set_data(vrfJson.dump());

        blockmsg.add_txvrfinfo()->CopyFrom(vrf.second);
    }
    
    auto msg = make_shared<BlockMsg>(blockmsg);
	ret = DoHandleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("DoHandleBlock failed The error code is {}",ret);
        CBlock cblock;
	    if (!cblock.ParseFromString(msg->block()))
	    {
		    ERRORLOG("fail to serialization!!");
		    return -3090;
	    }
        ClearVRF(cblock);
        return ret -4000;
    }

	return 0;
}

TransactionCache::TransactionCache()
{
    _buildTimer.AsyncLoop(
        _kBuildInterval, 
        [=](){ _blockBuilder.notify_one(); }
        );
}

int TransactionCache::AddCache(const CTransaction& transaction, const TxMsgReq& sendTxMsg)
{
    std::unique_lock<mutex> locker(_cacheMutex);
    if(CheckConflict(transaction))
    {
        DEBUGLOG("DoubleSpentTransactions, txHash:{}", transaction.hash());
        return -1;
    }
    uint64_t height = sendTxMsg.txmsginfo().height() + 1;

    auto find = _cache.find(height); 
    if(find == _cache.end()) 
    {
        _cache[height] = std::list<TransactionEntity>{}; 
    }

    time_t add_time = time(NULL);
    _cache.at(height).push_back(TransactionEntity(transaction, sendTxMsg, add_time)) ;
    for(auto txEntity: _cache)
    {
        if (txEntity.second.size() >= _kBuildThreshold)
        {
            _blockBuilder.notify_one();
        }
    }
    return 0;
}

bool TransactionCache::Process()
{
    _buildThread = std::thread(std::bind(&TransactionCache::_ProcessingFunc, this));
    _buildThread.detach();
    return true;
}

bool TransactionCache::CheckConflict(const CTransaction& transaction)
{
    return Checker::CheckConflict(transaction, _cache);
}
void TransactionCache::Stop(){
    _threadRun=false;
}

void TransactionCache::_ProcessingFunc()
{
    while (_threadRun)
    {
        std::unique_lock<mutex> locker(_cacheMutex);
        auto SPending = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        _blockBuilder.wait(locker);
        auto EPending = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        std::vector<cacheIter> emptyHeightCache;
        for(auto cacheEntity = _cache.begin(); cacheEntity != _cache.end(); ++cacheEntity)
        {
            if(cacheEntity == _cache.end())
            {
                break;
            }
            
            MagicSingleton<TFSbenchmark>::GetInstance()->SetBlockPendingTime(EPending);

            std::list<txEntitiesIter> buildTxs = _GetNeededCache(cacheEntity->second);
            std::vector<TransactionEntity> buildCaches;
            for(auto iter : buildTxs)
            {
                buildCaches.push_back(*iter);
            }
            auto ret = BuildBlock(buildCaches, false);
            if(ret != 0)
            {
                ERRORLOG("{} build block fail", ret);
                if(ret == -103 || ret == -104 || ret == -105 || ret == -2)
                { 
                    continue;
                }
                _TearDown(buildTxs, false, emptyHeightCache, cacheEntity);
                continue;
            }
            _TearDown(buildTxs, true, emptyHeightCache, cacheEntity);
        }
        for(auto cache: emptyHeightCache)
        {
            _cache.erase(cache);
        }
        locker.unlock();
        
    }
    
}

std::list<TransactionCache::txEntitiesIter> TransactionCache::_GetNeededCache(const std::list<TransactionEntity>& txs)
{
    std::list<txEntitiesIter> buildCaches;

    if(txs.empty())
    {
        return buildCaches;
    }

    txEntitiesIter iter = txs.begin();
    txEntitiesIter end = txs.end();

    for(int i = 0; i < _kBuildThreshold && iter != end; ++i, ++iter) 
    {
        buildCaches.push_back(iter);
    }        


    return buildCaches;
}

bool TransactionCache::_RemoveProcessedTransaction(const  std::list<txEntitiesIter>& txEntitiesIter, const bool buildSuccess, std::list<TransactionEntity>& txEntities)
{

    for(auto iter : txEntitiesIter)
    {
        std::string hash = iter->GetTransaction().hash();
        txEntities.erase(iter);
        std::string message;
        if(buildSuccess)
        {
            message = " successfully packaged";
        }
        else
        {
            message = " packaging fail";
        }
        std::cout << "transaction " << hash << message << std::endl;
    }
    

    for(auto txEntity = txEntities.begin(); txEntity != txEntities.end(); ++txEntity)
    {
        time_t current_time = time(NULL);
        if((current_time - txEntity->GetTimestamp()) > _kTxExpireInterval)
        {
            TRACELOG("transaction {} has expired", txEntity->GetTransaction().hash());
            std::cout << "transaction expired: " << txEntity->GetTransaction().hash() << std::endl;
        }
    }

    if(txEntities.empty())
    {
        return false;
    }            
    return true;
}

void TransactionCache::GetCache(std::map<uint64_t, std::list<TransactionEntity>>& cache)
{
    cache = _cache;
}

void TransactionCache::_TearDown(const  std::list<txEntitiesIter>& txEntitiesIters, const bool buildSuccess, std::vector<cacheIter>& emptyHeightCache , cacheIter cacheEntity)
{
    if(!_RemoveProcessedTransaction(txEntitiesIters, buildSuccess, cacheEntity->second))
    {
        emptyHeightCache.push_back(cacheEntity);         
    }
}
