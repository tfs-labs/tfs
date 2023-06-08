#include "TFSbenchmark.h"
#include "time_util.h"
#include "MagicSingleton.h"
#include "db/db_api.h"
#include "include/logging.h"
#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "json.hpp"
#include <sys/sysinfo.h>

static const double conversion_number = 1000000.0;
static const uint64_t conversion_number_u = 1000000;
static const std::string benchmark_filename = "benchmark.json";
static const std::string benchmark_filename2 = "benchmark2.json";

TFSbenchmark::TFSbenchmark() : benchmarkSwitch(false), transactionInitiateAmount(0), transactionInitiateHeight(0)
{
    auto memory_check_thread = std::thread(
            [this]()
            {
                while (true)
                {
                    struct sysinfo sys_info;
                    if (!sysinfo(&sys_info))
                    {
                        uint64_t mem_free_total = sys_info.freeram / 1024 / 1024; //unit MB
                        DEBUGLOG("memory left {} MB could be used", mem_free_total);
                    }
                    sleep(60);
                }
            }
    );
    memory_check_thread.detach();
};

void TFSbenchmark::OpenBenchmark()
{
    benchmarkSwitch = true;
    std::ofstream filestream;
    filestream.open(benchmark_filename, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json init_content = nlohmann::json::array();
    filestream << init_content.dump();
    filestream.close();
}

void TFSbenchmark::OpenBenchmark2()
{
    benchmarkSwitch2 = true;
    std::ofstream filestream;
    filestream.open(benchmark_filename2, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark2 file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json init_content = nlohmann::json::array();
    filestream << init_content.dump();
    filestream.close();
}

void TFSbenchmark::Clear()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    benchmarkSwitch = false;
    std::cout << "please wait" << std::endl;
    sleep(5);
    transactionInitiateMap.clear();
    transactionInitiateCache.clear();
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
    benchmarkSwitch = true;

}
void TFSbenchmark::SetTransactionInitiateBatchSize(uint32_t amount)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    batchSize = amount;
}

void TFSbenchmark::AddTransactionInitiateMap(uint64_t start, uint64_t end)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
    transactionInitiateMap.push_back({start, end});
    if (transactionInitiateMap.size() == batchSize)
    {
        CaculateTransactionInitiateAmountPerSecond();
    }
    
}

void TFSbenchmark::CaculateTransactionInitiateAmountPerSecond()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    if (transactionInitiateMap.empty())
    {
        return;
    }
    
    uint64_t time_diff_sum = 0;
    for(auto time_record : transactionInitiateMap)
    {
        time_diff_sum = (time_record.second - time_record.first) + time_diff_sum;
    }

    double transactionInitiatesCostPerTransaction = (double)time_diff_sum / (double)transactionInitiateMap.size();
    double transactionInitiatesPerSecond = (double)transactionInitiateMap.size() / ((double)time_diff_sum / conversion_number); 
    transactionInitiateCache[transactionInitiateMap.front().first] = {transactionInitiatesCostPerTransaction, transactionInitiatesPerSecond};
    transactionInitiateMap.clear();
}

void TFSbenchmark::ClearTransactionInitiateMap()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
    transactionInitiateMap.clear();
}

void TFSbenchmark::AddtransactionMemVerifyMap(const std::string& tx_hash, uint64_t cost_time)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
    auto found = transactionVerifyMap.find(tx_hash);
    if (found == transactionVerifyMap.end())
    {
        transactionVerifyMap[tx_hash] = verify_time_record();
    }

    auto& record = transactionVerifyMap.at(tx_hash);
    record.mem_verify_time = cost_time;
    record.mem_verify_amount_per_second = (double)1 / ((double)cost_time / conversion_number);
    if (record.db_verify_time != 0)
    {
        record.total_verify_time = record.mem_verify_time + record.db_verify_time;
        record.total_verify_amount_per_second = (double)1 / ((double) record.total_verify_time / conversion_number);
    }
    
}

