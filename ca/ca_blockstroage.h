#ifndef _BLOCK_STROAGE_
#define _BLOCK_STROAGE_

#include "ca/ca_blockmonitor.h"
#include "utils/MagicSingleton.h"
#include "utils/VRF.hpp"
#include <future>

using RetType = std::pair<std::string, uint16_t>;

enum class BroadcastType
{
	VerifyBroadcast,
	level1Broadcast
};
struct BlockStatusWrapper
{
	std::string blockHash;
	CBlock block;
	BroadcastType broadcastType;
	uint32_t VerifyNodesNumber;
	uint32_t level1NodesNumber;
	std::vector<BlockStatus> BlockStatusList = {};
	std::set<std::string> VerifyNodes = {};
	std::set<std::string> level1Nodes = {};
	std::map<std::string, Vrf> vrfMap = {};
	std::map<std::string, Vrf> txvrfMap = {};
};
class BlockStroage
{
public:
    BlockStroage(){ StartTimer(); };
    ~BlockStroage() = default;
    BlockStroage(BlockStroage &&) = delete;
    BlockStroage(const BlockStroage &) = delete;
    BlockStroage &operator=(BlockStroage&&) = delete;
    BlockStroage &operator=(const BlockStroage &) = delete;



public:
	void StopTimer(){_block_timer.Cancel();}
	int AddBlock(const BlockMsg &msg);
	int UpdateBlock(const BlockMsg &msg);
	bool isBlock(const std::string& blockHash);

	std::shared_future<RetType> GetPrehash(const uint64_t height);
	void commit_seek_task(uint64_t seek_height);
	void Force_commit_seek_task(uint64_t seek_height);
	bool is_seek_task(uint64_t seek_height);

	void newbuildBlockByBlockStatus(const std::string blockHash);
	void AddBlockStatus(const BlockStatus& blockStatus);
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes);
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes, const Vrf& vrf);
	void ExpiredDeleteCheck();
	int CheckData(const BlockStatus& blockStatus);
	int initNewBlock(const CBlock& oldBlock, CBlock& newBlock);

private:

	void StartTimer();
	void BlockCheck();
	void composeEndBlockmsg(std::vector<BlockMsg> &msgvec);
	void Remove(const std::string &hash);

	RetType SeekPreHashThread(uint64_t seek_height);
	RetType SeekPreHashByNode(
		const std::vector<std::string> &send_node_ids, uint64_t seek_height, const uint64_t &self_node_height, const uint64_t &chain_height);
private:
    friend std::string PrintCache(int where);
	CTimer _block_timer;
	mutable std::shared_mutex _block_mutex_;
	std::map<std::string, std::vector<BlockMsg>> _BlockMap;

	mutable std::shared_mutex prehash_mutex;
	std::map<uint64_t, std::shared_future<RetType>> PreHashMap;

	std::mutex status_mutex_;
	std::map<std::string, BlockStatusWrapper> blockStatusMap;
	double FailureRate = 0.75;
};
static int GetPrehashFindNode(uint32_t num, uint64_t self_node_height, const std::vector<std::string> &pledge_addr,
                            std::vector<std::string> &send_node_ids);
void SendSeekGetPreHashReq(const std::string &node_id, const std::string &msg_id, uint64_t seek_height);
void SendSeekGetPreHashAck(SeekPreHashByHightAck& ack,const std::string &node_id, const std::string &msg_id, uint64_t seek_height);

int HandleSeekGetPreHashReq(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgdata);
int HandleSeekGetPreHashAck(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgdata);

int doProtoBlockStatus(const BlockStatus& blockStatus, const std::string destNode);
int HandleBlockStatusMsg(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgdata);
#endif