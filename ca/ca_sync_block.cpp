#include "logging.h"
#include "utils/MagicSingleton.h"
#include "ca/ca_algorithm.h"
#include "ca/ca_transaction.h"
#include "ca/ca_txhelper.h"
#include "common/global_data.h"
#include "db/db_api.h"
#include "ca/ca_sync_block.h"
#include "ca_global.h"
#include "ca_blockcache.h"
#include "utils/console.h"

#include "ca_blockhelper.h"
#include "utils/AccountManager.h"
#include "utils/json.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <sys/file.h>
#include <tuple>
#include <utility>
#include <vector>
#include <tuple>
#include "net/dispatcher.h"
#include <cmath>
const static uint64_t stability_time = 60 * 1000000;
static uint64_t new_sync_fail_height = 0;
static bool run_fast_sync = false;
static bool in_fast_sync_fail = false;

static uint32_t sync_height_cnt_ = 50;
const static uint32_t sync_height_time_ = 50;
const static uint32_t sync_stuck_overtime_count = 6;
static uint64_t sync_send_new_sync_num = 25;
static uint64_t sync_send_fast_sync_num = 10;
static uint64_t sync_send_zero_sync_num = 20;
const  static int nnormal_sum_hash_num = 1;
static uint32_t run_fast_sunc_num = 0;
// static bool sync_to_tx = false;
// #include <fstream> 
// static std::ofstream fout("./test.txt", std::ios::trunc); 


inline static global::ca::SaveType GetSaveSyncType(uint64_t height, uint64_t chain_height)
{
    if (chain_height <= global::ca::sum_hash_range)
    {
        return global::ca::SaveType::SyncNormal;
    }
    
    global::ca::SaveType save_type;
    if(ca_algorithm::GetSumHashFloorHeight(chain_height) - global::ca::sum_hash_range <= height)
    {
        save_type = global::ca::SaveType::SyncNormal;
    }
    else
    {
        save_type = global::ca::SaveType::SyncFromZero;
    }
    return save_type;
}
static bool SumHeightHash(std::vector<std::string> &block_hashes, std::string &hash)
{
    std::sort(block_hashes.begin(), block_hashes.end());
    hash = getsha256hash(StringUtil::concat(block_hashes, ""));
    return true;
}

bool SyncBlock::SumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& height_blocks, std::string &hash)
{
    std::vector<std::string> height_block_hashes;
    for(auto height_block : height_blocks)
    {
        std::string sum_hash;
        SumHeightHash(height_block.second, sum_hash);
        height_block_hashes.push_back(sum_hash);
    }

    SumHeightHash(height_block_hashes, hash);
    return true;
}

static bool GetHeightBlockHash(uint64_t start_height, uint64_t end_height, std::vector<std::string> &block_hashes)
{
    DBReader db_reader;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(start_height, end_height, block_hashes))
    {
        return false;
    }
    return true;
}

static bool GetHeightBlockHash(uint64_t start_height, uint64_t end_height, std::vector<FastSyncBlockHashs> &block_height_hashes)
{
    DBReader db_reader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("(GetHeightBlockHash) GetBlockTop failed !");
        return false;
    }
    
    uint64_t current_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    while (start_height <= end_height && start_height <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(start_height, hashs))
        {
            return false;
        }
        std::vector<std::string> blocks_raw;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(hashs, blocks_raw))
        {
            return false;
        }
        FastSyncBlockHashs fastSyncBlockHashs;
        fastSyncBlockHashs.set_height(start_height);
        for(const auto& block_raw : blocks_raw)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                return false;
            }

            if((current_time - block.time()) < stability_time)
            {
                continue;
            }
            fastSyncBlockHashs.add_hashs(block.hash());
        }
        block_height_hashes.push_back(fastSyncBlockHashs);
        ++start_height;
    }
    return true;
}

static bool GetHeightBlockHash(uint64_t start_height, uint64_t end_height, std::map<uint64_t, std::vector<std::string>> &block_height_hashes)
{
    DBReader db_reader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("(GetHeightBlockHash) GetBlockTop failed !");
        return false;
    }
    while (start_height < end_height && start_height <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(start_height, hashs))
        {
            return false;
        }
        block_height_hashes[start_height] = hashs;
        ++start_height;
    }
    return true;
}

void  SyncBlock::ThreadStop(){

    sync_thread_runing=false;
}

void SyncBlock::ThreadStart(bool start)
{
    sync_thread_runing = start; 
}

void SyncBlock::ThreadStart()
{
    if(sync_height_cnt_ > sync_bound)
    {
        sync_height_cnt_ = sync_bound;
    }
    fast_sync_height_cnt_ = 0; // 0 means "this variable doesn't use for now"
    
    sync_thread_runing = true;
    sync_thread_ = std::thread(
        [this]()
        {
            uint32_t  sleep_time = sync_height_time_;
            while (1)
            {
                if(!sync_thread_runing)
                {
                    std::cout << "sleep" << std::endl;
                    sleep(sleep_time);
                    continue;
                }
                std::unique_lock<std::mutex> block_thread_runing_locker(sync_thread_runing_mutex);
                sync_thread_runing_condition.wait_for(block_thread_runing_locker, std::chrono::seconds(sleep_time));
                sync_thread_runing_mutex.unlock();
                if (!sync_thread_runing)
                {
                    break;
                }
                else
                {
                    sleep_time = sync_height_time_;
                }
                uint64_t chain_height = 0;
                if(!MagicSingleton<BlockHelper>::GetInstance()->obtain_chain_height(chain_height))
                {
                    continue;
                }
                uint64_t self_node_height = 0;
                std::vector<std::string> pledge_addr; // stake and invested addr
                {

                    DBReader db_reader;
                    auto status = db_reader.GetBlockTop(self_node_height);
                    if (DBStatus::DB_SUCCESS != status)
                    {
                        continue;
                    }
                    std::vector<std::string> stake_addr;
                    status = db_reader.GetStakeAddress(stake_addr);
                    if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
                    {
                        continue;
                    }

                    for(const auto& addr : stake_addr)
                    {
                        if(VerifyBonusAddr(addr) != 0)
                        {
                            DEBUGLOG("{} doesn't get invested, skip", addr);
                            continue;
                        }
                        pledge_addr.push_back(addr);
                    }
                }

                uint64_t start_sync_height = 0;
                uint64_t end_sync_height = 0;

                if (!sync_from_zero_cache.empty())
                {
                    for (auto iter = sync_from_zero_cache.begin(); iter != sync_from_zero_cache.end();)
                    {
                        if (iter->first < self_node_height)
                        {
                            iter = sync_from_zero_cache.erase(iter);
                        }
                        else
                        {
                            iter++;
                        }
                    }
                }

                if(run_fast_sunc_num >= 10)
                {
                    if(run_fast_sync)
                    {
                        run_fast_sunc_num = 0;
                        run_fast_sync = false;
                    }
                }

                if(run_fast_sync)
                {
                    ++run_fast_sunc_num;

                    if (new_sync_fail_height > fast_sync_height_cnt_)
                    {
                        start_sync_height = new_sync_fail_height - fast_sync_height_cnt_;
                    }
                    else
                    {
                        start_sync_height = new_sync_fail_height;
                    }

                    if(start_sync_height > self_node_height)
                    {
                        start_sync_height = self_node_height + 1;
                    }

                    end_sync_height = start_sync_height + fast_sync_height_cnt_;

                    INFOLOG("begin fast sync {} {} ", start_sync_height, end_sync_height);
                    bool if_success = RunFastSyncOnce(pledge_addr, chain_height, start_sync_height, end_sync_height);
                    if(!if_success)
                    {
                        sync_send_fast_sync_num += 15;
                        DEBUGLOG("fast sync fail");
                    }
                    else
                    {
                        sync_send_fast_sync_num = 10;
                    }
                    run_fast_sync = false;
                    if(in_fast_sync_fail)
                    {
                        in_fast_sync_fail = false;
                        run_fast_sync = true;
                    }

                }
                else
                { 
                    auto sync_type = GetSaveSyncType(self_node_height, chain_height);

                    if (sync_type == global::ca::SaveType::SyncFromZero)
                    {
                        INFOLOG("begin from zero sync");
                        int run_status = RunFromZeroSyncOnce(pledge_addr, chain_height, self_node_height);
                        if (run_status != 0)
                        {
                            sync_send_zero_sync_num *= 10;
                            ERRORLOG("from zero sync fail ret: {}", run_status);
                        }
                        else 
                        {
                            sync_send_zero_sync_num = 20;
                        }
                        DEBUGLOG("RunFromZeroSyncOnce ret: {}", run_status);
                    }
                    else
                    {
                        if(new_sync_fail_height != 0)
                        {
                            self_node_height = new_sync_fail_height;
                        }
                        if (self_node_height > sync_height_cnt_)
                        {
                            start_sync_height = self_node_height - sync_height_cnt_;
                            if(start_sync_height <= 0)
                            {
                                start_sync_height = 1;
                            }
                        }
                        else
                        {
                            start_sync_height = 1;
                        }

                        if(chain_height - 10 <= self_node_height)
                        {
                            end_sync_height = self_node_height + 1;
                        }
                        else
                        {
                            end_sync_height = self_node_height + sync_height_cnt_;
                        }
                        sleep_time = sync_height_time_;
                        INFOLOG("begin new sync {} {} ", start_sync_height, end_sync_height);

                        int run_status = RunNewSyncOnce(pledge_addr, chain_height, self_node_height, start_sync_height, end_sync_height, 0);
                        if(run_status < 0)
                        {   
                            new_sync_fail_height = 0;
                            run_fast_sync = false;
                            sync_send_new_sync_num *= 10;
                        }
                        if(run_status == 0)
                        {
                            new_sync_fail_height = 0;
                            run_fast_sync = false;
                            sync_send_new_sync_num = 25;
                        }
                        if(run_status > 0)
                        {
                            sync_send_new_sync_num *= 10;
                        }
                        DEBUGLOG("RunNewSyncOnce sync return: {}", run_status);
                    }
                }
            }
        });
    sync_thread_.detach();
}




