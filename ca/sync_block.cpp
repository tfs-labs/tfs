#include "logging.h"
#include "utils/magic_singleton.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/txhelper.h"
#include "common/global_data.h"
#include "db/db_api.h"
#include "ca/sync_block.h"
#include "global.h"
#include "block_cache.h"
#include "utils/console.h"

#include "block_helper.h"
#include "utils/account_manager.h"
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
const static uint64_t kStabilityTime = 60 * 1000000;
static uint64_t newSyncFailHeight = 0;
static bool runFastSync = false;
static bool inFastSyncFail = false;

static bool runNewSync = false;
static uint64_t newSyncStartHeight = 0;

static uint32_t syncHeightCnt = 100;
const static uint32_t kSyncHeightTime = 50;
const static uint32_t kSyncStuckOvertimeCount = 6;
static uint64_t syncSendNewSyncNum = UINT32_MAX;
static uint64_t syncSendFastSyncNum = UINT32_MAX;
static uint64_t syncSendZeroSyncNum = UINT32_MAX;
const  static int kNnormalSumHashNum = 1;
static uint32_t runFastSuncNum = 0;
static uint32_t runZeroSyncNum = 0;
// static bool sync_to_tx = false;
// #include <fstream> 
// static std::ofstream fout("./test.txt", std::ios::trunc); 


inline static global::ca::SaveType GetSaveSyncType(uint64_t height, uint64_t chainHeight)
{
    if (chainHeight <= global::ca::sum_hash_range)
    {
        return global::ca::SaveType::SyncNormal;
    }
    
    global::ca::SaveType saveType;
    if(ca_algorithm::GetSumHashFloorHeight(chainHeight) - global::ca::sum_hash_range <= height)
    {
        saveType = global::ca::SaveType::SyncNormal;
    }
    else
    {
        saveType = global::ca::SaveType::SyncFromZero;
    }
    return saveType;
}
static bool SumHeightHash(std::vector<std::string> &blockHashes, std::string &hash)
{
    std::sort(blockHashes.begin(), blockHashes.end());
    hash = Getsha256hash(StringUtil::concat(blockHashes, ""));
    return true;
}

bool SyncBlock::SumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& heightBlocks, std::string &hash)
{
    std::vector<std::string> heightBlockHashes;
    for(auto height_block : heightBlocks)
    {
        std::string sumHash;
        SumHeightHash(height_block.second, sumHash);
        heightBlockHashes.push_back(sumHash);
    }

    SumHeightHash(heightBlockHashes, hash);
    return true;
}

static bool GetHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes)
{
    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(startHeight, endHeight, blockHashes))
    {
        return false;
    }
    return true;
}

static bool GetHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::vector<FastSyncBlockHashs> &blockHeightHashes)
{
    DBReader dbReader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("(GetHeightBlockHash) GetBlockTop failed !");
        return false;
    }
    
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    while (startHeight <= endHeight && startHeight <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(startHeight, hashs))
        {
            return false;
        }
        std::vector<std::string> blocksRaw;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlocksByBlockHash(hashs, blocksRaw))
        {
            return false;
        }
        FastSyncBlockHashs fastSyncBlockHashs;
        fastSyncBlockHashs.set_height(startHeight);
        for(const auto& block_raw : blocksRaw)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                return false;
            }

            if((currentTime - block.time()) < kStabilityTime)
            {
                continue;
            }
            fastSyncBlockHashs.add_hashs(block.hash());
        }
        blockHeightHashes.push_back(fastSyncBlockHashs);
        ++startHeight;
    }
    return true;
}

static bool GetHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::map<uint64_t, std::vector<std::string>> &blockHeightHashes)
{
    DBReader dbReader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("(GetHeightBlockHash) GetBlockTop failed !");
        return false;
    }
    while (startHeight < endHeight && startHeight <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(startHeight, hashs))
        {
            return false;
        }
        blockHeightHashes[startHeight] = hashs;
        ++startHeight;
    }
    return true;
}

void  SyncBlock::ThreadStop(){

    _syncThreadRuning=false;
}

void SyncBlock::ThreadStart(bool start)
{
    _syncThreadRuning = start; 
}

void SyncBlock::ThreadStart()
{
    if(syncHeightCnt > kSyncBound)
    {
        syncHeightCnt = kSyncBound;
    }
    _fastSyncHeightCnt = 0; // 0 means "this variable doesn't use for now"
    
    _syncThreadRuning = true;
    _syncThread = std::thread(
        [this]()
        {
            uint32_t  sleepTime = kSyncHeightTime;
            while (1)
            {
                if(!_syncThreadRuning)
                {
                    sleep(sleepTime);
                    continue;
                }
                std::unique_lock<std::mutex> blockThreadRuningLocker(_syncThreadRuningMutex);
                _syncThreadRuningCondition.wait_for(blockThreadRuningLocker, std::chrono::seconds(sleepTime));
                _syncThreadRuningMutex.unlock();
                if (!_syncThreadRuning)
                {
                    break;
                }
                else
                {
                    sleepTime = kSyncHeightTime;
                }
                uint64_t chainHeight = 0;
                if(!MagicSingleton<BlockHelper>::GetInstance()->ObtainChainHeight(chainHeight))
                {
                    continue;
                }
                uint64_t selfNodeHeight = 0;
                std::vector<std::string> pledgeAddr; // stake and invested addr
                {

                    DBReader dbReader;
                    auto status = dbReader.GetBlockTop(selfNodeHeight);
                    if (DBStatus::DB_SUCCESS != status)
                    {
                        continue;
                    }
                    std::vector<std::string> stakeAddr;
                    status = dbReader.GetStakeAddress(stakeAddr);
                    if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
                    {
                        continue;
                    }

                    for(const auto& addr : stakeAddr)
                    {
                        if(VerifyBonusAddr(addr) != 0)
                        {
                            DEBUGLOG("{} doesn't get invested, skip", addr);
                            continue;
                        }
                        pledgeAddr.push_back(addr);
                    }
                }

                uint64_t startSyncHeight = 0;
                uint64_t endSyncHeight = 0;

                if (!_syncFromZeroCache.empty())
                {
                    for (auto iter = _syncFromZeroCache.begin(); iter != _syncFromZeroCache.end();)
                    {
                        if (iter->first < selfNodeHeight)
                        {
                            iter = _syncFromZeroCache.erase(iter);
                        }
                        else
                        {
                            iter++;
                        }
                    }
                }

                if(runFastSuncNum >= 5)
                {
                    if(runFastSync)
                    {
                        runFastSuncNum = 0;
                        runFastSync = false;
                    }
                }

                if(runNewSync)
                {
                    runFastSuncNum = 0;
                    runFastSync = false;
                }

                if(runFastSync)
                {
                    ++runFastSuncNum;

                    if (newSyncFailHeight > _fastSyncHeightCnt)
                    {
                        startSyncHeight = newSyncFailHeight - _fastSyncHeightCnt;
                    }
                    else
                    {
                        startSyncHeight = newSyncFailHeight;
                    }

                    if(startSyncHeight > selfNodeHeight)
                    {
                        startSyncHeight = selfNodeHeight + 1;
                    }

                    endSyncHeight = startSyncHeight + _fastSyncHeightCnt;

                    INFOLOG("begin fast sync {} {} ", startSyncHeight, endSyncHeight);
                    bool if_success = _RunFastSyncOnce(pledgeAddr, chainHeight, startSyncHeight, endSyncHeight);
                    if(!if_success)
                    {
                        syncSendFastSyncNum += 15;
                        DEBUGLOG("fast sync fail");
                    }
                    else
                    {
                        syncSendFastSyncNum = 10;
                    }
                    runFastSync = false;
                    if(inFastSyncFail)
                    {
                        inFastSyncFail = false;
                        runFastSync = true;
                    }

                }
                else
                { 
                    auto syncType = GetSaveSyncType(selfNodeHeight, chainHeight);
                    if(runNewSync || runZeroSyncNum > 2)
                    {
                        syncType = global::ca::SaveType::SyncNormal;
                    }
                    if (syncType == global::ca::SaveType::SyncFromZero)
                    {
                        INFOLOG("begin from zero sync");
                        int runStatus = _RunFromZeroSyncOnce(pledgeAddr, chainHeight, selfNodeHeight);
                        if (runStatus != 0)
                        {
                            ++runZeroSyncNum;
                            ERRORLOG("from zero sync fail ret: {},   runZeroSyncNum: {}", runStatus, runZeroSyncNum);
                        }
                        DEBUGLOG("_RunFromZeroSyncOnce ret: {}", runStatus);
                    }
                    else
                    {
                        if(newSyncFailHeight != 0)
                        {
                            selfNodeHeight = newSyncFailHeight;
                        }
                        if (selfNodeHeight > syncHeightCnt)
                        {
                            startSyncHeight = selfNodeHeight - syncHeightCnt;
                            if(startSyncHeight <= 0)
                            {
                                startSyncHeight = 1;
                            }
                        }
                        else
                        {
                            startSyncHeight = 1;
                        }

                        if(chainHeight - 10 <= selfNodeHeight)
                        {
                            endSyncHeight = selfNodeHeight + 1;
                        }
                        else
                        {
                            endSyncHeight = selfNodeHeight + syncHeightCnt;
                        }
                        sleepTime = kSyncHeightTime;
                        INFOLOG("begin new sync {} {} ", startSyncHeight, endSyncHeight);

                        if(runNewSync)
                        {
                            if(newSyncStartHeight > selfNodeHeight)
                            {
                                newSyncStartHeight = selfNodeHeight;
                            }
                            startSyncHeight = newSyncStartHeight > syncHeightCnt ? newSyncStartHeight - syncHeightCnt : 1;
                            endSyncHeight = newSyncStartHeight + syncHeightCnt;
                        }

                        int runStatus = _RunNewSyncOnce(pledgeAddr, chainHeight, selfNodeHeight, startSyncHeight, endSyncHeight, 0);
                        if(runStatus < 0)
                        {   
                            newSyncFailHeight = 0;
                            runFastSync = false;
                            syncSendNewSyncNum *= 10;
                        }
                        if(runStatus == 0)
                        {
                            runZeroSyncNum = 0;
                            runNewSync = false;
                            newSyncStartHeight = 0;
                            newSyncFailHeight = 0;
                            runFastSync = false;
                            syncSendNewSyncNum = 25;
                        }
                        if(runStatus > 0)
                        {
                            syncSendNewSyncNum *= 10;
                        }
                        DEBUGLOG("_RunNewSyncOnce sync return: {}", runStatus);
                    }
                }
            }
        });
    _syncThread.detach();
}




