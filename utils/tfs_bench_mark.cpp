#include "tfs_bench_mark.h"
#include "time_util.h"
#include "magic_singleton.h"
#include "db/db_api.h"
#include "include/logging.h"
#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "json.hpp"
#include <sys/sysinfo.h>

static const double kConversionNumber = 1000000.0;
static const uint64_t kConversionNumberU = 1000000;
static const std::string kBenchmarkFilename = "benchmark.json";
static const std::string kBenchmarkFilename2 = "benchmark2.json";

TFSbenchmark::TFSbenchmark() : _benchmarkSwitch(false), transactionInitiateAmount(0), transactionInitiateHeight(0)
{
    auto memoryCheckThread = std::thread(
            [this]()
            {
                while (_threadFlag)
                {
                    struct sysinfo sysInfo;
                    if (!sysinfo(&sysInfo))
                    {
                        uint64_t memFreeTotal = sysInfo.freeram / 1024 / 1024; //unit MB
                        DEBUGLOG("memory left {} MB could be used", memFreeTotal);
                    }
                    sleep(60);
                }
            }
    );
    memoryCheckThread.detach();
};

void TFSbenchmark::OpenBenchmark()
{
    _benchmarkSwitch = true;
    std::ofstream filestream;
    filestream.open(kBenchmarkFilename, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json initContent = nlohmann::json::array();
    filestream << initContent.dump();
    filestream.close();
}

void TFSbenchmark::OpenBenchmark2()
{
    _benchmarkSwitch2 = true;
    std::ofstream filestream;
    filestream.open(kBenchmarkFilename2, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark2 file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json initContent = nlohmann::json::array();
    filestream << initContent.dump();
    filestream.close();
}

void TFSbenchmark::Clear()
{
    _threadFlag=false;
    if (!_benchmarkSwitch)
    {
        return;
    }
    _benchmarkSwitch = false;
    std::cout << "please wait" << std::endl;
    sleep(5);
    _transactionInitiateMap.clear();
    _transactionInitiateCache.clear();
    transactionVerifyMap.clear();
    agentTransactionReceiveMap.clear();
    transactionSignReceiveMap.clear();
    transactionSignReceiveCache.clear();
    blockContainsTransactionAmountMap.clear();
    blockVerifyMap.clear();
    blockPoolSaveMap.clear();
    transactionInitiateAmount = 0;
    transactionInitiateHeight = 0;
    std::cout << "clear finish" << std::endl;
    _benchmarkSwitch = true;

}
void TFSbenchmark::SetTransactionInitiateBatchSize(uint32_t amount)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    _batchSize = amount;
}

void TFSbenchmark::AddTransactionInitiateMap(uint64_t start, uint64_t end)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_transactionInitiateMapMutex);
    _transactionInitiateMap.push_back({start, end});
    if (_transactionInitiateMap.size() == _batchSize)
    {
        _CaculateTransactionInitiateAmountPerSecond();
    }
    
}

void TFSbenchmark::_CaculateTransactionInitiateAmountPerSecond()
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    if (_transactionInitiateMap.empty())
    {
        return;
    }
    
    uint64_t timeDiffSum = 0;
    for(auto timeRecord : _transactionInitiateMap)
    {
        timeDiffSum = (timeRecord.second - timeRecord.first) + timeDiffSum;
    }

    double transactionInitiatesCostPerTransaction = (double)timeDiffSum / (double)_transactionInitiateMap.size();
    double transactionInitiatesPerSecond = (double)_transactionInitiateMap.size() / ((double)timeDiffSum / kConversionNumber); 
    _transactionInitiateCache[_transactionInitiateMap.front().first] = {transactionInitiatesCostPerTransaction, transactionInitiatesPerSecond};
    _transactionInitiateMap.clear();
}

void TFSbenchmark::ClearTransactionInitiateMap()
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_transactionInitiateMapMutex);
    _transactionInitiateMap.clear();
}

void TFSbenchmark::AddtransactionMemVerifyMap(const std::string& txHash, uint64_t costTime)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
    auto found = transactionVerifyMap.find(txHash);
    if (found == transactionVerifyMap.end())
    {
        transactionVerifyMap[txHash] = verify_time_record();
    }

    auto& record = transactionVerifyMap.at(txHash);
    record.mem_verify_time = costTime;
    record.mem_verify_amount_per_second = (double)1 / ((double)costTime / kConversionNumber);
    if (record.db_verify_time != 0)
    {
        record.totalVerifyTime = record.mem_verify_time + record.db_verify_time;
        record.total_verify_amount_per_second = (double)1 / ((double) record.totalVerifyTime / kConversionNumber);
    }
    
}

