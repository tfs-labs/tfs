#include "global.h"

namespace global
{
    std::string local_ip;
    int cpu_nums;
    atomic<int> nodelist_refresh_time = 100;
    MsgQueue queue_read("ReadQueue");   // Read queue
    MsgQueue queue_work("WorkQueue");   // Work queue is mainly used to process the queue calling CA code after read
    MsgQueue queue_write("WriteQueue"); // Write queue
    std::list<int> phone_list; // Store FD connected to mobile phone
    std::mutex mutex_for_phone_list;
    CTimer heart_timer;
    std::mutex mutex_listen_thread;
    std::mutex mutex_switch_thread;
    std::mutex mutex_set_fee;
    std::condition_variable_any cond_listen_thread;
    bool listen_thread_inited = false;

    std::mutex g_mutex_req_cnt_map;
    std::map<std::string, std::pair<uint32_t, uint64_t>> reqCntMap;


    int broadcast_threshold= 15;
}
