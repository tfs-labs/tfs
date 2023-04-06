#ifndef TFS_CA_SYNC_BLOCK_H_
#define TFS_CA_SYNC_BLOCK_H_

#include "db/db_api.h"
#include "net/msg_queue.h"
#include "proto/sync_block.pb.h"
#include <map>
#include "ca_blockcompare.h"
#include "utils/CTimer.hpp"
#include "utils/console.h"

struct fast_sync_helper
{
    uint32_t hits;
    std::set<std::string> ids;
    uint64_t height;
};


class SyncBlock
{
public:
    SyncBlock() = default;
    ~SyncBlock() = default;
    SyncBlock(SyncBlock &&) = delete;
    SyncBlock(const SyncBlock &) = delete;
    SyncBlock &operator=(SyncBlock &&) = delete;
    SyncBlock &operator=(const SyncBlock &) = delete;

    void ThreadStart();
    bool RunFastSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t start_sync_height, uint64_t end_sync_height);
    int RunNewSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t self_node_height, uint64_t start_sync_height, uint64_t end_sync_height);
    int RunFromZeroSyncOnce(const std::vector<std::string> &pledge_addr, uint64_t chain_height, uint64_t self_node_height);
    int GetNewSyncNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
                        std::vector<std::string> &send_node_ids);
    int GetFastSyncNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
                        std::vector<std::string> &send_node_ids);
    void SetFastSync(uint64_t sync_start_height);

    static bool check_byzantine(int receive_count, int hit_count);
    static bool SumHeightsHash(std::map<uint64_t, std::vector<std::string>> height_blocks, std::string &hash);

private:
    /**********************************************************************************************************************************/
    bool GetFastSyncSumHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height,
                                    std::vector<FastSyncBlockHashs> &request_hashs, std::vector<std::string> &ret_node_ids, uint64_t chain_height);
    bool GetFastSyncBlockData(const std::string &send_node_id, const std::vector<FastSyncBlockHashs> &request_hashs, uint64_t chain_height);

    /**********************************************************************************************************************************/
    int GetSyncSumHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height,
                            std::map<uint64_t, uint64_t> &need_sync_heights, std::vector<std::string> &ret_node_ids, uint64_t &chain_height);
    int GetSyncBlockHashNode(const std::vector<std::string> &send_node_ids, uint64_t start_sync_height, uint64_t end_sync_height, uint64_t self_node_height, uint64_t chain_height,
                            std::vector<std::string> &ret_node_ids, std::vector<std::string> &req_hashes);
    int GetSyncBlockData(const std::vector<std::string> &send_node_ids, const std::vector<std::string> &req_hashes, uint64_t chain_height);
    /**********************************************************************************************************************************/
    int GetFromZeroSyncSumHashNode(const std::vector<std::string> &send_node_ids, const std::vector<uint64_t>& send_heights, uint64_t self_node_height, std::vector<std::string> &ret_node_ids, std::map<uint64_t, std::string>& sum_hashs);
    int GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sum_hashes, std::vector<uint64_t> &send_heights, std::vector<std::string> &send_node_ids);
    /**********************************************************************************************************************************/
    int GetSyncNode(uint32_t num, uint64_t height_baseline, std::function<bool(uint64_t, uint64_t)> discard_comparator, std::function<bool(uint64_t, uint64_t)> reserve_comparator, const std::vector<std::string> &pledge_addr,
                    std::vector<std::string> &send_node_ids);
    void AddBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, CBlockCompare>> &sync_block_data);

    std::thread sync_thread_;
    bool sync_thread_runing{};
    std::mutex sync_thread_runing_mutex;
    std::condition_variable sync_thread_runing_condition;

    uint32_t sync_height_cnt_{};
    uint32_t sync_height_time_{};
    uint32_t fast_sync_height_cnt_{};
    bool syncing_{} ;

    std::map<uint64_t, std::map<uint64_t, std::set<CBlock, CBlockCompare>>> sync_from_zero_cache;
    std::vector<uint64_t> sync_from_zero_reserve_heights;
    std::mutex sync_from_zero_cache_mutex;
    const int sync_bound = 200;

};

void SendFastSyncGetHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendFastSyncGetHashAck(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendFastSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, const std::vector<FastSyncBlockHashs> &request_hashs);
void SendFastSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, const std::vector<FastSyncBlockHashs> &request_hashs);

int HandleFastSyncGetHashReq(const std::shared_ptr<FastSyncGetHashReq> &msg, const MsgData &msgdata);
int HandleFastSyncGetHashAck(const std::shared_ptr<FastSyncGetHashAck> &msg, const MsgData &msgdata);
int HandleFastSyncGetBlockReq(const std::shared_ptr<FastSyncGetBlockReq> &msg, const MsgData &msgdata);
int HandleFastSyncGetBlockAck(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata);

void SendSyncGetSumHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendSyncGetSumHashAck(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendSyncGetHeightHashReq(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendSyncGetHeightHashAck(SyncGetHeightHashAck& ack,const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);
void SendSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, const std::vector<std::string> &req_hashes);
void SendSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, uint64_t start_height, uint64_t end_height);

int HandleSyncGetSumHashReq(const std::shared_ptr<SyncGetSumHashReq> &msg, const MsgData &msgdata);
int HandleSyncGetSumHashAck(const std::shared_ptr<SyncGetSumHashAck> &msg, const MsgData &msgdata);
int HandleSyncGetHeightHashReq(const std::shared_ptr<SyncGetHeightHashReq> &msg, const MsgData &msgdata);
int HandleSyncGetHeightHashAck(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata);
int HandleSyncGetBlockReq(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata);
int HandleSyncGetBlockAck(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata);


void SendFromZeroSyncGetSumHashReq(const std::string &node_id, const std::string &msg_id, std::vector<uint64_t> heights);
void SendFromZeroSyncGetSumHashAck(const std::string &node_id, const std::string &msg_id, std::vector<uint64_t> heights);
void SendFromZeroSyncGetBlockReq(const std::string &node_id, const std::string &msg_id, uint64_t height);
void SendFromZeroSyncGetBlockAck(const std::string &node_id, const std::string &msg_id, uint64_t height);

int HandleFromZeroSyncGetSumHashReq(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetSumHashAck(const std::shared_ptr<SyncFromZeroGetSumHashAck> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetBlockReq(const std::shared_ptr<SyncFromZeroGetBlockReq> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetBlockAck(const std::shared_ptr<SyncFromZeroGetBlockAck> &msg, const MsgData &msgdata);

#endif