bool SyncBlock::_RunFastSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t startSyncHeight, uint64_t endSyncHeight)
{
    if (startSyncHeight > endSyncHeight)
    {
        return false;
    }
    std::vector<std::string> sendNodeIds;
    if (_GetSyncNode(syncSendFastSyncNum, chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
        return false;
    }
    std::vector<std::string> retNodeIds;
    std::vector<FastSyncBlockHashs> requestHashs;

    INFOLOG("begin _GetFastSyncSumHashNode {} {} ", startSyncHeight, endSyncHeight);
    if (!_GetFastSyncSumHashNode(sendNodeIds, startSyncHeight, endSyncHeight, requestHashs, retNodeIds, chainHeight))
    {
        ERRORLOG("get sync sum hash fail");
        return false;
    }
    bool flag = false;
    for (auto &nodeId : retNodeIds)
    {
        DEBUGLOG("fast sync block from {}", nodeId);
        if (_GetFastSyncBlockData(nodeId,requestHashs, chainHeight))
        {
            flag = true;
            break;
        }
    }
    return flag;
}

int SyncBlock::_RunNewSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t newSendNum)
{
    DEBUGLOG("_RunNewSyncOnce  startSyncHeight: {}\tendSyncHeight: ", startSyncHeight, endSyncHeight);
    uint64_t newSyncSnedNum = 0;
    if(newSendNum == 0)
    {
        newSyncSnedNum = syncSendNewSyncNum;
    }
    else 
    {
        newSyncSnedNum = newSendNum;
    }

    int ret = 0;
    if (startSyncHeight > endSyncHeight)
    {
        ret = -1;
        return ret;
    }
    std::vector<std::string> sendNodeIds;
    if ((ret = _GetSyncNode(newSyncSnedNum, chainHeight, pledgeAddr, sendNodeIds)) != 0)
    {
        ERRORLOG("get sync node fail");
        return ret - 1000;
    }

    std::map<uint64_t, uint64_t> needSyncHeights;
    std::vector<std::string> retNodeIds;
    chainHeight = 0;
    DEBUGLOG("_GetSyncSumHashNode begin:{}:{}", startSyncHeight, endSyncHeight);
    if ((ret = _GetSyncSumHashNode(pledgeAddr.size(), sendNodeIds, startSyncHeight, endSyncHeight, needSyncHeights, retNodeIds, chainHeight, newSyncSnedNum)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    bool fastSyncFlag = false;
    if(ret == 99)
    {
        fastSyncFlag = true;
    }
    DEBUGLOG("_GetSyncSumHashNode success:{}:{}", startSyncHeight, endSyncHeight);
    std::vector<std::string> secRetNodeIds;
    std::vector<std::string> reqHashes;
    for (auto syncHeight : needSyncHeights)
    {
        secRetNodeIds.clear();
        reqHashes.clear();
        DEBUGLOG("_GetSyncBlockHashNode begin:{}:{}", syncHeight.first, syncHeight.second);
        if ((ret = _GetSyncBlockHashNode(retNodeIds, syncHeight.first, syncHeight.second, selfNodeHeight, chainHeight, secRetNodeIds, reqHashes, newSyncSnedNum)) != 0)
        {
            return ret - 3000;
        }
        DEBUGLOG("_GetSyncBlockHashNode success:{}:{}", syncHeight.first, syncHeight.second);
        DEBUGLOG("_GetSyncBlockData begin:{}", reqHashes.size());
        if ((ret = _GetSyncBlockData(secRetNodeIds, reqHashes, chainHeight)) != 0 )
        {
            if (runFastSync)
            {
                 return 2;
            }

            return ret -4000;
        }
        DEBUGLOG("_GetSyncBlockData success:{}", reqHashes.size());
    }

    if(runFastSync)
    {
        return 99;
    }
    if(fastSyncFlag)
    {
        return 1;
    }
    return 0;
}

int SyncBlock::_RunFromZeroSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t selfNodeHeight)
{
    int ret = 0;

    std::vector<std::string> sendNodeIds;
    if ((ret = GetSyncNodeSimplify(syncSendZeroSyncNum, chainHeight, pledgeAddr, sendNodeIds)) != 0)
    {
        return ret - 1000;
    }
    std::set<std::string> retNodeIds;
    std::vector<uint64_t> heights;
    if (!_syncFromZeroReserveHeights.empty())
    {
        _syncFromZeroReserveHeights.swap(heights);
    }
    else
    {
        uint64_t heightCeiling = ca_algorithm::GetSumHashFloorHeight(chainHeight) - global::ca::sum_hash_range; //100

        auto syncHeight = ca_algorithm::GetSumHashCeilingHeight(selfNodeHeight);

        if(heightCeiling - syncHeight > 1000)
        {
            heightCeiling = syncHeight + 1000;
        }

        heights = {syncHeight};
        for(auto i = sendNodeIds.size() - 1; i > 0; --i)
        {
            syncHeight += global::ca::sum_hash_range;
            if (syncHeight > heightCeiling)
            {
                break;
            }
            heights.push_back(syncHeight);
        }        
    }

    std::map<uint64_t, std::string> sumHashes;

    std::string requestHeights;
    for(auto height : heights)
    {
        requestHeights += " ";
        requestHeights += std::to_string(height);
    }

    DEBUGLOG("_GetFromZeroSyncSumHashNode begin{}", requestHeights);
    if ((ret = _GetFromZeroSyncSumHashNode(sendNodeIds, heights, selfNodeHeight, retNodeIds, sumHashes)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    DEBUGLOG("_GetFromZeroSyncSumHashNode success");

    DEBUGLOG("_GetFromZeroSyncBlockData begin");
    if ((ret = _GetFromZeroSyncBlockData(sumHashes, heights, retNodeIds, selfNodeHeight)) != 0 )
    {
        return ret -3000;
    }
    DEBUGLOG("_GetSyncBlockData success");
    return 0;
}

int SyncBlock::_GetSyncNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                           std::vector<std::string> &sendNodeIds)
{
    static bool forceFindBaseChain = false;
    DBReader dbReader;
    uint64_t selfNodeHeight = 0;
    auto status = dbReader.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }

    if (selfNodeHeight == chainHeight)
    {
        static uint64_t lastEqualHeight = 0;
        static uint32_t equalHeightCount = 0;

        if (selfNodeHeight == lastEqualHeight)
        {
            equalHeightCount++;
            if (equalHeightCount >= kSyncStuckOvertimeCount)
            {
                std::vector<std::string> selectedAddrs;
                if (_NeedByzantineAdjustment(chainHeight, pledgeAddr, selectedAddrs))
                {
                    auto discardComparator = std::less<>();
                    auto reserveComparator = std::greater_equal<>();
                    int ret = _GetSyncNodeBasic(num, chainHeight, discardComparator, reserveComparator, selectedAddrs,
                                            sendNodeIds);
                    if (ret != 0)
                    {
                        DEBUGLOG("get sync node fail, ret: {}", ret);
                        forceFindBaseChain = true;
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
            equalHeightCount = 0;
            lastEqualHeight = selfNodeHeight;
        }

    }

    if (forceFindBaseChain || selfNodeHeight < chainHeight)
    {
        DEBUGLOG("find node base on chain height {}", chainHeight);
        forceFindBaseChain = false;
        auto discardComparator = std::less<>();
        auto reserveComparator = std::greater_equal<>();
        return _GetSyncNodeBasic(num, chainHeight, discardComparator, reserveComparator, pledgeAddr, sendNodeIds);
    }
    else
    {
        DEBUGLOG("find node base on self height {}", selfNodeHeight);
        forceFindBaseChain = true;
        auto discardComparator = std::less_equal<>();
        auto reserveComparator = std::greater<>();
        return _GetSyncNodeBasic(num, selfNodeHeight, discardComparator, reserveComparator, pledgeAddr,
                                sendNodeIds);
    }
}

int SyncBlock::GetSyncNodeSimplify(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                   std::vector<std::string> &sendNodeIds)
{
    DBReader dbReader;
    uint64_t selfNodeHeight = 0;
    auto status = dbReader.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }
    DEBUGLOG("find node base on self height {}", selfNodeHeight);
    auto discardComparator = std::less<>();
    auto reserveComparator = std::greater_equal<>();
    return _GetSyncNodeBasic(num, selfNodeHeight, discardComparator, reserveComparator, pledgeAddr, sendNodeIds);
}

int SyncBlock::_GetSyncNodeBasic(uint32_t num, uint64_t heightBaseline,
                                const std::function<bool(uint64_t, uint64_t)>& discardComparator,
                                const std::function<bool(uint64_t, uint64_t)>& reserveComparator,
                                const std::vector<std::string> &pledgeAddr,
                                std::vector<std::string> &sendNodeIds)
{
    DEBUGLOG("{} Nodes have passed qualification verification", pledgeAddr.size());
    int ret = 0;
    std::vector<Node> nodes;
    auto peerNode = MagicSingleton<PeerNode>::GetInstance();
    nodes = peerNode->GetNodelist();

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
            sendNodeIds.push_back(node.base58Address);
        }
    }
    else
    {
        std::vector<Node> nodeInfo = nodes;
        if(pledgeAddr.size() < num)
        {
            int count = 0;
            for(auto &node : nodeInfo)
            {
                if(reserveComparator(node.height, heightBaseline))
                {
                    ++count;
                }
            }
            if(count < num)
            {
                DEBUGLOG("Number of satisfying height less {}",num);
                return -2;
            }

            for (; sendNodeIds.size() < num && (!nodeInfo.empty());)
            {
                int index = rand() % nodeInfo.size();
                auto &node = nodeInfo.at(index);
                if (discardComparator(node.height, heightBaseline))
                {
                    continue;
                }
                sendNodeIds.push_back(nodeInfo[index].base58Address);
                nodeInfo.erase(nodeInfo.cbegin() + index);
            }
        }
        else
        {
            std::vector<std::string> pledge = pledgeAddr;
            for (; !pledge.empty() && sendNodeIds.size() < num && (!nodeInfo.empty());)
            {
                int index = rand() % nodeInfo.size();
                auto &node = nodeInfo.at(index);
                if (discardComparator(node.height, heightBaseline))
                {
                    nodeInfo.erase(nodeInfo.cbegin() + index);
                    continue;
                }
                auto it = std::find(pledge.cbegin(), pledge.cend(), node.base58Address);
                if (pledge.cend() == it)
                {
                    nodeInfo.erase(nodeInfo.cbegin() + index);
                    continue;
                }
                pledge.erase(it);
                sendNodeIds.push_back(nodeInfo[index].base58Address);
                nodeInfo.erase(nodeInfo.cbegin() + index);
            }
            for (; sendNodeIds.size() < num && (!nodes.empty());)
            {
                int index = rand() % nodes.size();
                auto &node = nodes.at(index);
                if (discardComparator(node.height, heightBaseline))
                {
                    nodes.erase(nodes.cbegin() + index);
                    continue;
                }
                auto it = std::find(sendNodeIds.cbegin(), sendNodeIds.cend(), node.base58Address);
                if (sendNodeIds.cend() != it)
                {
                    nodes.erase(nodes.cbegin() + index);
                    continue;
                }
                sendNodeIds.push_back(nodes[index].base58Address);
                nodes.erase(nodes.cbegin() + index);
            }
        }
    }
    if (sendNodeIds.size() < num * 0.5)
    {
        std::vector<Node> nodelist = peerNode->GetNodelist();
        for(auto &it : nodelist)
        {
            DEBUGLOG("Node height:{}", it.height);
        }
        sendNodeIds.clear();
        ret = -3;
        ERRORLOG("sendNodeIds size: {}",sendNodeIds.size());
        return ret;
    }
    return 0;
}

void SyncBlock::SetFastSync(uint64_t syncStartHeight)
{
    runFastSync = true;
    newSyncFailHeight = syncStartHeight;
}

void SyncBlock::SetNewSyncHeight(uint64_t height)
{
    runNewSync = true;
    if(newSyncStartHeight == 0) 
    {
        newSyncStartHeight = height;
    }
    if(newSyncStartHeight > height)
    {
        newSyncStartHeight = height;
    }
}

bool SyncBlock::_GetFastSyncSumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                                       std::vector<FastSyncBlockHashs> &requestHashs, std::vector<std::string> &retNodeIds, uint64_t chainHeight)
{
    DEBUGLOG("fast sync startSyncHeight: {}   tend_sync_height: {}", startSyncHeight, endSyncHeight);
    requestHashs.clear();
    retNodeIds.clear();
    std::string msgId;
    size_t sendNum = sendNodeIds.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, sendNum * 0.8, msgId))
    {
        return false;
    }
    for (auto &nodeId : sendNodeIds)
    {
        DEBUGLOG("fast sync get block hash from {}", nodeId);
        SendFastSyncGetHashReq(nodeId, msgId, startSyncHeight, endSyncHeight);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if (retDatas.size() < sendNum * 0.5)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return false;
        }
    }
    FastSyncGetHashAck ack;
    std::map<std::string, FastSyncHelper> syncHashs;
    std::map<uint64_t, std::set<std::string>> fastSyncHashs;
    uint32_t ret_num = 0;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        auto retHeightData = ack.hashs();

        uint64_t height = 0;
        std::set<string> heightHashs;

        for(const auto& ret_height_hashs : retHeightData)
        {
            height = ret_height_hashs.height();
            for(const auto& hash : ret_height_hashs.hashs())
            {
                heightHashs.insert(hash);
                auto it = syncHashs.find(hash);
                if (syncHashs.end() == it)
                {
                    FastSyncHelper helper = {0, std::set<string>(), ret_height_hashs.height()};
                    syncHashs.insert(make_pair(hash, helper));
                }
                auto &value = syncHashs.at(hash);
                value.hits = value.hits + 1;
                value.ids.insert(ack.self_node_id());
            }
            fastSyncHashs.insert(std::make_pair(height, heightHashs));

        }
        ++ret_num;
    }

    std::vector<decltype(syncHashs.begin())> remainSyncHashs;
    std::vector<std::string> rollbackSyncHashs;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollbackBlockData;

    DBReader dbReader;
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::map<uint64_t, std::vector<std::string>> blockHeightHashes;
    if(!GetHeightBlockHash(startSyncHeight, endSyncHeight , blockHeightHashes))
    {
        ERRORLOG("query database fail");
        return false;
    }
    
    uint64_t needRollbackHeight = 0;
    for(auto iter = syncHashs.begin(); iter != syncHashs.end(); ++iter)
    {
        std::vector<std::string> localHashs;
        auto result = blockHeightHashes.find(iter->second.height);
        if(result != blockHeightHashes.end())
        {
            localHashs = result->second;
        }
        
        bool byzantineSuccess = iter->second.hits >= ret_num * 0.66;//check_byzantine(ret_num, iter->second.hits);
        if (!byzantineSuccess)
        {
            std::string block_raw;
            if (DBStatus::DB_SUCCESS == dbReader.GetBlockByBlockHash(iter->first, block_raw))
            {
                CBlock block;
                if(!block.ParseFromString(block_raw))
                {
                    ERRORLOG("block parse fail");
                    return false;
                }
                if((currentTime - block.time()) > kStabilityTime)
                {
                    _AddBlockToMap(block, rollbackBlockData);
                    needRollbackHeight = block.height();
                    break;
                }
            }
        }
        
        if(byzantineSuccess)
        {
            std::vector<std::string> selfBlockHashes;
            if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(iter->second.height, iter->second.height, selfBlockHashes))
            {
                ERRORLOG("GetBlockHashesByBlockHeight error");
                return false;
            }
            std::sort(selfBlockHashes.begin(), selfBlockHashes.end());

            auto find = fastSyncHashs.find(iter->second.height);
            if(find != fastSyncHashs.end())
            {
                CBlock block;
                std::vector<std::string> diffHashes;
                std::set_difference(selfBlockHashes.begin(), selfBlockHashes.end(), find->second.begin(), find->second.end(), std::back_inserter(diffHashes));
                for(auto diffHash: diffHashes)
                {
                    block.Clear();
                    string strblock;
                    auto res = dbReader.GetBlockByBlockHash(diffHash, strblock);
                    if (DBStatus::DB_SUCCESS != res)
                    {
                        ERRORLOG("GetBlockByBlockHash failed");
                        return false;
                    }
                    block.ParseFromString(strblock);
                    
                    uint64_t tmp_height = block.height();
                    if ((tmp_height < chainHeight) && chainHeight - tmp_height > 10)
                    {
                        _AddBlockToMap(block, rollbackBlockData);
                        needRollbackHeight = tmp_height;
                        break;
                    }
                }
            }       
        }

        if (byzantineSuccess && (localHashs.empty() || std::find(localHashs.begin(), localHashs.end(), iter->first) == localHashs.end()))
        {
            remainSyncHashs.push_back(iter);
        }
    }

    if (!rollbackBlockData.empty())
    {
        int peerNodeSize = MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize();
        if(syncSendFastSyncNum < peerNodeSize)
        {
            ERRORLOG("syncSendFastSyncNum:{} < peerNodeSize:{}", syncSendFastSyncNum, peerNodeSize);
            newSyncFailHeight = needRollbackHeight;
            inFastSyncFail = true;
            syncSendFastSyncNum = peerNodeSize;
            return false;
        }

        DEBUGLOG("==== fast sync rollback ====");   
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollbackBlockData);
        return false;
    }

    auto remain_begin = remainSyncHashs.begin();
    auto remain_end = remainSyncHashs.end();
    if(remain_begin == remain_end)
    {
        return false;
    }
    std::set<std::string> ids = (*remain_begin)->second.ids;
    for(++remain_begin; remain_begin != remain_end; ++remain_begin)
    {
        std::set<std::string> intersectIds;
        auto& next_ids = (*remain_begin)->second.ids;
        std::set_intersection(next_ids.begin(), next_ids.end()
                                    , ids.begin(), ids.end()
                                    ,std::inserter(intersectIds, intersectIds.begin())
                                    );
        ids = intersectIds;
    }

    retNodeIds = std::vector<std::string>(ids.begin(), ids.end());

    for(auto remainSyncHash : remainSyncHashs)
    {
        uint64_t height = remainSyncHash->second.height;
        auto find_result = find_if(requestHashs.begin(), requestHashs.end(), 
                            [height](const FastSyncBlockHashs& entity)
                            {
                                return height == entity.height();
                            } 
                        );
        if(find_result == requestHashs.end())
        {
            FastSyncBlockHashs fastSyncBlockHashs;
            fastSyncBlockHashs.set_height(height);
            fastSyncBlockHashs.add_hashs(remainSyncHash->first);
            requestHashs.push_back(fastSyncBlockHashs);
        }
        else
        {
            find_result->add_hashs(remainSyncHash->first);
        }
    }
    return !ids.empty();
}

