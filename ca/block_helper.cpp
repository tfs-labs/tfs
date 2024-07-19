#include "block_helper.h"


#include "ca/test.h"
#include "ca/checker.h"
#include "ca/algorithm.h"
#include "ca/block_http_callback.h"
#include "ca/transaction_cache.h"
#include "ca/double_spend_cache.h"
#include "ca/sync_block.h"

#include "common.pb.h"
#include "common/task_pool.h"
#include "common/global_data.h"

#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/tfs_bench_mark.h"
#include "utils/contract_utils.h"

#include "db/db_api.h"
#include "net/interface.h"
#include "include/scope_guard.h"
#include "ca/evm/evm_manager.h"

static global::ca::SaveType g_syncType = global::ca::SaveType::Unknow;

BlockHelper::BlockHelper() : _missingPrehash(false){}

int GetUtxoFindNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds)
{
    return SyncBlock::GetSyncNodeSimplify(num, chainHeight, pledgeAddr, sendNodeIds);
}

int SendBlockByUtxoReq(const std::string &utxo)
{
    if(!MagicSingleton<BlockHelper>::GetInstance()->GetwhetherRunSendBlockByUtxoReq())
    {
        DEBUGLOG("RollbackPreviousBlocks is running");
        return 0;
    }
    MagicSingleton<BlockHelper>::GetInstance()->SetwhetherRunSendBlockByUtxoReq(false);

    ON_SCOPE_EXIT{
        MagicSingleton<BlockHelper>::GetInstance()->PopMissUTXO();
        MagicSingleton<BlockHelper>::GetInstance()->SetwhetherRunSendBlockByUtxoReq(true);
    };

    DEBUGLOG("begin get missing block utxo {}",utxo);
    std::vector<std::string> sendNodeIds;

    uint64_t chainHeight = 0;
    if(!BlockHelper::ObtainChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t selfNodeHeight = 0;
    std::vector<std::string> pledgeAddr;
    DBReader dbReader;
    {
        auto status = dbReader.GetBlockTop(selfNodeHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (GetUtxoFindNode(global::ca::KMinSyncQualNodes, chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = sendNodeIds.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, sendNum * 0.8, msgId))
    {
        return -5;
    }
    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    for (auto &nodeId : sendNodeIds)
    {
        GetBlockByUtxoReq req;
        req.set_addr(selfNodeId);
        req.set_utxo(utxo);
        req.set_msg_id(msgId);
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, nodeId))
        {
            return -6;
        }
        NetSendMessage<GetBlockByUtxoReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantine(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }
    GetBlockByUtxoAck ack;
    std::string blockRaw = "";
    for(auto iter = retDatas.begin(); iter != retDatas.end(); iter++)
    {
        ack.Clear();
        if (!ack.ParseFromString(*iter))
        {
            continue;
        }
        if(iter == retDatas.begin())
        {
            blockRaw = ack.block_raw();
        }
        else
        {
            if( blockRaw != ack.block_raw())
            {
                ERRORLOG("get different block");
                return -8;
            }
        }
    }

    if(blockRaw == "")
    {
        ERRORLOG("blockRaw is empty!");
        return -9;
    }

    CBlock block;
    if(!block.ParseFromString(blockRaw))
    {
        ERRORLOG("blockRaw parse fail!");
        return -10;
    }

    std::string strHeader;
    if (DBStatus::DB_SUCCESS == dbReader.GetBlockByBlockHash(block.hash(), strHeader)) 
    {
        DEBUGLOG("SendBlockByUtxoReq error in blockHash:{} , now run RollbackPreviousBlocks to find utxo: {}",block.hash(), utxo);
        MagicSingleton<SyncBlock>::GetInstance()->ThreadStop();
        int ret = MagicSingleton<BlockHelper>::GetInstance()->RollbackPreviousBlocks(utxo, selfNodeHeight, block.hash());
        MagicSingleton<SyncBlock>::GetInstance()->ThreadStart(true);
        if(ret != 0)
        {
            ERRORLOG("RollbackPreviousBlocks fail, fail num: {}", ret);
            return -11;
        }
    }


    MagicSingleton<BlockHelper>::GetInstance()->AddMissingBlock(block);
    
    return 0;
}

int BlockHelper::RollbackPreviousBlocks(const std::string utxo, uint64_t shelfHeight, const std::string blockHash)
{

    DEBUGLOG("running RollbackPreviousBlocks");
    DBReader dbReader;
    uint64_t chainHeight = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->ObtainChainHeight(chainHeight))
    {
        ERRORLOG("ObtainChainHeight error -1");
        return -1;
    }
    if(chainHeight < shelfHeight + 50)
    {
        ERRORLOG("chainHeight > shelfHeight  -2");
        return -2;
    }
    for(int i = shelfHeight / 100 * 100; i > 0; --i)
    {
        std::vector<std::string> selfBlockHashes;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(i, i, selfBlockHashes))
        {
            ERRORLOG("GetBlockHashesByBlockHeight error -3");
            return -3;
        }

        CBlock tempBlock;
        for(const auto& self_block_hashe: selfBlockHashes)
        { 
            std::string strblock;
            auto res = dbReader.GetBlockByBlockHash(self_block_hashe, strblock);
            if (DBStatus::DB_SUCCESS != res)
            {
                ERRORLOG("GetBlockByBlockHash failed -4");
                return -4;
            }

            if(!tempBlock.ParseFromString(strblock))
            {
                ERRORLOG("blockRaw parse fail! -5");
                return -5;
            }

            for(const auto& tx : tempBlock.txs())
            {     
                for(const auto& vin: tx.utxo().vin())
                {
                    for(const auto& prevOutput: vin.prevout())
                    {
                        if(prevOutput.hash() == utxo && tempBlock.hash() != blockHash)
                        {
                            DEBUGLOG("SetFastSync height: {}", tempBlock.height());
                            SyncBlock::SetFastSync(tempBlock.height());
                            return 0;
                        }
                    } 
                }
            }
        }
    }

    return -6;
}



int SendBlockByHashReq(const std::map<std::string, bool> &missingHashs)
{
    DEBUGLOG("SendBlockByHashReq Start");
    std::vector<std::string> sendNodeIds;

    uint64_t chainHeight = 0;
    if(!BlockHelper::ObtainChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t selfNodeHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.GetBlockTop(selfNodeHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }
        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        
        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (GetUtxoFindNode(global::ca::KMinSyncQualNodes, chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = sendNodeIds.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, sendNum * 0.8, msgId))
    {
        return -5;
    }
    GetBlockByHashReq req;
    for(auto &it : missingHashs)
    {
        auto missingHash = req.add_missinghashs();
        missingHash->set_hash(it.first);
        missingHash->set_tx_or_block(it.second);
    }

    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    req.set_addr(selfNodeId);
    req.set_msg_id(msgId);

    for (auto &nodeId : sendNodeIds)
    {
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, nodeId))
        {
            return -6;
        }
        NetSendMessage<GetBlockByHashReq>(nodeId, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantine(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }

    GetBlockByHashAck ack;
    uint32_t succentCount = 0;
    std::map<std::string, std::pair<std::string, uint32_t>> seekBlockHashes;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        succentCount++;
        for (auto &block : ack.blocks())
        {
            if (seekBlockHashes.end() == seekBlockHashes.find(block.hash()))
            {
                seekBlockHashes[block.hash()].first = std::move(block.block_raw());
                seekBlockHashes[block.hash()].second = 1;
            }
            seekBlockHashes[block.hash()].second++;
        }
    }

    uint32_t verifyNum = succentCount / 5 * 4;
    std::vector<std::pair<CBlock,std::string>> seekBlocks;
    for(const auto& it : seekBlockHashes)
    {
        if(it.second.second > verifyNum)
        {
            CBlock block;
            if(!block.ParseFromString(it.second.first))
            {
                ERRORLOG("blockRaw parse fail!");
                return -8;
            }
            seekBlocks.push_back({block, it.first});
        }
    }

    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockTask(std::bind(&BlockHelper::AddSeekBlock, MagicSingleton<BlockHelper>::GetInstance().get(), seekBlocks));
    return 0;
}

