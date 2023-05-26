#include "ca/ca_blockstroage.h"
#include "utils/VRF.hpp"
#include "utils/TFSbenchmark.h"
#include "proto/block.pb.h"
#include "ca/ca_blockhelper.h"
#include "ca/ca_transaction.h"
#include "ca/ca_sync_block.h"
#include "common/global_data.h"
#include "net/peer_node.h"
#include "proto/block.pb.h"
#include "common/task_pool.h"
#include "common/global.h"
#include "ca/ca_algorithm.h"
#include "ca/ca_test.h"



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
    int AddFromAddr(const std::vector<std::string> &addrUtxo);
    void CheckLoop();
private:
    CTimer _timer;
	std::mutex _double_spend_mutex_;
    std::map<std::vector<std::string>,uint64_t> _pendingAddrs;

};