void TFSbenchmark::AddtransactionDBVerifyMap(const std::string& tx_hash, uint64_t cost_time)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
    auto found = transactionVerifyMap.find(tx_hash);
    if (found == transactionVerifyMap.end())
    {
        transactionVerifyMap[tx_hash] = verify_time_record();
    }

    auto& record = transactionVerifyMap.at(tx_hash);
    record.db_verify_time = cost_time;
    record.db_verify_amount_per_second = (double)1 / ((double)cost_time / conversion_number);

    if (record.mem_verify_time != 0)
    {
        record.total_verify_time = record.mem_verify_time + record.db_verify_time;
        record.total_verify_amount_per_second = (double)1 / ((double) record.total_verify_time / conversion_number);
    }
}

void TFSbenchmark::AddAgentTransactionReceiveMap(const std::shared_ptr<TxMsgReq> &msg)
{
    if (!benchmarkSwitch)
    {
        return;
    }
	CTransaction tx_benchmark_tmp;
	if (tx_benchmark_tmp.ParseFromString(msg->txmsginfo().tx()) && tx_benchmark_tmp.verifysign_size() == 0)
	{
        std::lock_guard<std::mutex> lock(agentTransactionReceiveMapMutex);
        auto& tx_hash = tx_benchmark_tmp.hash();
        auto found = agentTransactionReceiveMap.find(tx_hash);
        if (found != agentTransactionReceiveMap.end())
        {
            return;
        }
        agentTransactionReceiveMap[tx_hash] = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
	}

}

void TFSbenchmark::AddTransactionSignReceiveMap(const std::string& tx_hash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);
    auto found = transactionSignReceiveMap.find(tx_hash);
    if (found == transactionSignReceiveMap.end())
    {
        transactionSignReceiveMap[tx_hash] = {};
    }
    auto& time_record = transactionSignReceiveMap.at(tx_hash);
    time_record.push_back(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
}

void TFSbenchmark::CalculateTransactionSignReceivePerSecond(const std::string& tx_hash, uint64_t compose_time)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);
    auto found = transactionSignReceiveMap.find(tx_hash);
    if (found == transactionSignReceiveMap.end())
    {
        return;
    }
    auto& time_record = transactionSignReceiveMap.at(tx_hash);
    auto span_time = compose_time - time_record.front();
    transactionSignReceiveCache[tx_hash] = {span_time, (double)1 / ((double)span_time / conversion_number)};
}

void TFSbenchmark::AddBlockContainsTransactionAmountMap(const std::string& block_hash, int tx_amount)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockContainsTransactionAmountMapMutex);
    blockContainsTransactionAmountMap[block_hash] = tx_amount;
}

void TFSbenchmark::AddBlockVerifyMap(const std::string& block_hash, uint64_t cost_time)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockVerifyMapMutex);
    auto found = blockVerifyMap.find(block_hash);
    if (found != blockVerifyMap.end())
    {
        return;
    }
    
    blockVerifyMap[block_hash] = {cost_time, (double)1 / ((double) cost_time / conversion_number) };
}

void TFSbenchmark::IncreaseTransactionInitiateAmount()
{
    if (!benchmarkSwitch)
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
    if (!benchmarkSwitch)
    {
        return;
    }
    std::cout << "there're " << transactionInitiateAmount << 
                " simple transactions hash been initiated since height " << transactionInitiateHeight << std::endl;
}

void TFSbenchmark::AddBlockPoolSaveMapStart(const std::string& block_hash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
    auto found = blockPoolSaveMap.find(block_hash);
    if (found == blockPoolSaveMap.end())
    {
        blockPoolSaveMap[block_hash] = {MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp(), 0};
    }
}

void TFSbenchmark::AddBlockPoolSaveMapEnd(const std::string& block_hash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
    auto found = blockPoolSaveMap.find(block_hash);
    if (found == blockPoolSaveMap.end())
    {
        return;
    }
    auto& record = blockPoolSaveMap.at(block_hash);
    if (record.first == 0)
    {
        return;
    }
    record.second = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
}

void TFSbenchmark::SetBlockPendingTime(uint64_t PendingTime)
{
    if (!benchmarkSwitch2)
    {
        return;
    }

    NewBlockPendingTime = PendingTime;
}

