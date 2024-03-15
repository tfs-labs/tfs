/**
 * *****************************************************************************
 * @file        task_pool.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __TASK_POOL_H__
#define __TASK_POOL_H__
#define BOOST_BIND_NO_PLACEHOLDERS

#include "./config.h"
#include "./global.h"

#include "../net/dispatcher.h"
#include "../utils/magic_singleton.h"
#include "../net/work_thread.h"

#include <boost/threadpool.hpp>
using boost::threadpool::pool;

/**
 * @brief       
 * 
 */
void Gettid();

class TaskPool{
public:
    ~TaskPool() = default;
    TaskPool(TaskPool &&) = delete;
    TaskPool(const TaskPool &) = delete;
    TaskPool &operator=(TaskPool &&) = delete;
    TaskPool &operator=(const TaskPool &) = delete;
public:
    TaskPool()
    :_caTaskPool(global::kCaThreadNumber)
    ,_netTaskPool(global::kNetThreadNumber)
    ,_broadcastTaskPool(global::kBroadcastThreadNumber)
    ,_txTaskPool(global::kTxThreadNumber)
    ,_syncBlockTaskPool(global::kSyncBlockThreadNumber)
    ,_saveBlockTaskPool(global::kSaveBlockThreadNumber)
    ,_blockTaskPool(global::kBlockThreadNumber)
    ,_workTaskPool(global::kWorkThreadNumber)
    {}
    
    /**
     * @brief       
     * 
     */
    void TaskPoolInit();
    
    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitCaTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _caTaskPool.schedule(boost::bind(func, subMsg, data));
    }
    
    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitCaTask(T func)
    {
        _caTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitNetTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _netTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     * @param       task 
     */
    void CommitBroadcastTask(std::function<void()> task) {
        _broadcastTaskPool.schedule(task);
    }
    

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitBroadcastTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _broadcastTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitTxTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _txTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitTxTask(T func)
    {
        _txTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitSyncBlockTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _syncBlockTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitSyncBlockTask(T func)
    {
        _syncBlockTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitSaveBlockTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _saveBlockTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitBlockTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        _blockTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitBlockTask(T func)
    {
        _blockTaskPool.schedule(func);
    }

    template<class T>
    void CommitWorkTask(T func)
    {
        _workTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t CaActive() const  {return _caTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t CaPending() const {return _caTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t NetActive() const {return _netTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t NetPending() const  {return _netTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BroadcastActive() const{return _broadcastTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BroadcastPending() const{return _broadcastTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t TxActive() const{return _txTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t TxPending() const{return _txTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t SyncBlockActive() const{return _syncBlockTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t SyncBlockPending() const{return _syncBlockTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t SaveBlockActive() const{return _saveBlockTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t SaveBlockPending() const{return _saveBlockTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BlockActive() const{return _blockTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BlockPending() const{return _blockTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t WorkActive() const{return _workTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t WorkPending() const{return _workTaskPool.pending();}

private:
    pool _caTaskPool;
    pool _netTaskPool;
    pool _broadcastTaskPool;

    pool _txTaskPool;
    pool _syncBlockTaskPool;
    pool _saveBlockTaskPool;

    pool _blockTaskPool;
    pool _workTaskPool;
};

#endif // __TASK_POOL_H__