/**
 * *****************************************************************************
 * @file        double_spend_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */

#include "ca/block_stroage.h"
#include "utils/tfs_bench_mark.h"
#include "common/global_data.h"
/**
 * @brief       
 * 
 */
class DoubleSpendCache
{
public:
    DoubleSpendCache(){	
        _timer.AsyncLoop(1000, [this](){
        CheckLoop();
	});
        
    };
    ~DoubleSpendCache() = default;
public:

    struct doubleSpendsuc
    {
        uint64_t time;
        std::vector<std::string> utxoVec;
    };
    
    /**
     * @brief       
     * 
     */
    void StopTimer(){_timer.Cancel();}

    /**
     * @brief       
     * 
     * @param       usings: 
     * @return      int 
     */
    int AddFromAddr(const std::pair<std::string,DoubleSpendCache::doubleSpendsuc> &usings);


    /**
     * @brief       
     * 
     */
    void CheckLoop();
    void Remove(const uint64_t& txtimeKey);
    void Detection(const CBlock & block);



private:


    CTimer _timer;
	std::mutex _doubleSpendMutex;
    std::map<std::string,DoubleSpendCache::doubleSpendsuc> _pending;

};