bool SyncBlock::_GetFastSyncBlockData(const std::string &sendNodeId, const std::vector<FastSyncBlockHashs> &requestHashs, uint64_t chainHeight)
{
    if (requestHashs.empty())
    {
        DEBUGLOG("no byzantine data available");
        return true;
    }
    
    std::string msgId;
    if (!GLOBALDATAMGRPTR.CreateWait(30, 1, msgId))
    {
        DEBUGLOG("create wait fail");
        return false;
    }
    SendFastSyncGetBlockReq(sendNodeId, msgId, requestHashs);
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        ERRORLOG("wait fast sync data for {} fail", sendNodeId);
        return false;
    }
    if (retDatas.empty())
    {
        DEBUGLOG("return data empty");
        return false;
    }
    FastSyncGetBlockAck ack;
    if (!ack.ParseFromString(retDatas.at(0)))
    {
        DEBUGLOG("FastSyncGetBlockAck parse fail");
        return false;
    }
    CBlock block;
    CBlock hash_block;
    std::vector<std::string> blockHashes;
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
                if (block.hash() != Getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                DEBUGLOG("FastSync  block.height(): {} \tblock.hash(): {}", block.height(), block.hash());
                _AddBlockToMap(block, fast_sync_block_data);
                blockHashes.push_back(block.hash());
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
    
    global::ca::SaveType syncType = GetSaveSyncType(top, chainHeight);
    MagicSingleton<BlockHelper>::GetInstance()->AddFastSyncBlock(fast_sync_block_data, syncType);
    return true;
}

