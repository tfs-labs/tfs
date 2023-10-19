/**
 * *****************************************************************************
 * @file        tfs_bench_mark.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef __TFSBENCHMARK_H_
#define __TFSBENCHMARK_H_

#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>

#include "proto/ca_protomsg.pb.h"

/**
 * @brief       
 * 
 */
class TFSbenchmark
{
public:
    TFSbenchmark();
    /**
     * @brief       
     * 
     */
    void OpenBenchmark();

    /**
     * @brief       
     * 
     */
    void OpenBenchmark2();

    /**
     * @brief       
     * 
     */
    void Clear();

    /**
     * @brief       Set the Transaction Initiate Batch Size object
     * 
     * @param       amount: 
     */
    void SetTransactionInitiateBatchSize(uint32_t amount);

    /**
     * @brief       
     * 
     * @param       start: 
     * @param       end: 
     */
    void AddTransactionInitiateMap(uint64_t start, uint64_t end);

    /**
     * @brief       
     * 
     */
    void ClearTransactionInitiateMap();

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       costTime: 
     */
    void AddtransactionMemVerifyMap(const std::string& txHash, uint64_t costTime);

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       costTime: 
     */
    void AddtransactionDBVerifyMap(const std::string& txHash, uint64_t costTime);

    /**
     * @brief       
     * 
     * @param       msg: 
     */
    void AddAgentTransactionReceiveMap(const std::shared_ptr<TxMsgReq> &msg);

    /**
     * @brief       
     * 
     * @param       txHash: 
     */
    void AddTransactionSignReceiveMap(const std::string& txHash);

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       composeTime: 
     */
    void CalculateTransactionSignReceivePerSecond(const std::string& txHash, uint64_t composeTime);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     * @param       txAmount: 
     */
    void AddBlockContainsTransactionAmountMap(const std::string& blockHash, int txAmount);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     * @param       costTime: 
     */
    void AddBlockVerifyMap(const std::string& blockHash, uint64_t costTime);

    /**
     * @brief       
     * 
     */
    void IncreaseTransactionInitiateAmount();

    /**
     * @brief       
     * 
     */
    void PrintTxCount();

    /**
     * @brief       
     * 
     * @param       blockHash: 
     */
    void AddBlockPoolSaveMapStart(const std::string& blockHash);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     */
    void AddBlockPoolSaveMapEnd(const std::string& blockHash);

    /**
     * @brief       
     * 
     * @param       exportToFile: 
     */
    void PrintBenchmarkSummary(bool exportToFile);


    /**
     * @brief       Set the Block Pending Time object
     * 
     * @param       pendingTime: 
     */
    void SetBlockPendingTime(uint64_t pendingTime);

    /**
     * @brief       Set the By Tx Hash object
     * 
     * @param       TxHash: 
     * @param       arg: 
     * @param       type: 
     */
    void SetByTxHash(const std::string& TxHash, void* arg, uint16_t type);

    /**
     * @brief       Set the By Block Hash object
     * 
     * @param       BlockHash: 
     * @param       arg: 
     * @param       type: 
     * @param       arg2: 
     * @param       arg3: 
     * @param       arg4: 
     */
    void SetByBlockHash(const std::string& BlockHash, void* arg, uint16_t type, void* arg2 = nullptr, void* arg3 = nullptr, void* arg4 = nullptr);
    
    /**
     * @brief       Set the Tx Hash By Block Hash object
     * 
     * @param       BlockHash: 
     * @param       TxHash: 
     */
    void SetTxHashByBlockHash(const std::string& BlockHash, const std::string& TxHash);

    /**
     * @brief       
     * 
     * @param       exportToFile: 
     */
    void PrintBenchmarkSummary_DoHandleTx(bool exportToFile);

private:
    bool _benchmarkSwitch;
    bool _benchmarkSwitch2{false};
    std::mutex _transactionInitiateMapMutex;
    uint32_t _batchSize;
    std::vector<std::pair<uint64_t, uint64_t>> _transactionInitiateMap;
    std::map<uint64_t, std::pair<double, double>> _transactionInitiateCache;

    /**
     * @brief       
     * 
     */
    void _CaculateTransactionInitiateAmountPerSecond();
    std::atomic<bool> _threadFlag=true;

    struct verify_time_record
    {
        verify_time_record() : mem_verify_time(0), db_verify_time(0){};
        uint64_t mem_verify_time;
        double mem_verify_amount_per_second;
        uint64_t db_verify_time;
        double db_verify_amount_per_second;
        uint64_t totalVerifyTime;
        double total_verify_amount_per_second;
    };
    // friend std::string PrintCache(int where);
    std::mutex transactionVerifyMapMutex;
    std::map<std::string, verify_time_record> transactionVerifyMap;

    std::mutex agentTransactionReceiveMapMutex;
    std::map<std::string, uint64_t> agentTransactionReceiveMap;

    std::mutex transactionSignReceiveMapMutex;
    std::map<std::string, std::vector<uint64_t>> transactionSignReceiveMap;
    std::map<std::string, std::pair<uint64_t, double>> transactionSignReceiveCache;

    std::mutex blockContainsTransactionAmountMapMutex;
    std::map<std::string, int> blockContainsTransactionAmountMap;

    std::mutex blockVerifyMapMutex;
    std::map<std::string, std::pair<uint64_t, double>> blockVerifyMap;

    std::atomic_uint64_t transactionInitiateAmount;
    std::atomic_uint64_t transactionInitiateHeight;

    std::mutex blockPoolSaveMapMutex;
    std::map<std::string, std::pair<uint64_t, uint64_t>> blockPoolSaveMap;

    
    struct TxVerify{
        TxVerify(): Verify_2(0), Verify_3(0), Timeout(false){};

        uint64_t StartTime; 

        uint64_t Verify_2;
        uint64_t Verify_3;

        uint64_t EndTime; 

        bool Timeout;   
    };
    struct BlockVerify{
        BlockVerify(): Verify_4(0), Verify_5(0){};

        uint64_t Time;
        uint64_t BroadcastTime;

        uint64_t Verify_4;
        uint64_t Verify_5;

        uint64_t TxNumber;
        uint64_t Hight;

        uint64_t BlockPendingTime;
        uint64_t BuildBlockTime;
        uint64_t CheckConflictTime;
        uint64_t SearchNodeTime;
        uint64_t totalTime;

        uint64_t seekPrehashTime;
    };
    struct ValidateNode
    {
        uint64_t Hight;
        uint64_t TxNumber;
        uint64_t MemVerifyTime;
        uint64_t TxVerifyTime;
    };
    std::mutex _doHandleTxMutex;
    std::map<std::string,TxVerify> _TV;
    std::multimap<std::string,std::string> _BT;
    std::map<std::string,BlockVerify> _BV;
    std::map<std::string,ValidateNode> _VN;
    uint64_t _newBlockPendingTime;

};

#endif