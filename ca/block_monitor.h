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

    // int AddBlockMonitor(const std::string& blockhash , const std::string & id , const uint32_t& ackFlag);

    /**
     * @brief       
     * 
     * @param       strBlock: 
     * @param       blockHeight: 
     * @param       sendSize: 
     * @return      int 
     */
    int SendBroadcastAddBlock(std::string strBlock, uint64_t blockHeight,uint64_t sendSize = 100);
    // int SendSuccessBlockSituationAck(const CBlock &block);
    // int HandleBroadcastAddBlockAck(const BuildBlockBroadcastMsgAck & msg);
    // int SendFailedBlockSituationAck(const CBlock &block);
    // void RemoveSelf(const std::string& bolckraw);
    // void RemoveCache(const std::string& bolckraw);
    // void RemoveRespon(const std::string& bolckraw);


private:
    static uint32_t _maxSendSize;
    // static uint64_t selfTime ;

    /**
     * @brief       
     * 
     */
    struct BlockAck
    {
        uint32_t isResponse = 0;
        std::string  id;
    };
    // friend std::string PrintCache(int where);

    /********Store it yourself******/
    // std::mutex _monitor_mutex_;
    // std::vector<std::string> success_id;
 
    // /********Receipt storage*********/

    // std::map<std::string , BlockAck> _Respon;//Other nodes store blocks
    // std::map<std::string, std::map<std::string,std::string>> _Self; //The initiating node stores the key and its own ID and success

    // std::map<std::string, std::string> blockCache;

};  

#endif