int SyncBlock::_GetSyncSumHashNode(uint64_t pledgeAddrSize, const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight,
                                   std::map<uint64_t, uint64_t> &needSyncHeights, std::vector<std::string> &retNodeIds, uint64_t &chainHeight, uint64_t newSyncSnedNum)
{
    DEBUGLOG("_GetSyncSumHashNode startSyncHeight: {}   endSyncHeight: {}", startSyncHeight, endSyncHeight);
    int ret = 0;
    needSyncHeights.clear();
    retNodeIds.clear();
    std::string msgId;
    size_t sendNum = sendNodeIds.size();

    auto peerNodeSize = MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize();
    double acceptanceRate = 0.8;
    if(newSyncSnedNum > peerNodeSize)
    {
        acceptanceRate = 0.9;
    }
    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNum * acceptanceRate, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : sendNodeIds)
    {
        SendSyncGetSumHashReq(nodeId, msgId, startSyncHeight, endSyncHeight);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if (retDatas.empty() || retDatas.size() < sendNum / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            ret = -2;
            return ret;
        }
    }

    std::vector<uint64_t> retNodeTops;
    SyncGetSumHashAck ack;
    DBReader dbReader;

    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> consensusMap;
    std::vector<std::pair<std::string, uint32_t>> syncHashDatas;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        retNodeIds.push_back(ack.self_node_id());
        retNodeTops.push_back(ack.node_block_height());
        for (auto &sync_sum_hash : ack.sync_sum_hashes())
        {
            auto find_sum_hash = consensusMap.find(sync_sum_hash.hash());
            if(find_sum_hash == consensusMap.end())
            {
                consensusMap.insert(std::make_pair(sync_sum_hash.hash(), std::make_pair(sync_sum_hash.start_height(), std::vector<std::string>() )));
            }
            auto &value = consensusMap.at(sync_sum_hash.hash());
            value.second.push_back(ack.self_node_id());

            

            std::string key = std::to_string(sync_sum_hash.start_height()) + "_" + std::to_string(sync_sum_hash.end_height()) + "_" + sync_sum_hash.hash();
            auto it = std::find_if(syncHashDatas.begin(), syncHashDatas.end(), [&key](const std::pair<std::string, uint32_t>& syncHashData){return key == syncHashData.first;}); //syncHashDatas.find(key);
            if (syncHashDatas.end() == it)
            {
                syncHashDatas.emplace_back(key, 1);
            }
            else
            {
                it->second += 1;
            }
        }
    }
    std::sort(retNodeTops.begin(), retNodeTops.end());
    int verifyNum = retNodeTops.size() * 0.66;
    if (verifyNum >= retNodeTops.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verifyNum, retNodeTops.size());
        ret = -3;
        return ret;
    }
    chainHeight = retNodeTops.at(verifyNum);
    std::set<uint64_t> heights;
    std::string hash;
    uint64_t startHeight = 0;
    uint64_t endHeight = 0;
    std::vector<std::string> block_hashes;
    std::vector<std::string> data_key;

    std::sort(syncHashDatas.begin(),syncHashDatas.end(),[](const std::pair<std::string, uint32_t> v1, const std::pair<std::string, uint32_t> v2){
        std::vector<std::string> v1Height;
        StringUtil::SplitString(v1.first, "_", v1Height);
        std::vector<std::string> v2Height;
        StringUtil::SplitString(v2.first, "_", v2Height);

        return v1Height.at(0) < v2Height.at(0);
    });

    for (auto &syncHashData : syncHashDatas)
    {
        data_key.clear();
        StringUtil::SplitString(syncHashData.first, "_", data_key);
        if (data_key.size() < 3)
        {
            continue;
        }
        startHeight = std::stoull(data_key.at(0));
        endHeight = std::stoull(data_key.at(1));

        if (syncHashData.second < verifyNum)
        {
            DEBUGLOG("verify error, error height: {} syncHashData.second: {} verifyNum: {}", startHeight, syncHashData.second, verifyNum);

            uint64_t selfNodeHeight = 0;
            if(DBStatus::DB_SUCCESS != dbReader.GetBlockTop(selfNodeHeight))
            {
                ERRORLOG("(_GetSyncSumHashNode) GetBlockTop failed !");
                ret = -4;
                return ret;
            }

            if(newSyncSnedNum >= peerNodeSize)
            {
                ERRORLOG("peerNodeSize{}   newSyncSnedNum{}   retDatas.size(){}", peerNodeSize, newSyncSnedNum, retDatas.size());
                std::vector<std::pair<std::string, std::vector<std::string>>> nodeLists;
                std::string self_sumhash;

                std::vector<std::string> selfBlockHashes;
                auto res = dbReader.GetBlockHashsByBlockHeight(startHeight, selfBlockHashes);
                if(res != DBStatus::DB_SUCCESS)
                {
                    ERRORLOG("get {}  block hash failed !", startHeight);
                    // return -5;
                }

                if(!SumHeightHash(selfBlockHashes, self_sumhash))
                {
                    self_sumhash = "error";
                    ERRORLOG("get {} block sum hash failed !", startHeight);
                    // return -6;
                }

                uint64_t currentHeightNodeSize = 0;

                for(auto consensus : consensusMap)
                {
                    if(consensus.second.first == startHeight)
                    {
                        currentHeightNodeSize += consensus.second.second.size();
                        nodeLists.emplace_back(std::make_pair(consensus.first, std::move(consensus.second.second)));
                    }
                }

                sort(nodeLists.begin(), nodeLists.end(), [](const std::pair<std::string, std::vector<std::string>> list1, const std::pair<std::string, std::vector<std::string>> list2){
                    return list1.second.size() > list2.second.size();
                });

                if(nodeLists.at(0).second.size() < retDatas.size() * 0.51  //The most forks are less than 51%
                        && retDatas.size() >= sendNum * 0.80                 //80% At the highest altitude
                        && startHeight <= selfNodeHeight)                   //startHeight is the Byzantine error height
                {                
                    ERRORLOG("first: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                        nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,selfNodeHeight);
                    std::vector<std::string> errorHeightBlockHashes;
                    std::string strblock;
                    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollbackBlockData;
                    CBlock block;
                    
                    if(!GetHeightBlockHash(startHeight, startHeight, errorHeightBlockHashes))
                    {
                        ERRORLOG("height:{} GetHeightBlockHash error", startHeight);
                        continue;
                    }


                    for(const auto& error_hash: errorHeightBlockHashes) 
                    {
                        if(DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(error_hash, strblock))
                        {
                            continue;
                        }
                        
                        if(!block.ParseFromString(strblock))
                        {
                            continue;
                        }
                        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                        if(block.time() < nowTime - 1 * 60 * 1000000)
                        {
                            DEBUGLOG("currentHeightNodeSize: {}\tret_datas.size() / 2:{}",currentHeightNodeSize, retDatas.size() / 2);
                            DEBUGLOG("_AddBlockToMap rollback block height: {}\tblock hash:{}",block.height(), block.hash());
                            _AddBlockToMap(block, rollbackBlockData);
                        }
                    }
                    if(!rollbackBlockData.empty())
                    {
                        DEBUGLOG("==== new sync rollback first ====");
                        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollbackBlockData);
                        return -7;
                    }
                }
                else if(nodeLists.at(0).second.size() >= retDatas.size() * 0.51
                        && retDatas.size() > sendNum * 0.80
                        && (startHeight <= selfNodeHeight || startHeight == selfNodeHeight + 1) )
                {
                    ERRORLOG("second: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                        nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,selfNodeHeight);
                    if(self_sumhash != nodeLists.at(0).first)
                    {

                        ret = _GetSyncBlockBySumHashNode(nodeLists.at(0).second, startHeight, endHeight, selfNodeHeight, chainHeight, newSyncSnedNum);
                        if(ret != 0)
                        {
                            ERRORLOG("_GetSyncBlockBySumHashNode failed !");
                            return -8;
                        }
                    }
                }
                else {
                    ERRORLOG("thread: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                        nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,selfNodeHeight);
                    continue;
                }
            }
            else 
            {
                return -9;
            }
        }

        hash.clear();
        block_hashes.clear();
        GetHeightBlockHash(startHeight, endHeight, block_hashes);
        SumHeightHash(block_hashes, hash);
        if (data_key.at(2) == hash)
        {
            continue;
        }
        for (uint64_t i = startHeight; i <= endHeight; i++)
        {
            heights.insert(i);
        }
    }

    int noVerifyHeight = 10;
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
                needSyncHeights.insert(std::make_pair(start, end));
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
        needSyncHeights.insert(std::make_pair(start, end));
    }
    if (endSyncHeight >= (chainHeight - noVerifyHeight))
    {
        if (chainHeight > noVerifyHeight)
        {
            needSyncHeights.insert(std::make_pair(chainHeight - noVerifyHeight, chainHeight));
            needSyncHeights.insert(std::make_pair(chainHeight, chainHeight + noVerifyHeight));
        }
        else
        {
            needSyncHeights.insert(std::make_pair(1, chainHeight + noVerifyHeight));
        }
    }
    uint64_t selfNodeHeight = 0;

    dbReader.GetBlockTop(selfNodeHeight);

    return 0;
}