void TFSbenchmark::SetByTxHash(const std::string& TxHash, void* arg , uint16_t type)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(DoHandleTxMutex);
    switch (type)
    {
    case 1:
        TV[TxHash].StartTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 2:
        TV[TxHash].Verify_2 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        TV[TxHash].Verify_3 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        TV[TxHash].EndTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 5:
        TV[TxHash].Timeout = true;
        break;
    default:
        break;
    }
}
void TFSbenchmark::SetByBlockHash(const std::string& BlockHash, void* arg , uint16_t type, void* arg2, void* arg3, void* arg4)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(DoHandleTxMutex);
    switch (type)
    {
    case 1:
        BV[BlockHash].Time = *reinterpret_cast<uint64_t*>(arg);
        BV[BlockHash].Verify_4 = *reinterpret_cast<uint64_t*>(arg2);
        BV[BlockHash].TxNumber = *reinterpret_cast<uint64_t*>(arg3);
        BV[BlockHash].Hight = *reinterpret_cast<uint64_t*>(arg4);
        break;
    case 2:
        BV[BlockHash].BroadcastTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        BV[BlockHash].Verify_5 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        VN[BlockHash].Hight =  *reinterpret_cast<uint64_t*>(arg);
        VN[BlockHash].TxNumber =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 5:
        VN[BlockHash].MemVerifyTime =  *reinterpret_cast<uint64_t*>(arg);
        VN[BlockHash].TxVerifyTime =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 6:
        BV[BlockHash].BlockPendingTime = NewBlockPendingTime;
        BV[BlockHash].BuildBlockTime = *reinterpret_cast<uint64_t*>(arg);
        BV[BlockHash].CheckConflictTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 7:
        BV[BlockHash].SearchNodeTime = *reinterpret_cast<uint64_t*>(arg);
        BV[BlockHash].totalTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 8:
        BV[BlockHash].seekPrehashTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    default:
        break;
    }
}