bool SyncBlock::RunFastSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t start_sync_height, uint64_t end_sync_height)
{
    if (start_sync_height > end_sync_height)
    {
        return false;
    }
    std::vector<std::string> send_node_ids;
    if (GetSyncNode(sync_send_fast_sync_num, chain_height, pledge_addr, send_node_ids) != 0)
    {
        ERRORLOG("get sync node fail");
        return false;
    }
    std::vector<std::string> ret_node_ids;
    std::vector<FastSyncBlockHashs> request_hashs;

    INFOLOG("begin GetFastSyncSumHashNode {} {} ", start_sync_height, end_sync_height);
    if (!GetFastSyncSumHashNode(send_node_ids, start_sync_height, end_sync_height, request_hashs, ret_node_ids, chain_height))
    {
        ERRORLOG("get sync sum hash fail");
        return false;
    }
    bool flag = false;
    for (auto &node_id : ret_node_ids)
    {
        DEBUGLOG("fast sync block from {}", node_id);
        if (GetFastSyncBlockData(node_id,request_hashs, chain_height))
        {
            flag = true;
            break;
        }
    }
    return flag;
}

int SyncBlock::RunNewSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t self_node_height, uint64_t start_sync_height, uint64_t end_sync_height, uint64_t new_send_num)
{
    uint64_t new_sync_sned_num = 0;
    if(new_send_num == 0)
    {
        new_sync_sned_num = sync_send_new_sync_num;
    }
    else 
    {
        new_sync_sned_num = new_send_num;
    }

    int ret = 0;
    if (start_sync_height > end_sync_height)
    {
        ret = -1;
        return ret;
    }
    std::vector<std::string> send_node_ids;
    if ((ret = GetSyncNode(new_sync_sned_num, chain_height, pledge_addr, send_node_ids)) != 0)
    {
        ERRORLOG("get sync node fail");
        return ret - 1000;
    }

    std::map<uint64_t, uint64_t> need_sync_heights;
    std::vector<std::string> ret_node_ids;
    chain_height = 0;
    DEBUGLOG("GetSyncSumHashNode begin:{}:{}", start_sync_height, end_sync_height);
    if ((ret = GetSyncSumHashNode(pledge_addr.size(), send_node_ids, start_sync_height, end_sync_height, need_sync_heights, ret_node_ids, chain_height, new_sync_sned_num)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    bool fast_sync_flag = false;
    if(ret == 99)
    {
        fast_sync_flag = true;
    }
    DEBUGLOG("GetSyncSumHashNode success:{}:{}", start_sync_height, end_sync_height);
    std::vector<std::string> sec_ret_node_ids;
    std::vector<std::string> req_hashes;
    for (auto sync_height : need_sync_heights)
    {
        sec_ret_node_ids.clear();
        req_hashes.clear();
        DEBUGLOG("GetSyncBlockHashNode begin:{}:{}", sync_height.first, sync_height.second);
        if ((ret = GetSyncBlockHashNode(ret_node_ids, sync_height.first, sync_height.second, self_node_height, chain_height, sec_ret_node_ids, req_hashes, new_sync_sned_num)) != 0)
        {
            return ret - 3000;
        }
        DEBUGLOG("GetSyncBlockHashNode success:{}:{}", sync_height.first, sync_height.second);
        DEBUGLOG("GetSyncBlockData begin:{}", req_hashes.size());
        if ((ret = GetSyncBlockData(sec_ret_node_ids, req_hashes, chain_height)) != 0 )
        {
            if (run_fast_sync)
            {
                 return 2;
            }

            return ret -4000;
        }
        DEBUGLOG("GetSyncBlockData success:{}", req_hashes.size());
    }

    if(run_fast_sync)
    {
        return 99;
    }
    if(fast_sync_flag)
    {
        return 1;
    }
    return 0;
}

int SyncBlock::RunFromZeroSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t self_node_height)
{
    int ret = 0;

    std::vector<std::string> send_node_ids;
    if ((ret = GetSyncNodeSimplify(sync_send_zero_sync_num, chain_height, pledge_addr, send_node_ids)) != 0)
    {
        return ret - 1000;
    }
    std::set<std::string> ret_node_ids;
    std::vector<uint64_t> heights;
    if (!sync_from_zero_reserve_heights.empty())
    {
        sync_from_zero_reserve_heights.swap(heights);
    }
    else
    {
        uint64_t height_ceiling = ca_algorithm::GetSumHashFloorHeight(chain_height) - global::ca::sum_hash_range; //100

        auto sync_height = ca_algorithm::GetSumHashCeilingHeight(self_node_height);

        if(height_ceiling - sync_height > 1000)
        {
            height_ceiling = sync_height + 1000;
        }

        heights = {sync_height};
        for(auto i = send_node_ids.size() - 1; i > 0; --i)
        {
            sync_height += global::ca::sum_hash_range;
            if (sync_height > height_ceiling)
            {
                break;
            }
            heights.push_back(sync_height);
        }        
    }

    std::map<uint64_t, std::string> sum_hashes;

    std::string request_heights;
    for(auto height : heights)
    {
        request_heights += " ";
        request_heights += std::to_string(height);
    }

    DEBUGLOG("GetFromZeroSyncSumHashNode begin{}", request_heights);
    if ((ret = GetFromZeroSyncSumHashNode(send_node_ids, heights, self_node_height, ret_node_ids, sum_hashes)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    DEBUGLOG("GetFromZeroSyncSumHashNode success");

    DEBUGLOG("GetFromZeroSyncBlockData begin");
    if ((ret = GetFromZeroSyncBlockData(sum_hashes, heights, ret_node_ids, self_node_height)) != 0 )
    {
        return ret -3000;
    }
    DEBUGLOG("GetSyncBlockData success");
    return 0;
}

int SyncBlock::GetSyncNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
                           std::vector<std::string> &send_node_ids)
{
    static bool force_find_base_chain = false;
    DBReader db_reader;
    uint64_t self_node_height = 0;
    auto status = db_reader.GetBlockTop(self_node_height);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }

    if (self_node_height == chain_height)
    {
        static uint64_t last_equal_height = 0;
        static uint32_t equal_height_count = 0;

        if (self_node_height == last_equal_height)
        {
            equal_height_count++;
            if (equal_height_count >= sync_stuck_overtime_count)
            {
                std::vector<std::string> selected_addrs;
                if (NeedByzantineAdjustment(chain_height, pledge_addr, selected_addrs))
                {
                    auto discard_comparator = std::less<>();
                    auto reserve_comparator = std::greater_equal<>();
                    int ret = GetSyncNodeBasic(num, chain_height, discard_comparator, reserve_comparator, selected_addrs,
                                            send_node_ids);
                    if (ret != 0)
                    {
                        DEBUGLOG("get sync node fail, ret: {}", ret);
                        force_find_base_chain = true;
                    }
                    else
                    {
                        DEBUGLOG("Preparing to solve the forking problem");
                        return 0;
                    }
                }
            }
        }
        else
        {
            equal_height_count = 0;
            last_equal_height = self_node_height;
        }

    }

    if (force_find_base_chain || self_node_height < chain_height)
    {
        DEBUGLOG("find node base on chain height {}", chain_height);
        force_find_base_chain = false;
        auto discard_comparator = std::less<>();
        auto reserve_comparator = std::greater_equal<>();
        return GetSyncNodeBasic(num, chain_height, discard_comparator, reserve_comparator, pledge_addr, send_node_ids);
    }
    else
    {
        DEBUGLOG("find node base on self height {}", self_node_height);
        force_find_base_chain = true;
        auto discard_comparator = std::less_equal<>();
        auto reserve_comparator = std::greater<>();
        return GetSyncNodeBasic(num, self_node_height, discard_comparator, reserve_comparator, pledge_addr,
                                send_node_ids);
    }
}

int SyncBlock::GetSyncNodeSimplify(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
                                   std::vector<std::string> &send_node_ids)
{
    DBReader db_reader;
    uint64_t self_node_height = 0;
    auto status = db_reader.GetBlockTop(self_node_height);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }
    DEBUGLOG("find node base on self height {}", self_node_height);
    auto discard_comparator = std::less<>();
    auto reserve_comparator = std::greater_equal<>();
    return GetSyncNodeBasic(num, self_node_height, discard_comparator, reserve_comparator, pledge_addr, send_node_ids);
}