int SyncBlock::_GetSyncBlockBySumHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight, uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight, uint64_t newSyncSnedNum)
{
    int ret = 0;
    std::string msgId;
    uint64_t succentCount = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNodeIds.size() * 0.8, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : sendNodeIds)
    {
        DEBUGLOG("new sync get block hash from {}", nodeId);
        SendSyncGetHeightHashReq(nodeId, msgId, startSyncHeight, endSyncHeight);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < sendNodeIds.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", sendNodeIds.size(), retDatas.size());
            ret = -2;
            return ret;
        }
    }

    SyncGetHeightHashAck ack;
    std::set<std::string> verifySet;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
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
        verifySet.insert(verify_str);

    }
    if(verifySet.size() != 1)
    {
        DEBUGLOG("Byzantium failed");
        return -3;
    }

    std::vector<std::string> selfBlockHashes;
    if(selfNodeHeight >= startSyncHeight)
    {
        DBReader dbReader;
        
        if(DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(startSyncHeight, endSyncHeight, selfBlockHashes))
        {
            ret = -4;
            DEBUGLOG("GetBlockHashesByBlockHeight failed");
            return ret;
        }

        std::sort(selfBlockHashes.begin(), selfBlockHashes.end());

        std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollbackBlockData;
        CBlock block;
        std::vector<std::string> diffHashes;
        std::set_difference(selfBlockHashes.begin(), selfBlockHashes.end(), ack.block_hashes().begin(), ack.block_hashes().end(), std::back_inserter(diffHashes));
        for(auto diffHash: diffHashes)
        {
            block.Clear();
            string strblock;
            auto res = dbReader.GetBlockByBlockHash(diffHash, strblock);
            if (DBStatus::DB_SUCCESS != res)
            {
                DEBUGLOG("GetBlockByBlockHash failed");
                return -5;
            }
            block.ParseFromString(strblock);

            uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            if(block.time() < nowTime - 5 * 60 * 1000000)
            {
                _AddBlockToMap(block, rollbackBlockData);
            }
        }

        if(!rollbackBlockData.empty())
        {
            DEBUGLOG("==== new sync rollback ====");
            MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollbackBlockData);
            return -7;
        }
    }

    std::vector<std::string> needSyncHashes;
    std::set_difference(ack.block_hashes().begin(), ack.block_hashes().end(), selfBlockHashes.begin(), selfBlockHashes.end(),  std::back_inserter(needSyncHashes));
    ret = _GetSyncBlockData(sendNodeIds, needSyncHashes, chainHeight);
    if(ret != 0)
    {
        DEBUGLOG("_GetSyncBlockData failed {}", ret);
        return -8;
    }

    return 0;
}

int SyncBlock::_GetFromZeroSyncSumHashNode(const std::vector<std::string> &sendNodeIds, const std::vector<uint64_t>& sendHeights, uint64_t selfNodeHeight, std::set<std::string> &retNodeIds, std::map<uint64_t, std::string>& sumHashes)
{
    retNodeIds.clear();
    std::string msgId;
    size_t sendNum = sendNodeIds.size();

    auto peerNodeSize = MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize();
    double acceptanceRate = 0.8;
    if(syncSendZeroSyncNum > peerNodeSize)
    {
        acceptanceRate = 0.9;
    }

    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNum * acceptanceRate, msgId))
    {
        return -1;
    }

    for (auto &nodeId : sendNodeIds)
    {
        DEBUGLOG("get from zero sync sum hash from {}", nodeId);
        SendFromZeroSyncGetSumHashReq(nodeId, msgId, sendHeights);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if (retDatas.empty() || retDatas.size() < sendNum / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -2;
        }
    }
    
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHashDatas;
    int success_count = 0;
    for (auto &retData : retDatas)
    {
        SyncFromZeroGetSumHashAck ack;
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if (ack.code() == 0)
        {
            if(syncSendZeroSyncNum >= peerNodeSize)
            {
                ++success_count;
            }
            continue;
        }
        ++success_count;
        auto ret_sum_hashes = ack.sum_hashes();
        for(const auto& sumHash : ret_sum_hashes)
        {
            const auto& hash = sumHash.hash();
            auto height = sumHash.height();
            

            auto found = sumHashDatas.find(hash);
            if (found == sumHashDatas.end())
            {
                sumHashDatas.insert(std::make_pair(hash, std::make_pair(height, std::vector<std::string>())));
                continue;
            }
            auto& content = found->second;
            content.second.push_back(ack.self_node_id());  
        }

    }

    uint64_t backNum = sendNum * 0.66;
    bool byzantineSuccess = success_count > backNum;
    if(syncSendZeroSyncNum >= peerNodeSize)
    {
        backNum = sendNum * 0.9;
        byzantineSuccess = success_count >= backNum;
    }
    // if (!check_byzantine(sendNum,success_count))
    if(!byzantineSuccess)
    {
        ERRORLOG("check_byzantine error, sendNum = {} , success_count = {}", sendNum, success_count);
        return -3;
    }


    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> byzantineErrorHeights;
    for(const auto& sumHashData : sumHashDatas)
    {
        bool byzSuccess = sumHashData.second.second.size() >= success_count * 0.66;
        // if (check_byzantine(success_count, sumHashData.second.second))
        if(byzSuccess)
        {
            sumHashes[sumHashData.second.first] = sumHashData.first;
            for(const auto& t : sumHashData.second.second)
            {
                retNodeIds.insert(std::move(t));
            }
        }
        else if(syncSendZeroSyncNum >= peerNodeSize)
        {
            //The whole network Byzantium failed     Only the last 100 heights are synchronized
            if(sumHashData.second.first > selfNodeHeight / 100 * 100 + 100)
            {
                continue;
            }
            
            auto find = sumHashes.find(sumHashData.second.first);
            if(find != sumHashes.end())
            {
                continue;
            }

                                 //sum hash         node ids  
            std::vector<std::pair<std::string, std::vector<std::string>>> nodeIds;
            for(const auto& sumHash: sumHashDatas)
            {
                if(sumHash.second.first == sumHashData.second.first)
                {
                    nodeIds.emplace_back(make_pair(sumHash.first, std::move(sumHash.second.second)));
                }
            }

            std::sort(nodeIds.begin(), nodeIds.end(), [](const std::pair<std::string, std::vector<std::string>>& list1, const std::pair<std::string, std::vector<std::string>>& list2){
                // return list1.size() > list2.size();
                return list1.second.size() > list2.second.size();
            });

            sumHashes.clear();
            retNodeIds.clear();
            
            sumHashes[sumHashData.second.first] = nodeIds[0].first;
            for(const auto& t : nodeIds[0].second)
            {
                retNodeIds.insert(std::move(t));
            }

            break;
        }
    }

    // for(const auto& t: retNodeIds)
    // {
    //     cout << "retNodeIds: " << t << std::endl;
    // }
    if (sumHashes.empty())
    {
        return -4;
    }
    
    return 0;
}