int SeekBlockByContractPreHashReq(const std::string &seekBlockHash, std::string& contractBlockStr)
{
    DEBUGLOG("SeekBlockByContractPreHashReq Start");
    std::vector<std::string> sendNodeIds;

    uint64_t chainHeight = 0;
    if(!BlockHelper::ObtainChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t selfNodeHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.GetBlockTop(selfNodeHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (GetUtxoFindNode(global::ca::KMinSyncQualNodes, chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = sendNodeIds.size();
    if (!GLOBALDATAMGRPTR.CreateWait(2, sendNum / 2, msgId))
    {
        return -5;
    }
    GetBlockByHashReq req;
    auto missingHash = req.add_missinghashs();
    missingHash->set_hash(seekBlockHash);
    missingHash->set_tx_or_block(false);
    
    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    req.set_addr(selfNodeId);
    req.set_msg_id(msgId);

    for (auto &node_id : sendNodeIds)
    {
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, node_id))
        {
            return -6;
        }
        NetSendMessage<GetBlockByHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> retDatas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantine(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }

    GetBlockByHashAck ack;
    std::map<std::string, std::pair<std::string, uint32_t>> seekBlockHashes;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        for (auto &iter : ack.blocks())
        {
            contractBlockStr = iter.block_raw();
            return 0;
        }
    }

    return -8;
}



int BlockHelper::VerifyFlowedBlock(const CBlock& block, BlockStatus* blockStatus , BlockMsg *msg)
{
    if( block.version() != global::ca::kCurrentBlockVersion)
	{
		return -1;
	}
    bool isVerify = true;
    auto selfAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(GenerateAddr(block.sign(0).pub()) == selfAddr)
    {
        isVerify = false;
    }
    uint64_t blockHeight = block.height();
    
	std::string blockHash = block.hash();
	if(blockHash.empty())
    {
        return -2;
    }
    
    DBReadWriter dbWriter;
	uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbWriter.GetBlockTop(nodeHeight))
    {
        return -3;
    }

    if ( (nodeHeight  > 9) && (nodeHeight - 9 > blockHeight))
	{
        ERRORLOG("VerifyHeight fail!!,blockHeight:{}, nodeHeight:{}, isVerify:{}",blockHeight, nodeHeight, isVerify);
		return -4;
	}
	else if (nodeHeight + 1 < blockHeight)
	{
        ERRORLOG("VerifyHeight fail!!,blockHeight:{}, nodeHeight:{}, isVerify:{}",blockHeight, nodeHeight, isVerify);
		return -5;
	}

    uint64_t chainHeight = 0;
    if(!ObtainChainHeight(chainHeight))
    {
        return -6;
    }

    //Increase the height and time of the block within a certain height without judgment
    if(chainHeight > global::ca::kMinUnstakeHeight)
    {
        uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        const static uint64_t kStabilityTime = 60 * 1000000;
        if(blockHeight < (chainHeight - 10) && currentTime - block.time() > kStabilityTime)
        {
            DEBUGLOG("broadcast block overtime , block height{}, block hash{}",blockHeight,blockHash);
            return -7;
        }
    }
    
	std::string strTempHeader;

	DBStatus status = dbWriter.GetBlockByBlockHash(blockHash, strTempHeader);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -8;
	}

	if (strTempHeader.size() != 0)
	{
		return -9;
	}

	std::string strPrevHeader;
	status = dbWriter.GetBlockByBlockHash(block.prevhash(), strPrevHeader);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -10;
	}

	if (strPrevHeader.size() == 0)
	{
        return -11;
	}
    std::vector<CTransaction> doubleSpentTransactions;
    Checker::CheckConflict(block, doubleSpentTransactions);
    if(!doubleSpentTransactions.empty())
    {
        if(blockStatus != NULL)
        {
            for(const auto& tx : doubleSpentTransactions)
            {
                auto txStatus = blockStatus->add_txstatus();
                txStatus->set_txhash(tx.hash());
                txStatus->set_status(global::ca::DoubleSpend::SingleBlock);
            }
            
        }
        std::ostringstream filestream;
        PrintBlock(block,true,filestream);

        std::string testStr = filestream.str();
        DEBUGLOG("doubleSpentTransactions block --> {}", testStr);
        return -12;
    }
    auto startT5 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    if(block.sign_size() >= 1)
    {
        DEBUGLOG("verifying block {} , isVerify:{}, addr:{}", blockHash.substr(0, 6), isVerify, GenerateAddr(block.sign(0).pub()));
    }
	auto ret = ca_algorithm::VerifyBlock(block, false, true, isVerify, blockStatus,msg);

	if (0 != ret)
	{
		ERRORLOG("verify block fail ret:{}:{}:{}", ret, blockHeight, blockHash);
		return -13;
	}
    
    if(!isVerify){
        auto endT5 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        auto t5 = endT5 - startT5;
        MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(block.hash(), &t5, 3);
    }
    return 0;
}

int BlockHelper::SaveBlock(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean)
{    
    DBReadWriter* dbWriterPtr = new DBReadWriter();
    ON_SCOPE_EXIT{
        if (dbWriterPtr != nullptr)
        {
            delete dbWriterPtr;
            dbWriterPtr = nullptr;
        }
        if (saveType == global::ca::SaveType::Broadcast)
        {
            DEBUGLOG("SAVETEST hash: {} , BlockHelper::SaveBlock end: {}", block.hash(), MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
        }
    };


    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbWriterPtr->GetBlockTop(nodeHeight))
    {
         ERRORLOG("GetBlockTop error!");
        return -3;
    }

    if (block.height() >= nodeHeight + 2)
    {
        ERRORLOG("block.height:{} >= (nodeHeight + 2):{}", block.height(), nodeHeight + 2);
        return -4;
    }

    int ret = 0;
    std::string blockRaw;
    std::string blockHash = block.hash();
    ret = dbWriterPtr->GetBlockByBlockHash(block.hash(), blockRaw);
    if (DBStatus::DB_SUCCESS == ret)
    {
        INFOLOG("BlockHelper block {} already in saved , skip",block.hash().substr(0, 6));
        return 0;
    }

    ret = PreSaveProcess(block, saveType, obtainMean);
    if (ret < 0)
    {
        ERRORLOG("PreSaveProcess ret : {}", ret);
        return -5;
    }
    DEBUGLOG("PreSaveProcess doubleSpendCheck ret:{}", ret);
    
    ResetMissingPrehash();
    uint64_t blockHeight = block.height();
    ret = ca_algorithm::SaveBlock(*dbWriterPtr, block, saveType, obtainMean);
    if (0 != ret)
    {
        ERRORLOG("save block ret:{}:{}:{}", ret, blockHeight, blockHash);
        if(saveType == global::ca::SaveType::SyncNormal || saveType == global::ca::SaveType::SyncFromZero)
        {
            DEBUGLOG("run new sync, start height: {}", blockHeight);
            SyncBlock::SetNewSyncHeight(blockHeight);
        }
        if (_missingPrehash)
        {
            ResetMissingPrehash();
            DEBUGLOG("run new sync, start height: {}", blockHeight - 1);
            SyncBlock::SetNewSyncHeight(blockHeight - 1);
            return -6;
        }
        if(!_missingUtxos.empty())
        {
            GetMissBlock();
            return -7;
        }
        return -8;
    }
    if(DBStatus::DB_SUCCESS != dbWriterPtr->TransactionCommit())
    {     
        ERRORLOG("Transaction commit fail");
        return -9;   
    }
    //TODO::
    MagicSingleton<DoubleSpendCache>::GetInstance()->Detection(block);

    INFOLOG("save block ret:{}:{}:{}", ret, blockHeight, blockHash);
    auto startTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    PostSaveProcess(block);
    auto endTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(); 
    postCommitCost += (endTime - startTime);
    postCommitCount++;

    return 0;
}

