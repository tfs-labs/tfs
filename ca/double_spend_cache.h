/**
 * *****************************************************************************
 * @file        double_spend_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */

#include "ca/block_stroage.h"
#include "utils/vrf.hpp"
#include "utils/tfs_bench_mark.h"
#include "proto/block.pb.h"
#include "ca/block_helper.h"
#include "ca/transaction.h"
#include "ca/sync_block.h"
#include "common/global_data.h"
#include "net/peer_node.h"
#include "proto/block.pb.h"
#include "common/task_pool.h"
#include "common/global.h"
#include "ca/algorithm.h"
#include "ca/test.h"


/**
 * @brief       
 * 
 */
class DoubleSpendCache
{
public:
    DoubleSpendCache(){	
        _timer.AsyncLoop(100, [this](){
        CheckLoop();
	});
        
    };
    ~DoubleSpendCache() = default;
public:
    /**
     * @brief       
     * 
     */
    void StopTimer(){_timer.Cancel();}

    /**
     * @brief       
     * 
     * @param       addrUtxo: 
     * @return      int 
     */
    int AddFromAddr(const std::vector<std::string> &addrUtxo);

    /**
     * @brief       
     * 
     */
    void CheckLoop();
private:
    CTimer _timer;
    // friend std::string PrintCache(int where);
	std::mutex _doubleSpendMutex;
    std::map<std::vector<std::string>,uint64_t> _pendingAddrs;

};
