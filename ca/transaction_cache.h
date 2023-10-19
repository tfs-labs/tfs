/**
 * *****************************************************************************
 * @file        transaction_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __CA_TRANSACTION_CACHE__
#define __CA_TRANSACTION_CACHE__

#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>

#include "../proto/transaction.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "utils/timer.hpp"
#include "ca/transaction_entity.h"


/**
 * @brief       Transaction cache class. After the transaction flow ends, add the transaction to this class. 
                Pack blocks every time a certain interval elapses or when the number of transactions reaches a certain number.
 */
class TransactionCache
{
    private:
        typedef std::list<TransactionEntity>::const_iterator txEntitiesIter;
        typedef std::map<uint64_t, std::list<TransactionEntity>>::iterator cacheIter;

    private:
    //    friend std::string PrintCache(int where);
        // Transaction container
        std::map<uint64_t ,std::list<TransactionEntity>> _cache;
        // The mutex of the transaction container
        std::mutex _cacheMutex;
        // Condition variables are used to package blocks
        std::condition_variable _blockBuilder;
        // Timers are used for packing at specific time intervals
        CTimer _buildTimer;
        // Thread variables are used for packaging
        std::thread _buildThread;
        // Packing interval
        static const int _kBuildInterval;
        // Transaction expiration interval
        static const time_t _kTxExpireInterval;
        // Packaging threshold
        static const int _kBuildThreshold;

    public:
        TransactionCache();
        ~TransactionCache() = default;
        /**
         * @brief       Add a cache
         * 
         * @param       transaction: 
         * @param       sendTxMsg: 
         * @return      int 
         */
        int AddCache(const CTransaction& transaction, const TxMsgReq& sendTxMsg);

        /**
         * @brief       Start the packaging block building thread  // ca_ini CaInit call
         * 
         * @return      true 
         * @return      false 
         */
        bool Process();

        /**
         * @brief       
         * 
         */
        void Stop();

        /**
         * @brief       Check for conflicting (overloaded) block pool calls
         * 
         * @param       transaction: 
         * @return      true 
         * @return      false 
         */
        bool CheckConflict(const CTransaction& transaction);

        /**
         * @brief       Get the transaction cache
         * 
         * @param       cache: 
         */
        void GetCache(std::map<uint64_t, std::list<TransactionEntity>>& cache); 

        /**
         * @brief       Query the cache for the existence of a transaction
         * 
         * @param       hash: 
         * @return      true 
         * @return      false 
         */
        bool ExistInCache(const std::string& hash);

        /**
         * @brief       Delete the pending transaction cache
         * 
         * @param       txHash: 
         * @return      true 
         * @return      false 
         */
        bool RemovePendingTransaction(const std::string& txHash);

        /**
         * @brief       Delete the double spend transaction of cache
         * 
         * @param       txHash: 
         * @return      true 
         * @return      false 
         */
        bool RemoveCacheTransaction(const std::string& txHash);

    private:
        /**
         * @brief       Threading functions
         */
        void _ProcessingFunc();
        /**
         * @brief       Get the cache that needs to be blocked
         * 
         * @param       txs: 
         * @return      std::list<txEntitiesIter> 
         */
        std::list<txEntitiesIter>  _GetNeededCache(const std::list<TransactionEntity>& txs);

        /**
         * @brief       Delete the block building cache and expired cache
                        Return value: Whether there are still transactions at that height
         * 
         * @param       txEntitiesIter: 
         * @param       buildSuccess: 
         * @param       txEntities: 
         * @return      true 
         * @return      false 
         */
        bool _RemoveProcessedTransaction(const  std::list<txEntitiesIter>& txEntitiesIter, const bool buildSuccess, std::list<TransactionEntity>& txEntities);


        std::atomic<bool> _threadRun=true;

        /**
         * @brief       Clean up functions
         * 
         * @param       txEntitiesIters: 
         * @param       buildSuccess: 
         * @param       emptyHeightCache: 
         * @param       cacheEntity: 
         */
        void _TearDown(const  std::list<txEntitiesIter>& txEntitiesIters, const bool buildSuccess, std::vector<cacheIter>& emptyHeightCache , cacheIter cacheEntity);
};
#endif
