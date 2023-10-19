#include "ca/checker.h"

#include "utils/base58.h"

bool Checker::CheckConflict(const CTransaction &tx, const std::map<uint64_t, std::list<TransactionEntity>> &cache)
{
    CTransaction curTx;
    for(const auto& pairHeightTxs : cache)
    {
        for(const auto& txEntity : pairHeightTxs.second)
        {
            curTx = txEntity.GetTransaction();
            if(CheckConflict(curTx, tx) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CTransaction &tx, const std::set<CBlock, compator::BlockTimeAscending> &blocks)
{
    for (const auto& block : blocks)
    {
        for(const auto& curTx : block.txs())
        {
            if(GetTransactionType(tx) != kTransactionType_Tx)
            {
                continue;
            }

            if(CheckConflict(curTx, tx) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CBlock &block, const std::set<CBlock, compator::BlockTimeAscending> &blocks, std::string* txHashPtr)
{
    for (const auto& currentBlock : blocks)
    {
        if(txHashPtr != NULL)
        {
            std::string txHash = "";
            if(CheckConflict(currentBlock, block, &txHash) == true)
            {
                *txHashPtr = txHash;
                return true;
            }
        }
        else
        {
            if(CheckConflict(currentBlock, block) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CBlock &block1, const CBlock &block2, std::string* txHashPtr)
{
    for(const auto& tx1 : block1.txs())
    {
        if(GetTransactionType(tx1) != kTransactionType_Tx)
        {
            continue;
        }

        for(const auto& tx2 : block2.txs())
        {
            if(GetTransactionType(tx2) != kTransactionType_Tx)
            {
                continue;
            }

            if(CheckConflict(tx1, tx2) == true)
            {
                if(txHashPtr != NULL)
                {
                    CTransaction copyTx = tx1;
                    copyTx.clear_hash();
                    copyTx.clear_verifysign();
                    *txHashPtr = Getsha256hash(copyTx.SerializeAsString());
                }
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CTransaction &tx1, const CTransaction &tx2)
{
    std::vector<std::string> vec1;
    for(const auto& vin : tx1.utxo().vin())
    {
        for (auto & prevout : vin.prevout())
        {
            vec1.push_back(prevout.hash() + "_" + GetBase58Addr(vin.vinsign().pub()));
        }
    }

    std::vector<std::string> vec2;
    for(const auto& vin : tx2.utxo().vin())
    {
        for (auto & prevout : vin.prevout())
        {
            vec2.push_back(prevout.hash() + "_" + GetBase58Addr(vin.vinsign().pub()));
        }
    }

    std::vector<std::string> vecIntersection;
    std::sort(vec1.begin(), vec1.end());
    std::sort(vec2.begin(), vec2.end());
    std::set_intersection(vec1.begin(), vec1.end(), vec2.begin(), vec2.end(), std::back_inserter(vecIntersection));
    return !vecIntersection.empty();
}

void Checker::CheckConflict(const CBlock &block, std::vector<CTransaction>& doubleSpentTransactions)
{
    std::map<std::string, std::vector<CTransaction>> transactionPool;
    for (const auto& tx : block.txs())
    {
        global::ca::TxType txType = (global::ca::TxType)tx.txtype();
        for(const auto& vin : tx.utxo().vin())
        {
            for (auto & prevout : vin.prevout())
            {
                std::string&& utxo = prevout.hash() + "_" + GetBase58Addr(vin.vinsign().pub());
                if(transactionPool.find(utxo) != transactionPool.end() 
                && global::ca::TxType::kTxTypeUnstake == txType || global::ca::TxType::kTxTypeDisinvest == txType)
                {
                    continue;
                }          
                transactionPool[utxo].push_back(tx);
            }
        }
    }
    for(auto& iter : transactionPool)
    {
        if(iter.second.size() > 1)
        {
            std::sort(iter.second.begin(), iter.second.end(), [](const CTransaction& a, const CTransaction& b) {
                return a.time() < b.time();
            });
            doubleSpentTransactions.insert(doubleSpentTransactions.end(), iter.second.begin()+1, iter.second.end());
        }
    }
}