int SyncBlock::GetSyncNodeBasic(uint32_t num, uint64_t height_baseline,
                                const std::function<bool(uint64_t, uint64_t)>& discard_comparator,
                                const std::function<bool(uint64_t, uint64_t)>& reserve_comparator,
                                const std::vector<std::string> &pledge_addr,
                                std::vector<std::string> &send_node_ids)
{
    DEBUGLOG("{} Nodes have passed qualification verification", pledge_addr.size());
    int ret = 0;
    std::vector<Node> nodes;
    auto peer_node = MagicSingleton<PeerNode>::GetInstance();
    nodes = peer_node->get_nodelist();

    if (nodes.empty())
    {
        ERRORLOG("node is empty");
        ret = -1;
        return ret;
    }

    if(num > nodes.size() || num < 10)
    {
        num = nodes.size();
    }
    if (nodes.size() <= num)
    {
        for (auto &node : nodes)
        {
            send_node_ids.push_back(node.base58address);
        }
    }
    else
    {
        std::vector<Node> node_info = nodes;
        if(pledge_addr.size() < num)
        {
            int count = 0;
            for(auto &node : node_info)
            {
                if(reserve_comparator(node.height, height_baseline))
                {
                    ++count;
                }
            }
            if(count < num)
            {
                DEBUGLOG("Number of satisfying height less {}",num);
                return -2;
            }

            for (; send_node_ids.size() < num && (!node_info.empty());)
            {
                int index = rand() % node_info.size();
                auto &node = node_info.at(index);
                if (discard_comparator(node.height, height_baseline))
                {
                    continue;
                }
                send_node_ids.push_back(node_info[index].base58address);
                node_info.erase(node_info.cbegin() + index);
            }
        }
        else
        {
            std::vector<std::string> pledge = pledge_addr;
            for (; !pledge.empty() && send_node_ids.size() < num && (!node_info.empty());)
            {
                int index = rand() % node_info.size();
                auto &node = node_info.at(index);
                if (discard_comparator(node.height, height_baseline))
                {
                    node_info.erase(node_info.cbegin() + index);
                    continue;
                }
                auto it = std::find(pledge.cbegin(), pledge.cend(), node.base58address);
                if (pledge.cend() == it)
                {
                    node_info.erase(node_info.cbegin() + index);
                    continue;
                }
                pledge.erase(it);
                send_node_ids.push_back(node_info[index].base58address);
                node_info.erase(node_info.cbegin() + index);
            }
            for (; send_node_ids.size() < num && (!nodes.empty());)
            {
                int index = rand() % nodes.size();
                auto &node = nodes.at(index);
                if (discard_comparator(node.height, height_baseline))
                {
                    nodes.erase(nodes.cbegin() + index);
                    continue;
                }
                auto it = std::find(send_node_ids.cbegin(), send_node_ids.cend(), node.base58address);
                if (send_node_ids.cend() != it)
                {
                    nodes.erase(nodes.cbegin() + index);
                    continue;
                }
                send_node_ids.push_back(nodes[index].base58address);
                nodes.erase(nodes.cbegin() + index);
            }
        }
    }
    if (send_node_ids.size() < num * 0.5)
    {
        std::vector<Node> nodelist = peer_node->get_nodelist();
        for(auto &it : nodelist)
        {
            DEBUGLOG("Node height:{}", it.height);
        }
        send_node_ids.clear();
        ret = -3;
        ERRORLOG("send_node_ids size: {}",send_node_ids.size());
        return ret;
    }
    return 0;
}

void SyncBlock::SetFastSync(uint64_t sync_start_height)
{
    run_fast_sync = true;
    new_sync_fail_height = sync_start_height;
}

bool SyncBlock::GetFastSyncSumHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height,
                                       std::vector<FastSyncBlockHashs> &request_hashs, std::vector<std::string> &ret_node_ids, uint64_t chain_height)
{
    DEBUGLOG("fast sync start_sync_height: {}   tend_sync_height: {}", start_sync_height, end_sync_height);
    request_hashs.clear();
    ret_node_ids.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, send_num * 0.8, msg_id))
    {
        return false;
    }
    for (auto &node_id : send_node_ids)
    {
        DEBUGLOG("fast sync get block hash from {}", node_id);
        SendFastSyncGetHashReq(node_id, msg_id, start_sync_height, end_sync_height);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if (ret_datas.size() < send_num * 0.5)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
            return false;
        }
    }
    FastSyncGetHashAck ack;
    std::map<std::string, fast_sync_helper> sync_hashs;
    std::map<uint64_t, std::set<std::string>> fast_sync_hashs;
    uint32_t ret_num = 0;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        auto ret_height_data = ack.hashs();

        uint64_t height = 0;
        std::set<string> height_hashs;

        for(const auto& ret_height_hashs : ret_height_data)
        {
            height = ret_height_hashs.height();
            for(const auto& hash : ret_height_hashs.hashs())
            {
                height_hashs.insert(hash);
                auto it = sync_hashs.find(hash);
                if (sync_hashs.end() == it)
                {
                    fast_sync_helper helper = {0, std::set<string>(), ret_height_hashs.height()};
                    sync_hashs.insert(make_pair(hash, helper));
                }
                auto &value = sync_hashs.at(hash);
                value.hits = value.hits + 1;
                value.ids.insert(ack.self_node_id());
            }
            fast_sync_hashs.insert(std::make_pair(height, height_hashs));

        }
        ++ret_num;
    }

    std::vector<decltype(sync_hashs.begin())> remain_sync_hashs;
    std::vector<std::string> rollback_sync_hashs;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollback_block_data;

    DBReader db_reader;
    uint64_t current_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    std::map<uint64_t, std::vector<std::string>> block_height_hashes;
    if(!GetHeightBlockHash(start_sync_height, end_sync_height , block_height_hashes))
    {
        ERRORLOG("query database fail");
        return false;
    }
    
    uint64_t need_rollback_height = 0;
    for(auto iter = sync_hashs.begin(); iter != sync_hashs.end(); ++iter)
    {
        std::vector<std::string> local_hashs;
        auto result = block_height_hashes.find(iter->second.height);
        if(result != block_height_hashes.end())
        {
            local_hashs = result->second;
        }
        
        bool byzantine_success = iter->second.hits >= ret_num * 0.66;//check_byzantine(ret_num, iter->second.hits);
        if (!byzantine_success)
        {
            std::string block_raw;
            if (DBStatus::DB_SUCCESS == db_reader.GetBlockByBlockHash(iter->first, block_raw))
            {
                CBlock block;
                if(!block.ParseFromString(block_raw))
                {
                    ERRORLOG("block parse fail");
                    return false;
                }
                if((current_time - block.time()) > stability_time)
                {
                    AddBlockToMap(block, rollback_block_data);
                    need_rollback_height = block.height();
                    break;
                }
            }
        }
        
        if(byzantine_success)
        {
            std::vector<std::string> self_block_hashes;
            if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(iter->second.height, iter->second.height, self_block_hashes))
            {
                ERRORLOG("GetBlockHashesByBlockHeight error");
                return false;
            }
            std::sort(self_block_hashes.begin(), self_block_hashes.end());

            auto find = fast_sync_hashs.find(iter->second.height);
            if(find != fast_sync_hashs.end())
            {
                CBlock block;
                std::vector<std::string> diff_hashes;
                std::set_difference(self_block_hashes.begin(), self_block_hashes.end(), find->second.begin(), find->second.end(), std::back_inserter(diff_hashes));
                for(auto diff_hash: diff_hashes)
                {
                    block.Clear();
                    string strblock;
                    auto res = db_reader.GetBlockByBlockHash(diff_hash, strblock);
                    if (DBStatus::DB_SUCCESS != res)
                    {
                        ERRORLOG("GetBlockByBlockHash failed");
                        return false;
                    }
                    block.ParseFromString(strblock);
                    
                    uint64_t tmp_height = block.height();
                    if ((tmp_height < chain_height) && chain_height - tmp_height > 10)
                    {
                        AddBlockToMap(block, rollback_block_data);
                        need_rollback_height = tmp_height;
                        break;
                    }
                }
            }       
        }

        if (byzantine_success && (local_hashs.empty() || std::find(local_hashs.begin(), local_hashs.end(), iter->first) == local_hashs.end()))
        {
            remain_sync_hashs.push_back(iter);
        }
    }

    if (!rollback_block_data.empty())
    {
        int peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
        if(sync_send_fast_sync_num < peer_node_size)
        {
            ERRORLOG("sync_send_fast_sync_num:{} < peer_node_size:{}", sync_send_fast_sync_num, peer_node_size);
            new_sync_fail_height = need_rollback_height;
            in_fast_sync_fail = true;
            sync_send_fast_sync_num = peer_node_size;
            return false;
        }

        DEBUGLOG("==== fast sync rollback ====");   
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollback_block_data);
        return false;
    }

    auto remain_begin = remain_sync_hashs.begin();
    auto remain_end = remain_sync_hashs.end();
    if(remain_begin == remain_end)
    {
        return false;
    }
    std::set<std::string> ids = (*remain_begin)->second.ids;
    for(++remain_begin; remain_begin != remain_end; ++remain_begin)
    {
        std::set<std::string> intersect_ids;
        auto& next_ids = (*remain_begin)->second.ids;
        std::set_intersection(next_ids.begin(), next_ids.end()
                                    , ids.begin(), ids.end()
                                    ,std::inserter(intersect_ids, intersect_ids.begin())
                                    );
        ids = intersect_ids;
    }

    ret_node_ids = std::vector<std::string>(ids.begin(), ids.end());

    for(auto remain_sync_hash : remain_sync_hashs)
    {
        uint64_t height = remain_sync_hash->second.height;
        auto find_result = find_if(request_hashs.begin(), request_hashs.end(), 
                            [height](const FastSyncBlockHashs& entity)
                            {
                                return height == entity.height();
                            } 
                        );
        if(find_result == request_hashs.end())
        {
            FastSyncBlockHashs fastSyncBlockHashs;
            fastSyncBlockHashs.set_height(height);
            fastSyncBlockHashs.add_hashs(remain_sync_hash->first);
            request_hashs.push_back(fastSyncBlockHashs);
        }
        else
        {
            find_result->add_hashs(remain_sync_hash->first);
        }
    }
    return !ids.empty();
}

bool SyncBlock::GetFastSyncBlockData(const std::string &send_node_id, const std::vector<FastSyncBlockHashs> &request_hashs, uint64_t chain_height)
{
    if (request_hashs.empty())
    {
        DEBUGLOG("no byzantine data available");
        return true;
    }
    
    std::string msg_id;
    if (!GLOBALDATAMGRPTR.CreateWait(30, 1, msg_id))
    {
        DEBUGLOG("create wait fail");
        return false;
    }
    SendFastSyncGetBlockReq(send_node_id, msg_id, request_hashs);
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        ERRORLOG("wait fast sync data for {} fail", send_node_id);
        return false;
    }
    if (ret_datas.empty())
    {
        DEBUGLOG("return data empty");
        return false;
    }
    FastSyncGetBlockAck ack;
    if (!ack.ParseFromString(ret_datas.at(0)))
    {
        DEBUGLOG("FastSyncGetBlockAck parse fail");
        return false;
    }
    CBlock block;
    CBlock hash_block;
    std::vector<std::string> block_hashes;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> fast_sync_block_data;
    auto fast_sync_blocks = ack.blocks();
    std::sort(fast_sync_blocks.begin(), fast_sync_blocks.end(), [](const FastSyncBlock& b1, const FastSyncBlock& b2){return b1.height() < b2.height();});
    for (auto &blocks : fast_sync_blocks)
    {
        for(auto & block_raw : blocks.blocks())
        {
            if (block.ParseFromString(block_raw))
            {
                hash_block = block;
                hash_block.clear_hash();
                hash_block.clear_sign();
                if (block.hash() != getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                DEBUGLOG("FastSync  block.height(): {} \tblock.hash(): {}", block.height(), block.hash());
                AddBlockToMap(block, fast_sync_block_data);
                block_hashes.push_back(block.hash());
            }
        }
    }
    DBReader reader;
    uint64_t top;
    if (reader.GetBlockTop(top) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetBlockTop fail");
        return false;
    }
    
    global::ca::SaveType sync_type = GetSaveSyncType(top, chain_height);
    MagicSingleton<BlockHelper>::GetInstance()->AddFastSyncBlock(fast_sync_block_data, sync_type);
    return true;
}

