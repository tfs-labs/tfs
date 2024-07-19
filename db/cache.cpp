#include "db/cache.h"
#include "include/logging.h"
#include <sys/sysinfo.h>
#include "db/db_api.h"
#include "ca/global.h"

int BonusAddrCache::getAmount(const std::string& bonusAddr, uint64_t& amount)
{
    amount = 0;
    std::unique_lock<std::shared_mutex> lock(BonusAddr_mutex);
    if(!bonus_addr_[bonusAddr].dirty)
    {
        auto it = bonus_addr_.find(bonusAddr);
        if(it != bonus_addr_.end())
        {
            amount = it->second.amount;
        }
    }
    
    if(amount >= global::ca::kMinInvestAmt)
    {
        return 0;
    }

    bonus_addr_[bonusAddr].utxos.clear();
    DBReader db_reader;
    std::vector<std::string> addrs;
    auto ret = db_reader.GetInvestAddrsByBonusAddr(bonusAddr, addrs);
    if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
    {
        DEBUGLOG("DBStatus is {}",ret);
        return -1;
    }

    uint64_t sum_invest_amount = 0;
    bool flag = true;
    for (auto &addr : addrs)
    {
        std::vector<std::string> utxos;
        ret = db_reader.GetBonusAddrInvestUtxosByBonusAddr(bonusAddr, addr, utxos);
        if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
        {
            DEBUGLOG("DBStatus is {}",ret);
            return -2;
        }

        for (const auto &hash : utxos)
        {
            std::string strTx;
             ret = db_reader.GetTransactionByHash(hash, strTx);
             if (ret != DBStatus::DB_SUCCESS)
             {
             DEBUGLOG("DBStatus is {}",ret);
                return -3;
             }
            bonus_addr_[bonusAddr].utxos.insert(hash);

            CTransaction tx;
            if (!tx.ParseFromString(strTx))
            {
                DEBUGLOG("parseFromString error");
                return -4;
            }
            for (int i = 0; i < tx.utxo().vout_size(); i++)
            {
                if (tx.utxo().vout(i).addr() == global::ca::kVirtualInvestAddr)
                {
                    sum_invest_amount += tx.utxo().vout(i).value();
                    break;
                }
            }
            if(sum_invest_amount >= global::ca::kMinInvestAmt && flag)
            {
                bonus_addr_[bonusAddr].time = tx.time();
                flag = false;
            }
        }
    }
    bonus_addr_[bonusAddr].amount = sum_invest_amount;
    bonus_addr_[bonusAddr].dirty = false;
    amount = sum_invest_amount;
    return 0;
}
uint64_t BonusAddrCache::getTime(const std::string& bonusAddr)
{
    std::shared_lock<std::shared_mutex> lock(BonusAddr_mutex);
    auto it = bonus_addr_.find(bonusAddr);
    if(it != bonus_addr_.end())
    {
        return it->second.time;
    }
    else return 0;
}

void BonusAddrCache::isDirty(const std::string& bonusAddr, bool dirty)
{
    std::unique_lock<std::shared_mutex> lock(BonusAddr_mutex);
    bonus_addr_[bonusAddr].dirty = dirty;
    return;
}