void TFSbenchmark::SetTxHashByBlockHash(const std::string& BlockHash, const std::string& TxHash)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(DoHandleTxMutex);
    BT.insert({BlockHash,TxHash});
}
void TFSbenchmark::PrintBenchmarkSummary(bool export_to_file)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    
    nlohmann::json benchmark_json;
    if (export_to_file)
    {
         std::ifstream readfilestream;
        readfilestream.open(benchmark_filename);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmark_json = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmark_item_json;
    if (!transactionInitiateCache.empty())
    {
        std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
        double cost_sum = 0;
        for(auto record : transactionInitiateCache)
        {
            cost_sum += record.second.first;
        }
        double transactionTimeCostAverage = cost_sum / transactionInitiateCache.size();
        double transactionAmountPerSecond = (double) 1 / (transactionTimeCostAverage / conversion_number);
        
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << transactionAmountPerSecond;
            benchmark_item_json["one-to-one_transactions_can_be_initiated_per_second"] = stream.str();
        }
        else
        {
            std::cout << "one-to-one transactions can be initiated per second: " << transactionAmountPerSecond << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["one-to-one_transactions_can_be_initiated_per_second"] = "";
        }
    }

    if (!transactionVerifyMap.empty())
    {
        std::lock_guard<std::mutex> lock(transactionVerifyMapMutex);
        uint64_t mem_cost_sum = 0;
        uint64_t db_cost_sum = 0;
        uint64_t total_cost_sum = 0;
        int skip_count = 0;
        for(auto record : transactionVerifyMap)
        {
            if (record.second.mem_verify_time == 0 
                || record.second.db_verify_time == 0
                || record.second.total_verify_time == 0)
            {
                skip_count++;
                continue;
            }
            
            mem_cost_sum += record.second.mem_verify_time;
            db_cost_sum += record.second.db_verify_time;
            total_cost_sum += record.second.total_verify_time;
        }
        double mem_cost_average = (double)mem_cost_sum / (double)(transactionVerifyMap.size() - skip_count);
        double db_cost_average = (double)db_cost_sum / (double)(transactionVerifyMap.size() - skip_count);
        double total_cost_average = (double)total_cost_sum / (double)(transactionVerifyMap.size() - skip_count);
        double mem_verify_per_second = (double) 1 / (mem_cost_average / conversion_number);
        double db_verify_per_second = (double) 1 / (db_cost_average / conversion_number);
        double total_verify_per_second = (double) 1 / (total_cost_average / conversion_number);
        if (export_to_file)
        {
            std::ostringstream total_stream;
            total_stream << total_verify_per_second;
            std::ostringstream mem_stream;
            mem_stream << mem_verify_per_second;
            std::ostringstream db_stream;
            db_stream << db_verify_per_second;
            benchmark_item_json["Number_of_verifiable_transactions_per_second"] = total_stream.str();
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_memory"] = mem_stream.str();
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_db"] = db_stream.str();
        }
        else
        {
            std::cout << "Number of verifiable transactions per second: " << total_verify_per_second 
                  << " (mem verify: " << mem_verify_per_second << " db verify: " << db_verify_per_second << ")" << std::endl;
        }

    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["Number_of_verifiable_transactions_per_second"] = "";
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_memory"] = "";
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_db"] = "";
        }
    }

    if (!agentTransactionReceiveMap.empty())
    {
        std::lock_guard<std::mutex> lock(agentTransactionReceiveMapMutex);
        std::map<uint64_t, uint64_t> hit_cache;
        for(auto record : agentTransactionReceiveMap)
        {
            uint64_t time = record.second / conversion_number_u;
            auto found = hit_cache.find(time);
            if (found == hit_cache.end())
            {
                hit_cache[time] = 1;
            }
            auto& hit_times = found->second;
            hit_times += 1;
        }

        uint64_t max_hit_times = 0;
        for(auto hits : hit_cache)
        {
            if (hits.second > max_hit_times)
            {
                max_hit_times = hits.second;
            }
        }
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << max_hit_times;
            benchmark_item_json["Number_of_transactions_per_second"] = stream.str();
        }
        else
        {
            std::cout << "Number of transactions per second from internet: " << max_hit_times << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["Number_of_transactions_per_second"] = "";
        }
    }

    if(!transactionSignReceiveMap.empty())
    {
        std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex);

        uint64_t transaction_compose_cost_sum = 0;
        for(auto record : transactionSignReceiveCache)
        {
            transaction_compose_cost_sum += record.second.first;
        }

        double transaction_compose_cost_average = (double)transaction_compose_cost_sum / (double)transactionSignReceiveCache.size();
        double transaction_compose_amout_per_second = (double)1 / (transaction_compose_cost_average / conversion_number);
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << transaction_compose_amout_per_second;
            benchmark_item_json["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = stream.str();
        }
        else
        {
            std::cout << "signature per second can be collected from the network and combined into a complete transaction body: " << transaction_compose_amout_per_second << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = "";
        }
    }

    if (!blockContainsTransactionAmountMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockContainsTransactionAmountMapMutex);
        uint64_t tx_amount_sum = 0;
        for(auto record : blockContainsTransactionAmountMap)
        {
            tx_amount_sum += record.second;
        }
        double tx_amount_average = (double)tx_amount_sum / (double)blockContainsTransactionAmountMap.size();
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << tx_amount_average;
            benchmark_item_json["transaction_count_can_be_packed_into_a_full_block_per_second"] = stream.str();
        }
        else
        {
            std::cout << "transaction count can be packed into a full block per second: " << tx_amount_average << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["transaction_count_can_be_packed_into_a_full_block_per_second"] = "";
        }
    }

    if (!blockVerifyMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockVerifyMapMutex);
        uint64_t block_verify_cost_sum = 0;
        for(auto record : blockVerifyMap)
        {
            block_verify_cost_sum += record.second.first;
        }
        double block_verify_cost_average = (double)block_verify_cost_sum / (double)blockVerifyMap.size();
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << block_verify_cost_average;
            benchmark_item_json["Block_validation_time_in_the_block_pool"] = stream.str();
        }
        else
        {
            std::cout << "Block validation time in the block pool: " << block_verify_cost_average << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["Block_validation_time_in_the_block_pool"] = "";
        }
    }
    
    if (!blockPoolSaveMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex);
        uint64_t block_save_time_sum = 0;
        int fail_count = 0;
        for(auto record : blockPoolSaveMap)
        {
            if (record.second.second <= record.second.first)
            {
                fail_count++;
                continue;
            }
            
            block_save_time_sum += (record.second.second - record.second.first);
        }
        double block_save_time_average = (double)block_save_time_sum / (double)(blockPoolSaveMap.size() - fail_count);
        if (export_to_file)
        {
            std::ostringstream stream;
            stream << block_save_time_average;
            benchmark_item_json["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = stream.str();
        }
        else
        {
            std::cout << "Time for blocks in the block pool to be stored in the database: " << block_save_time_average << std::endl;
        }
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = "";
        }
    }

    if (export_to_file)
    {
        std::ofstream filestream;
        filestream.open(benchmark_filename, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        benchmark_json.push_back(benchmark_item_json);
        filestream << benchmark_json.dump();
        filestream.close();
    } 
    return ;

}