void TFSbenchmark::AddtransactionDBVerifyMap(const std::string& txHash, uint64_t costTime)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
    auto found = transactionVerifyMap.find(txHash);
    if (found == transactionVerifyMap.end())
    {
        transactionVerifyMap[txHash] = verify_time_record();
    }

    auto& record = transactionVerifyMap.at(txHash);
    record.db_verify_time = costTime;
    record.db_verify_amount_per_second = (double)1 / ((double)costTime / kConversionNumber);

    if (record.mem_verify_time != 0)
    {
        record.totalVerifyTime = record.mem_verify_time + record.db_verify_time;
        record.total_verify_amount_per_second = (double)1 / ((double) record.totalVerifyTime / kConversionNumber);
    }
}

void TFSbenchmark::AddAgentTransactionReceiveMap(const std::shared_ptr<TxMsgReq> &msg)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
	CTransaction txBenchmarkTmp;
	if (txBenchmarkTmp.ParseFromString(msg->txmsginfo().tx()) && txBenchmarkTmp.verifysign_size() == 0)
	{
        std::lock_guard<std::mutex> lock(agentTransactionReceiveMapMutex);
        auto& txHash = txBenchmarkTmp.hash();
        auto found = agentTransactionReceiveMap.find(txHash);
        if (found != agentTransactionReceiveMap.end())
        {
            return;
        }
        agentTransactionReceiveMap[txHash] = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	}

}

void TFSbenchmark::AddTransactionSignReceiveMap(const std::string& txHash)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);
    auto found = transactionSignReceiveMap.find(txHash);
    if (found == transactionSignReceiveMap.end())
    {
        transactionSignReceiveMap[txHash] = {};
    }
    auto& time_record = transactionSignReceiveMap.at(txHash);
    time_record.push_back(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
}

void TFSbenchmark::CalculateTransactionSignReceivePerSecond(const std::string& txHash, uint64_t composeTime)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);
    auto found = transactionSignReceiveMap.find(txHash);
    if (found == transactionSignReceiveMap.end())
    {
        return;
    }
    auto& timeRecord = transactionSignReceiveMap.at(txHash);
    auto spanTime = composeTime - timeRecord.front();
    transactionSignReceiveCache[txHash] = {spanTime, (double)1 / ((double)spanTime / kConversionNumber)};
}

void TFSbenchmark::AddBlockContainsTransactionAmountMap(const std::string& blockHash, int txAmount)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockContainsTransactionAmountMapMutex);
    blockContainsTransactionAmountMap[blockHash] = txAmount;
}

void TFSbenchmark::AddBlockVerifyMap(const std::string& blockHash, uint64_t costTime)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockVerifyMapMutex);
    auto found = blockVerifyMap.find(blockHash);
    if (found != blockVerifyMap.end())
    {
        return;
    }
    
    blockVerifyMap[blockHash] = {costTime, (double)1 / ((double) costTime / kConversionNumber) };
}

void TFSbenchmark::IncreaseTransactionInitiateAmount()
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    ++transactionInitiateAmount;
    if(transactionInitiateHeight == 0)
    {
        DBReader dBReader;
        uint64_t top = 0;
        if (DBStatus::DB_SUCCESS != dBReader.GetBlockTop(top))
        {
            ERRORLOG("GetBlockTop fail");
        }
        transactionInitiateHeight = top + 1;
    }
}

void TFSbenchmark::PrintTxCount()
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::cout << "there're " << transactionInitiateAmount << 
                " simple transactions hash been initiated since height " << transactionInitiateHeight << std::endl;
}

void TFSbenchmark::AddBlockPoolSaveMapStart(const std::string& blockHash)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
    auto found = blockPoolSaveMap.find(blockHash);
    if (found == blockPoolSaveMap.end())
    {
        blockPoolSaveMap[blockHash] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), 0};
    }
}

void TFSbenchmark::AddBlockPoolSaveMapEnd(const std::string& blockHash)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
    auto found = blockPoolSaveMap.find(blockHash);
    if (found == blockPoolSaveMap.end())
    {
        return;
    }
    auto& record = blockPoolSaveMap.at(blockHash);
    if (record.first == 0)
    {
        return;
    }
    record.second = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
}