int SyncBlock::_GetFromZeroSyncBlockData(const std::map<uint64_t, std::string>& sumHashes, std::vector<uint64_t> &sendHeights, std::set<std::string> &sendNodeIds, uint64_t selfNodeHeight)
{
    int ret = 0;
    if (sendNodeIds.empty() || sumHashes.empty())
    {
        return -1;
    }

    // std::vector<std::string> sendNodeIds;

    // for(const auto& t : setSendNodeIds)  
    // {
    //     sendNodeIds.emplace_back(std::move(t));
    // }

    auto sendNodeSize = sendNodeIds.size();
    auto sendHashSize = sumHashes.size();
    if (sendNodeSize > sendHashSize)
    {
        for (auto diff = sendNodeSize - sendHashSize; diff > 0; --diff)
        {
            // sendNodeIds.pop_back();
            auto lastElement = std::prev(sendNodeIds.end());
            sendNodeIds.erase(lastElement);
        }
        
    }
    // else if (sendNodeSize < sendHashSize)
    // {
    //     for (auto diff =  sendHashSize - sendNodeSize; diff > 0; --diff)
    //     {
    //         int index = rand() % sendNodeIds.size();
    //         auto &nodeId = sendNodeIds.at(index);
    //         sendNodeIds.push_back(nodeId);            
    //     }
    // }

    // fout << "------------------------------------------------------------" << std::endl;
    // for(const auto& t: sendNodeIds)
    // {
    //     fout << "sendNodeId: " << t << std::endl;
    // }

    if (sendNodeIds.size() != sumHashes.size())
    {
        // fout << "sendNodeIds.size(): " << sendNodeIds.size() << "\tsum_hashes.size()" << sumHashes.size() << std::endl;
        DEBUGLOG("sendNodeIds.size() != sumHashes.size(), {}:{}", sendNodeIds.size(), sumHashes.size());
        return -2;
    }
    
    
    std::string msgId;
    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNodeIds.size(), msgId))
    {
        return -3;
    }

    int sendNodeIndex = 0;
    for(const auto& sumHashItem : sumHashes)
    {
        // auto nodeId = sendNodeIds[sendNodeIndex];
        auto nodeId  = std::next(sendNodeIds.begin(), sendNodeIndex);
        auto sum_hash_height = sumHashItem.first;
        // fout << "nodeId: " << *nodeId << std::endl; 
        SendFromZeroSyncGetBlockReq(*nodeId, msgId, sum_hash_height);
        DEBUGLOG("from zero sync get block at height {} from {}", sum_hash_height, *nodeId);
        ++sendNodeIndex;
    }
    
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        DEBUGLOG("wait sync block data time out send:{} recv:{}", sendNodeIds.size(), retDatas.size());
    }
    
    DBReader dbReader; 
    std::vector<uint64_t> successHeights;
    
    for(const auto& retData : retDatas)
    {
        std::map<uint64_t, std::set<CBlock, CBlockCompare>> syncBlockData;
        SyncFromZeroGetBlockAck ack;
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        
        auto blockraws = ack.blocks();
        std::map<uint64_t, std::vector<std::string>> sumHashCheckData; 
        std::vector<CBlock> sumHashData;
        for(const auto& block_raw : blockraws)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                ERRORLOG("block parse fail");
                break;
            }

            auto blockHeight = block.height();
            auto found = sumHashCheckData.find(blockHeight);
            if(found == sumHashCheckData.end())
            {
                sumHashCheckData[blockHeight] = std::vector<std::string>();
            }
            auto& sum_hashes_vector = sumHashCheckData[blockHeight];
            sum_hashes_vector.push_back(block.hash());
            
            sumHashData.push_back(block);

        }

        std::string calSumHash;
        SumHeightsHash(sumHashCheckData, calSumHash);
        auto found = sumHashes.find(ack.height());
        if(found == sumHashes.end())
        {
            DEBUGLOG("fail to get sum hash at height {}", ack.height());
            continue;
        }
        if (calSumHash != found->second)
        {
            ERRORLOG("check sum hash at height {} fail, calSumHash:{}, sumHash:{}", ack.height(), calSumHash, found->second);
            continue;
        }

        for(const auto& hash_check : sumHashCheckData)
        {
            std::vector<std::string> blockHashes;
            if(selfNodeHeight >= hash_check.first)
            {
                if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(hash_check.first, hash_check.first, blockHashes))
                {
                    return -4;
                }
                std::string self_sum_hash;
                std::string other_sum_hash;

                auto find_height = sumHashCheckData.find(hash_check.first);
                if(find_height != sumHashCheckData.end())
                {
                    if(SumHeightHash(blockHashes, self_sum_hash) && SumHeightHash(find_height->second, other_sum_hash))
                    {
                        if(self_sum_hash != other_sum_hash)
                        {
                            std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollbackBlockData;
                            CBlock block;
                            std::vector<std::string> diffHashes;
                            std::set_difference(blockHashes.begin(), blockHashes.end(), find_height->second.begin(), find_height->second.end(), std::back_inserter(diffHashes));
                            for(auto diffHash: diffHashes)
                            {
                                block.Clear();
                                string strblock;
                                auto res = dbReader.GetBlockByBlockHash(diffHash, strblock);
                                if (DBStatus::DB_SUCCESS != res)
                                {
                                    DEBUGLOG("GetBlockByBlockHash failed");
                                    return -5;
                                }
                                block.ParseFromString(strblock);
                                
                                _AddBlockToMap(block, rollbackBlockData);
                            }

                            if(!rollbackBlockData.empty())
                            {
                                int peerNodeSize = MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize();
                                if(syncSendZeroSyncNum < peerNodeSize)
                                {
                                    DEBUGLOG("syncSendZeroSyncNum:{} < peerNodeSize:{}", syncSendZeroSyncNum, peerNodeSize);
                                    syncSendZeroSyncNum = peerNodeSize;
                                    return -6;
                                }
                                DEBUGLOG("==== _GetFromZeroSyncBlockData rollback ====");
                                MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollbackBlockData);
                            }
                            // DEBUGLOG("self_sum_hash: {}", self_sum_hash);
                            // DEBUGLOG("other_sum_hash: {}", other_sum_hash);
                            // newSyncFailHeight = hash_check.first;
                            // runFastSync = true;   
                            // INFOLOG("_GetFromZeroSyncBlockData height:{} fail,next round will run fast sync", hash_check.first); 
                            // return -7;
                        }
                    }
                }
            }
        }

        successHeights.push_back(ack.height());
        for(const auto& block : sumHashData)
        {
            _AddBlockToMap(block, syncBlockData);
        }
        {
            std::lock_guard<std::mutex> lock(_syncFromZeroCacheMutex);
            _syncFromZeroCache[ack.height()] = syncBlockData;
        }
    }
    if (successHeights.empty())
    {
        return -8;
    }
        
    for(auto height : successHeights)
    {
        auto found = std::find(sendHeights.begin(), sendHeights.end(), height);
        if (found != sendHeights.end())
        {
            sendHeights.erase(found);
        }
        
    }
    _syncFromZeroReserveHeights.clear();
    for(auto fail_height : sendHeights)
    {
        _syncFromZeroReserveHeights.push_back(fail_height);
    }

    if(!sendHeights.empty())
    {
        return -9;
    }

    if (sendHeights.empty())
    {
        auto from_zero_sync_add_thread = std::thread(
                [this]()
                {
                    std::lock_guard<std::mutex> lock(_syncFromZeroCacheMutex);
                    INFOLOG("from_zero_sync_add_thread start");
                    for(const auto& cache : this->_syncFromZeroCache)
                    {
                        MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(cache.second, global::ca::SaveType::SyncFromZero);
                    }
                    _syncFromZeroCache.clear();
                }
        );
        from_zero_sync_add_thread.detach();
    }
    return 0;
}


int SyncBlock::_GetSyncBlockHashNode(const std::vector<std::string> &sendNodeIds, uint64_t startSyncHeight,
                                     uint64_t endSyncHeight, uint64_t selfNodeHeight, uint64_t chainHeight,
                                     std::vector<std::string> &retNodeIds, std::vector<std::string> &reqHashes, uint64_t newSyncSnedNum)
{
    int ret = 0;
    std::string msgId;
    uint64_t succentCount = 0;
    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNodeIds.size() * 0.8, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : sendNodeIds)
    {
        DEBUGLOG("new sync get block hash from {}", nodeId);
        SendSyncGetHeightHashReq(nodeId, msgId, startSyncHeight, endSyncHeight);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < sendNodeIds.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", sendNodeIds.size(), retDatas.size());
            ret = -2;
            return ret;
        }
    }
    SyncGetHeightHashAck ack;
    std::map<std::string, std::set<std::string>> syncBlockHashes;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            continue;
        }
        succentCount++;
        for (auto &key : ack.block_hashes())
        {
            auto it = syncBlockHashes.find(key);
            if (syncBlockHashes.end() == it)
            {
                syncBlockHashes.insert(std::make_pair(key, std::set<std::string>()));
            }
            auto &value = syncBlockHashes.at(key);
            value.insert(ack.self_node_id());
        }
    }

    std::set<std::string> nodes;
    std::vector<std::string> intersectionNodes;
    std::set<std::string> verify_hashes;
    reqHashes.clear();
    size_t verifyNum = succentCount / 5 * 4;
    // Put the block hash greater than 60% into the array
    std::string strblock;
    std::vector<std::string> exitsHashes;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollbackBlockData;
    CBlock block;
    DBReader dbReader;
    for (auto &syncBlockHash : syncBlockHashes)
    {
        strblock.clear();
        auto res = dbReader.GetBlockByBlockHash(syncBlockHash.first, strblock);
        if (DBStatus::DB_SUCCESS == res)
        {
            exitsHashes.push_back(syncBlockHash.first);
        }
        else if(DBStatus::DB_NOT_FOUND != res)
        {
            ret = -3;
            return ret;
        }
        if (syncBlockHash.second.size() >= verifyNum)
        {

            if (DBStatus::DB_NOT_FOUND == res)
            {
                verify_hashes.insert(syncBlockHash.first);
                if (nodes.empty())
                {
                    nodes = syncBlockHash.second;
                }
                else
                {
                    std::set_intersection(nodes.cbegin(), nodes.cend(), syncBlockHash.second.cbegin(), syncBlockHash.second.cend(), std::back_inserter(intersectionNodes));
                    nodes.insert(intersectionNodes.cbegin(), intersectionNodes.cend());
                    intersectionNodes.clear();
                }
            }
        }

        // When the number of nodes where the block is located is less than 80%, and the block exists locally, the block is rolled back
        else
        {
            if (DBStatus::DB_SUCCESS == res && block.ParseFromString(strblock))
            {
                if (block.height() < chainHeight - 10)
                {
                    _AddBlockToMap(block, rollbackBlockData);
                    continue;
                }
            }

            // if(selfNodeHeight - block.height() > 10)
            // {
            //     uint64_t curtime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            //     if(curtime - block.time() > 100000000) //100s
            //     {
            //         if(newSyncSnedNum < MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize())
            //         {
            //             return -4;
            //         }
            //         _AddBlockToMap(block, rollbackBlockData);
            //     }
            // }

        }
    }

    std::vector<std::string> v_diff;
    uint64_t endHeight = endSyncHeight > selfNodeHeight ? selfNodeHeight : endSyncHeight;
    if(endHeight >= startSyncHeight)
    {

        //Get all block hashes in the local height range, determine whether they are in the trusted list, and roll them back when they are no longer in the trusted list
        std::vector<std::string> blockHashes;
        if(DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(startSyncHeight, endHeight, blockHashes))
        {
            ret = -5;
            return ret;
        }
        std::sort(blockHashes.begin(), blockHashes.end());
        std::sort(exitsHashes.begin(), exitsHashes.end());
        std::set_difference(blockHashes.begin(), blockHashes.end(), exitsHashes.begin(), exitsHashes.end(), std::back_inserter(v_diff));
    }

    for (const auto & it : v_diff)
    {

        block.Clear();
        std::string().swap(strblock);
        auto ret_status = dbReader.GetBlockByBlockHash(it, strblock);
        if (DBStatus::DB_SUCCESS != ret_status)
        {
            continue;
        }
        block.ParseFromString(strblock);

        //It will only be rolled back when the height of the block to be added is less than the maximum height on the chain of -10
        uint64_t tmpHeight = block.height();
        if ((tmpHeight < chainHeight) && chainHeight - tmpHeight > 10)
        {
            _AddBlockToMap(block, rollbackBlockData);
            continue;
        }

        // if(selfNodeHeight - tmp_height > 10)
        // {
        //     uint64_t curtime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        //     if(curtime - block.time() > 100000000) //100s
        //     {
        //         if(newSyncSnedNum < MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize())
        //         {
        //             return -6;
        //         }
        //         _AddBlockToMap(block, rollbackBlockData);
        //     }
        // }
    }

    if(!rollbackBlockData.empty())
    {
        int peerNodeSize = MagicSingleton<PeerNode>::GetInstance()->GetNodelistSize();
        if(newSyncSnedNum < peerNodeSize)
        {
            DEBUGLOG("newSyncSnedNum:{} < peerNodeSize:{}", newSyncSnedNum, peerNodeSize);
            newSyncSnedNum = peerNodeSize;
            return -7;
        }
        DEBUGLOG("==== new sync rollback ====");
        MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollbackBlockData);
        return -8;
    }

    if (verify_hashes.empty())
    {
        return 0;
    }

    reqHashes.assign(verify_hashes.cbegin(), verify_hashes.cend());
    retNodeIds.assign(nodes.cbegin(), nodes.cend());
    if(retNodeIds.empty())
    {
        ret = -9;
        return ret;
    }
    return 0;
}

