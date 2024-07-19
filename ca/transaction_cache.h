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

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "proto/block.pb.h"
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
        typedef std::map<uint64_t, std::list<TransactionEntity>>::iterator cacheIter;

    private:
        // Transaction container
        std::vector<TransactionEntity> _transactionCache;
        std::list<CTransaction> buildTxs;
        std::mutex _buildTxsMutex;

        //std::map<uint64_t ,std::list<CTransaction>> _transactionCache;
        // The mutex of the transaction container
        std::mutex _transactionCacheMutex;
        // Contract Transaction container
        std::vector<TransactionEntity> _contractCache;
        // The mutex of the Contract transaction container
        std::mutex _contractCacheMutex;
        // Condition variables are used to package blocks
        std::condition_variable _blockBuilder;
        std::condition_variable _contractPreBlockWaiter;
        // Timers are used for packing at specific time intervals
        CTimer _buildTimer;
        // Thread variables are used for packaging
        std::thread _transactionCacheBuildThread;
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

        std::string _preContractBlockHash;
         std::map<std::string, std::pair<uint64_t, std::set<std::string> >> _dirtyContractMap;
        std::shared_mutex _dirtyContractMapMutex;
        std::atomic<bool> _threadRun = true;
    
        std::mutex _threadMutex;
    public:
        TransactionCache();
        ~TransactionCache() = default;

        /**
         * @brief
         * @param       height:
         * @return      void 
         */
        void setBlockCount(uint64_t height);

        /**
         * @brief
         * @return      uint64_t 
         */
        uint64_t getBlockCount();
        /**
         * @brief       Add a cache
         * 
         * @param       transaction: 
         * @param       sendTxMsg: 
         * @return      int 
         */
        int AddCache(CTransaction& transaction, const std::shared_ptr<TxMsgReq>& msg);

        /**
         * @brief       Start the packaging block building thread 
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

        void AddContractInfoCache(const std::string& transactionHash, const nlohmann::json& jTxInfo, const uint64_t& txtime);

        /**
         * @brief       Get the contract json information
         * 
         * @param       transaction:
         * @param       jTxInfo:  
         * @return      int 
         */

        int GetContractInfoCache(const std::string& transactionHash, nlohmann::json& jTxInfo);
        /**
         * @brief       

         * @return      string 
         */
        std::string GetContractPrevBlockHash();
        /**
         * @brief
         * @param       blockHash:       
         */
        void ContractBlockNotify(const std::string& blockHash);
        /**
         * @brief
         *
         * @param       contractAddress
         * @param       transactionHash
         * @param       contractPreHashCache
         * @return      string
         */
        std::string GetAndUpdateContractPreHash(const std::string &contractAddress, const std::string &transactionHash,
                                   std::map<std::string, std::string> &contractPreHashCache);
        /**
         * @brief
         *
         * @param       transactionHash
         * @param       dirtyContract
         */
        void SetDirtyContractMap(const std::string& transactionHash, const std::set<std::string>& dirtyContract);
        bool GetDirtyContractMap(const std::string& transactionHash, std::set<std::string>& dirtyContract);
 
        void removeExpiredEntriesFromDirtyContractMap();

        /**
         * @brief
         *
         * @param       addr
         * @param       transactionHeight
         * @param       time
         * @return      true
         * @return      false
         */
        static bool HasContractPackingPermission(const std::string& addr, uint64_t transactionHeight, uint64_t time);
        
        std::pair<int, std::string> _ExecuteContracts(const std::map<std::string, CTransaction> &dependentContractTxMap,
                                                      int64_t blockNumber);

        bool RemoveContractsCacheTransaction(const std::map<std::string, CTransaction>& contractTxs);

        bool RemoveContractInfoCacheTransaction(const std::map<std::string, CTransaction>& contractTxs);

        void ProcessContract(int64_t topTransactionHeight);

        int HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata);

    private:

        void _AddBuildTx(const CTransaction &transaction);
        const std::list<CTransaction>& _GetBuildTxs();
        void ClearBuildTxs();
        
        int GetBuildBlockHeight(std::vector<TransactionEntity>& txcache);
        /**
         * @brief       Threading functions
         */
        void _TransactionCacheProcessingFunc();
        /**
         * @brief
         * @param       transaction
         * @param       contractPreHashCache
         * @param       contractDataCache
         * @return      int
         */
        int _AddContractInfoCache(const CTransaction &transaction,
                                  std::map<std::string, std::string> &contractPreHashCache,
                                  ContractDataCache *contractDataCache, int64_t blockNumber);
        /**
         * @brief
         *
         * @param       transactionHash
         * @param       calledContract
         * @return      true
         * @return      false
         */
        bool _VerifyDirtyContract(const std::string &transactionHash, const std::vector<std::string> &calledContract);
        int _GetContractTxPreHash(const std::list<CTransaction>& txs, std::list<std::pair<std::string, std::string>>& contractTxPreHashList);

    	int DealContractTransaction(const std::vector<TxMsgReq> &txMsgReqs);
};
/**
 * @brief
 *
 * @param       num
 * @param       selfNodeHeight
 * @param       pledgeAddr
 * @param       sendNodeIds
 * @return      int
*/
static int GetContractPrehashFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);
/**
 * @brief
 *
 * @param       msg
 * @param       msgData
 * @return      int
*/  
int HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgData);


int _HandleSeekContractPreHashReq(const std::shared_ptr<newSeekContractPreHashReq> &msg, const MsgData &msgdata);
int _HandleSeekContractPreHashAck(const std::shared_ptr<newSeekContractPreHashAck> &msg, const MsgData &msgdata);
int _newSeekContractPreHash(const std::list<std::pair<std::string, std::string>> &contractTxPreHashList);

#endif
