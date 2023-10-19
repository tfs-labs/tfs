/**
 * *****************************************************************************
 * @file        global.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <list>

#include "./define.h"
#include "./msg_queue.h"

#include "../proto/net.pb.h"
#include "../utils/timer.hpp"


namespace global
{
    extern MsgQueue g_queueRead;
    extern MsgQueue g_queueWork;
    extern MsgQueue g_queueWrite;
    extern std::string g_localIp;
    extern int g_cpuNums;
    extern atomic<int> g_nodelistRefreshTime;
    extern std::list<int> g_phoneList;
    extern std::mutex g_mutexForPhoneList;
    extern CTimer g_heartTimer;
    extern std::mutex g_mutexListenThread;
    extern std::mutex g_mutexSetFee;
    extern std::condition_variable_any g_condListenThread;
    extern bool g_ListenThreadInited;

    extern std::mutex g_mutexReqCntMap;
    extern std::map<std::string, std::pair<uint32_t, uint64_t>> g_reqCntMap;
    extern int g_broadcastThreshold;
}

#endif
