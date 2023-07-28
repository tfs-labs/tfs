#ifndef CA_BLOCKCACHE_H
#define CA_BLOCKCACHE_H

#include <map>
#include <string>
#include <set>
#include <mutex>
#include <vector> 

#include "block.pb.h"
#include "ca_blockcompare.h"
#include "proto/block.pb.h"
#include "../db/db_api.h"
#include "ca/ca_global.h"


//Compute class interface
class CCalBlockCacheInterface
{
public:
    virtual int Process(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> & cache) = 0;
};

class CBlockCache
{
public:

    int Add(const CBlock & block);
    
    int Calculate(CCalBlockCacheInterface & cal) const;
    
    int Remove(const uint64_t block_height, const std::string & block_hash);

    void GetCache(std::map<uint64_t, std::set<CBlock, CBlockCompare>>& block_cache);


public:
    CBlockCache();
    CBlockCache(CBlockCache &&) = delete;
    CBlockCache(const CBlockCache &) = delete;
    CBlockCache &operator=(CBlockCache &&) = delete;
    CBlockCache &operator=(const CBlockCache &) = delete;
    ~CBlockCache() = default;

private:
    friend std::string PrintCache(int where);
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> cache;
    std::mutex cache_mutex;
    bool construct_success;

    bool CacheInit();
    void CacheAdd(const CBlock & block);
    void CacheRemove(uint64_t block_height);


};

#endif 