void TFSbenchmark::PrintBenchmarkSummary_DoHandleTx(bool export_to_file)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    
    nlohmann::json benchmark_json;
    if (export_to_file)
    {
         std::ifstream readfilestream;
        readfilestream.open(benchmark_filename2);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmark_json = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmark_item_json;
    std::map<uint64_t, std::pair<uint64_t, double>> HT;
    std::map<uint64_t, uint64_t> HS;
    
    if(!BV.empty())
    {
        std::lock_guard<std::mutex> lock(DoHandleTxMutex);
        for(auto& it : BV)
        {
            benchmark_item_json["BlockHash"] = it.first.substr(0,6);
            auto target_begin = BT.lower_bound(it.first);
            auto target_end = BT.upper_bound(it.first);
            uint64_t total_verify_time = 0;
            uint64_t TxTimeMin = it.second.Time;
            
            uint64_t TxAverageStartTime = 0;
            for (; target_begin != target_end ; target_begin++)
            {
                if(TV.find(target_begin->second) != TV.end())
                {
                    HS[it.second.Hight]++;
                    auto &tx = TV[target_begin->second];
                    if(TxTimeMin > tx.StartTime) TxTimeMin = tx.StartTime;
                    
                    TxAverageStartTime += tx.StartTime;
                    total_verify_time = total_verify_time + tx.Verify_2 + tx.Verify_3;
                }
            }
            benchmark_item_json["BlockTime"] = it.second.Time / 1000000;
            benchmark_item_json["BlockHight"] = it.second.Hight;
            benchmark_item_json["BlockTxNumber"] = it.second.TxNumber;
            benchmark_item_json["TxAverageCompositionTime"] = 0;
            benchmark_item_json["TxVerifyTime_2345"] = (double)total_verify_time / 1000000;
            benchmark_item_json["TxVerifyTime_5"] = (double)it.second.Verify_5 / 1000000;
            //Time to successful block construction
            benchmark_item_json["BuildBlockSuccessTime"] = (double)(it.second.Time - TxTimeMin) / 1000000;
            benchmark_item_json["BuildBlockSuccessAverageTime"] = (double)(it.second.Time - (TxAverageStartTime / it.second.TxNumber)) / 1000000;
            //The time to build into a block and the time to start broadcasting a SAVE message for a successful block flow
            benchmark_item_json["BroadcastSaveBlockTime"] = (double)(it.second.BroadcastTime - it.second.Time) / 1000000;
            //Time to wait for block creation
            benchmark_item_json["BlockPendingTime"] = (double)(it.second.BlockPendingTime - (TxAverageStartTime / it.second.TxNumber)) / 1000000;
            //Time from creation of block to start of DoHandleBlock
            benchmark_item_json["BuildBlockTime"] = (double)it.second.BuildBlockTime / 1000000;
            //Time to find broadcast node
            benchmark_item_json["SearchNodeTime"] = (double)it.second.SearchNodeTime / 1000000;
            //Time of the whole process
            benchmark_item_json["totalTime"] = (double)(it.second.totalTime - (TxAverageStartTime / it.second.TxNumber)) / 1000000;

            benchmark_item_json["seekPrehashTime"] = (double)(it.second.seekPrehashTime - TxTimeMin) / 1000000;
            benchmark_item_json["CheckConflictTime"] = (double)it.second.CheckConflictTime / 1000000;
            
            HT[it.second.Hight].second += (double)(it.second.Time - TxTimeMin) / 1000000;
            HT[it.second.Hight].first += it.second.TxNumber;
            benchmark_json.push_back(benchmark_item_json);
        }
        int i = 0;
        for(auto& it : HT)
        {
            benchmark_json.at(i)["Hight"] = it.first;
            benchmark_json.at(i)["HightTxNumber"] = it.second.first;
            benchmark_json.at(i)["HighBuildTime"] = it.second.second;
            i++;
        }

        uint64_t TxSuccessTotal = 0;
        i = 0;
        for(auto& it : HS)
        {
            benchmark_json.at(i)["TxHight"] = it.first;
            benchmark_json.at(i)["TxNumber"] = it.second;
            TxSuccessTotal += it.second;
            i++;
        }
        benchmark_json.at(0)["TxTotal"] = TV.size();
        benchmark_json.at(0)["TxSuccessTotal"] = TxSuccessTotal;
        benchmark_json.at(0)["TxFailTotal"] = TV.size() - TxSuccessTotal;
    }
    else
    {
        if (export_to_file)
        {
            benchmark_item_json["BlockHash"] = "";
            benchmark_item_json["TxNumber"] = "";
            benchmark_item_json["Hight"] = "";
            benchmark_item_json["TxAverageCompositionTime"] = "";
            benchmark_item_json["TxVerifyTime_2345"] = "";
            benchmark_item_json["BuildBlockSuccessTime"] = "";
            benchmark_item_json["BuildBlockBroadcastTime"] = "";
            benchmark_item_json["NumberFailTx"] = "";
            benchmark_json.push_back(benchmark_item_json);
            benchmark_item_json["BlockHash"] = "1";
            benchmark_item_json["TxNumber"] = "2";
            benchmark_item_json["Hight"] = "3";
            benchmark_item_json["TxAverageCompositionTime"] = "4";
            benchmark_item_json["TxVerifyTime_2345"] = "5";
            benchmark_item_json["BuildBlockSuccessTime"] = "6";
            benchmark_item_json["BuildBlockBroadcastTime"] = "7";
            benchmark_item_json["NumberFailTx"] = "8";
            benchmark_json.push_back(benchmark_item_json);
            benchmark_item_json["BlockHash"] = "11";
            benchmark_item_json["TxNumber"] = "22";;
            benchmark_item_json["Hight"] = "33";
            benchmark_item_json["TxAverageCompositionTime"] = "44";
            benchmark_item_json["TxVerifyTime_2345"] = "55";
            benchmark_item_json["BuildBlockSuccessTime"] = "66";
            benchmark_item_json["BuildBlockBroadcastTime"] = "77";
            benchmark_item_json["NumberFailTx"] = "88";
            benchmark_json.push_back(benchmark_item_json);

            benchmark_item_json.clear();
            benchmark_item_json["NumberFailTx"] = 100;
            benchmark_json.push_back(benchmark_item_json);

            benchmark_item_json.clear();
            benchmark_item_json["NumberFailTx"] = 200;
            benchmark_json.push_back(benchmark_item_json);
        }
    }

    if(!VN.empty())
    {
        benchmark_json.at(0)["VN_Hight"] = "";
        benchmark_json.at(0)["VN_TxNumber"] = "";
        benchmark_json.at(0)["VN_MemVerifyTime"] = "";
        benchmark_json.at(0)["VN_TxVerifyTime"] = "";

        for(auto& it : VN)
        {
            benchmark_item_json.clear();
            benchmark_item_json["VN_Hight"] = it.second.Hight;
            benchmark_item_json["VN_TxNumber"] = it.second.TxNumber;
            benchmark_item_json["VN_MemVerifyTime"] = (double)it.second.MemVerifyTime / 1000000;
            benchmark_item_json["VN_TxVerifyTime"] = (double)it.second.TxVerifyTime / 1000000;

            benchmark_json.push_back(benchmark_item_json);
        }


    }
    if (export_to_file)
    {
        std::ofstream filestream;
        filestream.open(benchmark_filename2, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        filestream << benchmark_json.dump();
        filestream.close();
    }
    TV.clear();
    BV.clear();
    BT.clear();
    VN.clear();
    return ;
}
