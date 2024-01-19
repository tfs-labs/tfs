/**
 * *****************************************************************************
 * @file        block_stroage.h
 * @brief       
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _BLOCK_STROAGE_
#define _BLOCK_STROAGE_

#include "ca/block_monitor.h"
#include "utils/magic_singleton.h"
#include "utils/vrf.hpp"
#include "block.pb.h"
#include <future>

using RetType = std::pair<std::string, uint16_t>;

enum class BroadcastType
{
	verifyBroadcast,
	level1Broadcast
};
struct BlockStatusWrapper
{
	std::string blockHash;
	CBlock block;
	BroadcastType broadcastType;
	uint32_t verifyNodesNumber;
	uint32_t level1NodesNumber;
	std::vector<BlockStatus> blockStatusList = {};
	std::set<std::string> verifyNodes = {};
	std::set<std::string> level1Nodes = {};
	std::map<std::string, Vrf> vrfMap = {};
	std::map<std::string, Vrf> txvrfMap = {};
};
class BlockStroage
{
public:
    BlockStroage(){ _StartTimer(); };
    ~BlockStroage() = default;
    BlockStroage(BlockStroage &&) = delete;
    BlockStroage(const BlockStroage &) = delete;
    BlockStroage &operator=(BlockStroage&&) = delete;
    BlockStroage &operator=(const BlockStroage &) = delete;



public:


	/**
	 * @brief       
	 * 
	 */
	void StopTimer(){_blockTimer.Cancel();}

	/**
	 * @brief       
	 * 
	 * @param       msg: 
	 * @return      int 
	 */
	int AddBlock(const BlockMsg &msg);

	/**
	 * @brief       
	 * 
	 * @param       msg: 
	 * @return      int 
	 */
	int UpdateBlock(const BlockMsg &msg);

	/**
	 * @brief       Get the Prehash object
	 * 
	 * @param       height: 
	 * @return      std::shared_future<RetType> 
	 */
	std::shared_future<RetType> GetPrehash(const uint64_t height);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 */
	void CommitSeekTask(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 */
	void ForceCommitSeekTask(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 */
	void ClearPreHashMap();

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 * @return      true 
	 * @return      false 
	 */
	bool IsSeekTask(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 */
	void NewbuildBlockByBlockStatus(const std::string blockHash);

	/**
	 * @brief       
	 * 
	 * @param       blockStatus: 
	 */
	void AddBlockStatus(const BlockStatus& blockStatus);

	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 * @param       Block: 
	 * @param       level1Nodes: 
	 */
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes);
	
	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 * @param       Block: 
	 * @param       verifyNodes: 
	 * @param       vrf: 
	 */
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes, const Vrf& vrf);
	
	/**
	 * @brief       
	 * 
	 */
	void ExpiredDeleteCheck();

	/**
	 * @brief       
	 * 
	 * @param       blockStatus: 
	 * @return      int 
	 */
	int CheckData(const BlockStatus& blockStatus);

	/**
	 * @brief       
	 * 
	 * @param       oldBlock: 
	 * @param       newBlock: 
	 * @return      int 
	 */
	int InitNewBlock(const CBlock& oldBlock, CBlock& newBlock);

private:

	/**
	 * @brief       
	 * 
	 */
	void _StartTimer();

	/**
	 * @brief       
	 * 
	 */
	void _BlockCheck();

	/**
	 * @brief       
	 * 
	 * @param       msgvec: 
	 */
	int _ComposeEndBlockmsg(const std::vector<BlockMsg> &msgvec, BlockMsg & outMsg , bool isVrf);

	/**
	 * @brief       
	 * 
	 * @param       hash: 
	 */
	void _Remove(const std::string &hash);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 * @return      RetType 
	 */
	RetType _SeekPreHashThread(uint64_t seekHeight);
	int VerifyBlockFlowSignNode(const BlockMsg & blockmsg);

	/**
	 * @brief       
	 * 
	 * @param       sendNodeIds: 
	 * @param       seekHeight: 
	 * @param       selfNodeHeight: 
	 * @param       chainHeight: 
	 * @return      RetType 
	 */
	RetType _SeekPreHashByNode(
		const std::vector<std::string> &sendNodeIds, uint64_t seekHeight, const uint64_t &selfNodeHeight, const uint64_t &chainHeight);
private:
	/**
	 * @brief       
	 * 
	 * @param       where: 
	 * @return      std::string 
	 */
    friend std::string PrintCache(int where);
	CTimer _blockTimer;
	mutable std::shared_mutex _blockMutex;
	std::map<std::string , std::vector<BlockMsg>> _blockCnt;

	mutable std::shared_mutex _prehashMutex;
	std::map<uint64_t, std::shared_future<RetType>> _preHashMap;

	std::mutex _statusMutex;
	std::map<std::string, BlockStatusWrapper> _blockStatusMap;


	double _failureRate = 0.75;
};
static int GetPrehashFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);
void SendSeekGetPreHashReq(const std::string &nodeId, const std::string &msgId, uint64_t seekHeight);
void SendSeekGetPreHashAck(SeekPreHashByHightAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t seekHeight);

int HandleSeekGetPreHashReq(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgdata);
int HandleSeekGetPreHashAck(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgdata);

int DoProtoBlockStatus(const BlockStatus& blockStatus, const std::string destNode);
int HandleBlockStatusMsg(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgdata);


#endif