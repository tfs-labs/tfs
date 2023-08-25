#include "ca_check_blocks.h"
#include "logging.h"
#include "ca_transaction.h"
#include "../common/global_data.h"
#include "ca_algorithm.h"
#include "ca_blockhelper.h"


#include <cstdint>
#include <fstream> 
#include <mutex>
#include <string>
#include <utility>
// static std::ofstream fout("./test.txt", std::ios::trunc); 

CheckBlocks::CheckBlocks()
{
    init();
    m_check_runing = false;
}

void CheckBlocks::init()
{
    DBReader db_reader;
    auto ret = db_reader.GetBlockComHashHeight(this->m_top_block_height);
    if(ret != DBStatus::DB_SUCCESS)
    {
        this->m_top_block_height = 0;
        this->m_top_block_hash = "";
    }

    SetTempTopData(0, "");
}

void CheckBlocks::StartTimer()
{
	m_timer.AsyncLoop(30000, [this](){
        int ret = ToCheck();
        if(ret != 0)
        {
            ERRORLOG("ToCheck error, error num: {}", ret);
        }
	});
}

//get stake and invested addr
int CheckBlocks::GetPledgeAddr(DBReadWriter& db_reader, std::vector<std::string>& pledge_addr)
{
    std::vector<std::string> stake_addr;
    auto status = db_reader.GetStakeAddress(stake_addr);
    if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
    {
        ERRORLOG("GetStakeAddress error, error num:{}", -1);
        return -1;
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
    return 0;
}

std::pair<uint64_t, std::string> CheckBlocks::GetTempTopData()
{
    std::unique_lock<std::mutex> lock(m_temp_top_date_mutex);
    return this->m_temp_top_date;
}

void CheckBlocks::SetTempTopData(uint64_t height, std::string hash)
{
    std::unique_lock<std::mutex> lock(m_temp_top_date_mutex);
    m_temp_top_date.first = height;
    m_temp_top_date.second = hash;
}

int CheckBlocks::ToCheck()
{
    if(m_check_runing)
    {
        DEBUGLOG("ToCheck is running");
        return 1;
    }

    ON_SCOPE_EXIT{
        m_check_runing = false;
    };

    m_check_runing = true;

    init();

    uint64_t self_node_height = 0;
    DBReadWriter db_reader_write;
    auto status = db_reader_write.GetBlockTop(self_node_height);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("GetBlockTop error, error num:{}", -1);
        return -1;
    }

    if(m_top_block_height == 0 && self_node_height < 1100)
    {
        DEBUGLOG("Currently less than 1100 height");
        return 2;
    }
    else if(self_node_height < m_top_block_height + 1100) 
    {
        DEBUGLOG("self_node_height:{} less than top_block_height + 100:{}", self_node_height, m_top_block_height + 1100);
        return 3;
    }

    while(true)
    {
        std::string temp_hash;
        ca_algorithm::Calc1000HeightsSumHash(m_top_block_height + 1000, db_reader_write, temp_hash);
        DEBUGLOG("self temp_hash: {}", temp_hash);
        if(temp_hash.empty())
        {
            ERRORLOG("Calc1000HeightsSumHash error, error num:{}", -1);
            return -1;
        }
        SetTempTopData(m_top_block_height + 1000, temp_hash);

        std::vector<std::string> pledge_addr;
        int ret = GetPledgeAddr(db_reader_write, pledge_addr);
        if(ret != 0)
        {
            ERRORLOG("GetPledgeAddr error, error num:{}", -2);
            return -2;
        }


        std::vector<std::string> send_node_ids;
        int peer_node_size = MagicSingleton<PeerNode>::GetInstance()->get_nodelist_size();
        ret = MagicSingleton<SyncBlock>::GetInstance()->GetSyncNode(peer_node_size, m_top_block_height + 1000, pledge_addr, send_node_ids);
        if(ret != 0)
        {
            ERRORLOG("GetSyncNode error, error num:{}", ret);
            return -3;
        }

        std::string msg_id;
        size_t send_num = send_node_ids.size();
        if (!GLOBALDATAMGRPTR.CreateWait(90, send_num * 0.9, msg_id))
        {
            ret = -4;
            ERRORLOG("GLOBALDATAMGRPTR.CreateWait error, error num:{}", ret);
            return ret;
        }
        for (auto &node_id : send_node_ids)
        {
            SendGetCheckSumHashReq(node_id, msg_id, m_top_block_height + 1000);
        }
        // string id = "15nSTKQKgiVh2cgMxACcgtrgJsUvnYQnQH";
        // SendGetCheckSumHashReq(id, msg_id, m_top_block_height + 1000);
        std::vector<std::string> ret_datas;
        if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
        {
            if (ret_datas.empty() || ret_datas.size() < send_num / 2)
            {
                ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
                ret = -5;
                return ret;
            }
        }

        std::map<std::string, uint64_t> consensus_map;
        GetCheckSumHashAck ack;
        uint64_t success_num = 0;
        for (auto &ret_data : ret_datas)
        {
            ack.Clear();
            if (!ack.ParseFromString(ret_data))
            {
                continue;
            }

            if(ack.success() == false)
            {
                continue;
            }

            if(ack.hash().empty())
            {
                continue;
            }

            auto find = consensus_map.find(ack.hash());
            if(find == consensus_map.end())
            {
                consensus_map.insert(std::make_pair(ack.hash(), 1));
            }
            else
            {
                ++find->second;
            }

            ++success_num;
        }

        bool back = success_num >= ret_datas.size() * 0.8;
        if(!back)
        {
            ERRORLOG("success num:{} less than ret_datas.size() * 0.8:{}", success_num, ret_datas.size() * 0.8);
            return -6;
        }

        auto compare = [](const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b) {
            return a.second < b.second;
        };


        auto max_iterator = std::max_element(consensus_map.begin(), consensus_map.end(), compare);

        if (max_iterator != consensus_map.end())
        {
            DEBUGLOG("max_iterator hash: {}", max_iterator->first);
            bool byzantine = max_iterator->second >= success_num * 0.9;
            if(byzantine)
            {
                auto [timp_height, timp_hash] = GetTempTopData();
                if(max_iterator->first == timp_hash)
                {
                    DBReadWriter db_Writer;
                    if (DBStatus::DB_SUCCESS !=  db_Writer.SetCheckBlockHashsByBlockHeight(timp_height, timp_hash))
                    {
                        ERRORLOG("SetCheckBlockHashsByBlockHeight failed !");
                        return -7;
                    }

                    uint64_t last_height;
                    if(DBStatus::DB_SUCCESS != db_Writer.GetBlockComHashHeight(last_height))
                    {
                        ERRORLOG("GetBlockComHashHeight failed !");
                        last_height = 0;
                    }

                    if(last_height != timp_height - 1000)
                    {
                        ERRORLOG("last_height is: {} != timp_height - 1000, timp_height is: {} !", last_height, timp_height);
                        return -8;
                    }

                    if ( DBStatus::DB_SUCCESS != db_Writer.SetBlockComHashHeight(timp_height))
                    {
                        return -9; 
                    }
                    if (DBStatus::DB_SUCCESS != db_Writer.TransactionCommit())
                    {
                        ERRORLOG("(rocksdb init) TransactionCommit failed !");
                        return -10;
                    }
                    return 0;
                }
                else 
                {     
                    int res = DoNewSync(send_node_ids,pledge_addr, self_node_height);
                    DEBUGLOG("first DoNewSync return num: {}", res);
                    continue;
                }
            }
            int res = DoNewSync(send_node_ids, pledge_addr, self_node_height);
            DEBUGLOG("second DoNewSync return num: {}", res);
            continue;
        }

    }

}


