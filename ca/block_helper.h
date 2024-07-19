/**
 * *****************************************************************************
 * @file        block_helper.h
 * @brief       
 * @date        2023-09-25
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _CA_BLOCKHELPER_H
#define _CA_BLOCKHELPER_H

#include <map>
#include <stack>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <atomic>
#include <thread>
#include <cstdint>
#include <condition_variable>

#include "ca/block_compare.h"
#include "global.h"

namespace compator
{
    struct BlockTimeAscending
    {
        bool operator()(const CBlock &a, const CBlock &b) const
        {
            if(a.height() == b.height()) return a.time() < b.time();
            else if(a.height() < b.height()) return true;
            return false;
        }
    };
}
struct MissingBlock
{
	std::string _hash;
	uint64_t _time;
    std::shared_ptr<bool> _txOrBlock; //  0 is block hash  1 is utxo
    std::shared_ptr<uint64_t> _triggerCount;
    
    MissingBlock(const std::string& hash, const uint64_t& time, const bool& txOrBlock)
    {
        _hash = hash;
        _time = time;
        _txOrBlock = std::make_shared<bool>(txOrBlock);
        _triggerCount = std::make_shared<uint64_t>(0);
    }
	bool operator<(const struct MissingBlock & right)const   // Overload<operator
	{
		if (this->_hash == right._hash)
		{
			return false;
		}
		else
		{
			return _time < right._time; // Small top reactor
		}
	}
};

enum ContractPreHashStatus {
    Normal,
    DbBlockException,
    MemBlockException,
    Waiting,
    Err
};

enum class doubleSpendType
{
    repeatenDoubleSpend,
    newDoubleSpend,
    oldDoubleSpend,
    invalidDoubleSpend,
    err
};

class BlockManager {
public:
    using Timestamp = std::chrono::steady_clock::time_point;

    bool addBlock(const std::string& blockHash) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto result = blocks_.emplace(blockHash, now);
        return result.second; // true if the block was added, false if it already existed
    }

    bool hasBlock(const std::string& blockHash) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return blocks_.find(blockHash) != blocks_.end();
    }

    void removeExpiredBlocks(std::chrono::seconds timeout) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = blocks_.begin(); it != blocks_.end(); ) {
            if (now - it->second > timeout) {
                it = blocks_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<std::string, Timestamp> blocks_;
    std::shared_mutex mutex_;
};

/**
 * @brief       
 */
class BlockHelper
{
    public:
        BlockHelper();

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       blockStatus: 
         * @return      int 
         */
        int VerifyFlowedBlock(const CBlock& block, BlockStatus* blockStatus = nullptr,BlockMsg *msg = nullptr);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       saveType: 
         * @param       obtainMean: 
         * @return      int 
         */
        int SaveBlock(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean);

        /**
         * @brief       Set the Missing Prehash object
         */
        void SetMissingPrehash();
        
        /**
         * @brief       
         */
        void ResetMissingPrehash();

        /**
         * @brief       
         * 
         * @param       utxo: 
         */
        void PushMissUTXO(const std::string& utxo);  

        /**
         * @brief       
         */
        void PopMissUTXO();

        /**
         * @brief       
         * 
         * @param       chainHeight: 
         * @return      true 
         * @return      false 
         */
        static bool ObtainChainHeight(uint64_t& chainHeight);

        /**
         * @brief       
         */
        void Process(); 

