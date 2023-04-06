#include "utils/MagicSingleton.h"
#include "ca/ca_algorithm.h"
#include "ca/ca_transaction.h"
#include "ca/ca_txhelper.h"
#include "common/global_data.h"
#include "db/db_api.h"
#include "ca/ca_block_http_callback.h"
#include "ca/ca_sync_block.h"
#include "../utils/time_util.h"
#include "ca_global.h"
#include "ca_transaction_cache.h"
#include "ca_blockcache.h"
#include "utils/MagicSingleton.h"
#include "utils/console.h"

#include "ca_blockhelper.h"
#include "utils/AccountManager.h"
#include <sys/file.h>

const static uint64_t stability_time = 60 * 1000000;
static uint64_t new_sync_fail_height = 0;
static bool run_fast_sync = false;

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

bool SyncBlock::SumHeightsHash(std::map<uint64_t, std::vector<std::string>> height_blocks, std::string &hash)
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
        for(auto block_raw : blocks_raw)
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

void SyncBlock::ThreadStart()
{

    sync_height_time_ = 50;
    sync_height_cnt_ = 50;
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
            while (sync_thread_runing)
            {
                syncing_ = false;
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
                syncing_ = true;
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

                if(run_fast_sync)
                {
                    if (new_sync_fail_height > fast_sync_height_cnt_)
                    {
                        start_sync_height = new_sync_fail_height - fast_sync_height_cnt_;
                    }
                    else
                    {
                        start_sync_height = new_sync_fail_height;
                    }
                    end_sync_height = new_sync_fail_height + fast_sync_height_cnt_;

                    INFOLOG("begin fast sync {} {} ", start_sync_height, end_sync_height);
                    if(!RunFastSyncOnce(pledge_addr, chain_height, start_sync_height, end_sync_height))
                    {
                        DEBUGLOG("fast sync fail");
                    }
                    run_fast_sync = false;
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
                            ERRORLOG("from zero sync fail ret: {}", run_status);
                        }
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
                        int run_status = RunNewSyncOnce(pledge_addr, chain_height, self_node_height, start_sync_height, end_sync_height);
                        if(run_status <= 0)
                        {
                            new_sync_fail_height = 0;
                            run_fast_sync = false;
                        }
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
    if (GetNewSyncNode(10, chain_height, pledge_addr, send_node_ids) != 0)
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

int SyncBlock::RunNewSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t self_node_height, uint64_t start_sync_height, uint64_t end_sync_height)
{
    int ret = 0;
    if (start_sync_height > end_sync_height)
    {
        ret = -1;
        return ret;
    }
    std::vector<std::string> send_node_ids;
    if ((ret = GetNewSyncNode(10, chain_height, pledge_addr, send_node_ids)) != 0)
    {
        ERRORLOG("get sync node fail");
        return ret - 1000;
    }
    std::map<uint64_t, uint64_t> need_sync_heights;
    std::vector<std::string> ret_node_ids;
    chain_height = 0;
    DEBUGLOG("GetSyncSumHashNode begin:{}:{}", start_sync_height, end_sync_height);
    if ((ret = GetSyncSumHashNode(send_node_ids, start_sync_height, end_sync_height, need_sync_heights, ret_node_ids, chain_height)) < 0)
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
        if ((ret = GetSyncBlockHashNode(ret_node_ids, sync_height.first, sync_height.second, self_node_height, chain_height, sec_ret_node_ids, req_hashes)) != 0)
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
    if ((ret = GetFastSyncNode(10, chain_height, pledge_addr, send_node_ids)) != 0)
    {
        ERRORLOG("get from zero sync node fail");
        return ret - 1000;
    }

    std::vector<std::string> ret_node_ids;
    std::vector<uint64_t> heights;
    if (!sync_from_zero_reserve_heights.empty())
    {
        sync_from_zero_reserve_heights.swap(heights);
    }
    else
    {
        uint64_t height_ceiling = ca_algorithm::GetSumHashFloorHeight(chain_height) - global::ca::sum_hash_range;
        auto sync_height = ca_algorithm::GetSumHashCeilingHeight(self_node_height);
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
        request_heights += height;
    }
    DEBUGLOG("GetFromZeroSyncSumHashNode begin{}", request_heights);
    if ((ret = GetFromZeroSyncSumHashNode(send_node_ids, heights, self_node_height, ret_node_ids, sum_hashes)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    DEBUGLOG("GetFromZeroSyncSumHashNode success");
    DEBUGLOG("GetFromZeroSyncBlockData begin");
    if ((ret = GetFromZeroSyncBlockData(sum_hashes, heights, ret_node_ids)) != 0 )
    {
        return ret -3000;
    }
    DEBUGLOG("GetSyncBlockData success");
    return 0;
}

int SyncBlock::GetNewSyncNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
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

    if (force_find_base_chain || self_node_height < chain_height)
    {
        DEBUGLOG("find node base on chain height {}", chain_height);
        force_find_base_chain = false;
        auto discard_comparator = std::less<uint64_t>();
        auto reserve_comparator = std::greater_equal<uint64_t>();
        return GetSyncNode(num, chain_height, discard_comparator, reserve_comparator, pledge_addr, send_node_ids);
    }
    else
    {
        DEBUGLOG("find node base on self height {}", self_node_height);
        force_find_base_chain = true;
        auto discard_comparator = std::less_equal<uint64_t>();
        auto reserve_comparator = std::greater<uint64_t>();
        return GetSyncNode(num, self_node_height, discard_comparator, reserve_comparator, pledge_addr, send_node_ids);
    }
}

int SyncBlock::GetFastSyncNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
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
    auto discard_comparator = std::less<uint64_t>();
    auto reserve_comparator = std::greater_equal<uint64_t>();
    return GetSyncNode(num, self_node_height, discard_comparator, reserve_comparator, pledge_addr, send_node_ids);
}

int SyncBlock::GetSyncNode(uint32_t num, uint64_t height_baseline, std::function<bool(uint64_t, uint64_t)> discard_comparator, std::function<bool(uint64_t, uint64_t)> reserve_comparator, const std::vector<std::string> &pledge_addr,
                            std::vector<std::string> &send_node_ids)
{
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
    if (nodes.size() <= num)
    {
        for (auto &node : nodes)
        {
            if (discard_comparator(node.height, height_baseline))
            {
                continue;
            }
            send_node_ids.push_back(node.base58address);
        }
    }
    else
    {
        std::vector<Node> node_info = nodes;
        if(pledge_addr.size() < 10)
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
    if (send_node_ids.size() < 5)
    {
        std::vector<Node> nodes = peer_node->get_nodelist();
        for(auto &it : nodes)
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
    uint32_t ret_num = 0;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        auto ret_height_data = ack.hashs();
        for(auto ret_height_hashs : ret_height_data)
        {
            for(auto hash : ret_height_hashs.hashs())
            {
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
    
    for(auto iter = sync_hashs.begin(); iter != sync_hashs.end(); ++iter)
    {
        std::vector<std::string> local_hashs;
        auto result = block_height_hashes.find(iter->second.height);
        if(result != block_height_hashes.end())
        {
            local_hashs = result->second;
        }
        
        bool byzantine_success = check_byzantine(ret_num, iter->second.hits);
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
                if (block.hash() != getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
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

int SyncBlock::GetSyncSumHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height,
                                   std::map<uint64_t, uint64_t> &need_sync_heights, std::vector<std::string> &ret_node_ids, uint64_t &chain_height)
{
    int ret = 0;
    need_sync_heights.clear();
    ret_node_ids.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_num * 0.8, msg_id))
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
            std::string key = std::to_string(sync_sum_hash.start_height()) + "_" + std::to_string(sync_sum_hash.end_height()) + "_" + sync_sum_hash.hash();
            auto it = std::find_if(sync_hash_datas.begin(), sync_hash_datas.end(), [&key](const std::pair<std::string, uint32_t>& sync_hash_data){return key == sync_hash_data.first;}); //sync_hash_datas.find(key);
            if (sync_hash_datas.end() == it)
            {
                sync_hash_datas.push_back(std::make_pair(key, 1));
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
            uint64_t self_node_height = 0;
            DBReader db_reader;
            if(DBStatus::DB_SUCCESS != db_reader.GetBlockTop(self_node_height))
            {
                ERRORLOG("(GetSyncSumHashNode) GetBlockTop failed !");
                ret = -4;
                return ret;
            }
            if(chain_height >= self_node_height && chain_height - self_node_height <= 500)
            {
                new_sync_fail_height = start_height;
                run_fast_sync = true;   
                INFOLOG("new sync fail,next round will run fast sync"); 
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
    if(run_fast_sync)
    {
        return 99;
    }
    // Synchronize the latest no_verify_height heights
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
    return 0;
}

int SyncBlock::GetFromZeroSyncSumHashNode(const std::vector<std::string> &send_node_ids, const std::vector<uint64_t>& send_heights, uint64_t self_node_height, std::vector<std::string> &ret_node_ids, std::map<uint64_t, std::string>& sum_hashes)
{
    ret_node_ids.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_num * 0.8, msg_id))
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
    

    std::map<std::string, std::pair<uint64_t, int>> sum_hash_datas;
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
            continue;
        }
        ++success_count;
        ret_node_ids.push_back(ack.self_node_id());
        auto sum_hashes = ack.sum_hashes();
        for(auto sum_hash : sum_hashes)
        {
            auto hash = sum_hash.hash();
            auto height = sum_hash.height();

            auto found = sum_hash_datas.find(hash);
            if (found == sum_hash_datas.end())
            {
                sum_hash_datas[hash].first = height;
                sum_hash_datas[hash].second = 1;
                continue;
            }
            auto& content = found->second;
            content.second = content.second + 1;            
        }

    }
    if (!check_byzantine(send_num,success_count))
    {
        return -3;
    }

    for(auto sum_hash_data : sum_hash_datas)
    {
        if (check_byzantine(success_count, sum_hash_data.second.second))
        {
            sum_hashes[sum_hash_data.second.first] = sum_hash_data.first;
        }
    }

    if (sum_hashes.empty())
    {
        return -4;
    }
    
    
    return 0;
}

int SyncBlock::GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sum_hashes, std::vector<uint64_t> &send_heights, std::vector<std::string> &send_node_ids)
{
    int ret = 0;
    if (send_node_ids.empty() || sum_hashes.empty())
    {
        return -1;
    }
    auto send_node_size = send_node_ids.size();
    auto send_hash_size = sum_hashes.size();
    if (send_node_size > send_hash_size)
    {
        for (auto diff = send_node_size - send_hash_size; diff > 0; --diff)
        {
            send_node_ids.pop_back();
        }
        
    }
    else if (send_node_size < send_hash_size)
    {
        for (auto diff =  send_hash_size - send_node_size; diff > 0; --diff)
        {
            int index = rand() % send_node_ids.size();
            auto &node_id = send_node_ids.at(index);
            send_node_ids.push_back(node_id);            
        }
    }

    if (send_node_ids.size() != sum_hashes.size())
    {
        DEBUGLOG("send_node_ids.size() != sum_hashes.size(), {}:{}", send_node_ids.size(), sum_hashes.size());
        return -2;
    }
    
    
    std::string msg_id;
    if (!GLOBALDATAMGRPTR.CreateWait(90, send_node_ids.size(), msg_id))
    {
        return -3;
    }

    int send_node_index = 0;
    for(auto sum_hash_item : sum_hashes)
    {
        auto node_id = send_node_ids[send_node_index];
        auto sum_hash_height = sum_hash_item.first;
        SendFromZeroSyncGetBlockReq(node_id, msg_id, sum_hash_height);
        DEBUGLOG("from zero sync get block at height {} from {}", sum_hash_height, node_id);
        ++send_node_index;
    }
    
    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        DEBUGLOG("wait sync block data time out send:{} recv:{}", send_node_ids.size(), ret_datas.size());
    }
    
    std::vector<uint64_t> success_heights;
    
    for(auto ret_data : ret_datas)
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
        for(auto block_raw : blockraws)
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
        success_heights.push_back(ack.height());
        for(auto block : sum_hash_data)
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
        return -1;
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
                    for(auto cache : this->sync_from_zero_cache)
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
                                     std::vector<std::string> &ret_node_ids, std::vector<std::string> &req_hashes)
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
            ret = -4;
            return ret;
        }
        std::sort(block_hashes.begin(), block_hashes.end());
        std::sort(exits_hashes.begin(), exits_hashes.end());
        std::set_difference(block_hashes.begin(), block_hashes.end(), exits_hashes.begin(), exits_hashes.end(), std::back_inserter(v_diff));
    }

    for (auto it = v_diff.cbegin(); it != v_diff.cend(); it++)
    {

        block.Clear();
        std::string().swap(strblock);
        auto ret = db_reader.GetBlockByBlockHash(*it, strblock);
        if (DBStatus::DB_SUCCESS != ret)
        {
            continue;
        }
        block.ParseFromString(strblock);

        //It will only be rolled back when the height of the block to be added is less than the maximum height on the chain of -10
        uint64_t tmp_height = block.height();
        if ((tmp_height < chain_height) && chain_height - tmp_height > 10)
        {
            AddBlockToMap(block, rollback_block_data);
        }
    }
    if(!rollback_block_data.empty())
    {
        DEBUGLOG("==== new sync rollback ====");
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollback_block_data);
        return -5;
    }

    if (verify_hashes.empty())
    {
        return 0;
    }
    req_hashes.assign(verify_hashes.cbegin(), verify_hashes.cend());
    ret_node_ids.assign(nodes.cbegin(), nodes.cend());
    if(ret_node_ids.empty())
    {
        ret = -6;
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

    std::string hash;
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
    for(auto height_hashs : request_hashs)
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
    for (auto hash : block_hashes)
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
    for (auto hash : req_hashes)
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
    for (auto hash : msg->block_hashes())
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

void SendFromZeroSyncGetSumHashReq(const std::string &node_id, const std::string &msg_id, std::vector<uint64_t> heights)
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

void SendFromZeroSyncGetSumHashAck(const std::string &node_id, const std::string &msg_id, std::vector<uint64_t> heights)
{
    DEBUGLOG("handle FromZeroSyncGetSumHashAck");
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
