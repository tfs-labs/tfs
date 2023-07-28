#include "ca/ca_checker.h"

#include "utils/base58.h"

bool Checker::CheckConflict(const CTransaction &tx, const std::map<uint64_t, std::list<TransactionEntity>> &cache)
{
    CTransaction cur_tx;
    for(const auto& pairHeightTxs : cache)
    {
        for(const auto& tx_entity : pairHeightTxs.second)
        {
            cur_tx = tx_entity.get_transaction();
            if(CheckConflict(cur_tx, tx) == true)
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
        for(const auto& cur_tx : block.txs())
        {
            if(GetTransactionType(tx) != kTransactionType_Tx)
            {
                continue;
            }

            if(CheckConflict(cur_tx, tx) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CBlock &block, const std::set<CBlock, compator::BlockTimeAscending> &blocks, std::string* txHashPtr)
{
    for (const auto& current_block : blocks)
    {
        if(txHashPtr != NULL)
        {
            std::string txHash = "";
            if(CheckConflict(current_block, block, &txHash) == true)
            {
                *txHashPtr = txHash;
                return true;
            }
        }
        else
        {
            if(CheckConflict(current_block, block) == true)
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
                    *txHashPtr = getsha256hash(copyTx.SerializeAsString());
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

void Checker::CheckConflict(const CBlock &block, std::vector<CTransaction>& DoubleSpentTransactions)
{
    std::map<std::string, std::vector<CTransaction>> TransactionPool;
    for (const auto& tx : block.txs())
    {
        global::ca::TxType tx_type = (global::ca::TxType)tx.txtype();
        for(const auto& vin : tx.utxo().vin())
        {
            for (auto & prevout : vin.prevout())
            {
                std::string&& utxo = prevout.hash() + "_" + GetBase58Addr(vin.vinsign().pub());
                if(TransactionPool.find(utxo) != TransactionPool.end() 
                && global::ca::TxType::kTxTypeUnstake == tx_type || global::ca::TxType::kTxTypeDisinvest == tx_type)
                {
                    continue;
                }          
                TransactionPool[utxo].push_back(tx);
            }
        }
    }
    for(auto& iter : TransactionPool)
    {
        if(iter.second.size() > 1)
        {
            std::sort(iter.second.begin(), iter.second.end(), [](const CTransaction& a, const CTransaction& b) {
                return a.time() < b.time();
            });
            DoubleSpentTransactions.insert(DoubleSpentTransactions.end(), iter.second.begin()+1, iter.second.end());
        }
    }
}