bool BlockHelper::VerifyHeight(const CBlock& block, uint64_t ownblockHeight)
{
    DBReader dbReader;

	unsigned int preheight = 0;
	if (DBStatus::DB_SUCCESS != dbReader.GetBlockHeightByBlockHash(block.prevhash(), preheight))
	{
		ERRORLOG("get block height failed,block.prehash() = {} ,block.hash() = {}, preheight = {} " ,block.prevhash(),block.hash(),preheight);
		return false;
	}

	if(ownblockHeight > (preheight + 5))
	{
		return false;
	}
	return true;
}

void BlockHelper::PostMembershipCancellationProcess(const CBlock &block)
{
    for (int i = 0; i < block.txs_size(); i++)
    {
        CTransaction tx = block.txs(i);
        if (GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }

        global::ca::TxType txType;
        txType = (global::ca::TxType)tx.txtype();

        if (global::ca::TxType::kTxTypeUnstake == txType || global::ca::TxType::kTxTypeDisinvest == txType)
        {
            DBReadWriter dbWriter;
            std::vector<std::string> blockHashs;
            uint64_t blockHeight = block.height();
            if (DBStatus::DB_SUCCESS != dbWriter.GetBlockHashsByBlockHeight(blockHeight, blockHashs))
            {
                ERRORLOG("fail to get block hash at height {}", blockHeight);
                continue;
            }
            std::vector<std::string> blocks;
            if (DBStatus::DB_SUCCESS != dbWriter.GetBlocksByBlockHash(blockHashs, blocks))
            {
                ERRORLOG("fail to get block at height {}", blockHeight);
                continue;
            }
            
            for (auto &blockRaw : blocks)
            {                                                                               
                CBlock heightBlock;                
                if (!heightBlock.ParseFromString(blockRaw))
                {
                    ERRORLOG("block parse fail!");
                    continue;
                }
                if(heightBlock.hash() == block.hash())
                {
                    continue;
                }
                for (int i = 0; i < heightBlock.txs_size(); i++)
                {
                    CTransaction height_tx = heightBlock.txs(i);
                    bool isNeedAgent = TxHelper::IsNeedAgent(tx);

                    for (int i = (isNeedAgent ? 0 : 1); i < block.sign_size(); ++i)
                    {
                        std::string signAddr = GenerateAddr(block.sign(i).pub());
                        if(std::find(tx.utxo().owner().begin(), tx.utxo().owner().end(), signAddr) != tx.utxo().owner().end())
                        {
                            int ret = ca_algorithm::RollBackByHash(heightBlock.hash());
                            if (ret != 0)
                            {
                                ERRORLOG("rollback hash {} fail, ret: ", heightBlock.hash(), ret);
                            }
                        }                    
                    }
                }

            }
        }
    }
}

