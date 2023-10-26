/**
 * *****************************************************************************
 * @file        block_cache.h
 * @brief       
 * @date        2023-09-25
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef CA_BLOCKCACHE_H
#define CA_BLOCKCACHE_H

#include <map>
#include <string>
#include <set>
#include <mutex>
#include <vector> 

#include "block.pb.h"
#include "block_compare.h"
#include "proto/block.pb.h"
#include "../db/db_api.h"
#include "ca/global.h"


/**
 * Compute class interface   
 */
class CCalBlockCacheInterface
{
public:
    /**
     * @brief       
     * 
     * @param       cache: 
     * @return      int 
     */
    virtual int Process(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> & cache) = 0;
};

/*
*
*/
class CBlockCache
{
public:
    /**
     * @brief       
     * 
     * @param       block: 
     * @return      int 
     */
    int Add(const CBlock & block);
    
    /**
     * @brief       
     * 
     * @param       cal: 
     * @return      int 
     */
    int Calculate(CCalBlockCacheInterface & cal) const;
    
    /**
     * @brief       
     * 
     * @param       block_height: 
     * @param       block_hash: 
     * @return      int 
     */
    int Remove(const uint64_t blockHeight, const std::string & blockHash);

    /**
     * @brief       Get the Cache object
     * 
     * @param       block_cache: 
     */
    void GetCache(std::map<uint64_t, std::set<CBlock, CBlockCompare>>& blockCache);


public:
    CBlockCache();
    CBlockCache(CBlockCache &&) = delete;
    CBlockCache(const CBlockCache &) = delete;
    CBlockCache &operator=(CBlockCache &&) = delete;
    CBlockCache &operator=(const CBlockCache &) = delete;
    ~CBlockCache() = default;

private:
    /**
     * @brief       
     * 
     * @param       where: 
     * @return      std::string 
     */
    friend std::string PrintCache(int where);


    std::map<uint64_t, std::set<CBlock, CBlockCompare>> _cache;
    std::mutex _cacheMutex;
    bool _constructSuccess;

    bool CacheInit();

    /**
     * @brief       
     * 
     * @param       block: 
     */
    void CacheAdd(const CBlock & block);

    /**
     * @brief       
     * 
     * @param       block_height: 
     */
    void CacheRemove(uint64_t blockHeight);


};

#endif 