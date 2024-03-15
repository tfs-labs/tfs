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
#include <utils/json.hpp>
#include <shared_mutex>

#include "../proto/transaction.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/block.pb.h"
#include "transaction_entity_V33_1.h"
#include "utils/timer.hpp"
#include "ca/transaction_entity.h"
#include "net/msg_queue.h"
#include "mpt/trie.h"
#include "ca/packager_dispatch.h"

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
        std::map<uint64_t ,std::list<CTransaction>> _transactionCache;
        // The mutex of the transaction container
        std::mutex _transactionCacheMutex;
        // Contract Transaction container
        std::vector<TransactionEntity> _contractCache;
        // The mutex of the Contract transaction container
        std::mutex _contractCacheMutex;
        std::mutex _contractPreHashMutex;
        // Condition variables are used to package blocks
        std::condition_variable _blockBuilder;
        std::condition_variable _contractPreBlockWaiter;
        // Timers are used for packing at specific time intervals
        CTimer _buildTimer;
        // Thread variables are used for packaging
        std::thread _transactionCacheBuildThread;
        // Thread variables are used for packaging
        std::thread _contractCacheBuildThread;
        // Packing interval
        static const int _kBuildInterval;
        // Transaction expiration interval
        static const time_t _kTxExpireInterval;
        // Packaging threshold
        static const int _kBuildThreshold;
        // cache of contract info
        std::map<std::string, std::pair<nlohmann::json, uint64_t>> _contractInfoCache;
    // The mutex of _contractInfoCache
        std::shared_mutex _contractInfoCacheMutex;

        constexpr static int _precedingContractBlockLookupInterval = 10;
        constexpr static int _precedingContractBlockLookupStartTime = 3 * 1000000;
        constexpr static int _precedingContractBlockLookupEndtime = _precedingContractBlockLookupInterval * 1000000;
        constexpr static int _contractBlockPackingTime = _precedingContractBlockLookupInterval * 1000000;
        std::string _preContractBlockHash;
        std::map<std::string, std::pair<uint64_t, std::set<std::string> >> _dirtyContractMap;
        std::shared_mutex _dirtyContractMapMutex;

        std::mutex _threadMutex;
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
        int AddCache(const CTransaction& transaction, const uint64_t& height, const std::vector<std::string> dirtyContract);

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
//        bool CheckConflict(const CTransaction& transaction);

        void AddContractInfoCache(const std::string& transactionHash, const nlohmann::json& jTxInfo, const uint64_t& txtime);
        int GetContractInfoCache(const std::string& transactionHash, nlohmann::json& jTxInfo);

        std::string GetContractPrevBlockHash();

        void ContractBlockNotify(const std::string& blockHash);
        std::string _SeekContractPreHash();

        std::string _SeekContractPreHashByNode(const std::vector<std::string> &sendNodeIds);

        std::string GetAndUpdateContractPreHash(const std::string &contractAddress, const std::string &transactionHash,
                                                std::map<std::string, std::string> &contractPreHashCache);
        void SetDirtyContractMap(const std::string& transactionHash, const std::set<std::string>& dirtyContract);
        bool GetDirtyContractMap(const std::string& transactionHash, std::set<std::string>& dirtyContract);
        void removeExpiredEntriesFromDirtyContractMap();
        static bool HasContractPackingPermission(const std::string& addr, uint64_t transactionHeight, uint64_t time);

        std::pair<int, std::string> _ExecuteContracts(const std::map<std::string, CTransaction>& dependentContractTxMap);

        bool RemoveContractsCacheTransaction(const std::map<std::string, CTransaction>& contractTxs);

        bool RemoveContractInfoCacheTransaction(const std::map<std::string, CTransaction>& contractTxs);

        void ProcessContract();

        int HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata);
    private:
        /**
         * @brief       Threading functions
         */
        void _TransactionCacheProcessingFunc();
//        void _ContractCacheProcessingFunc();

        int _AddContractInfoCache(const CTransaction &transaction,
                                  std::map<std::string, std::string> &contractPreHashCache, ContractDataCache* contractDataCache);
        static bool _HasContractPackingPermission(uint64_t transactionHeight, uint64_t time);

        std::atomic<bool> _threadRun=true;

        int _GetContractTxPreHash(const std::list<CTransaction>& txs, std::list<std::pair<std::string, std::string>>& contractTxPreHashList);

        bool _VerifyDirtyContract(const std::string &transactionHash, const vector<string> &calledContract);

};

static int GetContractPrehashFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);
void SendSeekContractPreHashReq(const std::string &nodeId, const std::string &msgId);
void SendSeekContractPreHashAck(SeekContractPreHashAck& ack,const std::string &nodeId, const std::string &msgId);

int HandleSeekContractPreHashReq(const std::shared_ptr<SeekContractPreHashReq> &msg, const MsgData &msgdata);
int HandleSeekContractPreHashAck(const std::shared_ptr<SeekContractPreHashAck> &msg, const MsgData &msgdata);

int HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata);





class TransactionCache_V33_1
{
    private:
        typedef std::list<TransactionEntity_V33_1>::const_iterator txEntitiesIter;
        typedef std::map<uint64_t, std::list<TransactionEntity_V33_1>>::iterator cacheIter;

    private:
    //    friend std::string PrintCache(int where);
        // Transaction container
        std::map<uint64_t ,std::list<TransactionEntity_V33_1>> _cache;
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
        TransactionCache_V33_1();
        ~TransactionCache_V33_1() = default;
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
        void GetCache(std::map<uint64_t, std::list<TransactionEntity_V33_1>>& cache); 

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
        std::list<txEntitiesIter>  _GetNeededCache(const std::list<TransactionEntity_V33_1>& txs);

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
        bool _RemoveProcessedTransaction(const  std::list<txEntitiesIter>& txEntitiesIter, const bool buildSuccess, std::list<TransactionEntity_V33_1>& txEntities);


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
                           

int _HandleSeekContractPreHashReq(const std::shared_ptr<newSeekContractPreHashReq> &msg, const MsgData &msgdata);
int _HandleSeekContractPreHashAck(const std::shared_ptr<newSeekContractPreHashAck> &msg, const MsgData &msgdata);

int _newSeekContractPreHash(const std::list<std::pair<std::string, std::string>> &contractTxPreHashList);

#endif
