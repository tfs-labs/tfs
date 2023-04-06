#include "db/cache.h"
#include "include/logging.h"
#include <sys/sysinfo.h>
#include "db/db_api.h"
#include "ca/ca_global.h"

int BounsAddrCache::get_amount(const std::string& BounsAddr, uint64_t& amount)
{
    std::unique_lock<std::shared_mutex> lock(BounsAddr_mutex);
    if(!bouns_addr_[BounsAddr].dirty)
    {
        auto it = bouns_addr_.find(BounsAddr);
        if(it != bouns_addr_.end())
        {
            amount = it->second.amount;
            return 0;
        }
    }

    bouns_addr_[BounsAddr].utxos.clear();
    DBReader db_reader;
    std::vector<std::string> addrs;
    auto ret = db_reader.GetInvestAddrsByBonusAddr(BounsAddr, addrs);
    if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
    {
        return -1;
    }

    uint64_t sum_invest_amount = 0;
    bool flag = true;
    for (auto &addr : addrs)
    {
        std::vector<std::string> utxos;
        ret = db_reader.GetBonusAddrInvestUtxosByBonusAddr(BounsAddr, addr, utxos);
        if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
        {
            return -2;
        }

        for (const auto &hash : utxos)
        {
            std::string strTx;
            if (db_reader.GetTransactionByHash(hash, strTx) != DBStatus::DB_SUCCESS)
            {
                return -3;
            }
            bouns_addr_[BounsAddr].utxos.insert(hash);

            CTransaction tx;
            if (!tx.ParseFromString(strTx))
            {
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
                bouns_addr_[BounsAddr].time = tx.time();
                flag = false;
            }
        }
    }
    bouns_addr_[BounsAddr].amount = sum_invest_amount;
    bouns_addr_[BounsAddr].dirty = false;
    amount = sum_invest_amount;
    return 0;
}
uint64_t BounsAddrCache::get_time(const std::string& BounsAddr)
{
    std::shared_lock<std::shared_mutex> lock(BounsAddr_mutex);
    auto it = bouns_addr_.find(BounsAddr);
    if(it != bouns_addr_.end())
    {
        return it->second.time;
    }
    else return 0;
}

void BounsAddrCache::is_dirty(const std::string& BounsAddr, bool dirty)
{
    std::unique_lock<std::shared_mutex> lock(BounsAddr_mutex);
    bouns_addr_[BounsAddr].dirty = dirty;
    return;
}


DBCache::DBCache()
{
    save_time_ = 24 * 60 * 60;
    save_key_num_ = 500000;
    struct sysinfo info;
    int ret = sysinfo(&info);
    if(0 == ret)
    {
        int num = info.totalram >> 30;
        num -= 4;
        if(num > 1)
        {
            save_key_num_ *= num;
        }
    }

    cache_size_ = 500 * 1024 * 1024;
    SetCacheSize();

    timer_.AsyncLoop(10 * 1000, [this](){
        if(GetDateSize() >= cache_size_)
        {
           ClearExpireData();
        }
    });
}

DBCache::~DBCache()
{
    timer_.Cancel();
}

bool DBCache::GetData(const std::string &key, std::string &value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if(data_.cend() == it)
    {
        return false;
    }
    value = it->second.second;
    return true;
}

bool DBCache::SetData(const std::string &key, const std::string &value)
{
    {
        std::set<std::string> data_key;
        data_key.insert(key);
        DeleteData(data_key);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = time(nullptr);
    data_.insert(std::make_pair(key, std::make_pair(now, value)));
    auto it = time_data_.find(now);
    if(time_data_.cend() == it)
    {
        std::set<std::string> keys;
        keys.insert(key);
        time_data_.insert(std::make_pair(now, keys));
    }
    else
    {
        it->second.insert(key);
    }
    return true;
}
bool DBCache::DeleteData(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto data_it = data_.find(key);
    if(data_.cend() != data_it)
    {
        data_.erase(data_it);
    }
    auto it = time_data_.find(data_it->second.first);
    if(time_data_.cend() != it)
    {
        it->second.erase(key);
    }

    return true;
}

bool DBCache::AddData(const std::map<std::string, std::string> &add_data)
{
    {
        std::set<std::string> data_key;
        for(auto data : add_data)
        {
            data_key.insert(data.first);
        }
        DeleteData(data_key);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<std::string> keys;
    uint64_t now = 0;
    for(auto data : add_data)
    {
        now = time(nullptr);
        data_.insert(std::make_pair(data.first, std::make_pair(now, data.second)));
        auto it = time_data_.find(now);
        if(time_data_.cend() == it)
        {
            keys.clear();
            keys.insert(data.first);
            time_data_.insert(std::make_pair(now, keys));
        }
        else
        {
            it->second.insert(data.second);
        }
    }
    return true;
}

bool DBCache::DeleteData(const std::set<std::string> &keys)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto key : keys)
    {
        auto data_it = data_.find(key);
        if(data_.cend() == data_it)
        {
            continue;
        }
        auto time_data_key = data_it->second.first;
        data_.erase(data_it);
        auto it = time_data_.find(time_data_key); 
        if(time_data_.cend() == it)
        {
            continue;
        }
        it->second.erase(key);
    }
    return true;
}
void DBCache::ClearExpireData()
{
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    time_data_.clear();
}

uint64_t DBCache::GetDateSize()
 {
    uint64_t n64data_size = 0;
    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& it : data_)
    {
        n64data_size += it.second.second.size();
        n64data_size += sizeof(it.second.first);
    }
    return n64data_size;
 }

void DBCache::SetCacheSize()
{
    FILE* fpMemInfo = fopen("/proc/meminfo", "r");
    if (NULL == fpMemInfo)
    {
        return;
    }

    int value = 0;
    char name[1024];
    char line[1024];
    int nFiledNumber = 2;
    unsigned int total;
    while (fgets(line, sizeof(line) - 1, fpMemInfo))
    {
        if (sscanf(line, "%s%u", name, &value) != nFiledNumber)
        {
            continue;
        }
        if (0 == strcmp(name, "MemTotal:"))
        {
            total = value;
            break;
        }
    }

    int demarcation = 12 * 1024 * 1024;
    if(total > demarcation)
    {
        cache_size_ = 1 * 1024 * 1024 * 1024;
    }

}