void TFSbenchmark::SetBlockPendingTime(uint64_t pendingTime)
{
    if (!_benchmarkSwitch2)
    {
        return;
    }

    _newBlockPendingTime = pendingTime;
}

void TFSbenchmark::SetByTxHash(const std::string& TxHash, void* arg , uint16_t type)
{
    if (!_benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_doHandleTxMutex);
    switch (type)
    {
    case 1:
        _TV[TxHash].StartTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 2:
        _TV[TxHash].Verify_2 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        _TV[TxHash].Verify_3 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        _TV[TxHash].EndTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 5:
        _TV[TxHash].Timeout = true;
        break;
    default:
        break;
    }
}
void TFSbenchmark::SetByBlockHash(const std::string& BlockHash, void* arg , uint16_t type, void* arg2, void* arg3, void* arg4)
{
    if (!_benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_doHandleTxMutex);
    switch (type)
    {
    case 1:
        _BV[BlockHash].Time = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].Verify_4 = *reinterpret_cast<uint64_t*>(arg2);
        _BV[BlockHash].TxNumber = *reinterpret_cast<uint64_t*>(arg3);
        _BV[BlockHash].Hight = *reinterpret_cast<uint64_t*>(arg4);
        break;
    case 2:
        _BV[BlockHash].BroadcastTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        _BV[BlockHash].Verify_5 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        _VN[BlockHash].Hight =  *reinterpret_cast<uint64_t*>(arg);
        _VN[BlockHash].TxNumber =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 5:
        _VN[BlockHash].MemVerifyTime =  *reinterpret_cast<uint64_t*>(arg);
        _VN[BlockHash].TxVerifyTime =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 6:
        _BV[BlockHash].BlockPendingTime = _newBlockPendingTime;
        _BV[BlockHash].BuildBlockTime = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].CheckConflictTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 7:
        _BV[BlockHash].SearchNodeTime = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].totalTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 8:
        _BV[BlockHash].seekPrehashTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    default:
        break;
    }
}