int SyncBlock::GetSyncSumHashNode(uint64_t pledge_addr_size, const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height,
                                   std::map<uint64_t, uint64_t> &need_sync_heights, std::vector<std::string> &ret_node_ids, uint64_t &chain_height, uint64_t new_sync_sned_num)
{
    DEBUGLOG("GetSyncSumHashNode start_sync_height: {}   end_sync_height: {}", start_sync_height, end_sync_height);
    int ret = 0;
    need_sync_heights.clear();
    ret_node_ids.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();

    auto peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
    double acceptance_rate = 0.8;
    if(new_sync_sned_num > peer_node_size)
    {
        acceptance_rate = 0.9;
    }
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_num * acceptance_rate, msg_id))
    {
        ret = -1;
        return ret;
    }
    for (auto &node_id : send_node_ids)
    {
        SendSyncGetSumHashReq(node_id, msg_id, start_sync_height, end_sync_height);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if (ret_datas.empty() || ret_datas.size() < send_num / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
            ret = -2;
            return ret;
        }
    }

    std::vector<uint64_t> ret_node_tops;
    SyncGetSumHashAck ack;
    DBReader db_reader;

    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> consensus_map;
    std::vector<std::pair<std::string, uint32_t>> sync_hash_datas;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        ret_node_ids.push_back(ack.self_node_id());
        ret_node_tops.push_back(ack.node_block_height());
        for (auto &sync_sum_hash : ack.sync_sum_hashes())
        {
            auto find_sum_hash = consensus_map.find(sync_sum_hash.hash());
            if(find_sum_hash == consensus_map.end())
            {
                consensus_map.insert(std::make_pair(sync_sum_hash.hash(), std::make_pair(sync_sum_hash.start_height(), std::vector<std::string>() )));
            }
            auto &value = consensus_map.at(sync_sum_hash.hash());
            value.second.push_back(ack.self_node_id());

            

            std::string key = std::to_string(sync_sum_hash.start_height()) + "_" + std::to_string(sync_sum_hash.end_height()) + "_" + sync_sum_hash.hash();
            auto it = std::find_if(sync_hash_datas.begin(), sync_hash_datas.end(), [&key](const std::pair<std::string, uint32_t>& sync_hash_data){return key == sync_hash_data.first;}); //sync_hash_datas.find(key);
            if (sync_hash_datas.end() == it)
            {
                sync_hash_datas.emplace_back(key, 1);
            }
            else
            {
                it->second += 1;
            }
        }
    }
    std::sort(ret_node_tops.begin(), ret_node_tops.end());
    int verify_num = ret_node_tops.size() * 0.66;
    if (verify_num >= ret_node_tops.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verify_num, ret_node_tops.size());
        ret = -3;
        return ret;
    }
    chain_height = ret_node_tops.at(verify_num);
    std::set<uint64_t> heights;
    std::string hash;
    uint64_t start_height = 0;
    uint64_t end_height = 0;
    std::vector<std::string> block_hashes;
    std::vector<std::string> data_key;

    std::sort(sync_hash_datas.begin(),sync_hash_datas.end(),[](const std::pair<std::string, uint32_t> v1, const std::pair<std::string, uint32_t> v2){
        std::vector<std::string> v1Height;
        StringUtil::SplitString(v1.first, "_", v1Height);
        std::vector<std::string> v2Height;
        StringUtil::SplitString(v2.first, "_", v2Height);

        return v1Height.at(0) < v2Height.at(0);
    });

    for (auto &sync_hash_data : sync_hash_datas)
    {
        data_key.clear();
        StringUtil::SplitString(sync_hash_data.first, "_", data_key);
        if (data_key.size() < 3)
        {
            continue;
        }
        start_height = std::stoull(data_key.at(0));
        end_height = std::stoull(data_key.at(1));

        if (sync_hash_data.second < verify_num)
        {
            DEBUGLOG("verify error, error height: {} sync_hash_data.second: {} verify_num: {}", start_height, sync_hash_data.second, verify_num);
            uint64_t self_node_height = 0;
            if(DBStatus::DB_SUCCESS != db_reader.GetBlockTop(self_node_height))
            {
                ERRORLOG("(GetSyncSumHashNode) GetBlockTop failed !");
                ret = -4;
                return ret;
            }

            if(start_height > self_node_height)
            {
                ERRORLOG("error height > self height  error height:{}  self_node_height:{}" ,start_height, self_node_height);
                if(new_sync_sned_num >= peer_node_size)
                {
                    new_sync_fail_height = start_height;
                    run_fast_sync = true;   
                    INFOLOG("new sync fail,next round will run fast sync"); 
                }

                continue;
            }
            if(chain_height >= self_node_height && chain_height - self_node_height <= 500)
            {

                if(new_sync_sned_num >= peer_node_size)
                {
                    ERRORLOG("peer_node_size{}   new_sync_sned_num{}   ret_datas.size(){}", peer_node_size, new_sync_sned_num, ret_datas.size());
                    std::vector<std::pair<std::string, std::vector<std::string>>> node_lists;
                    std::string self_sumhash;

                    std::vector<std::string> self_block_hashes;
                    auto res = db_reader.GetBlockHashsByBlockHeight(start_height, self_block_hashes);
                    if(res != DBStatus::DB_SUCCESS)
                    {
                        ERRORLOG("get {}  block hash failed !", start_height);
                        return -5;
                    }

                    if(!SumHeightHash(self_block_hashes, self_sumhash))
                    {
                        ERRORLOG("get {} block sum hash failed !", start_height);
                        return -6;
                    }
                    uint32_t same_node_height = 0;
                    
                    for(auto consensus : consensus_map)
                    {
                        if(consensus.second.first == start_height)
                        {
                            node_lists.emplace_back(std::make_pair(consensus.first, std::move(consensus.second.second)));
                        }
                        sort(node_lists.begin(), node_lists.end(), [](const std::pair<std::string, std::vector<std::string>> list1, const std::pair<std::string, std::vector<std::string>> list2){
                            return list1.second.size() > list2.second.size();
                        });
                    }
                    if(self_sumhash != node_lists.at(0).first)
                    {

                        ret = GetSyncBlockBySumHashNode(node_lists.at(0).second, start_height, end_height, self_node_height, chain_height, new_sync_sned_num);
                        if(ret != 0)
                        {
                            ERRORLOG("GetSyncBlockBySumHashNode failed !");
                            return -6;
                        }
                    }
                    else 
                    {
                        continue;
                    }
                    // }
                }
                else 
                {
                    return -7;
                }
            }
            break;
        }

        hash.clear();
        block_hashes.clear();
        GetHeightBlockHash(start_height, end_height, block_hashes);
        SumHeightHash(block_hashes, hash);
        if (data_key.at(2) == hash)
        {
            continue;
        }
        for (uint64_t i = start_height; i <= end_height; i++)
        {
            heights.insert(i);
        }
    }

    int no_verify_height = 10;
    int64_t start = -1;
    int64_t end = -1;
    for (auto value : heights)
    {
        if (-1 == start && -1 == end)
        {
            start = value;
            end = start;
        }
        else
        {
            if (value != (end + 1))
            {
                need_sync_heights.insert(std::make_pair(start, end));
                start = -1;
                end = -1;
            }
            else
            {
                end = value;
            }
        }
    }
    if (-1 != start && -1 != end)
    {
        need_sync_heights.insert(std::make_pair(start, end));
    }
    if (end_sync_height >= (chain_height - no_verify_height))
    {
        if (chain_height > no_verify_height)
        {
            need_sync_heights.insert(std::make_pair(chain_height - no_verify_height, chain_height));
            need_sync_heights.insert(std::make_pair(chain_height, chain_height + no_verify_height));
        }
        else
        {
            need_sync_heights.insert(std::make_pair(1, chain_height + no_verify_height));
        }
    }
    uint64_t self_node_height = 0;

    db_reader.GetBlockTop(self_node_height);

    return 0;
}

