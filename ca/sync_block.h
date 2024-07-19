/**
 * *****************************************************************************
 * @file        sync_block.h
 * @brief       Synchronize data blocks to other investment staking nodes
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef TFS_CA_SYNC_BLOCK_H_
#define TFS_CA_SYNC_BLOCK_H_

#include <map>
#include <cstdint>

#include "net/msg_queue.h"
#include "proto/sync_block.pb.h"
#include "utils/timer.hpp"
#include "ca/check_blocks.h"
#include "ca/block_compare.h"

struct FastSyncHelper
{
    uint32_t hits;
    std::set<std::string> ids;
    uint64_t height;
};

/**
 * @brief       
 * sync block
 */
class SyncBlock
{
friend class CheckBlocks;
public:

    SyncBlock() = default;
    ~SyncBlock() = default;
    SyncBlock(SyncBlock &&) = delete;
    SyncBlock(const SyncBlock &) = delete;
    SyncBlock &operator=(SyncBlock &&) = delete;
    SyncBlock &operator=(const SyncBlock &) = delete;

    /**
     * @brief       
     * Start synchronization thread
     */
    void ThreadStart();

    /**
     * @brief    Enable or disable synchronization threads   
     * 
     * @param       start: Enable or disable
     */
    void ThreadStart(bool start);

    /**
     * @brief    disable synchronization threads
     * 
     */
    void ThreadStop();

    /**
     * @brief       Get the Sync Node list
     * 
     * @param       num: sync node number
     * @param       chainHeight: Current whole network chain height
     * @param       pledgeAddr:  Investment pledge list
     * @param       sendNodeIds: sync node ids
     * @return      int : Return 0 on success
     */
    static int GetSyncNodeSimplify(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);
    /**
     * @brief       Set the Fast Sync object
     * 
     * @param       syncStartHeight: sync height
     */
    static void SetFastSync(uint64_t syncStartHeight);

    /**
     * @brief       Set the new Sync object
     * 
     * @param       syncStartHeight: sync begin height
     */
    static void SetNewSyncHeight(uint64_t height);

    /**
     * @brief   check byzantine    
     * 
     * @param       receiveCount: receive num
     * @param       hitCount:   hit num
     * @return      true    check success
     * @return      false   check fail
     */
    static bool checkByzantine(int receiveCount, int hitCount);

    /**
     * @brief       Calculate height sum hash    
     * 
     * @param       heightBlocks: block height
     * @param       hash: block hash
     * @return      true    hash = sum hash
     * @return      false   hash = nullptr
     */
    static bool SumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& heightBlocks, std::string &hash);

    /**
     * @brief       get sync node
     * 
     * @param       num: sync send num
     * @param       heightBaseline: filter height
     * @param       discardComparator: comparators
     * @param       reserveComparator: comparators
     * @param       pledgeAddr: Investment pledge list
     * @param       sendNodeIds: send node list ids
     * @return      int return 0 success
     */
    static int _GetSyncNodeBasic(uint32_t num, uint64_t heightBaseline, const std::function<bool(uint64_t, uint64_t)>& discardComparator, const std::function<bool(uint64_t, uint64_t)>& reserveComparator, const std::vector<std::string> &pledgeAddr,
                         std::vector<std::string> &sendNodeIds);

