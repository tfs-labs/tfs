#ifndef TFS_DB_REDIS_H_
#define TFS_DB_REDIS_H_

#include "utils/CTimer.hpp"
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <shared_mutex>
struct BounsAddrInfo
{
    bool dirty{true}; 
    uint64_t amount;//Amount invested
    std::set<std::string> utxos;//utxos invested
    uint64_t time;//Qualified investment time
};

class BounsAddrCache
{
public:
    int get_amount(const std::string& BounsAddr, uint64_t& amount);
    uint64_t get_time(const std::string& BounsAddr);
    void is_dirty(const std::string& BounsAddr, bool dirty = true);
private:
    mutable std::shared_mutex BounsAddr_mutex;
    std::map<std::string, BounsAddrInfo> bouns_addr_;
};

class DBCache
{
public:
    DBCache();
    ~DBCache();
    DBCache(DBCache &&) = delete;
    DBCache(const DBCache &) = delete;
    DBCache &operator=(DBCache &&) = delete;
    DBCache &operator=(const DBCache &) = delete;
    bool GetData(const std::string &key, std::string &value);
    bool SetData(const std::string &key, const std::string &value);
    bool DeleteData(const std::string &key);
    bool AddData(const std::map<std::string, std::string> &add_data);
    bool DeleteData(const std::set<std::string> &keys);

private:
    uint64_t GetDateSize();
    void SetCacheSize();
    void ClearExpireData();
    std::mutex mutex_;
    std::unordered_map<std::string, std::pair<uint64_t, std::string>> data_;
    std::map<uint64_t, std::set<std::string>> time_data_;
    CTimer timer_;
    //Save up to save_key_num keys
    uint64_t save_key_num_;
    //Save up to save_time Long time (seconds)
    uint64_t save_time_;

    uint64_t cache_size_;
};

#endif