void TFSbenchmark::SetTxHashByBlockHash(const std::string& BlockHash, const std::string& TxHash)
{
    if (!_benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_doHandleTxMutex);
    _BT.insert({BlockHash,TxHash});
}
void TFSbenchmark::PrintBenchmarkSummary(bool exportToFile)
{
    if (!_benchmarkSwitch)
    {
        return;
    }
    
    nlohmann::json benchmarkJson;
    if (exportToFile)
    {
         std::ifstream readfilestream;
        readfilestream.open(kBenchmarkFilename);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmarkJson = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmarkItemJson;
    if (!_transactionInitiateCache.empty())
    {
        std::lock_guard<std::mutex> lock(_transactionInitiateMapMutex);
        double costSum = 0;
        for(auto record : _transactionInitiateCache)
        {
            costSum += record.second.first;
        }
        double transactionTimeCostAverage = costSum / _transactionInitiateCache.size();
        double transactionAmountPerSecond = (double) 1 / (transactionTimeCostAverage / kConversionNumber);
        
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << transactionAmountPerSecond;
            benchmarkItemJson["one-to-one_transactions_can_be_initiated_per_second"] = stream.str();
        }
        else
        {
            std::cout << "one-to-one transactions can be initiated per second: " << transactionAmountPerSecond << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["one-to-one_transactions_can_be_initiated_per_second"] = "";
        }
    }

    if (!transactionVerifyMap.empty())
    {
        std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
        uint64_t memCostSum = 0;
        uint64_t dbCostSum = 0;
        uint64_t totalCostSum = 0;
        int skipCount = 0;
        for(auto record : transactionVerifyMap)
        {
            if (record.second.mem_verify_time == 0 
                || record.second.db_verify_time == 0
                || record.second.totalVerifyTime == 0)
            {
                skipCount++;
                continue;
            }
            
            memCostSum += record.second.mem_verify_time;
            dbCostSum += record.second.db_verify_time;
            totalCostSum += record.second.totalVerifyTime;
        }
        double memCostAverage = (double)memCostSum / (double)(transactionVerifyMap.size() - skipCount);
        double dbCostAverage = (double)dbCostSum / (double)(transactionVerifyMap.size() - skipCount);
        double totalCostAverage = (double)totalCostSum / (double)(transactionVerifyMap.size() - skipCount);
        double memVerifyPerSecond = (double) 1 / (memCostAverage / kConversionNumber);
        double dbVerifyPerSecond = (double) 1 / (dbCostAverage / kConversionNumber);
        double totalVerifyPerSecond = (double) 1 / (totalCostAverage / kConversionNumber);
        if (exportToFile)
        {
            std::ostringstream totalStream;
            totalStream << totalVerifyPerSecond;
            std::ostringstream memStream;
            memStream << memVerifyPerSecond;
            std::ostringstream dbStream;
            dbStream << dbVerifyPerSecond;
            benchmarkItemJson["Number_of_verifiable_transactions_per_second"] = totalStream.str();
            benchmarkItemJson["Number_of_verifiable_transactions_per_second_in_memory"] = memStream.str();
            benchmarkItemJson["Number_of_verifiable_transactions_per_second_in_db"] = dbStream.str();
        }
        else
        {
            std::cout << "Number of verifiable transactions per second: " << totalVerifyPerSecond 
                  << " (mem verify: " << memVerifyPerSecond << " db verify: " << dbVerifyPerSecond << ")" << std::endl;
        }

    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["Number_of_verifiable_transactions_per_second"] = "";
            benchmarkItemJson["Number_of_verifiable_transactions_per_second_in_memory"] = "";
            benchmarkItemJson["Number_of_verifiable_transactions_per_second_in_db"] = "";
        }
    }

    if (!agentTransactionReceiveMap.empty())
    {
        std::lock_guard<std::mutex> lock(agentTransactionReceiveMapMutex);
        std::map<uint64_t, uint64_t> hitCache;
        for(auto record : agentTransactionReceiveMap)
        {
            uint64_t time = record.second / kConversionNumberU;
            auto found = hitCache.find(time);
            if (found == hitCache.end())
            {
                hitCache[time] = 1;
            }
            auto& hitTimes = found->second;
            hitTimes += 1;
        }

        uint64_t maxHitTimes = 0;
        for(auto hits : hitCache)
        {
            if (hits.second > maxHitTimes)
            {
                maxHitTimes = hits.second;
            }
        }
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << maxHitTimes;
            benchmarkItemJson["Number_of_transactions_per_second"] = stream.str();
        }
        else
        {
            std::cout << "Number of transactions per second from internet: " << maxHitTimes << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["Number_of_transactions_per_second"] = "";
        }
    }

    if(!transactionSignReceiveMap.empty())
    {
        std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);

        uint64_t transactionComposeCostSum = 0;
        for(auto record : transactionSignReceiveCache)
        {
            transactionComposeCostSum += record.second.first;
        }

        double transactionComposeCostAverage = (double)transactionComposeCostSum / (double)transactionSignReceiveCache.size();
        double transactionComposeAmoutPerSecond = (double)1 / (transactionComposeCostAverage / kConversionNumber);
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << transactionComposeAmoutPerSecond;
            benchmarkItemJson["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = stream.str();
        }
        else
        {
            std::cout << "signature per second can be collected from the network and combined into a complete transaction body: " << transactionComposeAmoutPerSecond << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = "";
        }
    }

    if (!blockContainsTransactionAmountMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockContainsTransactionAmountMapMutex);
        uint64_t txAmountSum = 0;
        for(auto record : blockContainsTransactionAmountMap)
        {
            txAmountSum += record.second;
        }
        double txAmountAverage = (double)txAmountSum / (double)blockContainsTransactionAmountMap.size();
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << txAmountAverage;
            benchmarkItemJson["transaction_count_can_be_packed_into_a_full_block_per_second"] = stream.str();
        }
        else
        {
            std::cout << "transaction count can be packed into a full block per second: " << txAmountAverage << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["transaction_count_can_be_packed_into_a_full_block_per_second"] = "";
        }
    }

    if (!blockVerifyMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockVerifyMapMutex);
        uint64_t blockVerifyCostSum = 0;
        for(auto record : blockVerifyMap)
        {
            blockVerifyCostSum += record.second.first;
        }
        double blockVerifyCostAverage = (double)blockVerifyCostSum / (double)blockVerifyMap.size();
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << blockVerifyCostAverage;
            benchmarkItemJson["Block_validation_time_in_the_block_pool"] = stream.str();
        }
        else
        {
            std::cout << "Block validation time in the block pool: " << blockVerifyCostAverage << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["Block_validation_time_in_the_block_pool"] = "";
        }
    }
    
    if (!blockPoolSaveMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
        uint64_t blockSaveTimeSum = 0;
        int fail_count = 0;
        for(auto record : blockPoolSaveMap)
        {
            if (record.second.second <= record.second.first)
            {
                fail_count++;
                continue;
            }
            
            blockSaveTimeSum += (record.second.second - record.second.first);
        }
        double blockSaveTimeAverage = (double)blockSaveTimeSum / (double)(blockPoolSaveMap.size() - fail_count);
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << blockSaveTimeAverage;
            benchmarkItemJson["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = stream.str();
        }
        else
        {
            std::cout << "Time for blocks in the block pool to be stored in the database: " << blockSaveTimeAverage << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = "";
        }
    }

    if (exportToFile)
    {
        std::ofstream filestream;
        filestream.open(kBenchmarkFilename, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        benchmarkJson.push_back(benchmarkItemJson);
        filestream << benchmarkJson.dump();
        filestream.close();
    } 
    return ;

}