private:
    /**
     * @brief       fast sync 
     * 
     * @param       pledgeAddr: Investment pledge list
     * @param       chainHeight: Current chain height
     * @param       startSyncHeight: fast sync start height
     * @param       endSyncHeight: fast sync end height
     * @return      true    fast sync success
     * @return      false   fast sync fail
     */
    static bool _RunFastSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t startSyncHeight, uint64_t endSyncHeight, uint32_t syncNodeCount);
    
    /**
     * @brief       new sync
     * 
     * @param       pledgeAddr: Investment pledge list
     * @param       chainHeight: Current chain height
     * @param       selfNodeHeight: self node height
     * @param       startSyncHeight: new sync start height
     * @param       endSyncHeight: new sync end height
     * @param       newSendNum: new sync send node num
     * @return      int  return 0 success 
     */
    static int _RunNewSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight, uint64_t startSyncHeight, uint64_t endSyncHeight, uint32_t syncNodeCount);
    
    /**
     * @brief       from zero sync 
     * 
     * @param       pledgeAddr: Investment pledge list
     * @param       chainHeight: Current chain height
     * @param       selfNodeHeight: self node height
     * @return      int  return 0 success 
     */
    int _RunFromZeroSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight, uint32_t syncNodeCount);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       fast sync get block hash
     * 
     * @param       sendNodeIds: send node list ids
     * @param       startSyncHeight: get block start height
     * @param       endSyncHeight: get block  end height
     * @param       requestHashs: need request block hash
     * @param       retNodeIds: list of successfully returned nodes
     * @param       chainHeight: current chain height
     * @return      true    success
     * @return      false   fail
     */
    static bool _GetFastSyncSumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                                    std::vector<FastSyncBlockHashs> &requestHashs, std::vector<std::string> &retNodeIds, uint64_t chainHeight);
    
    /**
     * @brief       fast sync get block
     * 
     * @param       sendNodeId: send node list ids
     * @param       requestHashs: need request block hash
     * @param       chainHeight: current chain height
     * @return      true    success
     * @return      false   fail
     */
    static bool _GetFastSyncBlockData(const std::string &sendNodeId, const std::vector<FastSyncBlockHashs> &requestHashs, uint64_t chainHeight);

    /**********************************************************************************************************************************/
    
    /**
     * @brief       new sync get block sum hash
     * 
     * @param       pledgeAddrSize: Investment pledge list
     * @param       sendNodeIds: send node list ids
     * @param       startSyncHeight: get block sum hash start height
     * @param       endSyncHeight: get block sum hash end height
     * @param       needSyncHeights: need sync height
     * @param       retNodeIds: list of successfully returned nodes
     * @param       chainHeight: current chain height
     * @param       newSyncSnedNum: new sync send node num
     * @return      int     return 0 success 
     */
    static int _GetSyncSumHashNode(uint64_t pledgeAddrSize, const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                            std::map<uint64_t, uint64_t> &needSyncHeights, std::vector<std::string> &retNodeIds, uint64_t &chainHeight, uint64_t newSyncSnedNum);
    /**
     * @brief       synchronize data to the most forks in the entire network
     * 
     * @param       sendNodeIds: send node list ids
     * @param       startSyncHeight:  get block hash start height
     * @param       endSyncHeight: get block hash end height
     * @param       selfNodeHeight: self node height
     * @param       chainHeight: current chain height
     * @param       newSyncSnedNum: new sync send node num
     * @return      int return 0 success 
     */
    static int _GetSyncBlockBySumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight, uint64_t newSyncSnedNum);
    
    /**
     * @brief       new sync get block hash
     * 
     * @param       sendNodeIds: send node list ids
     * @param       startSyncHeight: get block hash start height
     * @param       endSyncHeight: get block hash end height
     * @param       selfNodeHeight: self node height
     * @param       chainHeight: current chain height
     * @param       retNodeIds: list of successfully returned nodes
     * @param       reqHashes: need sync block hash
     * @param       newSyncSnedNum: new sync send node num
     * @return      int int return 0 success 
     */
    static int _GetSyncBlockHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight,
                            std::vector<std::string> &retNodeIds, std::vector<std::string> &reqHashes, uint64_t newSyncSnedNum);
    
    /**
     * @brief       new sync get block 
     * 
     * @param       sendNodeIds: send node list ids
     * @param       reqHashes: need sync block hash
     * @param       chainHeight: current chain height
     * @return      int return 0 success
     */
    static int _GetSyncBlockData(const std::vector<std::string> &sendNodeIds, const std::vector<std::string> &reqHashes, uint64_t chainHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       from zero sync get sum hash
     * 
     * @param       sendNodeIds: send node list ids
     * @param       sendHeights: get block sum hash start height
     * @param       selfNodeHeight: get block sum hash end height
     * @param       retNodeIds: list of successfully returned nodes
     * @param       sumHashs: need sync block sum hash
     * @return      int return 0 success
     */
    static int _GetFromZeroSyncSumHashNode(const std::vector<std::string> &sendNodeIds, const std::vector<uint64_t>& sendHeights, uint64_t selfNodeHeight, std::set<std::string> &retNodeIds, std::map<uint64_t, std::string>& sumHashs);
    
    /**
     * @brief       from zero sync get block
     * 
     * @param       sumHashes: need sync block sum hash
     * @param       sendHeights: send node list ids
     * @param       setSendNodeIds: send node list ids
     * @param       selfNodeHeight: get block sum hash end height
     * @return      int return 0 success
     */
    int _GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sumHashes, std::vector<uint64_t> &sendHeights, std::set<std::string> &setSendNodeIds, uint64_t selfNodeHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       get sync node
     * 
     * @param       num: sync send num
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Investment pledge list
     * @param       sendNodeIds: send node list ids
     * @return      int return 0 success
     */
    static int _GetSyncNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                    std::vector<std::string> &sendNodeIds);

    /**
     * @brief       add blocks that need to be rolled back
     * 
     * @param       block: need to be rolled back
     * @param       syncBlockData: rolled back block set
     */
    static void _AddBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData);
    
    /**
     * @brief       need byzantine adjustment
     * 
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Investment pledge list
     * @param       selectedAddr: selected addr
     * @return      true    success
     * @return      false   fail
     */
    static bool _NeedByzantineAdjustment(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                 std::vector<std::string> &selectedAddr);

    /**
     * @brief       Check Requirement And Filter Qualifying Nodes
     * 
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Investment pledge list
     * @param       nodes: node
     * @param       qualifyingStakeNodes: qualify stake nodes
     * @return      true    success
     * @return      false   fail
     */
    static bool _CheckRequirementAndFilterQualifyingNodes(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                const std::vector<Node> &nodes,std::vector<std::string> &qualifyingStakeNodes);

    /**
     * @brief       Get Sync Node Sumhash Info
     * 
     * @param       nodes: node 
     * @param       qualifyingStakeNodes: qualify stake nodes
     * @param       sumHash: sum hash
     * @return      true    success
     * @return      false   fail
     */
    static bool _GetSyncNodeSumhashInfo(const std::vector<Node> &nodes, const std::vector<std::string> &qualifyingStakeNodes,
                                       std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHash);

    /**
     * @brief        Get selected Addr
     * 
     * @param       sumHash: sum hash
     * @param       selectedAddr: selected addr
     * @return      true    success
     * @return      false   fail
     */
    static bool _GetSelectedAddr(std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sumHash,
                                std::vector<std::string> &selectedAddr);
    std::thread _syncThread; 
    std::atomic<bool> _syncThreadRuning;
    std::mutex _syncThreadRuningMutex;
    std::condition_variable _syncThreadRuningCondition;

    uint32_t _fastSyncHeightCnt{};
    bool _syncing{} ;

    std::map<uint64_t, std::map<uint64_t, std::set<CBlock, CBlockCompare>>> _syncFromZeroCache;
    std::vector<uint64_t> _syncFromZeroReserveHeights;
    std::mutex _syncFromZeroCacheMutex;
    const int kSyncBound = 200;

};

void SendFastSyncGetHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendFastSyncGetHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendFastSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs);
void SendFastSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs);

int HandleFastSyncGetHashReq(const std::shared_ptr<FastSyncGetHashReq> &msg, const MsgData &msgdata);
int HandleFastSyncGetHashAck(const std::shared_ptr<FastSyncGetHashAck> &msg, const MsgData &msgdata);
int HandleFastSyncGetBlockReq(const std::shared_ptr<FastSyncGetBlockReq> &msg, const MsgData &msgdata);
int HandleFastSyncGetBlockAck(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata);

void SendSyncGetSumHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendSyncGetSumHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendSyncGetHeightHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendSyncGetHeightHashAck(SyncGetHeightHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void SendSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, const std::vector<std::string> &reqHashes);
void SendSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);

int HandleSyncGetSumHashReq(const std::shared_ptr<SyncGetSumHashReq> &msg, const MsgData &msgdata);
int HandleSyncGetSumHashAck(const std::shared_ptr<SyncGetSumHashAck> &msg, const MsgData &msgdata);
int HandleSyncGetHeightHashReq(const std::shared_ptr<SyncGetHeightHashReq> &msg, const MsgData &msgdata);
int HandleSyncGetHeightHashAck(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata);
int HandleSyncGetBlockReq(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata);
int HandleSyncGetBlockAck(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata);


void SendFromZeroSyncGetSumHashReq(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights);
void SendFromZeroSyncGetSumHashAck(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights);
void SendFromZeroSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, uint64_t height);
void SendFromZeroSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, uint64_t height);

int HandleFromZeroSyncGetSumHashReq(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetSumHashAck(const std::shared_ptr<SyncFromZeroGetSumHashAck> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetBlockReq(const std::shared_ptr<SyncFromZeroGetBlockReq> &msg, const MsgData &msgdata);
int HandleFromZeroSyncGetBlockAck(const std::shared_ptr<SyncFromZeroGetBlockAck> &msg, const MsgData &msgdata);

void SendSyncNodeHashReq(const std::string &nodeId, const std::string &msgId);
void SendSyncNodeHashAck(const std::string &nodeId, const std::string &msgId);
int HandleSyncNodeHashReq(const std::shared_ptr<SyncNodeHashReq> &msg, const MsgData &msgdata);
int HandleSyncNodeHashAck(const std::shared_ptr<SyncNodeHashAck> &msg, const MsgData &msgdata);

int HandleBlockByUtxoReq(const std::shared_ptr<GetBlockByUtxoReq> &msg, const MsgData &msgdata);
int HandleBlockByUtxoAck(const std::shared_ptr<GetBlockByUtxoAck> &msg, const MsgData &msgdata);
int HandleBlockByHashReq(const std::shared_ptr<GetBlockByHashReq> &msg, const MsgData &msgdata);
int HandleBlockByHashAck(const std::shared_ptr<GetBlockByHashAck> &msg, const MsgData &msgdata);

#endif
