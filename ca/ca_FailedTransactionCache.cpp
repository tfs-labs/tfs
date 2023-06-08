#include "ca/ca_FailedTransactionCache.h" 
#include "db/db_api.h"
#include "ca_transaction.h"
#include "utils/AccountManager.h"
#include "utils/TFSbenchmark.h"
#include "common/time_report.h"
#include "utils/tmplog.h"
#include "common/task_pool.h"

void FailedTransactionCache::Start_Timer()
{
	//Notifications for inspections at regular intervals
	_timer.AsyncLoop(2 * 1000, [this](){
        if(!is_empty())
        {
            Check();
        }
	});
}

bool FailedTransactionCache::is_empty()
{
    std::shared_lock<std::shared_mutex> lock(txPending_mutex);
    if(txPending.empty())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void FailedTransactionCache::Check()
{
    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
    }
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    std::map<uint64_t, uint64_t> heightMap;
    for(auto &node : nodelist)
    {
        heightMap[node.height]++;
    }
    auto begin = txPending.begin();
    std::vector<decltype(begin)> delete_txPending;
    std::unique_lock<std::shared_mutex> lock(txPending_mutex);
    for(auto iter = begin; iter != txPending.end(); iter++)
    {
        auto txHeight = iter->first;
        if(heightMap.find(txHeight) != heightMap.end() && heightMap[txHeight] >= global::ca::kConsensus * 2 && top >= txHeight)
        {
            for(auto& txmsg : iter->second)
            {
                MagicSingleton<taskPool>::GetInstance()->commit_tx_task(
                    [=](){
                        CTransaction tx;
                        int ret = DoHandleTx(std::make_shared<TxMsgReq>(txmsg), tx);

                        tx.clear_hash();
                        tx.clear_verifysign();
                        tx.set_hash(getsha256hash(tx.SerializeAsString()));
                        if (ret != 0)
                        {
                            DEBUGLOG("TTT tx verify <fail!!!> txhash:{}, ret:{}", tx.hash(), ret);
                            return;
                        }

                        DEBUGLOG("TTT tx verify <success> txhash:{}", tx.hash());
                    });
            }
            delete_txPending.push_back(iter);
        }
    }
    for(auto& iter : delete_txPending)
    {
        txPending.erase(iter);
    }
}
	
int FailedTransactionCache::Add(uint64_t height, const TxMsgReq& msg)
{
    std::unique_lock<std::shared_mutex> lock(txPending_mutex);
    txPending[height].push_back(msg);
	return 0;
}
