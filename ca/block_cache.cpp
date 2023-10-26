#include "block_cache.h"
#include "db/db_api.h"
#include "proto/block.pb.h"
#include "include/logging.h"
#include "transaction.h"
#include "utils/magic_singleton.h"
#include "transaction_cache.h"
#include "algorithm.h"
#include "block_http_callback.h"
#include "sync_block.h"
#include "include/scope_guard.h"
#include "test.h"
#include "global.h"
#include "utils/time_util.h"
#include <sys/sysinfo.h>


CBlockCache::CBlockCache()
{
    _constructSuccess = false;
}

bool CBlockCache::CacheInit()
{
    DBReader db_reader;
    uint64_t blockHeight;
    if(DBStatus::DB_SUCCESS != db_reader.GetBlockTop(blockHeight))
    {
        ERRORLOG("CBlockCache init failed, can't GetBlockTop");
        return false;
    }

    uint64_t end_height = blockHeight;
    uint64_t start_height = blockHeight > 1000 ? blockHeight - 1000 : 0;
    std::vector<std::string> blockHashes;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(start_height, end_height, blockHashes))
    {
        ERRORLOG("CBlockCache init failed, can't GetBlockHashesByBlockHeight");
        return false;
    }

    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(blockHashes, blocks))
    {
        ERRORLOG("CBlockCache init failed, can't GetBlocksByBlockHash");
        return false;
    }

    for(const auto & block_str : blocks)
    {
        CBlock block;
        block.ParseFromString(block_str);
        CacheAdd(block);
    }
    return true;
}

int CBlockCache::Add(const CBlock & block)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    uint64_t cacheLow = _cache.begin()->first;
    uint64_t blockHeight = block.height();
    if (blockHeight < cacheLow)
    {
        return 0;
    }
    
    if(!_constructSuccess){
        return -1;
    } 
    CacheAdd(block);
    if(_cache.size() > 1000)
    {
        CacheRemove(_cache.begin()->first);
    }

    return 0;
}

void CBlockCache::CacheAdd(const CBlock & block)
{
    uint64_t height = block.height();
    auto iter = _cache.find(height);
    if(iter == _cache.end())
    {
        _cache[height] = std::set<CBlock, CBlockCompare>{};
    }
    auto &value = _cache.at(height);
    value.insert(block);
}

void CBlockCache::CacheRemove(uint64_t blockHeight)
{
    auto iter = _cache.find(blockHeight);
    if(iter != _cache.end())
    {
        _cache.erase(iter);
    }
    else
    {
        INFOLOG("not found block height {} in the _cache",blockHeight);
    }
}

int CBlockCache::Remove(const uint64_t blockHeight, const std::string & blockHash)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    if(!_constructSuccess){
        return -1;
    }
    auto iter = _cache.find(blockHeight);
    DBReader db_reader;
    if(iter != _cache.end())
    {
        for(auto &block : iter->second)
        {
            if(block.hash() == blockHash)
            {
                if(_cache.size() >= 1000 && iter->second.size() == 1)
                {
                    _cache.erase(blockHeight);
                    DEBUGLOG("Remove delete _cache block hash :{}", blockHash);

                    uint64_t lowHeight = _cache.begin()->first-1;
                    std::vector<std::string> blockHashes;
                    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(lowHeight,blockHashes))
                    {
                        ERRORLOG("CBlockCache init failed, can't GetBlockHashsByBlockHeight");
                        return -2;
                    }
                    std::vector<std::string> blocks;
                    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(blockHashes, blocks))
                    {
                        ERRORLOG("CBlockCache init failed, can't GetBlocksByBlockHash");
                        return -3;
                    }

                    for(const auto & block_str : blocks)
                    {
                        CBlock block;
                        block.ParseFromString(block_str);
                        CacheAdd(block);
                    }
                    return 0;
                }
                else
                {
                    iter->second.erase(block);
                    return 0;
                }
            }
        }
    }
    else
    {
        INFOLOG("not found block height {} in the _cache",blockHeight);
    }
    
    return 0;
}

int CBlockCache::Calculate(CCalBlockCacheInterface & cal) const
{
    if(!_constructSuccess){
        return -999;
    }
    return cal.Process(this->_cache);
}

 void CBlockCache::GetCache(std::map<uint64_t, std::set<CBlock, CBlockCompare>>& block_cache)
 {
    block_cache = _cache;
 }