int CheckBlocks::DoNewSync(std::vector<std::string> send_node_ids, std::vector<std::string>& pledge_addr, uint64_t self_node_height)
{
    std::vector<uint64_t> heights;
    for(int i = 1; i <= 10; i++)
    {
        heights.push_back(m_top_block_height + i * 100); 
    }

    std::vector<uint64_t> need_sync_heights;
    int ret = ByzantineSumHash(send_node_ids, heights, need_sync_heights);
    if(ret != 0)
    {
        ERRORLOG("ByzantineSumHash fail:{}", ret);
        return ret;
    }

    uint64_t chain_height = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->obtain_chain_height(chain_height))
    {
        return -1;
    }
    for(const auto& sync_heiht: need_sync_heights)
    {
        DEBUGLOG("need_sync_heights: {}",sync_heiht);
        ret = MagicSingleton<SyncBlock>::GetInstance()->RunNewSyncOnce(pledge_addr, chain_height, self_node_height, sync_heiht - 100, sync_heiht, 99999);
        if(ret != 0)
        {
            ERRORLOG("RunNewSyncOnce fail:{}", ret);
            return ret;
        }
    }

    sleep(10);
    return 0;
}


int CheckBlocks::ByzantineSumHash(const std::vector<std::string> &send_node_ids, const std::vector<uint64_t>& send_heights, std::vector<uint64_t>& need_sync_heights)
{
    need_sync_heights.clear();
    std::string msg_id;
    size_t send_num = send_node_ids.size();

    double acceptance_rate = 0.9;

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
    
    //      height               sumhash       num
    std::map<uint64_t, std::map<std::string, uint64_t>>sum_hash_datas;

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
            ++success_count;
            continue;
        }
        ++success_count;
        auto ret_sum_hashes = ack.sum_hashes();
        for(const auto& sum_hash : ret_sum_hashes)
        {
            const auto hash = sum_hash.hash();
            auto height = sum_hash.height();
            
            auto found = sum_hash_datas.find(height);
            if (found == sum_hash_datas.end())
            {
                std::map<std::string, uint64_t> temp;
                temp.insert(make_pair(hash, 1));
                sum_hash_datas.insert(std::make_pair(height, temp));
                continue;
            }
            auto& content = found->second;
            auto find_hash = content.find(hash);
            if(find_hash == content.end())
            {
                content.insert(make_pair(hash, 1));
                continue;
            }
            find_hash->second++;
        }

    }

    uint64_t back_num = send_num * 0.8;
    bool byzantine_success = success_count >= back_num;

    if(!byzantine_success)
    {
        ERRORLOG("check_byzantine error, send_num = {} , success_count = {}", send_num, success_count);
        return -3;
    }

    DBReader db_reader;

    auto compare = [](const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b) {
        return a.second < b.second;
    };

    for(const auto& sum_hashdata: sum_hash_datas)
    {
        std::string shelf_sum_hash;
        if (DBStatus::DB_SUCCESS != db_reader.GetSumHashByHeight(sum_hashdata.first, shelf_sum_hash))
        {
            DEBUGLOG("fail to get sum hash height at height {}", sum_hashdata.first);
            need_sync_heights.push_back(sum_hashdata.first);
            continue;
        }


        auto max_iterator = std::max_element(sum_hashdata.second.begin(), sum_hashdata.second.end(), compare);

        if (max_iterator != sum_hashdata.second.end())
        {
           bool byzantine_success = max_iterator->second >= success_count * 0.85;
           if(byzantine_success)
           {
                if(shelf_sum_hash == max_iterator->first)
                {
                    continue;
                }
           }
           need_sync_heights.push_back(sum_hashdata.first);
        }
        else 
        {
            need_sync_heights.push_back(sum_hashdata.first);
        }
    }

    return 0;
}









