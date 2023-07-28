#ifndef __CA_TRANSACTION_CACHE__
#define __CA_TRANSACTION_CACHE__

#include "../proto/transaction.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "utils/CTimer.hpp"
#include "ca/ca_transactionentity.h"


#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>



// Transaction cache class. After the transaction flow ends, add the transaction to this class. Pack blocks every time a certain interval elapses or when the number of transactions reaches a certain number.
class CtransactionCache
{
    private:
        typedef std::list<TransactionEntity>::const_iterator tx_entities_iter;
        typedef std::map<uint64_t, std::list<TransactionEntity>>::iterator cache_iter;

    private:
       friend std::string PrintCache(int where);
        // Transaction container
        std::map<uint64_t ,std::list<TransactionEntity>> cache_;
        // The mutex of the transaction container
        std::mutex cache_mutex_;
        // Condition variables are used to package blocks
        std::condition_variable blockbuilder_;
        // Timers are used for packing at specific time intervals
        CTimer build_timer_;
        // Thread variables are used for packaging
        std::thread build_thread_;
        // Packing interval
        static const int build_interval_;
        // Transaction expiration interval
        static const time_t tx_expire_interval_;
        // Packaging threshold
        static const int build_threshold_;

    public:
        CtransactionCache();
        ~CtransactionCache() = default;
        // Add a cache
        int add_cache(const CTransaction& transaction, const TxMsgReq& SendTxMsg);
        //  Start the packaging block building thread  // ca_ini ca_init call
        bool process();

        void Stop();

        // Check for conflicting (overloaded) block pool calls
        bool check_conflict(const CTransaction& transaction);
        // Get the transaction cache
        void get_cache(std::map<uint64_t, std::list<TransactionEntity>>& cache); 
        // Query the cache for the existence of a transaction
        bool exist_in_cache(const std::string& hash);
        // Delete the pending transaction cache
        bool remove_pending_transaction(const std::string& tx_hash);
        //  Delete the double spend transaction of cache
        bool remove_cache_transaction(const std::string& tx_hash);

    private:
        // Threading functions
        void processing_func();
        // Get the cache that needs to be blocked
        std::list<tx_entities_iter>  get_needed_cache(const std::list<TransactionEntity>& txs);
        // Delete the block building cache and expired cache
        // Return value: Whether there are still transactions at that height
        bool remove_processed_transaction(const  std::list<tx_entities_iter>& tx_entities_iter, const bool build_success, std::list<TransactionEntity>& tx_entities);


        std::atomic<bool> thread_run=true;
        // Clean up functions
        void tear_down(const  std::list<tx_entities_iter>& tx_entities_iters, const bool build_success, std::vector<cache_iter>& empty_height_cache , cache_iter cache_entity);
};
#endif
