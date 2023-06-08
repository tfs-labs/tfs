#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "./define.h"
#include "./msg_queue.h"
#include <list>
#include "../utils/CTimer.hpp"
#include "net.pb.h"

namespace global
{
    extern MsgQueue queue_read;
    extern MsgQueue queue_work;
    extern MsgQueue queue_write;
    extern std::string local_ip;
    extern int cpu_nums;
    extern atomic<int> nodelist_refresh_time;
    extern std::list<int> phone_list;
    extern std::mutex mutex_for_phone_list;
    extern CTimer heart_timer;
    extern std::mutex mutex_listen_thread;
    extern std::mutex mutex_set_fee;
    extern std::condition_variable_any cond_listen_thread;
    extern bool listen_thread_inited;

    extern std::mutex g_mutex_req_cnt_map;
    extern std::map<std::string, std::pair<uint32_t, uint64_t>> reqCntMap;
    extern int broadcast_threshold;
}

#endif
