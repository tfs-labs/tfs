#include "ca/failed_transaction_cache.h" 
#include "db/db_api.h"
#include "transaction.h"

#include "utils/tmp_log.h"
#include "common/task_pool.h"
#include "include/logging.h"

void FailedTransactionCache::_StartTimer()
{
	//Notifications for inspections at regular intervals
	_timer.AsyncLoop(2 * 1000, [this](){
        if(!_IsEmpty())
        {
            _Check();
        }
	});
}

bool FailedTransactionCache::_IsEmpty()
{
    std::shared_lock<std::shared_mutex> lock(_txPendingMutex);
    if(_txPending.empty())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void FailedTransactionCache::_Check()
{
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
    }
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    auto begin = _txPending.begin();
    std::vector<decltype(begin)> deleteTxPending;
    std::unique_lock<std::shared_mutex> lock(_txPendingMutex);
    for(auto iter = begin; iter != _txPending.end(); iter++)
    {
        auto txHeight = iter->first;
        uint64_t nodeHeight = 0;
        for(auto &node : nodelist)
        {
            if(node.height >= txHeight)
            {
                nodeHeight++;
            }
        }

        if(nodeHeight < global::ca::kConsensus * 2 || top < txHeight)
        {
            if(txHeight > top + 2)
            {
                deleteTxPending.push_back(iter);
            }
            continue;
        }

        for(auto& txmsg : iter->second)
        {
            MagicSingleton<TaskPool>::GetInstance()->CommitTxTask(
                [=](){
                    CTransaction tx;
                    int ret = DoHandleTx(std::make_shared<TxMsgReq>(txmsg), tx);

                    tx.clear_hash();
                    tx.clear_verifysign();
                    tx.set_hash(Getsha256hash(tx.SerializeAsString()));
                    if (ret != 0)
                    {
                        DEBUGLOG("TTT tx verify <fail!!!> txhash:{}, ret:{}", tx.hash(), ret);
                        return;
                    }

                    DEBUGLOG("TTT tx verify <success> txhash:{}", tx.hash());
                });
        }
        deleteTxPending.push_back(iter);
    }

    for(auto& iter : deleteTxPending)
    {
        _txPending.erase(iter);
    }
}
	
int FailedTransactionCache::Add(uint64_t height, const TxMsgReq& msg)
{
    DEBUGLOG("TTT NodelistHeight discontent, repeat commit tx ,NodeHeight:{}, txUtxoHeight:{}",height, msg.txmsginfo().txutxoheight());
    std::unique_lock<std::shared_mutex> lock(_txPendingMutex);
    _txPending[height].push_back(msg);
	return 0;
}
