/**
 * *****************************************************************************
 * @file        block_monitor.h
 * @brief       
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _CA_BLOCKMONITOR_H
#define _CA_BLOCKMONITOR_H

#include <net/if.h>
#include <unistd.h>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <shared_mutex>
#include <mutex>

#include "../ca/global.h"
#include "../net/node.hpp"
#include "../net/peer_node.h"
#include "utils/magic_singleton.h"
#include "../include/logging.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/block.pb.h"
#include "net/interface.h"
#include "utils/time_util.h"

/**
 * @brief       
 * 
 */
class BlockMonitor
{
public:
    BlockMonitor() = default;
    ~BlockMonitor() = default;
    BlockMonitor(BlockMonitor &&) = delete;
    BlockMonitor(const BlockMonitor &) = delete;
    BlockMonitor &operator=(BlockMonitor &&) = delete;
    BlockMonitor &operator=(const BlockMonitor &) = delete;

    /**
     * @brief       
     * 
     * @param       strBlock: 
     * @param       blockHeight: 
     * @param       sendSize: 
     * @return      int 
     */
    int SendBroadcastAddBlock(std::string strBlock, uint64_t blockHeight, uint64_t sendSize = 100);
    
    void addDropshippingTxVec(const std::string& txHash);
    void addDoHandleTxTxVec(const std::string& txHash);
    void checkTxSuccessRate();

private:
    static uint32_t _maxSendSize;

    /**
     * @brief       
     * 
     */
    struct BlockAck
    {
        uint32_t isResponse = 0;
        std::string  id;
    };

    std::mutex mutex_;
    std::vector<std::string> DropshippingTxVec;
    std::vector<std::string> DoHandleTxVec;

};  

#endif