int SyncBlock::_GetSyncBlockData(const std::vector<std::string> &sendNodeIds, const std::vector<std::string> &reqHashes, uint64_t chainHeight)
{
    int ret = 0;
    if (reqHashes.empty() || sendNodeIds.empty())
    {
        return 0;
    }
    std::string msgId;
    if (!GLOBALDATAMGRPTR.CreateWait(90, sendNodeIds.size() * 0.8, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : sendNodeIds)
    {
        SendSyncGetBlockReq(nodeId, msgId, reqHashes);
        DEBUGLOG("new sync block from {}", nodeId);
    }
    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(retDatas.empty())
        {
            ERRORLOG("wait sync block data time out send:{} recv:{}", sendNodeIds.size(), retDatas.size());
            ret = -2;
            return ret;
        }
    }

    CBlock block;
    CBlock hash_block;
    SyncGetBlockAck ack;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> syncBlockData;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        for (auto &block_raw : ack.blocks())
        {
            if (block.ParseFromString(block_raw))
            {
                if (reqHashes.cend() == std::find(reqHashes.cbegin(), reqHashes.cend(), block.hash()))
                {
                    continue;
                }
                hash_block = block;
                hash_block.clear_hash();
                hash_block.clear_sign();
                if (block.hash() != Getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                _AddBlockToMap(block, syncBlockData);
            }
        }
    }

    MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(syncBlockData, global::ca::SaveType::SyncNormal);

    return 0;
}

void SyncBlock::_AddBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData)
{
    if (syncBlockData.end() == syncBlockData.find(block.height()))
    {
        syncBlockData.insert(std::make_pair(block.height(), std::set<CBlock, CBlockCompare>()));
    }
    auto &value = syncBlockData.at(block.height());
    value.insert(block);
}

bool SyncBlock::check_byzantine(int receiveCount, int hitCount)
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
    auto found = level_table.find(receiveCount);
    if(found != end && found->second.find(hitCount) != found->second.end())
    {
        DEBUGLOG("byzantine success total {} hit {}", receiveCount, hitCount);
        return true;
    }
    DEBUGLOG("byzantine fail total {} hit {}", receiveCount, hitCount);
    return false;
}

bool SyncBlock::_NeedByzantineAdjustment(uint64_t chainHeight, const vector<std::string> &pledgeAddr,
                                        std::vector<std::string> &selectedAddr)
{
    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<std::string> qualifyingStakeNodes;
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHash;

    return _CheckRequirementAndFilterQualifyingNodes(chainHeight, pledgeAddr, nodes, qualifyingStakeNodes)
            && _GetSyncNodeSumhashInfo(nodes, qualifyingStakeNodes, sumHash)
            && _GetSelectedAddr(sumHash, selectedAddr);
}

bool SyncBlock::_GetSelectedAddr(map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sumHash,
                                vector<std::string> &selectedAddr)
{
    const static double filter_scale = 0.34;
    auto receiveNum = static_cast<double>(sumHash.size());
    DEBUGLOG("receive_num: {}", receiveNum);

    for (auto iter = sumHash.begin(); iter !=  sumHash.end();)
    {
        const auto& [hits, _] = iter->second;
        double scale = static_cast<double>(hits) / receiveNum;
        if (scale < filter_scale)
        {
            DEBUGLOG("erase sum hash {} which scale is {}", iter->first.substr(0, 6), scale);
            iter = sumHash.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    DEBUGLOG("sumHash size: {}", sumHash.size());
    if (sumHash.size() == 2)
    {
        auto& [firstHits, firstAddrs] = sumHash.begin()->second;
        auto& [lastHits, lastAddrs] = sumHash.rbegin()->second;
        DEBUGLOG("first hash {} hits {}; last hash {} hits {}",
                 sumHash.begin()->first.substr(0, 6), firstHits, sumHash.rbegin()->first.substr(0, 6), lastHits);
        if ((static_cast<double>(firstHits) / receiveNum) >= (static_cast<double>(lastHits) / receiveNum))
        {
            std::swap(selectedAddr, firstAddrs);
        }
        else
        {
            std::swap(selectedAddr, lastAddrs);
        }
        return true;
    }

    return false;
}

bool SyncBlock::_GetSyncNodeSumhashInfo(const vector<Node> &nodes, const vector<string> &qualifyingStakeNodes,
                                       map<string, pair<uint64_t, vector<string>>> sumHash)
{
    uint64_t byzantineFailTolerance = 5;
    if (nodes.size() < global::ca::kNeed_node_threshold)
    {
        byzantineFailTolerance = 0;
    }
    uint64_t receiveRequirement = qualifyingStakeNodes.size() - byzantineFailTolerance;
    DEBUGLOG("receiveRequirement size: {}", receiveRequirement);

    string msgId;
    if (!GLOBALDATAMGRPTR.CreateWait(60, receiveRequirement, msgId))
    {
        ERRORLOG("Create wait fail");
        return false;
    }
    for (const auto& addr : qualifyingStakeNodes)
    {
        DEBUGLOG("get sync node hash from {}", addr);
        SendSyncNodeHashReq(addr, msgId);
    }

    vector<string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        ERRORLOG("wait sync node hash time out send:{} recv:{}", qualifyingStakeNodes.size(), retDatas.size());
        return false;
    }

    SyncNodeHashAck ack;
    for (const auto& retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            ERRORLOG("SyncNodeHashAck parse fail");
            continue;
        }
        const auto& ret_hash = ack.hash();
        if (sumHash.find(ret_hash) == sumHash.end())
        {
            sumHash[ret_hash] = {0, vector<string>()};
        }
        auto& [hits, addrs] = sumHash.at(ret_hash);
        hits += 1;
        addrs.push_back(ack.self_node_id());
    }
    return true;
}

bool SyncBlock::_CheckRequirementAndFilterQualifyingNodes(uint64_t chainHeight, const vector<std::string> &pledgeAddr,
                                                         const vector<Node> &nodes,
                                                         vector<std::string> &qualifyingStakeNodes)
{
    const static uint32_t higherThanChainHeightBar = 3;
    uint32_t higherThanChainHeightCount = 0;

    for (const auto& node : nodes)
    {
        if (node.height > chainHeight)
        {
            higherThanChainHeightCount++;
            if (higherThanChainHeightCount > higherThanChainHeightBar)
            {
                DEBUGLOG("chain height isn't top hight in the network");
                return false;
            }
        }
        else if (node.height == chainHeight)
        {
            const auto& node_addr = node.base58Address;
            if (find(pledgeAddr.cbegin(), pledgeAddr.cend(), node_addr) != pledgeAddr.cend())
            {
                qualifyingStakeNodes.push_back(node_addr);
            }
        }
    }
    DEBUGLOG("qualifyingStakeNodes size: ", qualifyingStakeNodes.size());
    return true;
}

void SendFastSyncGetHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    FastSyncGetHashReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<FastSyncGetHashReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);

}

void SendFastSyncGetHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        return;
    }
    if ((endHeight - startHeight) > 100000)
    {
        return;
    }
    FastSyncGetHashAck ack;
    ack.set_self_node_id(NetGetSelfNodeId());
    ack.set_msg_id(msgId);
    uint64_t nodeBlockHeight = 0;
    if (DBStatus::DB_SUCCESS != DBReader().GetBlockTop(nodeBlockHeight))
    {
        ERRORLOG("GetBlockTop error");
        return;
    }
    ack.set_node_block_height(nodeBlockHeight);

    std::vector<FastSyncBlockHashs> blockHeightHashes;
    if (GetHeightBlockHash(startHeight, endHeight, blockHeightHashes))
    {
        for(auto &blockHeightHash : blockHeightHashes)
        {
            auto heightHash = ack.add_hashs();
            heightHash->set_height(blockHeightHash.height());
            for(auto &hash : blockHeightHash.hashs())
            {
                heightHash->add_hashs(hash);
            }
        }
        NetSendMessage<FastSyncGetHashAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }
}

void SendFastSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs)
{
    FastSyncGetBlockReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    for(auto &blockHeightHash : requestHashs)
    {
        auto heightHash = req.add_hashs();
        heightHash->set_height(blockHeightHash.height());
        for(auto &hash : blockHeightHash.hashs())
        {
            heightHash->add_hashs(hash);
        }
    }

    NetSendMessage<FastSyncGetBlockReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFastSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs)
{
    FastSyncGetBlockAck ack;
    ack.set_msg_id(msgId);
    DBReader dbReader;
    std::vector<std::string> blockHashes;
    for(const auto& heightHashs : requestHashs)
    {
        std::vector<std::string> dbHashs;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(heightHashs.height(), dbHashs))
        {
            return ;
        }
        for(auto& dbHash : dbHashs)
        {
            auto hashs = heightHashs.hashs();
            auto end = hashs.end();
            auto found = find_if(hashs.begin(), hashs.end(), [&dbHash](const string& hash){return dbHash == hash;});
            if(found != end)
            {
                blockHashes.push_back(dbHash);
            }
        }
    }

    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlocksByBlockHash(blockHashes, blocks))
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
        auto syncBlocks = ack.mutable_blocks();
        auto found = std::find_if(syncBlocks->begin(), syncBlocks->end(), [height](const FastSyncBlock& sync_block){return sync_block.height() == height;});
        if(found == syncBlocks->end())
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

    NetSendMessage<FastSyncGetBlockAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
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
    std::vector<FastSyncBlockHashs> heightHashs;
    for(int i = 0; i < msg->hashs_size(); ++i)
    {
        auto& hash = msg->hashs(i);
        heightHashs.push_back(hash);
    }
    SendFastSyncGetBlockAck(msg->self_node_id(), msg->msg_id(), heightHashs);  
    return 0;
}

int HandleFastSyncGetBlockAck(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

void SendSyncGetSumHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    SyncGetSumHashReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<SyncGetSumHashReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetSumHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{

    if(startHeight > endHeight)
    {
        return;
    }
    if (endHeight - startHeight > 1000)
    {
        return;
    }
    SyncGetSumHashAck ack;
    ack.set_self_node_id(NetGetSelfNodeId());
    DBReader dbReader;
    uint64_t selfNodeHeight = 0;
    if (0 != dbReader.GetBlockTop(selfNodeHeight))
    {
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_node_block_height(selfNodeHeight);
    ack.set_msg_id(msgId);

    uint64_t end = endHeight > selfNodeHeight ? selfNodeHeight : endHeight;
    std::string hash;
    uint64_t j = 0;
    std::vector<std::string> block_hashes;
    for (uint64_t i = startHeight; j <= end; ++i)
    {
        j = i + 1;
        j = j > end ? end : j;
        block_hashes.clear();
        hash.clear();
        if (GetHeightBlockHash(i, i, block_hashes) && SumHeightHash(block_hashes, hash))
        {
            auto syncSumHash = ack.add_sync_sum_hashes();
            syncSumHash->set_start_height(i);
            syncSumHash->set_end_height(i);
            syncSumHash->set_hash(hash);
        }
        else
        {
            return;
        }
        if(i == j) break;
    }
    NetSendMessage<SyncGetSumHashAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetHeightHashReq(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    SyncGetHeightHashReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<SyncGetHeightHashReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetHeightHashAck(SyncGetHeightHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        ack.set_code(-1);
        return;
    }
    if (endHeight - startHeight > 500)
    {
        ack.set_code(-2);
        return;
    }
    ack.set_self_node_id(NetGetSelfNodeId());
    DBReader dbReader;
    uint64_t selfNodeHeight = 0;
    if (0 != dbReader.GetBlockTop(selfNodeHeight))
    {
        ack.set_code(-3);
        ERRORLOG("GetBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msgId);
    std::vector<std::string> blockHashes;
    if(endHeight > selfNodeHeight)
    {
        endHeight = selfNodeHeight + 1;
    }
    if(startHeight > endHeight)
    {
        ack.set_code(-4);
        return;
    }
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(startHeight, endHeight, blockHashes))
    {
        ack.set_code(-5);
        return;
    }
    
    std::sort(blockHashes.begin(), blockHashes.end());
    for (const auto& hash : blockHashes)
    {
        ack.add_block_hashes(hash);
    }
    ack.set_code(0);
}

void SendSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, const std::vector<std::string> &reqHashes)
{
    SyncGetBlockReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    for (const auto& hash : reqHashes)
    {
        req.add_block_hashes(hash);
    }
    NetSendMessage<SyncGetBlockReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, const std::vector<std::string> &reqHashes)
{

    if (reqHashes.size() > 5000)
    {
        return;
    }
    SyncGetBlockAck ack;
    ack.set_msg_id(msgId);
    DBReader dbReader;
    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlocksByBlockHash(reqHashes, blocks))
    {
        return;
    }
    for (auto &block : blocks)
    {
        ack.add_blocks(block);
    }
    NetSendMessage<SyncGetBlockAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
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
    NetSendMessage<SyncGetHeightHashAck>(msg->self_node_id(), ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}

int HandleSyncGetHeightHashAck(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int HandleSyncGetBlockReq(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata)
{
    std::vector<std::string> reqHashes;
    for (const auto& hash : msg->block_hashes())
    {
        reqHashes.push_back(hash);
    }
    SendSyncGetBlockAck(msg->self_node_id(), msg->msg_id(), reqHashes);
    return 0;
}

int HandleSyncGetBlockAck(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

void SendFromZeroSyncGetSumHashReq(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights)
{
    SyncFromZeroGetSumHashReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    for(auto height : heights)
    {
        req.add_heights(height);
    }
    NetSendMessage<SyncFromZeroGetSumHashReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);

}

void SendFromZeroSyncGetSumHashAck(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights)
{
    DEBUGLOG("handle FromZeroSyncGetSumHashAck from {}", nodeId);
    SyncFromZeroGetSumHashAck ack;
    DBReader dbReader;
    
    for(auto height : heights)
    {
        DEBUGLOG("FromZeroSyncGetSumHashAck get height {}", height);
        std::string sumHash;
        if (DBStatus::DB_SUCCESS != dbReader.GetSumHashByHeight(height, sumHash))
        {
            DEBUGLOG("fail to get sum hash height at height {}", height);
            continue;
        }
        SyncFromZeroSumHash* sumHashItem = ack.add_sum_hashes();
        sumHashItem->set_height(height);
        sumHashItem->set_hash(sumHash);
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
    ack.set_self_node_id(NetGetSelfNodeId());
    ack.set_msg_id(msgId);
    DEBUGLOG("SyncFromZeroGetSumHashAck: id:{} , msgId:{}", nodeId, msgId);
    NetSendMessage<SyncFromZeroGetSumHashAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFromZeroSyncGetBlockReq(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    SyncFromZeroGetBlockReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    req.set_height(height);
    NetSendMessage<SyncFromZeroGetBlockReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendFromZeroSyncGetBlockAck(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    if(height < global::ca::sum_hash_range)
    {
        DEBUGLOG("sum height {} less than sum hash range", height);
        return;
    }
    SyncFromZeroGetBlockAck ack;
    DBReader dbReader;
    std::vector<std::string> blockhashes;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(height - global::ca::sum_hash_range + 1, height, blockhashes))
    {
        DEBUGLOG("GetBlockHashesByBlockHeight at height {}:{} fail", height - global::ca::sum_hash_range + 1, height);
        return;
    }
    std::vector<std::string> blockraws;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlocksByBlockHash(blockhashes, blockraws))
    {
        DEBUGLOG("GetBlocksByBlockHash fail");
        return;
    }

    for (auto &blockraw : blockraws)
    {
        ack.add_blocks(blockraw);
    }
    ack.set_height(height);
    ack.set_msg_id(msgId);
    ack.set_self_node_id(NetGetSelfNodeId());
    DEBUGLOG("response sum hash blocks at height {} to {}", height, nodeId);
    NetSendMessage<SyncFromZeroGetBlockAck>(nodeId, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

int HandleFromZeroSyncGetSumHashReq(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata)
{
    std::vector<uint64_t> heights;
    for(auto height : msg->heights())
    {
        heights.push_back(height);
    }
    DEBUGLOG("SyncFromZeroGetSumHashReq: id:{}, msgId:{}", msg->self_node_id(), msg->msg_id());
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

void SendSyncNodeHashReq(const string &nodeId, const string &msgId)
{
    SyncNodeHashReq req;
    req.set_self_node_id(NetGetSelfNodeId());
    req.set_msg_id(msgId);
    NetSendMessage<SyncNodeHashReq>(nodeId, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
}

void SendSyncNodeHashAck(const string &nodeId, const string &msgId)
{
    DEBUGLOG("handle SendSyncNodeHashAck from {}", nodeId);
    SyncNodeHashAck ack;
    ack.set_self_node_id(NetGetSelfNodeId());
    ack.set_msg_id(msgId);

    DBReadWriter reader;
    uint64_t nodeBlockHeight = 0;
    if (DBStatus::DB_SUCCESS != reader.GetBlockTop(nodeBlockHeight))
    {
        ERRORLOG("GetBlockTop error");
        return;
    }
    std::string sumHash;
    if (!ca_algorithm::CalculateHeightSumHash(nodeBlockHeight - syncHeightCnt, nodeBlockHeight, reader, sumHash))
    {
        ERRORLOG("CalculateHeightSumHash error");
        return;
    }
    ack.set_hash(sumHash);
    NetSendMessage<SyncNodeHashAck>(nodeId, ack, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
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