        /**
         * @brief       
         */ 
        void SeekBlockThread();

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       status: 
         */
        void AddBroadcastBlock(const CBlock& block, bool status = false);

        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         * @param       type: 
         */
        void AddSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData, global::ca::SaveType type);
        
        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         * @param       type: 
         */
        void AddFastSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData, global::ca::SaveType type);
        
        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         */
        void AddRollbackBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData);
        
        /**
         * @brief       
         * 
         * @param       block: 
         */
        void AddMissingBlock(const CBlock& block);
        
        /**
         * @brief       
         * 
         * @param       seekBlocks: 
         */
        void AddSeekBlock(std::vector<std::pair<CBlock,std::string>>& seekBlocks);
        
        /**
         * @brief       
         * 
         * @param       utxo: 
         * @param       shelfHeight: 
         * @param       blockHash: 
         * @return      int 
         */
        int RollbackPreviousBlocks(const std::string utxo, uint64_t shelfHeight, const std::string blockHash);
        
        /**
         * @brief       
         * 
         * @return      true 
         * @return      false 
         */
        bool GetwhetherRunSendBlockByUtxoReq() { return _whetherRunSendBlockByUtxoReq; };
        
        /**
         * @brief       
         * 
         * @param       flag: 
         */
        void SetwhetherRunSendBlockByUtxoReq(bool flag) {_whetherRunSendBlockByUtxoReq = flag;};

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       tx: 
         * @param       missingUtxo: 
         * @return      std::pair<doubleSpendType, CBlock> 
         */
        std::pair<doubleSpendType, CBlock> DealDoubleSpend(const CBlock& block, const CTransaction& tx, const std::string& missingUtxo);
        
        /**
         * @brief       
         */
        void RollbackTest();

        /**
         * @brief       
         * 
         * @param       oldBlock: 
         * @param       newBlock: 
         */
        void MakeTxStatusMsg(const CBlock &oldBlock, const CBlock &newBlock);
        
        /**
         * @brief       
         * 
         * @param       doubleSpendInfo: 
         * @param       block: 
         */
        void CheckDoubleBlooming(const std::pair<doubleSpendType, CBlock>& doubleSpendInfo, const CBlock &block);
        
        /**
         * @brief       
         */
        void StopSaveBlock() { _stopBlocking = false; }

    private:

        /**
         * @brief       
         * 
         * @param       contractAddr: 
         * @param       MEMContractPreHash: 
         * @param       blockTime: 
         * @param       DBBlockHash: 
         */
        ContractPreHashStatus CheckContractPreHashStatus(const std::string& contractAddr, const std::string& MEMContractPreHash, const uint64_t blockTime, std::string& DBBlockHash);
        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreHash: 
         */
        int checkContractBlock(const CBlock& block, std::string& contractTxPreHash);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreHash: 
         */
        int checkContractBlockCache(const CBlock& block, const std::string& contractTxPreHash);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       selfNodeHeight: 
         */
        void AddPendingBlock(const CBlock& block);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreHash: 
         */
        void AddContractBlock(const CBlock& block, const std::string& contractTxPreHash);

       /**
        * @brief       
        * 
        * @param       where: 
        * @return      std::string 
        */
        friend std::string PrintCache(int where);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       ownblockHeight: 
         * @return      true 
         * @return      false 
         */
        bool VerifyHeight(const CBlock& block, uint64_t ownblockHeight);

        /**
         * @brief       
         */
        bool GetMissBlock();

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void PostMembershipCancellationProcess(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void PostTransactionProcess(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void PostSaveProcess(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       saveType: 
         * @param       obtainMean: 
         * @return      int 
         */
        int PreSaveProcess(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean);
        
        /**
         * @brief       
         * 
         * @return      int 
         */
        int RollbackBlocks();

        int RollbackContractBlock();

        std::mutex _helperMutex;
        std::mutex _helperMutexLow1;
        std::atomic<bool> _missingPrehash;
        std::mutex _missingUtxosMutex;
        std::stack<std::string> _missingUtxos;

        std::set<CBlock, compator::BlockTimeAscending> _broadcastBlocks; //Polling of blocks to be added after broadcasting
        std::set<CBlock, compator::BlockTimeAscending> _syncBlocks; // Synchronized block polling
        std::set<CBlock, compator::BlockTimeAscending> _fastSyncBlocks; //Quickly synchronize the block polling to be added
        std::map<uint64_t, std::set<CBlock, CBlockCompare>> _rollbackBlocks; // Polling of blocks to be rolled back
        std::map<uint64_t, std::multimap<std::string, CBlock>> _pendingBlocks; // Because there is no block trigger waiting to be added in the previous hash
        std::map<std::string, std::pair<uint64_t, CBlock>> _hashPendingBlocks; //  Polling for blocks found by the find block protocol and blocks not found by utxo
        std::vector<CBlock> _utxoMissingBlocks; //Poll the block found by finding the protocol of utxo
        std::mutex _seekMutex;
        std::set<MissingBlock> _missingBlocks; // Wait for the hash polling to trigger the block finding protocol
        std::map<std::string, CBlock> _doubleSpendBlocks;
        std::map<std::string, std::pair<bool, uint64_t>> _duplicateChecker;
        std::unordered_map<std::string, CBlock> _contractBlocks;
        const static int _kMaxMissingBlockSize = 10;
        const static int _kMaxMissingUxtoSize = 10;
        const static int _kSyncSaveFailTolerance = 2;
        std::atomic<bool> _stopBlocking = true;
        std::atomic<bool> _whetherRunSendBlockByUtxoReq = true;

        uint64_t postCommitCost = 0;
        uint64_t postCommitCount = 0;
};

/**
 * @brief       Get the Utxo Find Node object
 * 
 * @param       num: 
 * @param       selfNodeHeight: 
 * @param       pledgeAddr: 
 * @param       sendNodeIds: 
 * @return      int 
 */
static int GetUtxoFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);

/**
 * @brief       
 * 
 * @param       utxo: 
 * @return      int 
 */
int SendBlockByUtxoReq(const std::string &utxo);




/**
 * @brief       
 * 
 * @param       missingHashs: 
 * @return      int 
 */
int SendBlockByHashReq(const std::map<std::string, bool> &missingHashs);

/**
 * @brief       
 * 
 * @param       seekBlockHash: 
 * @param       contractBlockStr: 
 * @return      int 
 */
int SeekBlockByContractPreHashReq(const std::string &seekBlockHash, std::string& contractBlockStr);




#endif