#include "task_pool.h"

std::mutex gettidMutex;
std::set<boost::thread::id> thread_ids;

void Gettid()
{
    std::lock_guard<std::mutex> lock(gettidMutex);
    boost::thread::id tid = boost::this_thread::get_id();
    for(int i=0;i<100000;i++){};
    if((thread_ids.find(tid)) == thread_ids.end())
    {
        thread_ids.insert(tid);
    }
}

void taskPool::taskPool_init()
{
    for(int i=0; i < global::ca_thread_number * 100; i++) ca_taskPool.schedule(&Gettid);
    for(int i=0; i < global::net_thread_number * 100; i++) net_taskPool.schedule(&Gettid);
    for(int i=0; i < global::broadcast_thread_number * 100; i++) broadcast_taskPool.schedule(&Gettid);
    for(int i=0; i < global::tx_thread_number * 100; i++) tx_taskPool.schedule(&Gettid);
    for(int i=0; i < global::syncBlock_thread_number * 100; i++) syncBlock_taskPool.schedule(&Gettid);
    for(int i=0; i < global::saveBlock_thread_number * 100; i++) saveBlock_taskPool.schedule(&Gettid);
    for(int i=0; i < global::block_thread_number * 100; i++) block_taskPool.schedule(&Gettid);
    for(int i=0; i < global::work_thread_number * 100; i++) work_taskPool.schedule(&Gettid);

    ca_taskPool.wait();
    net_taskPool.wait();
    broadcast_taskPool.wait();
    tx_taskPool.wait();
    syncBlock_taskPool.wait();
    saveBlock_taskPool.wait();
    block_taskPool.wait();
    work_taskPool.wait();

    std::cout << "ThreadNumber:" << thread_ids.size() << std::endl;

    for(auto &it : thread_ids)
    {
        int index = get_cpu_index();
        std::ostringstream tid;
        tid << it;
        uint64_t Utid = std::stoul(tid.str(), nullptr, 16);
        sys_thread_set_cpu(index, Utid);
    }

    thread_ids.clear();
}