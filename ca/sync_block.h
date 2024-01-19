/**
 * *****************************************************************************
 * @file        sync_block.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef TFS_CA_SYNC_BLOCK_H_
#define TFS_CA_SYNC_BLOCK_H_

#include "db/db_api.h"
#include "net/msg_queue.h"
#include "proto/sync_block.pb.h"
#include <cstdint>
#include <map>
#include "block_compare.h"
#include "utils/timer.hpp"
#include "utils/console.h"
#include "check_blocks.h"

struct FastSyncHelper
{
    uint32_t hits;
    std::set<std::string> ids;
    uint64_t height;
};

/**
 * @brief       
 * 
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
     * 
     */
    void ThreadStart();

    /**
     * @brief       
     * 
     * @param       start: 
     */
    void ThreadStart(bool start);

    /**
     * @brief       
     * 
     */
    void ThreadStop();

    /**
     * @brief       Get the Sync Node Simplify object
     * 
     * @param       num: 
     * @param       chainHeight: 
     * @param       pledgeAddr: 
     * @param       sendNodeIds: 
     * @return      int 
     */
    static int GetSyncNodeSimplify(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds);
    /**
     * @brief       Set the Fast Sync object
     * 
     * @param       syncStartHeight: 
     */
    static void SetFastSync(uint64_t syncStartHeight);

    static void SetNewSyncHeight(uint64_t height);

    /**
     * @brief       
     * 
     * @param       receiveCount: 
     * @param       hitCount: 
     * @return      true 
     * @return      false 
     */
    static bool check_byzantine(int receiveCount, int hitCount);

    /**
     * @brief       
     * 
     * @param       heightBlocks: 
     * @param       hash: 
     * @return      true 
     * @return      false 
     */
    static bool SumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& heightBlocks, std::string &hash);

