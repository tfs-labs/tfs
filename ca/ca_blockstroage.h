#ifndef _BLOCK_STROAGE_
#define _BLOCK_STROAGE_

#include "ca/ca_blockmonitor.h"
#include "utils/MagicSingleton.h"
#include "utils/VRF.hpp"
#include <future>

using RetType = std::pair<std::string, uint16_t>;
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
	int AddBlock(const BlockMsg &msg);
	int UpdateBlock(const BlockMsg &msg);

	std::shared_future<RetType> GetPrehash(const uint64_t height);
	void commit_seek_task(uint64_t seek_height);
	void Force_commit_seek_task(uint64_t seek_height);
	bool is_seek_task(uint64_t seek_height);
private:

	void StartTimer();
	void BlockCheck();
	void composeEndBlockmsg(std::vector<BlockMsg> &msgvec);
	void Remove(const uint64_t &time);

	RetType SeekPreHashThread(uint64_t seek_height);
	RetType SeekPreHashByNode(
		const std::vector<std::string> &send_node_ids, uint64_t seek_height, const uint64_t &self_node_height, const uint64_t &chain_height);
private:

	CTimer _block_timer;
	std::mutex _block_mutex_;
	std::map<uint64_t, std::vector<BlockMsg>> _BlockMap;

	mutable std::shared_mutex prehash_mutex;
	std::map<uint64_t, std::shared_future<RetType>> PreHashMap;
};
static int GetPrehashFindNode(uint32_t num, uint64_t self_node_height, const std::vector<std::string> &pledge_addr,
                            std::vector<std::string> &send_node_ids);
void SendSeekGetPreHashReq(const std::string &node_id, const std::string &msg_id, uint64_t seek_height);
void SendSeekGetPreHashAck(SeekPreHashByHightAck& ack,const std::string &node_id, const std::string &msg_id, uint64_t seek_height);

int HandleSeekGetPreHashReq(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgdata);
int HandleSeekGetPreHashAck(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgdata);
#endif