void SendGetCheckSumHashReq(const std::string &node_id, const std::string &msg_id, uint64_t height)
{
    GetCheckSumHashReq req;
    req.set_height(height);
    req.set_self_node_id(net_get_self_node_id());
    req.set_msg_id(msg_id);
    net_send_message<GetCheckSumHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendGetCheckSumHashAck(const std::string &node_id, const std::string &msg_id, uint64_t height)
{
    GetCheckSumHashAck ack;
    DBReader db_reader;
    string hash;
    bool success = true;
    ack.set_height(height);
    ack.set_msg_id(msg_id);
    ack.set_self_node_id(net_get_self_node_id());
    if(DBStatus::DB_SUCCESS != db_reader.GetCheckBlockHashsByBlockHeight(height, hash))
    {
        auto [timp_height, timp_hash] = MagicSingleton<CheckBlocks>::GetInstance()->GetTempTopData();
        if(timp_height == height)
        {
            ack.set_success(success);
            ack.set_hash(timp_hash);
            net_send_message<GetCheckSumHashAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
            return;
        }

        ERRORLOG("GetCheckBlockHashsByBlockHeight Error");
        success = false;
    }
    ack.set_success(success);
    ack.set_hash(hash);
    net_send_message<GetCheckSumHashAck>(node_id, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleGetCheckSumHashReq(const shared_ptr<GetCheckSumHashReq> &msg, const MsgData &msgdata)
{
    SendGetCheckSumHashAck(msg->self_node_id(), msg->msg_id(),msg->height());
    return 0;
}

int HandleGetCheckSumHashAck(const shared_ptr<GetCheckSumHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}