private:
    /**
     * @brief       
     * 
     * @param       pledgeAddr: 
     * @param       chainHeight: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @return      true 
     * @return      false 
     */
    static bool _RunFastSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t startSyncHeight, uint64_t endSyncHeight);
    
    /**
     * @brief       
     * 
     * @param       pledgeAddr: 
     * @param       chainHeight: 
     * @param       selfNodeHeight: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @param       newSendNum: 
     * @return      int 
     */
    static int _RunNewSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t newSendNum = 0);
    
    /**
     * @brief       
     * 
     * @param       pledgeAddr: 
     * @param       chainHeight: 
     * @param       selfNodeHeight: 
     * @return      int 
     */
    int _RunFromZeroSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       
     * 
     * @param       sendNodeIds: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @param       requestHashs: 
     * @param       retNodeIds: 
     * @param       chainHeight: 
     * @return      true 
     * @return      false 
     */
    static bool _GetFastSyncSumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                                    std::vector<FastSyncBlockHashs> &requestHashs, std::vector<std::string> &retNodeIds, uint64_t chainHeight);
    
    /**
     * @brief       
     * 
     * @param       sendNodeId: 
     * @param       requestHashs: 
     * @param       chainHeight: 
     * @return      true 
     * @return      false 
     */
    static bool _GetFastSyncBlockData(const std::string &sendNodeId, const std::vector<FastSyncBlockHashs> &requestHashs, uint64_t chainHeight);

    /**********************************************************************************************************************************/
    
    /**
     * @brief       
     * 
     * @param       pledgeAddrSize: 
     * @param       sendNodeIds: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @param       needSyncHeights: 
     * @param       retNodeIds: 
     * @param       chainHeight: 
     * @param       newSyncSnedNum: 
     * @return      int 
     */
    static int _GetSyncSumHashNode(uint64_t pledgeAddrSize, const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                            std::map<uint64_t, uint64_t> &needSyncHeights, std::vector<std::string> &retNodeIds, uint64_t &chainHeight, uint64_t newSyncSnedNum);
    // static int _GetSyncBlockHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight,
    /**
     * @brief       
     * 
     * @param       sendNodeIds: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @param       selfNodeHeight: 
     * @param       chainHeight: 
     * @param       newSyncSnedNum: 
     * @return      int 
     */
    static int _GetSyncBlockBySumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight, uint64_t newSyncSnedNum);
    
    /**
     * @brief       
     * 
     * @param       sendNodeIds: 
     * @param       startSyncHeight: 
     * @param       endSyncHeight: 
     * @param       selfNodeHeight: 
     * @param       chainHeight: 
     * @param       retNodeIds: 
     * @param       reqHashes: 
     * @param       newSyncSnedNum: 
     * @return      int 
     */
    static int _GetSyncBlockHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight,
                            std::vector<std::string> &retNodeIds, std::vector<std::string> &reqHashes, uint64_t newSyncSnedNum);
    
    /**
     * @brief       
     * 
     * @param       sendNodeIds: 
     * @param       reqHashes: 
     * @param       chainHeight: 
     * @return      int 
     */
    static int _GetSyncBlockData(const std::vector<std::string> &sendNodeIds, const std::vector<std::string> &reqHashes, uint64_t chainHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       
     * 
     * @param       sendNodeIds: 
     * @param       sendHeights: 
     * @param       selfNodeHeight: 
     * @param       retNodeIds: 
     * @param       sumHashs: 
     * @return      int 
     */
    static int _GetFromZeroSyncSumHashNode(const std::vector<std::string> &sendNodeIds, const std::vector<uint64_t>& sendHeights, uint64_t selfNodeHeight, std::set<std::string> &retNodeIds, std::map<uint64_t, std::string>& sumHashs);
    
    /**
     * @brief       
     * 
     * @param       sumHashes: 
     * @param       sendHeights: 
     * @param       setSendNodeIds: 
     * @param       selfNodeHeight: 
     * @return      int 
     */
    int _GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sumHashes, std::vector<uint64_t> &sendHeights, std::set<std::string> &setSendNodeIds, uint64_t selfNodeHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       
     * 
     * @param       num: 
     * @param       heightBaseline: 
     * @param       discardComparator: 
     * @param       reserveComparator: 
     * @param       pledgeAddr: 
     * @param       sendNodeIds: 
     * @return      int 
     */
    static int _GetSyncNodeBasic(uint32_t num, uint64_t heightBaseline, const std::function<bool(uint64_t, uint64_t)>& discardComparator, const std::function<bool(uint64_t, uint64_t)>& reserveComparator, const std::vector<std::string> &pledgeAddr,
                         std::vector<std::string> &sendNodeIds);
    
    /**
     * @brief       
     * 
     * @param       num: 
     * @param       chainHeight: 
     * @param       pledgeAddr: 
     * @param       sendNodeIds: 
     * @return      int 
     */
    static int _GetSyncNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                    std::vector<std::string> &sendNodeIds);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       syncBlockData: 
     */
    static void _AddBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData);
    
    /**
     * @brief       
     * 
     * @param       chainHeight: 
     * @param       pledgeAddr: 
     * @param       selectedAddr: 
     * @return      true 
     * @return      false 
     */
    static bool _NeedByzantineAdjustment(uint64_t chainHeight, const vector<std::string> &pledgeAddr,
                                 std::vector<std::string> &selectedAddr);

    /**
     * @brief       
     * 
     * @param       chainHeight: 
     * @param       pledgeAddr: 
     * @param       nodes: 
     * @param       qualifyingStakeNodes: 
     * @return      true 
     * @return      false 
     */
    static bool _CheckRequirementAndFilterQualifyingNodes(uint64_t chainHeight, const vector<std::string> &pledgeAddr,
                const vector<Node> &nodes,vector<std::string> &qualifyingStakeNodes);

    /**
     * @brief       
     * 
     * @param       nodes: 
     * @param       qualifyingStakeNodes: 
     * @param       sumHash: 
     * @return      true 
     * @return      false 
     */
    static bool _GetSyncNodeSumhashInfo(const vector<Node> &nodes, const vector<string> &qualifyingStakeNodes,
                                       map<string, pair<uint64_t, vector<string>>> sumHash);

    /**
     * @brief       
     * 
     * @param       sumHash: 
     * @param       selectedAddr: 
     * @return      true 
     * @return      false 
     */
    static bool _GetSelectedAddr(map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sumHash,
                                vector<std::string> &selectedAddr);
    // friend std::string PrintCache(int where);
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

#endif
