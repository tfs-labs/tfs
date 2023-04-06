#ifndef __TASK_POOL_H__
#define __TASK_POOL_H__
#define BOOST_BIND_NO_PLACEHOLDERS

#include "net/dispatcher.h"
#include "utils/MagicSingleton.h"
#include "./config.h"
#include "./global.h"
#include "net/work_thread.h"
#include <boost/threadpool.hpp>
using boost::threadpool::pool;


void Gettid();

class taskPool{
public:
    ~taskPool() = default;
    taskPool(taskPool &&) = delete;
    taskPool(const taskPool &) = delete;
    taskPool &operator=(taskPool &&) = delete;
    taskPool &operator=(const taskPool &) = delete;
public:
    taskPool():
      ca_taskPool(global::ca_thread_number),
      net_taskPool(global::net_thread_number),
      broadcast_taskPool(global::broadcast_thread_number),
      tx_taskPool(global::tx_thread_number),
      syncBlock_taskPool(global::syncBlock_thread_number),
      saveBlock_taskPool(global::saveBlock_thread_number),
      block_taskPool(global::block_thread_number),
      work_taskPool(global::work_thread_number)
      {}
    
    void taskPool_init();
    
    void commit_ca_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        ca_taskPool.schedule(boost::bind(func, sub_msg, data));
    }
    template<class T>
    void commit_ca_task(T func)
    {
        ca_taskPool.schedule(func);
    }

    void commit_net_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        net_taskPool.schedule(boost::bind(func, sub_msg, data));
    }

    void commit_broadcast_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        broadcast_taskPool.schedule(boost::bind(func, sub_msg, data));
    }

    void commit_tx_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        tx_taskPool.schedule(boost::bind(func, sub_msg, data));
    }

    template<class T>
    void commit_tx_task(T func)
    {
        tx_taskPool.schedule(func);
    }

    void commit_syncBlock_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        syncBlock_taskPool.schedule(boost::bind(func, sub_msg, data));
    }

    template<class T>
    void commit_syncBlock_task(T func)
    {
        syncBlock_taskPool.schedule(func);
    }

    void commit_saveBlock_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        saveBlock_taskPool.schedule(boost::bind(func, sub_msg, data));
    }

    void commit_block_task(ProtoCallBack func, MessagePtr sub_msg, const MsgData &data)
    {
        block_taskPool.schedule(boost::bind(func, sub_msg, data));
    }
    template<class T>
    void commit_block_task(T func)
    {
        block_taskPool.schedule(func);
    }

    template<class T>
    void commit_work_task(T func)
    {
        work_taskPool.schedule(func);
    }

    size_t ca_active() const  {return ca_taskPool.active();}
    size_t ca_pending() const {return ca_taskPool.pending();}

    size_t net_active() const {return net_taskPool.active();}
    size_t net_pending() const  {return net_taskPool.pending();}

    size_t broadcast_active() const{return broadcast_taskPool.active();}
    size_t broadcast_pending() const{return broadcast_taskPool.pending();}

    size_t tx_active() const{return tx_taskPool.active();}
    size_t tx_pending() const{return tx_taskPool.pending();}

    size_t syncBlock_active() const{return syncBlock_taskPool.active();}
    size_t syncBlock_pending() const{return syncBlock_taskPool.pending();}

    size_t saveBlock_active() const{return saveBlock_taskPool.active();}
    size_t saveBlock_pending() const{return saveBlock_taskPool.pending();}

    size_t block_active() const{return block_taskPool.active();}
    size_t block_pending() const{return block_taskPool.pending();}

    size_t work_active() const{return work_taskPool.active();}
    size_t work_pending() const{return work_taskPool.pending();}

private:
    pool ca_taskPool;
    pool net_taskPool;
    pool broadcast_taskPool;

    pool tx_taskPool;
    pool syncBlock_taskPool;
    pool saveBlock_taskPool;

    pool block_taskPool;
    pool work_taskPool;
};

#endif // __TASK_POOL_H__