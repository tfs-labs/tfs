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

#include "ca_global.h"

#include "../net/node.hpp"
#include "../net/peer_node.h"
#include "utils/MagicSingleton.h"
#include "../include/logging.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/block.pb.h"
#include "include/net_interface.h"
#include "utils/time_util.h"


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
    int SendBroadcastAddBlock(std::string strBlock, uint64_t blockHeight,uint64_t SendSize = 100);
    // int SendSuccessBlockSituationAck(const CBlock &block);
    // int HandleBroadcastAddBlockAck(const BuildBlockBroadcastMsgAck & msg);
    // int SendFailedBlockSituationAck(const CBlock &block);
    // void RemoveSelf(const std::string& bolckraw);
    // void RemoveCache(const std::string& bolckraw);
    // void RemoveRespon(const std::string& bolckraw);


private:
    static uint32_t MaxSendSize;
    // static uint64_t selfTime ;

    struct BlockAck
    {
        uint32_t is_response = 0;
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

