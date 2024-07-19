#include "task_pool.h"
#include "../common/bind_thread.h"

std::mutex getTidMutex;
std::set<boost::thread::id> threadIds;

void Gettid()
{
    std::lock_guard<std::mutex> lock(getTidMutex);
    boost::thread::id tid = boost::this_thread::get_id();
    for(int i=0;i<100000;i++){};
    if((threadIds.find(tid)) == threadIds.end())
    {
        threadIds.insert(tid);
    }
}

void TaskPool::TaskPoolInit()
{
    for(int i=0; i < global::kCaThreadNumber * 100; i++) _caTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kNetThreadNumber * 100; i++) _netTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kBroadcastThreadNumber * 100; i++) _broadcastTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kTxThreadNumber * 100; i++) _txTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kSyncBlockThreadNumber * 100; i++) _syncBlockTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kSaveBlockThreadNumber * 100; i++) _saveBlockTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kBlockThreadNumber * 100; i++) _blockTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kWorkThreadNumber * 100; i++) _workTaskPool.schedule(&Gettid);

    _caTaskPool.wait();
    _netTaskPool.wait();
    _broadcastTaskPool.wait();
    _txTaskPool.wait();
    _syncBlockTaskPool.wait();
    _saveBlockTaskPool.wait();
    _blockTaskPool.wait();
    _workTaskPool.wait();

    std::cout << "ThreadNumber:" << threadIds.size() << std::endl;

    for(auto &it : threadIds)
    {
        int index = GetCpuIndex();
        std::ostringstream tid;
        tid << it;
        uint64_t Utid = std::stoul(tid.str(), nullptr, 16);
        SysThreadSetCpu(index, Utid);
    }

    threadIds.clear();
}