int SyncBlock::GetSyncBlockBySumHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height, uint64_t self_node_height, uint64_t chain_height, uint64_t new_sync_sned_num)
{
    int ret = 0;
    std::string msg_id;
    uint64_t succent_count = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_node_ids.size() * 0.8, msg_id))
    {
        ret = -1;
        return ret;
    }
    for (auto &node_id : send_node_ids)
    {
        DEBUGLOG("new sync get block hash from {}", node_id);
        SendSyncGetHeightHashReq(node_id, msg_id, start_sync_height, end_sync_height);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(ret_datas.size() < send_node_ids.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
            ret = -2;
            return ret;
        }
    }

    SyncGetHeightHashAck ack;
    std::set<std::string> verify_set;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            continue;
        }
        std::string verify_str = "";
        for (auto &key : ack.block_hashes())
        {
            verify_str += key;
        }
        verify_set.insert(verify_str);

    }
    if(verify_set.size() != 1)
    {
        DEBUGLOG("Byzantium failed");
        return -3;
    }

    DBReader db_reader;
    std::vector<std::string> self_block_hashes;
    if(DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(start_sync_height, end_sync_height, self_block_hashes))
    {
        ret = -4;
        DEBUGLOG("GetBlockHashesByBlockHeight failed");
        return ret;
    }

    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollback_block_data;
    CBlock block;
    std::vector<std::string> diff_hashes;
    std::set_difference(self_block_hashes.begin(), self_block_hashes.end(), ack.block_hashes().begin(), ack.block_hashes().end(), std::back_inserter(diff_hashes));
    for(auto diff_hash: diff_hashes)
    {
        block.Clear();
        string strblock;
        auto res = db_reader.GetBlockByBlockHash(diff_hash, strblock);
        if (DBStatus::DB_SUCCESS != res)
        {
            DEBUGLOG("GetBlockByBlockHash failed");
            return -5;
        }
        block.ParseFromString(strblock);
        
        uint64_t tmp_height = block.height();
        if ((tmp_height < chain_height) && chain_height - tmp_height > 10)
        {
            AddBlockToMap(block, rollback_block_data);
            continue;
        }

        if(self_node_height - block.height() > 10)
        {
            uint64_t curtime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
            if(curtime - block.time() > 100000000) //100s
            {
                if(new_sync_sned_num < MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size())
                {
                    return -5;
                }
                AddBlockToMap(block, rollback_block_data);
            }
        }
    }

    if(!rollback_block_data.empty())
    {
        DEBUGLOG("==== new sync rollback ====");
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollback_block_data);
        return -7;
    }

    std::vector<std::string> need_sync_hashes;
    std::set_difference(ack.block_hashes().begin(), ack.block_hashes().end(), self_block_hashes.begin(), self_block_hashes.end(),  std::back_inserter(need_sync_hashes));
    ret = GetSyncBlockData(send_node_ids, need_sync_hashes, chain_height);
    if(ret != 0)
    {
        DEBUGLOG("GetSyncBlockData failed {}", ret);
        return -8;
    }

    return 0;
}

int SyncBlock::GetFromZeroSyncSumHashNode(const std::vector<std::string> &send_node_ids, const std::vector<uint64_t>& send_heights, uint64_t self_node_height, std::set<std::string> &ret_node_ids, std::map<uint64_t, std::string>& sum_hashes)
{
    ret_node_ids.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();

    auto peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
    double acceptance_rate = 0.8;
    if(sync_send_zero_sync_num > peer_node_size)
    {
        acceptance_rate = 0.9;
    }

    if (!GLOBALDATAMGRPTR.CreateWait(90, send_num * acceptance_rate, msg_id))
    {
        return -1;
    }

    for (auto &node_id : send_node_ids)
    {
        DEBUGLOG("get from zero sync sum hash from {}", node_id);
        SendFromZeroSyncGetSumHashReq(node_id, msg_id, send_heights);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if (ret_datas.empty() || ret_datas.size() < send_num / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
            return -2;
        }
    }
    
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sum_hash_datas;
    int success_count = 0;
    for (auto &ret_data : ret_datas)
    {
        SyncFromZeroGetSumHashAck ack;
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        if (ack.code() == 0)
        {
            if(sync_send_zero_sync_num >= peer_node_size)
            {
                ++success_count;
            }
            continue;
        }
        ++success_count;
        auto ret_sum_hashes = ack.sum_hashes();
        for(const auto& sum_hash : ret_sum_hashes)
        {
            const auto& hash = sum_hash.hash();
            auto height = sum_hash.height();
            

            auto found = sum_hash_datas.find(hash);
            if (found == sum_hash_datas.end())
            {
                sum_hash_datas.insert(std::make_pair(hash, std::make_pair(height, std::vector<std::string>())));
                continue;
            }
            auto& content = found->second;
            content.second.push_back(ack.self_node_id());  
        }

    }

    uint64_t back_num = send_num * 0.66;
    bool byzantine_success = success_count > back_num;
    if(sync_send_zero_sync_num >= peer_node_size)
    {
        back_num = send_num * 0.9;
        byzantine_success = success_count >= back_num;
    }
    // if (!check_byzantine(send_num,success_count))
    if(!byzantine_success)
    {
        ERRORLOG("check_byzantine error, send_num = {} , success_count = {}", send_num, success_count);
        return -3;
    }


    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> byzantine_error_heights;
    for(const auto& sum_hash_data : sum_hash_datas)
    {
        bool byzantine_success = sum_hash_data.second.second.size() >= success_count * 0.66;
        // if (check_byzantine(success_count, sum_hash_data.second.second))
        if(byzantine_success)
        {
            sum_hashes[sum_hash_data.second.first] = sum_hash_data.first;
            for(const auto& t : sum_hash_data.second.second)
            {
                ret_node_ids.insert(std::move(t));
            }
        }
        else if(sync_send_zero_sync_num >= peer_node_size)
        {
            //The whole network Byzantium failed     Only the last 100 heights are synchronized
            if(sum_hash_data.second.first > self_node_height / 100 * 100 + 100)
            {
                continue;
            }
            
            auto find = sum_hashes.find(sum_hash_data.second.first);
            if(find != sum_hashes.end())
            {
                continue;
            }

                                 //sum hash         node ids  
            std::vector<std::pair<std::string, std::vector<std::string>>> node_ids;
            for(const auto& sum_hash: sum_hash_datas)
            {
                if(sum_hash.second.first == sum_hash_data.second.first)
                {
                    node_ids.emplace_back(make_pair(sum_hash.first, std::move(sum_hash.second.second)));
                }
            }

            std::sort(node_ids.begin(), node_ids.end(), [](const std::pair<std::string, std::vector<std::string>>& list1, const std::pair<std::string, std::vector<std::string>>& list2){
                // return list1.size() > list2.size();
                return list1.second.size() > list2.second.size();
            });

            sum_hashes.clear();
            ret_node_ids.clear();
            
            sum_hashes[sum_hash_data.second.first] = node_ids[0].first;
            for(const auto& t : node_ids[0].second)
            {
                ret_node_ids.insert(std::move(t));
            }

            break;
        }
    }

    // for(const auto& t: ret_node_ids)
    // {
    //     cout << "ret_node_ids: " << t << std::endl;
    // }
    if (sum_hashes.empty())
    {
        return -4;
    }
    
    return 0;
}

int SyncBlock::GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sum_hashes, std::vector<uint64_t> &send_heights, std::set<std::string> &send_node_ids, uint64_t self_node_height)
{
    int ret = 0;
    if (send_node_ids.empty() || sum_hashes.empty())
    {
        return -1;
    }

    // std::vector<std::string> send_node_ids;

    // for(const auto& t : set_send_node_ids)  
    // {
    //     send_node_ids.emplace_back(std::move(t));
    // }

    auto send_node_size = send_node_ids.size();
    auto send_hash_size = sum_hashes.size();
    if (send_node_size > send_hash_size)
    {
        for (auto diff = send_node_size - send_hash_size; diff > 0; --diff)
        {
            // send_node_ids.pop_back();
            auto lastElement = std::prev(send_node_ids.end());
            send_node_ids.erase(lastElement);
        }
        
    }
    // else if (send_node_size < send_hash_size)
    // {
    //     for (auto diff =  send_hash_size - send_node_size; diff > 0; --diff)
    //     {
    //         int index = rand() % send_node_ids.size();
    //         auto &node_id = send_node_ids.at(index);
    //         send_node_ids.push_back(node_id);            
    //     }
    // }

    // fout << "------------------------------------------------------------" << std::endl;
    // for(const auto& t: send_node_ids)
    // {
    //     fout << "send_node_id: " << t << std::endl;
    // }

    if (send_node_ids.size() != sum_hashes.size())
    {
        // fout << "send_node_ids.size(): " << send_node_ids.size() << "\tsum_hashes.size()" << sum_hashes.size() << std::endl;
        DEBUGLOG("send_node_ids.size() != sum_hashes.size(), {}:{}", send_node_ids.size(), sum_hashes.size());
        return -2;
    }
    
    
    std::string msg_id;
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_node_ids.size(), msg_id))
    {
        return -3;
    }

    int send_node_index = 0;
    for(const auto& sum_hash_item : sum_hashes)
    {
        // auto node_id = send_node_ids[send_node_index];
        auto node_id  = std::next(send_node_ids.begin(), send_node_index);
        auto sum_hash_height = sum_hash_item.first;
        // fout << "node_id: " << *node_id << std::endl; 
        SendFromZeroSyncGetBlockReq(*node_id, msg_id, sum_hash_height);
        DEBUGLOG("from zero sync get block at height {} from {}", sum_hash_height, *node_id);
        ++send_node_index;
    }
    
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        DEBUGLOG("wait sync block data time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
    }
    
    DBReader db_reader; 
    std::vector<uint64_t> success_heights;
    
    for(const auto& ret_data : ret_datas)
    {
        std::map<uint64_t, std::set<CBlock, CBlockCompare>> sync_block_data;
        SyncFromZeroGetBlockAck ack;
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        
        auto blockraws = ack.blocks();
        std::map<uint64_t, std::vector<std::string>> sum_hash_check_data; 
        std::vector<CBlock> sum_hash_data;
        for(const auto& block_raw : blockraws)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                ERRORLOG("block parse fail");
                break;
            }

            auto block_height = block.height();
            auto found = sum_hash_check_data.find(block_height);
            if(found == sum_hash_check_data.end())
            {
                sum_hash_check_data[block_height] = std::vector<std::string>();
            }
            auto& sum_hashes_vector = sum_hash_check_data[block_height];
            sum_hashes_vector.push_back(block.hash());
            
            sum_hash_data.push_back(block);

        }

        std::string cal_sum_hash;
        SumHeightsHash(sum_hash_check_data, cal_sum_hash);
        auto found = sum_hashes.find(ack.height());
        if(found == sum_hashes.end())
        {
            DEBUGLOG("fail to get sum hash at height {}", ack.height());
            continue;
        }
        if (cal_sum_hash != found->second)
        {
            ERRORLOG("check sum hash at height {} fail, cal_sum_hash:{}, sum_hash:{}", ack.height(), cal_sum_hash, found->second);
            continue;
        }

        for(const auto& hash_check : sum_hash_check_data)
        {
            std::vector<std::string> block_hashes;
            if(self_node_height >= hash_check.first)
            {
                if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(hash_check.first, hash_check.first, block_hashes))
                {
                    return -4;
                }
                std::string self_sum_hash;
                std::string other_sum_hash;

                auto find_height = sum_hash_check_data.find(hash_check.first);
                if(find_height != sum_hash_check_data.end())
                {
                    if(SumHeightHash(block_hashes, self_sum_hash) && SumHeightHash(find_height->second, other_sum_hash))
                    {
                        if(self_sum_hash != other_sum_hash)
                        {
                            std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollback_block_data;
                            CBlock block;
                            std::vector<std::string> diff_hashes;
                            std::set_difference(block_hashes.begin(), block_hashes.end(), find_height->second.begin(), find_height->second.end(), std::back_inserter(diff_hashes));
                            for(auto diff_hash: diff_hashes)
                            {
                                block.Clear();
                                string strblock;
                                auto res = db_reader.GetBlockByBlockHash(diff_hash, strblock);
                                if (DBStatus::DB_SUCCESS != res)
                                {
                                    DEBUGLOG("GetBlockByBlockHash failed");
                                    return -5;
                                }
                                block.ParseFromString(strblock);
                                
                                AddBlockToMap(block, rollback_block_data);
                            }

                            if(!rollback_block_data.empty())
                            {
                                int peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
                                if(sync_send_zero_sync_num < peer_node_size)
                                {
                                    DEBUGLOG("sync_send_zero_sync_num:{} < peer_node_size:{}", sync_send_zero_sync_num, peer_node_size);
                                    sync_send_zero_sync_num = peer_node_size;
                                    return -6;
                                }
                                DEBUGLOG("==== GetFromZeroSyncBlockData rollback ====");
                                MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollback_block_data);
                            }
                            // DEBUGLOG("self_sum_hash: {}", self_sum_hash);
                            // DEBUGLOG("other_sum_hash: {}", other_sum_hash);
                            // new_sync_fail_height = hash_check.first;
                            // run_fast_sync = true;   
                            // INFOLOG("GetFromZeroSyncBlockData height:{} fail,next round will run fast sync", hash_check.first); 
                            // return -7;
                        }
                    }
                }
            }
        }

        success_heights.push_back(ack.height());
        for(const auto& block : sum_hash_data)
        {
            AddBlockToMap(block, sync_block_data);
        }
        {
            std::lock_guard<std::mutex> lock(sync_from_zero_cache_mutex);
            sync_from_zero_cache[ack.height()] = sync_block_data;
        }
    }
    if (success_heights.empty())
    {
        return -8;
    }
        
    for(auto height : success_heights)
    {
        auto found = std::find(send_heights.begin(), send_heights.end(), height);
        if (found != send_heights.end())
        {
            send_heights.erase(found);
        }
        
    }
    sync_from_zero_reserve_heights.clear();
    for(auto fail_height : send_heights)
    {
        sync_from_zero_reserve_heights.push_back(fail_height);
    }


    if (send_heights.empty())
    {
        auto from_zero_sync_add_thread = std::thread(
                [this]()
                {
                    std::lock_guard<std::mutex> lock(sync_from_zero_cache_mutex);
                    INFOLOG("from_zero_sync_add_thread start");
                    for(const auto& cache : this->sync_from_zero_cache)
                    {
                        MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(cache.second, global::ca::SaveType::SyncFromZero);
                    }
                    sync_from_zero_cache.clear();
                }
        );
        from_zero_sync_add_thread.detach();
    }
    return 0;
}