void TFSbenchmark::PrintBenchmarkSummary_DoHandleTx(bool exportToFile)
{
    if (!_benchmarkSwitch2)
    {
        return;
    }
    
    nlohmann::json benchmarkJson;
    if (exportToFile)
    {
         std::ifstream readfilestream;
        readfilestream.open(kBenchmarkFilename2);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmarkJson = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmarkItemJson;
    std::map<uint64_t, std::pair<uint64_t, double>> HT;
    std::map<uint64_t, uint64_t> HS;
    
    if(!_BV.empty())
    {
        std::lock_guard<std::mutex> lock(_doHandleTxMutex);
        for(auto& it : _BV)
        {
            benchmarkItemJson["BlockHash"] = it.first.substr(0,6);
            auto targetBegin = _BT.lower_bound(it.first);
            auto targetEnd = _BT.upper_bound(it.first);
            uint64_t totalVerifyTime = 0;
            uint64_t TxTimeMin = it.second.Time;
            
            uint64_t TxAverageStartTime = 0;
            for (; targetBegin != targetEnd ; targetBegin++)
            {
                if(_TV.find(targetBegin->second) != _TV.end())
                {
                    HS[it.second.Hight]++;
                    auto &tx = _TV[targetBegin->second];
                    if(TxTimeMin > tx.StartTime) TxTimeMin = tx.StartTime;
                    
                    TxAverageStartTime += tx.StartTime;
                    totalVerifyTime = totalVerifyTime + tx.Verify_2 + tx.Verify_3;
                }
            }
            benchmarkItemJson["BlockTime"] = it.second.Time / 1000000;
            benchmarkItemJson["BlockHight"] = it.second.Hight;
            benchmarkItemJson["BlockTxNumber"] = it.second.TxNumber;
            benchmarkItemJson["TxAverageCompositionTime"] = 0;
            benchmarkItemJson["TxVerifyTime_2345"] = (double)totalVerifyTime / 1000000;
            benchmarkItemJson["TxVerifyTime_5"] = (double)it.second.Verify_5 / 1000000;
            //Time to successful block construction
            benchmarkItemJson["BuildBlockSuccessTime"] = (double)(it.second.Time - TxTimeMin) / 1000000;
            benchmarkItemJson["BuildBlockSuccessAverageTime"] = (double)(it.second.Time - (TxAverageStartTime / it.second.TxNumber)) / 1000000;
            //The time to build into a block and the time to start broadcasting a SAVE message for a successful block flow
            benchmarkItemJson["BroadcastSaveBlockTime"] = (double)(it.second.BroadcastTime - it.second.Time) / 1000000;
            //Time to wait for block creation
            benchmarkItemJson["BlockPendingTime"] = (double)(it.second.BlockPendingTime - (TxAverageStartTime / it.second.TxNumber)) / 1000000;
            //Time from creation of block to start of DoHandleBlock
            benchmarkItemJson["BuildBlockTime"] = (double)it.second.BuildBlockTime / 1000000;
            //Time to find broadcast node
            benchmarkItemJson["SearchNodeTime"] = (double)it.second.SearchNodeTime / 1000000;
            //Time of the whole process
            benchmarkItemJson["totalTime"] = (double)(it.second.totalTime - (TxAverageStartTime / it.second.TxNumber)) / 1000000;

            benchmarkItemJson["seekPrehashTime"] = (double)(it.second.seekPrehashTime - TxTimeMin) / 1000000;
            benchmarkItemJson["CheckConflictTime"] = (double)it.second.CheckConflictTime / 1000000;
            
            HT[it.second.Hight].second += (double)(it.second.Time - TxTimeMin) / 1000000;
            HT[it.second.Hight].first += it.second.TxNumber;
            benchmarkJson.push_back(benchmarkItemJson);
        }
        int i = 0;
        for(auto& it : HT)
        {
            benchmarkJson.at(i)["Hight"] = it.first;
            benchmarkJson.at(i)["HightTxNumber"] = it.second.first;
            benchmarkJson.at(i)["HighBuildTime"] = it.second.second;
            i++;
        }

        uint64_t TxSuccessTotal = 0;
        i = 0;
        for(auto& it : HS)
        {
            benchmarkJson.at(i)["TxHight"] = it.first;
            benchmarkJson.at(i)["TxNumber"] = it.second;
            TxSuccessTotal += it.second;
            i++;
        }
        benchmarkJson.at(0)["TxTotal"] = _TV.size();
        benchmarkJson.at(0)["TxSuccessTotal"] = TxSuccessTotal;
        benchmarkJson.at(0)["TxFailTotal"] = _TV.size() - TxSuccessTotal;
    }
    else
    {
        if (exportToFile)
        {
            benchmarkItemJson["BlockHash"] = "";
            benchmarkItemJson["TxNumber"] = "";
            benchmarkItemJson["Hight"] = "";
            benchmarkItemJson["TxAverageCompositionTime"] = "";
            benchmarkItemJson["TxVerifyTime_2345"] = "";
            benchmarkItemJson["BuildBlockSuccessTime"] = "";
            benchmarkItemJson["BuildBlockBroadcastTime"] = "";
            benchmarkItemJson["NumberFailTx"] = "";
            benchmarkJson.push_back(benchmarkItemJson);
            benchmarkItemJson["BlockHash"] = "1";
            benchmarkItemJson["TxNumber"] = "2";
            benchmarkItemJson["Hight"] = "3";
            benchmarkItemJson["TxAverageCompositionTime"] = "4";
            benchmarkItemJson["TxVerifyTime_2345"] = "5";
            benchmarkItemJson["BuildBlockSuccessTime"] = "6";
            benchmarkItemJson["BuildBlockBroadcastTime"] = "7";
            benchmarkItemJson["NumberFailTx"] = "8";
            benchmarkJson.push_back(benchmarkItemJson);
            benchmarkItemJson["BlockHash"] = "11";
            benchmarkItemJson["TxNumber"] = "22";;
            benchmarkItemJson["Hight"] = "33";
            benchmarkItemJson["TxAverageCompositionTime"] = "44";
            benchmarkItemJson["TxVerifyTime_2345"] = "55";
            benchmarkItemJson["BuildBlockSuccessTime"] = "66";
            benchmarkItemJson["BuildBlockBroadcastTime"] = "77";
            benchmarkItemJson["NumberFailTx"] = "88";
            benchmarkJson.push_back(benchmarkItemJson);

            benchmarkItemJson.clear();
            benchmarkItemJson["NumberFailTx"] = 100;
            benchmarkJson.push_back(benchmarkItemJson);

            benchmarkItemJson.clear();
            benchmarkItemJson["NumberFailTx"] = 200;
            benchmarkJson.push_back(benchmarkItemJson);
        }
    }

    if(!_VN.empty())
    {
        benchmarkJson.at(0)["VN_Hight"] = "";
        benchmarkJson.at(0)["VN_TxNumber"] = "";
        benchmarkJson.at(0)["VN_MemVerifyTime"] = "";
        benchmarkJson.at(0)["VN_TxVerifyTime"] = "";

        for(auto& it : _VN)
        {
            benchmarkItemJson.clear();
            benchmarkItemJson["VN_Hight"] = it.second.Hight;
            benchmarkItemJson["VN_TxNumber"] = it.second.TxNumber;
            benchmarkItemJson["VN_MemVerifyTime"] = (double)it.second.MemVerifyTime / 1000000;
            benchmarkItemJson["VN_TxVerifyTime"] = (double)it.second.TxVerifyTime / 1000000;

            benchmarkJson.push_back(benchmarkItemJson);
        }


    }
    if (exportToFile)
    {
        std::ofstream filestream;
        filestream.open(kBenchmarkFilename2, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        filestream << benchmarkJson.dump();
        filestream.close();
    }
    _TV.clear();
    _BV.clear();
    _BT.clear();
    _VN.clear();
    return ;
}
