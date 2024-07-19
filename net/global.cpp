#include "global.h"

namespace global
{
    std::string g_localIp;
    int g_cpuNums;
    std::atomic<int> g_nodelistRefreshTime = 100; 
    MsgQueue g_queueRead("ReadQueue");   // Read queue
    MsgQueue g_queueWork("WorkQueue");   // Work queue is mainly used to process the queue calling CA code after read
    MsgQueue g_queueWrite("WriteQueue"); // Write queue
    std::list<int> g_phoneList; // Store FD connected to mobile phone
    std::mutex g_mutexForPhoneList;
    CTimer g_heartTimer;
    std::mutex g_mutexListenThread;
    std::mutex g_mutex_switch_thread;
    std::mutex g_mutexSetFee;
    std::condition_variable_any g_condListenThread;
    bool g_ListenThreadInited = false;

    std::mutex g_mutexReqCntMap;
    std::map<std::string, std::pair<uint32_t, uint64_t>> g_reqCntMap;

    int g_broadcastThreshold= 15;
}