int SyncBlock::GetSyncBlockHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height,
                                     uint64_t end_sync_height, uint64_t self_node_height, uint64_t chain_height,
                                     std::vector<std::string> &ret_node_ids, std::vector<std::string> &req_hashes, uint64_t new_sync_sned_num)
{
    int ret = 0;
    std::string msg_id;
    uint64_t succent_count = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_node_ids.size() * 0.8, msg_id))
    {
        ret = -1;
        return ret;
    }
    for (auto &node_id : send_node_ids)
    {
        DEBUGLOG("new sync get block hash from {}", node_id);
        SendSyncGetHeightHashReq(node_id, msg_id, start_sync_height, end_sync_height);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(ret_datas.size() < send_node_ids.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
            ret = -2;
            return ret;
        }
    }
    SyncGetHeightHashAck ack;
    std::map<std::string, std::set<std::string>> sync_block_hashes;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            continue;
        }
        succent_count++;
        for (auto &key : ack.block_hashes())
        {
            auto it = sync_block_hashes.find(key);
            if (sync_block_hashes.end() == it)
            {
                sync_block_hashes.insert(std::make_pair(key, std::set<std::string>()));
            }
            auto &value = sync_block_hashes.at(key);
            value.insert(ack.self_node_id());
        }
    }

    std::set<std::string> nodes;
    std::vector<std::string> intersection_nodes;
    std::set<std::string> verify_hashes;
    req_hashes.clear();
    size_t verify_num = succent_count / 5 * 4;
    // Put the block hash greater than 60% into the array
    std::string strblock;
    std::vector<std::string> exits_hashes;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollback_block_data;
    CBlock block;
    DBReader db_reader;
    for (auto &sync_block_hash : sync_block_hashes)
    {
        strblock.clear();
        auto res = db_reader.GetBlockByBlockHash(sync_block_hash.first, strblock);
        if (DBStatus::DB_SUCCESS == res)
        {
            exits_hashes.push_back(sync_block_hash.first);
        }
        else if(DBStatus::DB_NOT_FOUND != res)
        {
            ret = -3;
            return ret;
        }
        if (sync_block_hash.second.size() >= verify_num)
        {

            if (DBStatus::DB_NOT_FOUND == res)
            {
                verify_hashes.insert(sync_block_hash.first);
                if (nodes.empty())
                {
                    nodes = sync_block_hash.second;
                }
                else
                {
                    std::set_intersection(nodes.cbegin(), nodes.cend(), sync_block_hash.second.cbegin(), sync_block_hash.second.cend(), std::back_inserter(intersection_nodes));
                    nodes.insert(intersection_nodes.cbegin(), intersection_nodes.cend());
                    intersection_nodes.clear();
                }
            }
        }

        // When the number of nodes where the block is located is less than 80%, and the block exists locally, the block is rolled back
        else
        {
            if (DBStatus::DB_SUCCESS == res && block.ParseFromString(strblock))
            {
                if (block.height() < chain_height - 10)
                {
                    AddBlockToMap(block, rollback_block_data);
                    continue;
                }
            }

            if(self_node_height - block.height() > 10)
            {
                uint64_t curtime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
                if(curtime - block.time() > 100000000) //100s
                {
                    if(new_sync_sned_num < MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size())
                    {
                        return -4;
                    }
                    AddBlockToMap(block, rollback_block_data);
                }
            }

        }
    }

    std::vector<std::string> v_diff;
    uint64_t end_height = end_sync_height > self_node_height ? self_node_height : end_sync_height;
    if(end_height >= start_sync_height)
    {

        //Get all block hashes in the local height range, determine whether they are in the trusted list, and roll them back when they are no longer in the trusted list
        std::vector<std::string> block_hashes;
        if(DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(start_sync_height, end_height, block_hashes))
        {
            ret = -5;
            return ret;
        }
        std::sort(block_hashes.begin(), block_hashes.end());
        std::sort(exits_hashes.begin(), exits_hashes.end());
        std::set_difference(block_hashes.begin(), block_hashes.end(), exits_hashes.begin(), exits_hashes.end(), std::back_inserter(v_diff));
    }

    for (const auto & it : v_diff)
    {

        block.Clear();
        std::string().swap(strblock);
        auto ret_status = db_reader.GetBlockByBlockHash(it, strblock);
        if (DBStatus::DB_SUCCESS != ret_status)
        {
            continue;
        }
        block.ParseFromString(strblock);

        //It will only be rolled back when the height of the block to be added is less than the maximum height on the chain of -10
        uint64_t tmp_height = block.height();
        if ((tmp_height < chain_height) && chain_height - tmp_height > 10)
        {
            AddBlockToMap(block, rollback_block_data);
            continue;
        }

        if(self_node_height - tmp_height > 10)
        {
            uint64_t curtime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
            if(curtime - block.time() > 100000000) //100s
            {
                if(new_sync_sned_num < MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size())
                {
                    return -6;
                }
                AddBlockToMap(block, rollback_block_data);
            }
        }
    }

    if(!rollback_block_data.empty())
    {
        int peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
        if(new_sync_sned_num < peer_node_size)
        {
            DEBUGLOG("new_sync_sned_num:{} < peer_node_size:{}", new_sync_sned_num, peer_node_size);
            new_sync_sned_num = peer_node_size;
            return -7;
        }
        DEBUGLOG("==== new sync rollback ====");
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollback_block_data);
        return -8;
    }

    if (verify_hashes.empty())
    {
        return 0;
    }

    req_hashes.assign(verify_hashes.cbegin(), verify_hashes.cend());
    ret_node_ids.assign(nodes.cbegin(), nodes.cend());
    if(ret_node_ids.empty())
    {
        ret = -9;
        return ret;
    }
    return 0;
}