std::pair<doubleSpendType, CBlock> BlockHelper::DealDoubleSpend(const CBlock& block, const CTransaction& tx, const std::string& missingUtxo)
{
    uint64_t blockHeight = block.height();
    std::string blockHash = block.hash();

    DBReader dbReader;
    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(nodeHeight))
    {
        ERRORLOG("GetBlockTop fail!!!");
        return {doubleSpendType::err,{}};
    }
    
    std::set<std::string> setOwner(tx.utxo().owner().begin(), tx.utxo().owner().end());
    std::vector<std::string> blockHashes;
    if(blockHeight > nodeHeight)
    {
        DEBUGLOG("blockHeight:({}) > nodeHeight:({})", blockHeight, nodeHeight);
        return {doubleSpendType::err,{}};
    }
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(blockHeight, nodeHeight, blockHashes))
    {
        ERRORLOG("GetBlockHashesByBlockHeight fail!!!");
        return {doubleSpendType::err,{}};
    }
    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlocksByBlockHash(blockHashes, blocks))
    {
        ERRORLOG("GetBlocksByBlockHash fail!!!");
        return {doubleSpendType::err,{}};
    }

    for (auto &PBlockStr : blocks)
    {
        CBlock PBlock;
        if(PBlock.ParseFromString(PBlockStr))
        {
            for(const auto& PTx : PBlock.txs())
            {
                if((global::ca::TxType)PTx.txtype() != global::ca::TxType::kTxTypeTx)
                {
                    continue;                              
                }
                for (auto &PVin : PTx.utxo().vin())
                {
                    std::string PVinAddr = GenerateAddr(PVin.vinsign().pub());
                    if(setOwner.find(PVinAddr) != setOwner.end())
                    {
                        for (auto & PPrevout : PVin.prevout())
                        {
                            std::string PUtxo = PPrevout.hash();
                            if(missingUtxo == PUtxo)
                            {
                                DEBUGLOG("DoubleSpend, blockHeight:{}, PBlock.height:{} , block_time:{}, PBlock.time:{}, blockHash:{}, PBlockHash:{}", blockHeight, PBlock.height() , block.time() , PBlock.time(), block.hash().substr(0,6), PBlock.hash().substr(0,6));
                                //same height doublespend
                                if((blockHeight == PBlock.height() && block.time() >= PBlock.time()) || blockHeight > PBlock.height())
                                {
                                    DEBUGLOG("_doubleSpendBlocks.insert(blockHash):{}", blockHash.substr(0,6));
                                    _doubleSpendBlocks.insert({blockHash, block});
                                    CheckDoubleBlooming({doubleSpendType::newDoubleSpend, std::move(PBlock)}, block);
                                    return {doubleSpendType::newDoubleSpend,std::move(PBlock)};
                                }
                                else
                                {
                                    DEBUGLOG("_doubleSpendBlocks PBlock roll oldBackHash:{} at newBlockHash:{}", PBlock.hash().substr(0,6), blockHash.substr(0,6));
                                    auto ret = ca_algorithm::RollBackByHash(PBlock.hash());
                                    if (ret != 0)
                                    {
                                        ERRORLOG("PBlock rollback hash {} fail, ret:{}", PBlock.hash(), ret);
                                        return {doubleSpendType::err,{}};
                                    }
                                    CheckDoubleBlooming({doubleSpendType::oldDoubleSpend, std::move(PBlock)}, block);
                                    return {doubleSpendType::oldDoubleSpend,std::move(PBlock)};
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    DEBUGLOG("PBlock Not found _doubleSpendBlocks.insert(blockHash):{}", blockHash);
    _doubleSpendBlocks.insert({blockHash, block});
    return {doubleSpendType::invalidDoubleSpend,{}};
}

int BlockHelper::PreSaveProcess(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean)
{
    uint64_t blockHeight = block.height();
    std::string blockHash = block.hash();
    if(saveType == global::ca::SaveType::SyncNormal)
    {
        DEBUGLOG("verifying block {}", blockHash.substr(0, 6));
        ResetMissingPrehash();
        auto ret = ca_algorithm::VerifyBlock(block, true, false);
        if (0 != ret)
        {
            ERRORLOG("verify block ret:{}:{}:{}", ret, blockHeight, blockHash);
            if (_missingPrehash)
            {
                ResetMissingPrehash();
                DEBUGLOG("run new sync, start height: {}", blockHeight - 1);
                SyncBlock::SetNewSyncHeight(blockHeight - 1);
                return -1;
            }
            if(!_missingUtxos.empty())
            {
                GetMissBlock();
                return -2;
            }
            return -3;
        }
    }
    else if(saveType == global::ca::SaveType::Broadcast)
    {
        DBReader dbReader;
        uint64_t nodeHeight = 0;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(nodeHeight))
        {
            return -4;
        }
        if(obtainMean == global::ca::BlockObtainMean::Normal && blockHeight + 50 < nodeHeight)
        {
            DEBUGLOG("blockHeight + 50 < nodeHeight");
            return -5;
        }

        if(ca_algorithm::VerifyPreSaveBlock(block) < 0)
        {
            ERRORLOG("Verify PreSave Block fail!");
            return -6;
        }
        
        for (auto& tx : block.txs())
        {
            if (GetTransactionType(tx) != kTransactionType_Tx)
            {
                continue;
            }
            std::string missingUtxo;
            int ret = ca_algorithm::DoubleSpendCheck(tx, false, &missingUtxo);
            if (0 != ret)
            {
                if(ret == -5 || ret == -7 || ret == -8 && !missingUtxo.empty())
                {
                    std::string blockHash;
                    if(dbReader.GetBlockHashByTransactionHash(missingUtxo, blockHash) == DBStatus::DB_SUCCESS)
                    {
                        DEBUGLOG("DoubleSpendCheck fail!! <utxo>: {}, ", missingUtxo);
                        auto doubleSpendInfo = DealDoubleSpend(block, tx , missingUtxo);
                        if(doubleSpendInfo.first == doubleSpendType::newDoubleSpend)
                        {
                            return -7;
                        }
                        else if(doubleSpendInfo.first == doubleSpendType::oldDoubleSpend)
                        {
                            DEBUGLOG("oldDoubleSpend,blockHash:{}, txHash:{}", block.hash().substr(0,6), tx.hash().substr(0,10));
                            continue;
                        }
                        else if(doubleSpendInfo.first == doubleSpendType::invalidDoubleSpend)
                        {
                            return -8;
                        }
                        else
                        {
                            return -9;
                        }
                    }
                    else
                    {
                        DEBUGLOG("not found!! <utxo>: {}, ", missingUtxo);
                        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                        std::unique_lock<std::mutex> locker(_seekMutex);
                        _missingBlocks.insert({missingUtxo, nowTime, 1});
                    }
                }

                auto found = _hashPendingBlocks.find(block.hash());
                if(found == _hashPendingBlocks.end())
                {
                    _hashPendingBlocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), block};
                }
                
                DEBUGLOG("DoubleSpendCheck fail!! block height:{}, hash:{}, ret: {}, ", block.height(), block.hash().substr(0,6), ret);
                return ret;
            }
        }
        DEBUGLOG("++++++block height:{}, Hash:{}",block.height(), block.hash().substr(0,6));
    }
    return 0;
}

void BlockHelper::PostTransactionProcess(const CBlock &block)
{
    if (IsContractBlock(block))
    {
        MagicSingleton<TransactionCache>::GetInstance()->ContractBlockNotify(block.hash());
    }

    MagicSingleton<PeerNode>::GetInstance()->SetSelfHeight(block.height());

    // Run http callback
    if (MagicSingleton<CBlockHttpCallback>::GetInstance()->IsRunning())
    {
        MagicSingleton<CBlockHttpCallback>::GetInstance()->AddBlock(block);
    }
}

void BlockHelper::PostSaveProcess(const CBlock &block)
{
    MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapEnd(block.hash());
    MagicSingleton<TaskPool>::GetInstance()->CommitCaTask(std::bind(&BlockHelper::PostTransactionProcess, this, block));

    auto found = _pendingBlocks.find(block.height() + 1);
    if (found != _pendingBlocks.end())
    {
        auto& blocks = found->second;
        auto targetBegin = blocks.lower_bound(block.hash());
        auto target_end = blocks.upper_bound(block.hash());
        for (; targetBegin != target_end ; targetBegin++)
        {
            DEBUGLOG("_pendingBlocks Add block height:{}, hash:{}", targetBegin->second.height(), targetBegin->second.hash().substr(0,6));
            SaveBlock(targetBegin->second, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::ByPreHash);
        }     
    }
    for(auto& tx : block.txs())
    {
        if ((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeCallContract || (global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeDeployContract)
        {
            break;
        }
        auto contractBlockIter = _contractBlocks.find(tx.hash());
        if(contractBlockIter != _contractBlocks.end())
        {
            auto contractBlock = contractBlockIter->second;
            std::string contractTxPreHash;
            auto ret = checkContractBlock(contractBlock, contractTxPreHash);
            if(ret < 0)
            {
                DEBUGLOG("checkContractBlock error, contractBlockHash:{}, contractTxPreHash:{}",contractBlock.hash().substr(0,6), contractTxPreHash);
                break;
            }
            if(ret == 0)
            {
                if(!contractTxPreHash.empty())
                {
                    DEBUGLOG("Still can't find contractTxPreHash, contractBlockHash:{}, contractTxPreHash:{}",contractBlock.hash().substr(0,6), contractTxPreHash);
                    break;
                }
                else
                {
                    std::string blockRaw;
                    DBReader dbReader;
                    if(DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(contractBlock.prevhash(), blockRaw))
                    {
                        AddPendingBlock(contractBlock);
                        return;
                    }
                    DEBUGLOG("__contractBlocks Add block height:{}, hash:{}", contractBlockIter->second.height(), contractBlockIter->second.hash().substr(0,6));
                    SaveBlock(contractBlockIter->second, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::ByPreHash);
                }
            }
        }
    }

    PostMembershipCancellationProcess(block);
}

int BlockHelper::RollbackContractBlock()
{
    int ret = 0;
    std::set<std::string> addrMap; 
    std::set<std::string> rollbackBlocksHashs;
    for (auto it = _rollbackBlocks.rbegin(); it != _rollbackBlocks.rend(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            rollbackBlocksHashs.insert(sit->hash());
            if(IsContractBlock(*sit))
            {
                for(auto& tx :sit->txs())
                {
                    auto addr = GetContractAddr(tx);
                    if(!addr.empty())
                    {
                        addrMap.insert(addr);
                    }
                }
            }
        }
    }

    uint64_t selfNodeHeight = 0;
    DBReader dbReader;
    auto status = dbReader.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("RollbackContractBlock GetBlockTop error");
        return -1;
    }

    uint64_t beginHeight = _rollbackBlocks.begin()->first;
    std::vector<std::string> block_hashes;
    if(DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(beginHeight, selfNodeHeight, block_hashes))
    {
        ERRORLOG("RollbackContractBlock GetBlockHashesByBlockHeight error");
        return -2;
    }

    for(const auto& blockHash: block_hashes)
    {
        std::string blockStr;
        CBlock block;
        if(DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(blockHash, blockStr))
        {
            ERRORLOG("RollbackContractBlock GetBlockByBlockHash error");
            return -3;
        }
        block.ParseFromString(blockStr);

        auto findBlock = rollbackBlocksHashs.find(block.hash());
        if(findBlock != rollbackBlocksHashs.end())
        {
            continue;
        }

        if(IsContractBlock(block))
        {
            for(auto& tx :block.txs())
            {
                auto addr = GetContractAddr(tx);
                if(!addr.empty())
                {
                    auto findAddr = addrMap.find(addr);
                    if(findAddr != addrMap.end())
                    {
                        _rollbackBlocks[block.height()].insert(block);
                    }
                }
            }
        }
    }

    return 0;
}


int BlockHelper::RollbackBlocks()
{
    if (_rollbackBlocks.empty())
    {
        return 0;
    }

    int ret = RollbackContractBlock();
    if(ret != 0)
    {
        ERRORLOG("RollbackContractBlock error, error num: {}", ret);
        return -1;
    }

    for (auto it = _rollbackBlocks.rbegin(); it != _rollbackBlocks.rend(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            DEBUGLOG("roll back {} at height {}", sit->hash(), sit->height());
            ret = ca_algorithm::RollBackByHash(sit->hash());
            if (ret != 0)
            {
                ERRORLOG("rollback hash {} fail, ret: ", sit->hash(), ret);
                return -1;
            }
            
        }
    }
    return 0;
}

void BlockHelper::SetMissingPrehash()
{
    _missingPrehash = true;
}

void BlockHelper::ResetMissingPrehash()
{
    _missingPrehash = false;
}

void BlockHelper::PushMissUTXO(const std::string& utxo)
{
    std::lock_guard<std::mutex> lock(_missingUtxosMutex);
    _missingUtxos.push(utxo);
    if(_missingUtxos.size() > _kMaxMissingUxtoSize)
    {
        std::stack<std::string>().swap(_missingUtxos);
    }
}

bool BlockHelper::GetMissBlock()
{
    std::string utxo;
    {
        std::lock_guard<std::mutex> lock(_missingUtxosMutex);
        if(_missingUtxos.empty())
        {
            INFOLOG("utxo is empty!");
            return false;
        }
        utxo = _missingUtxos.top();
    }

    auto asyncThread = std::thread(SendBlockByUtxoReq, utxo);
	asyncThread.detach();
    return true;
}
void BlockHelper::PopMissUTXO()
{
    std::scoped_lock lock(_helperMutex, _missingUtxosMutex);
    if(_missingUtxos.empty())
    {
        return;
    }
    _missingUtxos.pop();
}

void BlockHelper::Process()
{
    static int count = 0;
    static int broadcastSaveFailCount = 0;
    static bool processing_ = false;
    if(processing_)
    {
        DEBUGLOG("BlockPoll::Process is processing_");
        return;
    }
    processing_ = true;
    std::lock_guard<std::mutex> lock(_helperMutex);
    postCommitCost = 0;
    postCommitCount = 0;
    DBReader dbReader;
    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(nodeHeight))
    {
        return;
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

    ON_SCOPE_EXIT{
        processing_ = false;
        MagicSingleton<BlockManager>::GetInstance()->removeExpiredBlocks(std::chrono::seconds(60));
        uint64_t newTop = 0;
        DBReader reader;
        if (reader.GetBlockTop(newTop) == DBStatus::DB_SUCCESS)
        {
            if (nodeHeight != newTop)
            {
                NotifyNodeHeightChange();
                DEBUGLOG("NotifyNodeHeightChange update ok.");
            }
        }
        _fastSyncBlocks.clear();
        auto begin = _pendingBlocks.begin();
        std::vector<decltype(begin)> deletePendingBlock;
        for(auto iter = begin; iter != _pendingBlocks.end(); ++iter)
        {
            if (newTop >= iter->first + 20)
            {
                deletePendingBlock.push_back(iter);
            }
        }

        for (auto pendingIter : deletePendingBlock)
        {
            DEBUGLOG("_pendingBlocks.erase height:{}", pendingIter->first);
            _pendingBlocks.erase(pendingIter);
        }
        _rollbackBlocks.clear();
        _syncBlocks.clear();
        _broadcastBlocks.clear();
        
        for(auto iter = _doubleSpendBlocks.begin(); iter != _doubleSpendBlocks.end();)
        {
            if(nowTime >= iter->second.time() + 30 * 1000000ull)
            {
                DEBUGLOG("AAAC _doubleSpendBlocks deleteBlockHash:{}", iter->first);
                _doubleSpendBlocks.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for(auto iter = _duplicateChecker.begin(); iter != _duplicateChecker.end();)
        {
            if(nowTime >= iter->second.second + 60 * 1000000)
            {
                _duplicateChecker.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for(auto iter = _contractBlocks.begin(); iter != _contractBlocks.end();)
        {
            if(newTop >= iter->second.height() + 10 || nowTime >= iter->second.time() + 30 * 1000000ull)
            {
                _contractBlocks.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    };


    uint64_t RollbackTimeout = 60 * 1000000;

    std::map<uint64_t, std::vector<std::set<CBlock, CBlockCompare>::iterator>> delete_rollback_blocks;
    for(auto iter = _rollbackBlocks.begin(); iter != _rollbackBlocks.end(); ++iter)
    {
        for(auto rollBack = iter->second.begin(); rollBack != iter->second.end(); ++rollBack)
        {
            if(nowTime <= RollbackTimeout + rollBack->time())
            {
                delete_rollback_blocks[iter->first].push_back(rollBack);
            }
        }
    }

    for(auto& iter : delete_rollback_blocks)
    {
        if(_rollbackBlocks.find(iter.first) != _rollbackBlocks.end())
        {
            for(auto& rollBack : iter.second)
            {
                DEBUGLOG("RollBackBlock No timeout blockHash:{}", rollBack->hash().substr(0,6));
                _rollbackBlocks[iter.first].erase(rollBack);
                
            }
        }
        if(_rollbackBlocks[iter.first].empty())
        {
            DEBUGLOG("_rollbackBlocks[*{}*].empty()", iter.first);
            _rollbackBlocks.erase(iter.first);
        }
    }

    int result = RollbackBlocks();
    if(result != 0)
    {
        return;
    }

    uint64_t chainHeight = 0;
    if(!ObtainChainHeight(chainHeight))
    {
        //ERRORLOG("fail to get chain height");
        return;
    }

    for(const auto& block : _fastSyncBlocks)
    {
        global::ca::BlockObtainMean obtain_mean = global::ca::BlockObtainMean::Normal;
        if (block.height() + 1 == nodeHeight)
        {
            obtain_mean = global::ca::BlockObtainMean::ByPreHash;
        }
        DEBUGLOG("_fastSyncBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, obtain_mean);
        usleep(100000);
        DEBUGLOG("fast_sync save block height: {}\tblock hash: {}\tresult: {}", block.height(), block.hash(), result);

        if (result == -2)
        {
            DEBUGLOG("next run new sync, start height: {}", block.height() - 1);
            SyncBlock::SetNewSyncHeight(block.height() - 1);
            return;
        }

        if(result != 0)
        {
            break;
        }
    }
    for(const auto& block : _utxoMissingBlocks)
    {
        DEBUGLOG("_utxoMissingBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, global::ca::BlockObtainMean::ByUtxo);
        if(result != 0)
        {
            if(_utxoMissingBlocks.size() > _kMaxMissingBlockSize)
            {
                _utxoMissingBlocks.clear();
            }
            break;
        }
    }
    _utxoMissingBlocks.clear();

    for(const auto& block : _syncBlocks)
    {
        if(!_stopBlocking)
        {
            return;
        }

        DEBUGLOG("chain height: {}, height: {}, sync type: {}", chainHeight, block.height(), g_syncType);
        DEBUGLOG("_syncBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, global::ca::BlockObtainMean::Normal);
        if(result != 0)
        {
            break;
        }
    }

    for(const auto& block : _broadcastBlocks)
    {
        std::string blockRaw;
        if (DBStatus::DB_SUCCESS == dbReader.GetBlockByBlockHash(block.hash(), blockRaw))
        {
            INFOLOG("block {} already saved", block.hash().substr(0,6));
            continue;
        }
        DEBUGLOG("_broadcastBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        SaveBlock(block, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::Normal);
    }

    auto begin = _hashPendingBlocks.begin();
    auto end = _hashPendingBlocks.end();
    std::vector<decltype(begin)> deleteUtxoBlocks;
    for(auto iter = begin; iter != end; ++iter)
    {
        if(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp() - iter->second.first > 3 * 1000000)
        {
            DEBUGLOG("_hashPendingBlocks.erase timeout block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            deleteUtxoBlocks.push_back(iter);
            continue;
        }
        DEBUGLOG("_hashPendingBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", iter->second.second.hash().substr(0, 6), iter->second.second.height(), iter->second.second.prevhash().substr(0, 6));
        int result = SaveBlock(iter->second.second, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::ByUtxo);
        if(result == 0)
        {
            DEBUGLOG("_hashPendingBlocks Add <success> block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            deleteUtxoBlocks.push_back(iter);
        }
        else
        {
            DEBUGLOG("_hashPendingBlocks Add <fail> block height:{}, hash:{}", iter->second.second.height(), iter->second.second.hash());
        }

    }

    for(auto uxtoBlockIter: deleteUtxoBlocks)
    {
        _hashPendingBlocks.erase(uxtoBlockIter);
    }
    
    return;
}

void BlockHelper::SeekBlockThread()
{
    if(_missingBlocks.empty())
    {
        DEBUGLOG("_missingBlocks.empty() == true");
        return;
    }

    DEBUGLOG("SeekBlockThread start");
    std::map<std::string, bool> missingHashs;
    auto begin = _missingBlocks.begin();
    auto end = _missingBlocks.end();
    std::vector<decltype(begin)> deleteMissingBlocks;

    {
        DBReader dbReader;
        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        std::unique_lock<std::mutex> locker(_seekMutex);
        for(auto iter = begin; iter != end; ++iter)
        {
            if(nowTime - iter->_time > 30 * 1000000 || *iter->_triggerCount > 3)
            {
                DEBUGLOG("missing_Hash:{}, timeout:({}),*iter->_triggerCount:({}),*iter->_txOrBlock:{}", iter->_hash, nowTime - iter->_time > 30 * 1000000, *iter->_triggerCount > 3, *iter->_txOrBlock);
                deleteMissingBlocks.push_back(iter);
                continue;
            }
            else if(nowTime - iter->_time > 15 * 1000000)
            {
                std::string strBlock;
                if(*iter->_txOrBlock)
                {
                    if (DBStatus::DB_SUCCESS == dbReader.GetBlockHashByTransactionHash(iter->_hash, strBlock))
                    {
                        deleteMissingBlocks.push_back(iter);
                        continue;
                    }
                }
                else if(DBStatus::DB_SUCCESS == dbReader.GetBlockByBlockHash(iter->_hash, strBlock))
                {
                    deleteMissingBlocks.push_back(iter);
                    continue;
                }

                if(missingHashs.find(iter->_hash) == missingHashs.end())
                {
                    DEBUGLOG("missing_Hash:{},*iter->_triggerCount:{},*iter->_txOrBlock:{}", iter->_hash, *iter->_triggerCount, *iter->_txOrBlock);
                    missingHashs[iter->_hash] = *(iter->_txOrBlock);
                }
                else
                {
                    //Filtering duplicate hash
                    deleteMissingBlocks.push_back(iter);
                }
                *iter->_triggerCount = *iter->_triggerCount + 1;
            }
            else
            {
                break;
            }
        }

        for(auto iter: deleteMissingBlocks)
        {
            DEBUGLOG("_missingBlocks.erase_Hash:{}", iter->_hash);
            _missingBlocks.erase(iter);
        }
    }

    if(!missingHashs.empty())
    {
        SendBlockByHashReq(missingHashs);
    }
            
}

void BlockHelper::AddSeekBlock(std::vector<std::pair<CBlock,std::string>>& seekBlocks)
{
    std::lock_guard<std::mutex> lock(_helperMutex);
    for(const auto &iter : seekBlocks)
    {
        auto& block = iter.first;
        auto found = _hashPendingBlocks.find(block.hash());
        if(found == _hashPendingBlocks.end())
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
            _hashPendingBlocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), block};
        }

        DEBUGLOG("AddSeekBlock missing_block_hash:{}, tx_or_block_hash:{}", block.hash(), iter.second); 
    }
}

void BlockHelper::CheckDoubleBlooming(const std::pair<doubleSpendType, CBlock>& doubleSpendInfo, const CBlock &block)
{
    if(doubleSpendInfo.first == doubleSpendType::newDoubleSpend)
    {
        auto found = _duplicateChecker.find(block.hash());
        DEBUGLOG("doubleSpendType:{}, status:{}, blockHash:{}", doubleSpendInfo.first, found->second.first, block.hash().substr(0,6));
        if(found != _duplicateChecker.end() && found->second.first)
        {
            MakeTxStatusMsg(doubleSpendInfo.second, block);
            return;
        }
    }  
    else if(doubleSpendInfo.first == doubleSpendType::oldDoubleSpend)
    {
        auto found = _duplicateChecker.find(doubleSpendInfo.second.hash());
        DEBUGLOG("doubleSpendType:{}, status:{}, blockHash:{}", doubleSpendInfo.first, found->second.first, block.hash().substr(0,6));
        if(found != _duplicateChecker.end() && found->second.first)
        {
            MakeTxStatusMsg(block, doubleSpendInfo.second);
            return;
        }
    }
    return;
}

void BlockHelper::MakeTxStatusMsg(const CBlock &oldBlock, const CBlock &newBlock)
{
    DEBUGLOG("AAAC MakeTxStatusMsg oldBlock:{}, newBlock:{}", oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
    BlockStatus blockStatus;
    for(const auto& tx1 : oldBlock.txs())
    {
        if(GetTransactionType(tx1) != kTransactionType_Tx)
        {
            continue;
        }

        for(const auto& tx2 : newBlock.txs())
        {
            if(GetTransactionType(tx2) != kTransactionType_Tx)
            {
                continue;
            }

            if(Checker::CheckConflict(tx1, tx2) == true)
            {
                DEBUGLOG("AAAC MakeTxStatusMsg oldBlocktx1:{}, newBlocktx2:{}", tx1.hash().substr(0,10), tx2.hash().substr(0,10));
                auto txStatus = blockStatus.add_txstatus();
                txStatus->set_txhash(tx2.hash());
                txStatus->set_status(global::ca::DoubleSpend::DoubleBlock);
            }
        }
    }

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    blockStatus.set_blockhash(newBlock.hash());
    blockStatus.set_status(-99);
    blockStatus.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    std::string destNode = GenerateAddr(newBlock.sign(0).pub());
    if(destNode != defaultAddr)
    {
        DEBUGLOG("AAAC DoProtoBlockStatus, destNode:{}, ret:{}, blockHash:{}", destNode, -99, newBlock.hash().substr(0,6));
        DoProtoBlockStatus(blockStatus, destNode);
    }
        
}

ContractPreHashStatus BlockHelper::CheckContractPreHashStatus(const std::string& contractAddr, const std::string& MEMContractPreHash, const uint64_t blockTime, std::string& DBBlockHash)
{
    if(MEMContractPreHash.empty() || contractAddr.empty())
    {
        return ContractPreHashStatus::Err;
    }

    DBReader dbReader;
    std::string DBContractPreHash;
    if (DBStatus::DB_SUCCESS != dbReader.GetLatestUtxoByContractAddr(contractAddr, DBContractPreHash))
    {
        return ContractPreHashStatus::Err;
    }
    if(DBContractPreHash == MEMContractPreHash)
    {
        return ContractPreHashStatus::Normal;
    }

    std::string strPrevBlockHash;
    if(dbReader.GetBlockHashByTransactionHash(DBContractPreHash, strPrevBlockHash) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetBlockHashByTransactionHash failed!");
        return ContractPreHashStatus::Err;
    }

    std::string blockRaw;
    if(dbReader.GetBlockByBlockHash(strPrevBlockHash, blockRaw) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetBlockByBlockHash failed!");
        return ContractPreHashStatus::Err;
    }

    CBlock block;
    if(!block.ParseFromString(blockRaw))
    {
        ERRORLOG("parse failed!");
        return ContractPreHashStatus::Err;
    }

    DBBlockHash = block.hash();

    try
    {
        nlohmann::json jPrevData = nlohmann::json::parse(block.data());

        for (const auto&[key, value] : jPrevData.items())
        {
            if(key == DBContractPreHash)
            {
                for(auto &it : value[Evmone::contractPreHashKeyName].items())
                {
                    if(it.key() == contractAddr && MEMContractPreHash == it.value())
                    {
                        if(blockTime > block.time())
                        {
                            return ContractPreHashStatus::MemBlockException;
                        }
                        else
                        {
                            return ContractPreHashStatus::DbBlockException;
                        }
                    }
                }
                return ContractPreHashStatus::Waiting;
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return ContractPreHashStatus::Err;
    }
    return ContractPreHashStatus::Err;
}
int BlockHelper::checkContractBlockCache(const CBlock& block, const std::string& contractTxPreHash)
{
    auto contractBlockIter = _contractBlocks.find(contractTxPreHash);
    if(contractBlockIter != _contractBlocks.end())
    {
        if(contractBlockIter->second.time() > block.time())
        {
            AddContractBlock(block, contractTxPreHash);
            DEBUGLOG("delete mem oldContractBlock ,contractTxPreHash:{}, oldblockHash:{}, newblockHash:{}", contractTxPreHash, contractBlockIter->second.hash().substr(0,6), block.hash().substr(0,6));
            return 0;
        }
        else
        {
            DEBUGLOG("delete mem newContractBlock ,contractTxPreHash:{}, oldblockHash:{}, newblockHash:{}", contractTxPreHash, contractBlockIter->second.hash().substr(0,6), block.hash().substr(0,6));
            return -1;
        }
    }
    else
    {
        AddContractBlock(block, contractTxPreHash);
    }
    return 0;
}
int BlockHelper::checkContractBlock(const CBlock& block, std::string& contractTxPreHash)
{
    DBReader dbReader;
    uint64_t selfNodeHeight;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(selfNodeHeight))
    {
        DEBUGLOG("Get selfNodeHeight error");
        return -1;
    }

    try
    {
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> contractTxPreHashMap;
        nlohmann::json dataJson = nlohmann::json::parse(block.data());
        for (const auto&[key, value] : dataJson.items())
        {
            for(auto &it : value[Evmone::contractPreHashKeyName].items())
            {
                contractTxPreHashMap[key].push_back({it.key(), it.value()});
            }
        }

        for(auto& iter : contractTxPreHashMap)
        {
            for(auto& preHashPair : iter.second)
            {
                if(contractTxPreHashMap.find(preHashPair.second) != contractTxPreHashMap.end())
                {
                    continue;
                }

                std::string DBBlockHash;
                auto preHashStatus = CheckContractPreHashStatus(preHashPair.first, preHashPair.second, block.time(), DBBlockHash);
                if(preHashStatus == ContractPreHashStatus::Normal)
                {
                    DEBUGLOG("checkContractBlockCache blockHash:{}, contractPrehash:{}", block.hash().substr(0,6), preHashPair.second.substr(0,10));
                    if(checkContractBlockCache(block, preHashPair.second) != 0)
                    {
                        return -2;
                    }
                    continue;
                }
                else if(preHashStatus == ContractPreHashStatus::MemBlockException)
                {
                    DEBUGLOG("contractBlockConflicts DBBlockHash :{}, blockHash:{}", DBBlockHash.substr(0,6), block.hash().substr(0,6));
                    return -3;
                }
                else if(preHashStatus == ContractPreHashStatus::DbBlockException)
                {
                    DEBUGLOG("contractBlock rollback RollBlockHash:{},blockHash:{}", DBBlockHash.substr(0,6), block.hash().substr(0,6));
                    if(DBBlockHash == block.hash())
                    {
                        continue;
                    }
                    auto ret = ca_algorithm::RollBackByHash(DBBlockHash);
                    if (ret != 0)
                    {
                        ERRORLOG("contractBlock rollback hash {} fail, ret:{}", DBBlockHash, ret);
                        return -4;
                    }
                    continue;
                }
                else if(preHashStatus == ContractPreHashStatus::Waiting)
                {
                    if(checkContractBlockCache(block, preHashPair.second) != 0)
                    {
                        return -5;
                    }
                    contractTxPreHash = preHashPair.second;
                    break;
                }
            }

            if(!contractTxPreHash.empty())
            {
                if(block.height() <= selfNodeHeight + 3)
                {
                    DEBUGLOG("_missingContractBlocks.insert height:{}, hash:{}, contractTxPreHash:{}, ", block.height(), block.hash().substr(0,6), contractTxPreHash.substr(0,6));
                    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                    std::unique_lock<std::mutex> locker(_seekMutex);
                    _missingBlocks.insert({contractTxPreHash, nowTime, 1});
                }
                return 0;
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -6;
    }
    return 0;
}
void BlockHelper::AddPendingBlock(const CBlock& block)
{
    DBReader dbReader;
    uint64_t selfNodeHeight;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(selfNodeHeight))
    {
        DEBUGLOG("Get selfNodeHeight error");
        return;
    }

    uint64_t blockHeight = block.height();
    INFOLOG("_pendingBlocks height:{}, hash:{}", blockHeight, block.hash().substr(0,6));
    if(_pendingBlocks.size() < 1000)
    {
        _pendingBlocks[blockHeight].insert({block.prevhash(), block}); 
    }
    
    if(blockHeight > selfNodeHeight + 3)
    {
        return;
    }

    DEBUGLOG("_missingBlocks.insert height:{}, hash:{}, prevhash:{}, ", blockHeight, block.hash().substr(0,6), block.prevhash().substr(0,6));
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::unique_lock<std::mutex> locker(_seekMutex);
    _missingBlocks.insert({block.prevhash(), nowTime, 0});
}

void BlockHelper::AddContractBlock(const CBlock& block, const std::string& contractTxPreHash)
{
    INFOLOG("_contractBlocks height:{}, hash:{}, contractTxPreHash:{}", block.height(), block.hash().substr(0,6), contractTxPreHash);
    if(_contractBlocks.size() < 1000)
    {
        _contractBlocks[contractTxPreHash] = block;
    }
    return;
}

void BlockHelper::AddBroadcastBlock(const CBlock& block, bool status)
{

    std::lock_guard<std::mutex> lock_low1(_helperMutexLow1);
    std::lock_guard<std::mutex> lock(_helperMutex);
    
    if(_duplicateChecker.find(block.hash()) != _duplicateChecker.end())
    {
        DEBUGLOG("+++Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
        if(status) _duplicateChecker[block.hash()] = {status, block.time()};
        return;
    }

    if(_doubleSpendBlocks.find(block.hash()) != _doubleSpendBlocks.end())
    {
        DEBUGLOG("_doubleSpendBlocks blockHash:{}", block.hash().substr(0, 6));
        return;
    }

    DEBUGLOG("Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
    _duplicateChecker[block.hash()] = {status, block.time()};

    for (auto it = _broadcastBlocks.begin(); it != _broadcastBlocks.end(); ++it) 
    {
        auto &curr_block = *it;
        bool ret = Checker::CheckConflict(curr_block, block);
        if(ret)
        {
            if((curr_block.height() == block.height() && curr_block.time() <= block.time()) || (curr_block.height() < block.height()))
            {
                if(status)
                {
                    MagicSingleton<TaskPool>::GetInstance()->CommitCaTask(std::bind(&BlockHelper::MakeTxStatusMsg, this, curr_block, block));
                } 
                INFOLOG("block {} has conflict, discard!", block.hash().substr(0,6));
                return;
            }
            else
            {
                auto result = _duplicateChecker.find(curr_block.hash());
                if(result != _duplicateChecker.end() && result->second.first)
                {
                    MagicSingleton<TaskPool>::GetInstance()->CommitCaTask(std::bind(&BlockHelper::MakeTxStatusMsg, this, block, curr_block));
                }
                INFOLOG("blockHash:{}, deleteBlockHash:{}", block.hash().substr(0,6), curr_block.hash().substr(0,6));
                it = _broadcastBlocks.erase(it);
                break;
            }
        }
    }
    DEBUGLOG("_broadcastBlocks _broadcastBlocks.size:{}", _broadcastBlocks.size());
    
    DBReader dbReader;
    uint64_t selfNodeHeight;
    auto res = dbReader.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != res)
    {
        DEBUGLOG("Get selfNodeHeight error");
        return;
    }

    std::string blockRaw;
    auto prevHashStatus = dbReader.GetBlockByBlockHash(block.prevhash(), blockRaw);
    bool ContractPrevHashStatus = true;
    std::string contractPrevBlockHash;
    
    bool isContractBlock = false;
    if(IsContractBlock(block))
    {
        std::string contractTxPreHash;
        auto ret = checkContractBlock(block, contractTxPreHash);
        if(ret < 0)
        {
            DEBUGLOG("checkContractBlock error, contractBlockHash:{}, contractTxPreHash:{}",block.hash().substr(0,6), contractTxPreHash);
            return;
        }
        if(ret == 0 && !contractTxPreHash.empty())
        {
            ContractPrevHashStatus = false;
        }
        isContractBlock = true;
    }

    if(!isContractBlock)
    {
        if(DBStatus::DB_SUCCESS == prevHashStatus)
        {
            INFOLOG("_broadcastBlocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
            if(block.height() <= selfNodeHeight + 1000)
            {
                _broadcastBlocks.insert(block);
            }
        }
        else
        {
            AddPendingBlock(block);
        }
    }

    if(isContractBlock)
    {
        if(ContractPrevHashStatus)
        {
            if(DBStatus::DB_SUCCESS == prevHashStatus)
            {
                INFOLOG("_broadcastBlocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
                if(block.height() <= selfNodeHeight + 1000)
                {
                    _broadcastBlocks.insert(block);
                }
            }
            else
            {
                AddPendingBlock(block);
            }
        }
    }
}

void BlockHelper::AddSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData, global::ca::SaveType type)
{
    DEBUGLOG("AddSyncBlock syncBlockData.size(): {}", syncBlockData.size());
    std::lock_guard<std::mutex> lock(_helperMutex);
    for(const auto&[key,value]:syncBlockData)
    {
        for(const auto& sit: value)
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(sit.hash());
            _syncBlocks.insert(std::move(sit));
        }
    }
    g_syncType = type;
}

void BlockHelper::AddFastSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &syncBlockData, global::ca::SaveType type)
{
    std::lock_guard<std::mutex> lock(_helperMutex);
    for (auto it = syncBlockData.begin(); it != syncBlockData.end(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(sit->hash());
            _fastSyncBlocks.insert(*sit);
        }
    }
    g_syncType = type;
}

void BlockHelper::AddRollbackBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &rollback_block_data)
{
    std::lock_guard<std::mutex> lock(_helperMutex);
    _rollbackBlocks = rollback_block_data;
}

void BlockHelper::AddMissingBlock(const CBlock& block)
{
    std::lock_guard<std::mutex> lock(_helperMutex);
    MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
    _utxoMissingBlocks.push_back(block);
}

bool BlockHelper::ObtainChainHeight(uint64_t& chainHeight)
{
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return false;
    }

    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    // uint64_t nodeAmount = nodes.size();
    if (nodes.empty())
    {
        DEBUGLOG("nodes.empty() == true, chainHeight:{}", chainHeight);
        chainHeight = top;
        return true;
    }

    std::vector<Node> qualifyingNode;

    if(top < global::ca::kMinUnstakeHeight)
    {
        qualifyingNode = nodes;
    }
    else
    {
        for (const auto &node : nodes)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
            if (stakeTime > 0 && ret == 0)
            {
                qualifyingNode.push_back(node);
            }
        }
        if(qualifyingNode.empty())
        {
            DEBUGLOG("qualifyingNode.empty() == true, chainHeight:{}", chainHeight);
            chainHeight = top;
            return true;
        }
    }


    std::vector<uint64_t> nodeHeights;
    for (auto &node : qualifyingNode)
    {
        nodeHeights.push_back(node.height);
    }
    std::sort(nodeHeights.begin(), nodeHeights.end());
    //const static int malicious_node_tolerated_amount = 25;
    double sampleRate = 0.75;

    int verifyNum = nodeHeights.size() * sampleRate;
    if (verifyNum >= nodeHeights.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verifyNum, nodeHeights.size());
        return false;
    }
    chainHeight = nodeHeights.at(verifyNum);

    return true;
}


void BlockHelper::RollbackTest()
{
    std::cout << "1.Rollback block from Height" << std::endl;
    std::cout << "2.Rollback block from Hash" << std::endl;
    std::cout << "0.Quit" << std::endl;

    int iSwitch = 0;
    std::cin >> iSwitch;
    switch (iSwitch)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            unsigned int height = 0;
            std::cout << "Rollback block height: ";
            std::cin >> height;
            std::lock_guard<std::mutex> lock(_helperMutex);
            auto ret = ca_algorithm::RollBackToHeight(height);
            if (0 != ret)
            {
                std::cout << std::endl
                          << "ca_algorithm::RollBackToHeight:" << ret << std::endl;
                break;
            }
            MagicSingleton<PeerNode>::GetInstance()->SetSelfHeight();
            break;
        }
        case 2:
        {
            std::map<uint64_t, std::set<CBlock, CBlockCompare>> rollBackMap;
            std::string hash;
            std::cout << "Enter rollback block hash, Enter 0 exit" << std::endl;
            std::cin >> hash;
            while(hash != "0")
            {
                CBlock block;
                std::string blockStr;
                DBReader dbReader;
                if(DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(hash, blockStr))
                {
                    std::cout << "RollbackContractBlock GetBlockByBlockHash error" << std::endl;
                    return;
                }
                block.ParseFromString(blockStr);
                rollBackMap[block.height()].insert(block);
                hash.clear();
                std::cout << "Enter rollback block hash, Enter 0 exit" << std::endl;
                std::cin >> hash;
            }
            if(!rollBackMap.empty())
            {
                MagicSingleton<BlockHelper>::GetInstance()->AddRollbackBlock(rollBackMap);
            }
            return;
        }
        default:
        {
            std::cout << "Input error !" << std::endl;
            break;
        }
    }
}