int SyncBlock::GetSyncBlockData(const std::vector<std::string> &send_node_ids, const std::vector<std::string> &req_hashes, uint64_t chain_height)
{
    int ret = 0;
    if (req_hashes.empty() || send_node_ids.empty())
    {
        return 0;
    }
    std::string msg_id;
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_node_ids.size() * 0.8, msg_id))
    {
        ret = -1;
        return ret;
    }
    for (auto &node_id : send_node_ids)
    {
        SendSyncGetBlockReq(node_id, msg_id, req_hashes);
        DEBUGLOG("new sync block from {}", node_id);
    }
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(ret_datas.empty())
        {
            ERRORLOG("wait sync block data time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
            ret = -2;
            return ret;
        }
    }

    CBlock block;
    CBlock hash_block;
    SyncGetBlockAck ack;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> sync_block_data;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        for (auto &block_raw : ack.blocks())
        {
            if (block.ParseFromString(block_raw))
            {
                if (req_hashes.cend() == std::find(req_hashes.cbegin(), req_hashes.cend(), block.hash()))
                {
                    continue;
                }
                hash_block = block;
                hash_block.clear_hash();
                hash_block.clear_sign();
                if (block.hash() != getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                AddBlockToMap(block, sync_block_data);
            }
        }
    }

    MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(sync_block_data, global::ca::SaveType::SyncNormal);

    return 0;
}

void SyncBlock::AddBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, CBlockCompare>> &sync_block_data)
{
    if (sync_block_data.end() == sync_block_data.find(block.height()))
    {
        sync_block_data.insert(std::make_pair(block.height(), std::set<CBlock, CBlockCompare>()));
    }
    auto &value = sync_block_data.at(block.height());
    value.insert(block);
}

bool SyncBlock::check_byzantine(int receive_count, int hit_count)
{
    const static std::unordered_map<int, std::set<int>> level_table = 
        {
            {1, {1}},
            {2, {2}},
            {3, {2, 3}},
            {4, {3, 4}},
            {5, {3, 4, 5}},
            {6, {4, 5, 6}},
            {7, {4, 5, 6, 7}},
            {8, {5, 6, 7, 8}},
            {9, {5, 6, 7, 8, 9}},
            {10, {6, 7, 8, 9, 10}}
        };
    auto end = level_table.end();
    auto found = level_table.find(receive_count);
    if(found != end && found->second.find(hit_count) != found->second.end())
    {
        DEBUGLOG("byzantine success total {} hit {}", receive_count, hit_count);
        return true;
    }
    DEBUGLOG("byzantine fail total {} hit {}", receive_count, hit_count);
    return false;
}

bool SyncBlock::NeedByzantineAdjustment(uint64_t chain_height, const vector<std::string> &pledge_addr,
                                        std::vector<std::string> &selected_addr)
{
    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    std::vector<std::string> qualifying_stake_nodes;
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sum_hash;

    return CheckRequirementAndFilterQualifyingNodes(chain_height, pledge_addr, nodes, qualifying_stake_nodes)
            && GetSyncNodeSumhashInfo(nodes, qualifying_stake_nodes, sum_hash)
            && GetSelectedAddr(sum_hash, selected_addr);
}

bool SyncBlock::GetSelectedAddr(map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sum_hash,
                                vector<std::string> &selected_addr)
{
    const static double filter_scale = 0.34;
    auto receive_num = static_cast<double>(sum_hash.size());
    DEBUGLOG("receive_num: {}", receive_num);

    for (auto iter = sum_hash.begin(); iter !=  sum_hash.end();)
    {
        const auto& [hits, _] = iter->second;
        double scale = static_cast<double>(hits) / receive_num;
        if (scale < filter_scale)
        {
            DEBUGLOG("erase sum hash {} which scale is {}", iter->first.substr(0, 6), scale);
            iter = sum_hash.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    DEBUGLOG("sum_hash size: {}", sum_hash.size());
    if (sum_hash.size() == 2)
    {
        auto& [first_hits, first_addrs] = sum_hash.begin()->second;
        auto& [last_hits, last_addrs] = sum_hash.rbegin()->second;
        DEBUGLOG("first hash {} hits {}; last hash {} hits {}",
                 sum_hash.begin()->first.substr(0, 6), first_hits, sum_hash.rbegin()->first.substr(0, 6), last_hits);
        if ((static_cast<double>(first_hits) / receive_num) >= (static_cast<double>(last_hits) / receive_num))
        {
            std::swap(selected_addr, first_addrs);
        }
        else
        {
            std::swap(selected_addr, last_addrs);
        }
        return true;
    }

    return false;
}

bool SyncBlock::GetSyncNodeSumhashInfo(const vector<Node> &nodes, const vector<string> &qualifying_stake_nodes,
                                       map<string, pair<uint64_t, vector<string>>> sum_hash)
{
    uint64_t byzantine_fail_tolerance = 5;
    if (nodes.size() < global::ca::kNeed_node_threshold)
    {
        byzantine_fail_tolerance = 0;
    }
    uint64_t receive_requirement = qualifying_stake_nodes.size() - byzantine_fail_tolerance;
    DEBUGLOG("receive_requirement size: {}", receive_requirement);

    string msg_id;
    if (!GLOBALDATAMGRPTR.CreateWait(60, receive_requirement, msg_id))
    {
        ERRORLOG("Create wait fail");
        return false;
    }
    for (const auto& addr : qualifying_stake_nodes)
    {
        DEBUGLOG("get sync node hash from {}", addr);
        SendSyncNodeHashReq(addr, msg_id);
    }

    vector<string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        ERRORLOG("wait sync node hash time out send:{} recv:{}", qualifying_stake_nodes.size(), ret_datas.size());
        return false;
    }

    SyncNodeHashAck ack;
    for (const auto& ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            ERRORLOG("SyncNodeHashAck parse fail");
            continue;
        }
        const auto& ret_hash = ack.hash();
        if (sum_hash.find(ret_hash) == sum_hash.end())
        {
            sum_hash[ret_hash] = {0, vector<string>()};
        }
        auto& [hits, addrs] = sum_hash.at(ret_hash);
        hits += 1;
        addrs.push_back(ack.self_node_id());
    }
    return true;
}

bool SyncBlock::CheckRequirementAndFilterQualifyingNodes(uint64_t chain_height, const vector<std::string> &pledge_addr,
                                                         const vector<Node> &nodes,
                                                         vector<std::string> &qualifying_stake_nodes)
{
    const static uint32_t higher_than_chain_height_bar = 3;
    uint32_t higher_than_chain_height_count = 0;

    for (const auto& node : nodes)
    {
        if (node.height > chain_height)
        {
            higher_than_chain_height_count++;
            if (higher_than_chain_height_count > higher_than_chain_height_bar)
            {
                DEBUGLOG("chain height isn't top hight in the network");
                return false;
            }
        }
        else if (node.height == chain_height)
        {
            const auto& node_addr = node.base58address;
            if (find(pledge_addr.cbegin(), pledge_addr.cend(), node_addr) != pledge_addr.cend())
            {
                qualifying_stake_nodes.push_back(node_addr);
            }
        }
    }
    DEBUGLOG("qualifying_stake_nodes size: ", qualifying_stake_nodes.size());
    return true;
}

void SendFastSyncGetHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{
    FastSyncGetHashReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    req.set_start_height(start_height);
    req.set_end_height(end_height);
    net_send_message<FastSyncGetHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFastSyncGetHashAck(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{
    if(start_height > end_height)
    {
        return;
    }
    if ((end_height - start_height) > 100000)
    {
        return;
    }
    FastSyncGetHashAck ack;
    ack.set_self_node_id(net_get_self_node_id());
    ack.set_msg_id(msg_id);
    uint64_t node_block_height = 0;
    if (DBStatus::DB_SUCCESS != DBReader().GetBlockTop(node_block_height))
    {
        ERRORLOG("GetBlockTop error");
        return;
    }
    ack.set_node_block_height(node_block_height);

    std::vector<FastSyncBlockHashs> block_height_hashes;
    if (GetHeightBlockHash(start_height, end_height, block_height_hashes))
    {
        for(auto &block_height_hash : block_height_hashes)
        {
            auto height_hash = ack.add_hashs();
            height_hash->set_height(block_height_hash.height());
            for(auto &hash : block_height_hash.hashs())
            {
                height_hash->add_hashs(hash);
            }
        }
        net_send_message<FastSyncGetHashAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }
}

void SendFastSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, const std::vector<FastSyncBlockHashs> &request_hashs)
{
    FastSyncGetBlockReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    for(auto &block_height_hash : request_hashs)
    {
        auto height_hash = req.add_hashs();
        height_hash->set_height(block_height_hash.height());
        for(auto &hash : block_height_hash.hashs())
        {
            height_hash->add_hashs(hash);
        }
    }

    net_send_message<FastSyncGetBlockReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFastSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, const std::vector<FastSyncBlockHashs> &request_hashs)
{
    FastSyncGetBlockAck ack;
    ack.set_msg_id(msg_id);
    DBReader db_reader;
    std::vector<std::string> block_hashes;
    for(const auto& height_hashs : request_hashs)
    {
        std::vector<std::string> db_hashs;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(height_hashs.height(), db_hashs))
        {
            return ;
        }
        for(auto& db_hash : db_hashs)
        {
            auto hashs = height_hashs.hashs();
            auto end = hashs.end();
            auto found = find_if(hashs.begin(), hashs.end(), [&db_hash](const string& hash){return db_hash == hash;});
            if(found != end)
            {
                block_hashes.push_back(db_hash);
            }
        }
    }

    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(block_hashes, blocks))
    {
        return;
    }
    for (auto &block_raw : blocks)
    {
        CBlock block;
        if(!block.ParseFromString(block_raw))
        {
            return;
        }
        auto height = block.height();
        auto sync_blocks = ack.mutable_blocks();
        auto found = std::find_if(sync_blocks->begin(), sync_blocks->end(), [height](const FastSyncBlock& sync_block){return sync_block.height() == height;});
        if(found == sync_blocks->end())
        {
            auto ack_block = ack.add_blocks();
            ack_block->set_height(height);
            ack_block->add_blocks(block_raw);
        }
        else
        {
                found->add_blocks(block_raw);
        }
    }
    net_send_message<FastSyncGetBlockAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleFastSyncGetHashReq(const std::shared_ptr<FastSyncGetHashReq> &msg, const MsgData &msgdata)
{
    SendFastSyncGetHashAck(msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    return 0;
}

int HandleFastSyncGetHashAck(const std::shared_ptr<FastSyncGetHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int HandleFastSyncGetBlockReq(const std::shared_ptr<FastSyncGetBlockReq> &msg, const MsgData &msgdata)
{
    std::vector<FastSyncBlockHashs> height_hashs;
    for(int i = 0; i < msg->hashs_size(); ++i)
    {
        auto& hash = msg->hashs(i);
        height_hashs.push_back(hash);
    }
    SendFastSyncGetBlockAck(msg->self_node_id(), msg->msg_id(), height_hashs);  
    return 0;
}

int HandleFastSyncGetBlockAck(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

void SendSyncGetSumHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{
    SyncGetSumHashReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    req.set_start_height(start_height);
    req.set_end_height(end_height);
    net_send_message<SyncGetSumHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetSumHashAck(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{

    if(start_height > end_height)
    {
        return;
    }
    if (end_height - start_height > 1000)
    {
        return;
    }
    SyncGetSumHashAck ack;
    ack.set_self_node_id(net_get_self_node_id());
    DBReader db_reader;
    uint64_t self_node_height = 0;
    if (0 != db_reader.GetBlockTop(self_node_height))
    {
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_node_block_height(self_node_height);
    ack.set_msg_id(msg_id);

    uint64_t end = end_height > self_node_height ? self_node_height : end_height;
    std::string hash;
    uint64_t j = 0;
    std::vector<std::string> block_hashes;
    for (uint64_t i = start_height; j <= end; ++i)
    {
        j = i + 1;
        j = j > end ? end : j;
        block_hashes.clear();
        hash.clear();
        if (GetHeightBlockHash(i, i, block_hashes) && SumHeightHash(block_hashes, hash))
        {
            auto sync_sum_hash = ack.add_sync_sum_hashes();
            sync_sum_hash->set_start_height(i);
            sync_sum_hash->set_end_height(i);
            sync_sum_hash->set_hash(hash);
        }
        else
        {
            return;
        }
        if(i == j) break;
    }
    net_send_message<SyncGetSumHashAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetHeightHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{
    SyncGetHeightHashReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    req.set_start_height(start_height);
    req.set_end_height(end_height);
    net_send_message<SyncGetHeightHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetHeightHashAck(SyncGetHeightHashAck& ack,const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height)
{
    if(start_height > end_height)
    {
        ack.set_code(-1);
        return;
    }
    if (end_height - start_height > 500)
    {
        ack.set_code(-2);
        return;
    }
    ack.set_self_node_id(net_get_self_node_id());
    DBReader db_reader;
    uint64_t self_node_height = 0;
    if (0 != db_reader.GetBlockTop(self_node_height))
    {
        ack.set_code(-3);
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msg_id);
    std::vector<std::string> block_hashes;
    if(end_height > self_node_height)
    {
        end_height = self_node_height + 1;
    }
    if(start_height > end_height)
    {
        ack.set_code(-4);
        return;
    }
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(start_height, end_height, block_hashes))
    {
        ack.set_code(-5);
        return;
    }
    
    std::sort(block_hashes.begin(), block_hashes.end());
    for (const auto& hash : block_hashes)
    {
        ack.add_block_hashes(hash);
    }
    ack.set_code(0);
}

void SendSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, const std::vector<std::string> &req_hashes)
{
    SyncGetBlockReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    for (const auto& hash : req_hashes)
    {
        req.add_block_hashes(hash);
    }
    net_send_message<SyncGetBlockReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, const std::vector<std::string> &req_hashes)
{

    if (req_hashes.size() > 5000)
    {
        return;
    }
    SyncGetBlockAck ack;
    ack.set_msg_id(msg_id);
    DBReader db_reader;
    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(req_hashes, blocks))
    {
        return;
    }
    for (auto &block : blocks)
    {
        ack.add_blocks(block);
    }
    net_send_message<SyncGetBlockAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleSyncGetSumHashReq(const std::shared_ptr<SyncGetSumHashReq> &msg, const MsgData &msgdata)
{
    SendSyncGetSumHashAck(msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    return 0;
}

int HandleSyncGetSumHashAck(const std::shared_ptr<SyncGetSumHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int HandleSyncGetHeightHashReq(const std::shared_ptr<SyncGetHeightHashReq> &msg, const MsgData &msgdata)
{
    SyncGetHeightHashAck ack;
    SendSyncGetHeightHashAck(ack,msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    net_send_message<SyncGetHeightHashAck>(msg->self_node_id(), ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}

int HandleSyncGetHeightHashAck(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int HandleSyncGetBlockReq(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata)
{
    std::vector<std::string> req_hashes;
    for (const auto& hash : msg->block_hashes())
    {
        req_hashes.push_back(hash);
    }
    SendSyncGetBlockAck(msg->self_node_id(), msg->msg_id(), req_hashes);
    return 0;
}

int HandleSyncGetBlockAck(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

void SendFromZeroSyncGetSumHashReq(const std::string &node_id, const std::string &msg_id, const std::vector<uint64_t>& heights)
{
    SyncFromZeroGetSumHashReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    for(auto height : heights)
    {
        req.add_heights(height);
    }
    net_send_message<SyncFromZeroGetSumHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFromZeroSyncGetSumHashAck(const std::string &node_id, const std::string &msg_id, const std::vector<uint64_t>& heights)
{
    DEBUGLOG("handle FromZeroSyncGetSumHashAck from {}", node_id);
    SyncFromZeroGetSumHashAck ack;
    DBReader db_reader;
    
    for(auto height : heights)
    {
        DEBUGLOG("FromZeroSyncGetSumHashAck get height {}", height);
        std::string sum_hash;
        if (DBStatus::DB_SUCCESS != db_reader.GetSumHashByHeight(height, sum_hash))
        {
            DEBUGLOG("fail to get sum hash height at height {}", height);
            continue;
        }
        SyncFromZeroSumHash* sum_hash_item = ack.add_sum_hashes();
        sum_hash_item->set_height(height);
        sum_hash_item->set_hash(sum_hash);
    }

    DEBUGLOG("sum hash size {}:{}", ack.sum_hashes().size(), ack.sum_hashes_size());
    if (ack.sum_hashes_size() == 0) 
    {
        ack.set_code(0);
    }
    else
    {
        ack.set_code(1);
    }

    ack.set_self_node_id(net_get_self_node_id());
    ack.set_msg_id(msg_id);
    DEBUGLOG("SyncFromZeroGetSumHashAck: id:{} , msg_id:{}", node_id, msg_id);
    net_send_message<SyncFromZeroGetSumHashAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFromZeroSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, uint64_t height)
{
    SyncFromZeroGetBlockReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    req.set_height(height);
    net_send_message<SyncFromZeroGetBlockReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFromZeroSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, uint64_t height)
{
    if(height < global::ca::sum_hash_range)
    {
        DEBUGLOG("sum height {} less than sum hash range", height);
        return;
    }
    SyncFromZeroGetBlockAck ack;
    DBReader db_reader;
    std::vector<std::string> blockhashes;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(height - global::ca::sum_hash_range + 1, height, blockhashes))
    {
        DEBUGLOG("GetBlockHashesByBlockHeight at height {}:{} fail", height - global::ca::sum_hash_range + 1, height);
        return;
    }
    std::vector<std::string> blockraws;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(blockhashes, blockraws))
    {
        DEBUGLOG("GetBlocksByBlockHash fail");
        return;
    }

    for (auto &blockraw : blockraws)
    {
        ack.add_blocks(blockraw);
    }
    ack.set_height(height);
    ack.set_msg_id(msg_id);
    ack.set_self_node_id(net_get_self_node_id());
    DEBUGLOG("response sum hash blocks at height {} to {}", height, node_id);
    net_send_message<SyncFromZeroGetBlockAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleFromZeroSyncGetSumHashReq(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata)
{
    std::vector<uint64_t> heights;
    for(auto height : msg->heights())
    {
        heights.push_back(height);
    }
    DEBUGLOG("SyncFromZeroGetSumHashReq: id:{}, msg_id:{}", msg->self_node_id(), msg->msg_id());
    SendFromZeroSyncGetSumHashAck(msg->self_node_id(), msg->msg_id(), heights);
    return 0;
}

int HandleFromZeroSyncGetSumHashAck(const std::shared_ptr<SyncFromZeroGetSumHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int HandleFromZeroSyncGetBlockReq(const std::shared_ptr<SyncFromZeroGetBlockReq> &msg, const MsgData &msgdata)
{
    SendFromZeroSyncGetBlockAck(msg->self_node_id(), msg->msg_id(), msg->height());
    return 0;
}

int HandleFromZeroSyncGetBlockAck(const std::shared_ptr<SyncFromZeroGetBlockAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

void SendSyncNodeHashReq(const string &node_id, const string &msg_id)
{
    SyncNodeHashReq req;
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    net_send_message<SyncNodeHashReq>(node_id, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncNodeHashAck(const string &node_id, const string &msg_id)
{
    DEBUGLOG("handle SendSyncNodeHashAck from {}", node_id);
    SyncNodeHashAck ack;
    ack.set_self_node_id(net_get_self_node_id());
    ack.set_msg_id(msg_id);

    DBReadWriter reader;
    uint64_t node_block_height = 0;
    if (DBStatus::DB_SUCCESS != reader.GetBlockTop(node_block_height))
    {
        ERRORLOG("GetBlockTop error");
        return;
    }
    std::string sum_hash;
    if (!ca_algorithm::CalculateHeightSumHash(node_block_height - sync_height_cnt_, node_block_height, reader, sum_hash))
    {
        ERRORLOG("CalculateHeightSumHash error");
        return;
    }
    ack.set_hash(sum_hash);
    net_send_message<SyncNodeHashAck>(node_id, ack, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleSyncNodeHashReq(const shared_ptr<SyncNodeHashReq> &msg, const MsgData &msgdata)
{
    SendSyncNodeHashAck(msg->self_node_id(), msg->msg_id());
    return 0;
}

int HandleSyncNodeHashAck(const shared_ptr<SyncNodeHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}
