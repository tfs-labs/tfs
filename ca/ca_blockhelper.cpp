#include "ca_blockhelper.h"

#include "utils/MagicSingleton.h"
#include "include/ScopeGuard.h"
#include "net/net_api.h"
#include "db/db_api.h"
#include "ca_algorithm.h"
#include "ca_blockcache.h"
#include "ca_block_http_callback.h"
#include "ca_transaction_cache.h"
#include "ca_transaction.h"
#include "common.pb.h"
#include "common/global_data.h"
#include "utils/AccountManager.h"
#include "utils/VRF.hpp"
#include "ca_checker.h"
#include "utils/TFSbenchmark.h"
#include "common/time_report.h"
#include "common/task_pool.h"
#include "ca/ca_test.h"
#include <cstdint>
#include <utility>

static global::ca::SaveType sync_type = global::ca::SaveType::Unknow;

BlockHelper::BlockHelper() : missing_prehash(false){}

int GetUtxoFindNode(uint32_t num, uint64_t chain_height, const std::vector<std::string> &pledge_addr,
                            std::vector<std::string> &send_node_ids)
{
    return SyncBlock::GetSyncNodeSimplify(num, chain_height, pledge_addr, send_node_ids);
}

int SendBlockByUtxoReq(const std::string &utxo)
{
    ON_SCOPE_EXIT{
        MagicSingleton<BlockHelper>::GetInstance()->PopMissUTXO();
    };

    DEBUGLOG("begin get missing block utxo {}",utxo);
    std::vector<std::string> send_node_ids;

    uint64_t chain_height = 0;
    if(!BlockHelper::obtain_chain_height(chain_height))
    {
        return -1;
    }
    uint64_t self_node_height = 0;
    std::vector<std::string> pledge_addr;
    {
        DBReader db_reader;
        auto status = db_reader.GetBlockTop(self_node_height);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }
        status = db_reader.GetStakeAddress(pledge_addr);
        if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
        {
            return -3;
        }
    }
    
    if (GetUtxoFindNode(10, chain_height, pledge_addr, send_node_ids) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msg_id;
    size_t send_num = send_node_ids.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, send_num * 0.8, msg_id))
    {
        return -5;
    }
    std::string self_node_id = net_get_self_node_id();
    for (auto &node_id : send_node_ids)
    {
        GetBlockByUtxoReq req;
        req.set_addr(self_node_id);
        req.set_utxo(utxo);
        req.set_msg_id(msg_id);
        net_send_message<GetBlockByUtxoReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(!SyncBlock::check_byzantine(send_num, ret_datas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
            return -6;
        }
    }
    GetBlockByUtxoAck ack;
    std::string block_raw = "";
    for(auto iter = ret_datas.begin(); iter != ret_datas.end(); iter++)
    {
        ack.Clear();
        if (!ack.ParseFromString(*iter))
        {
            continue;
        }
        if(iter == ret_datas.begin())
        {
            block_raw = ack.block_raw();
        }
        else
        {
            if( block_raw != ack.block_raw())
            {
                ERRORLOG("get different block");
                return -7;
            }
        }
    }

    if(block_raw == "")
    {
        ERRORLOG("block_raw is empty!");
        return -8;
    }

    CBlock block;
    if(!block.ParseFromString(block_raw))
    {
        ERRORLOG("block_raw parse fail!");
        return -9;
    }
    
    MagicSingleton<BlockHelper>::GetInstance()->AddMissingBlock(block);
    
    return 0;
}

int SendBlockByUtxoAck(const std::string &utxo, const std::string &addr, const std::string &msg_id)
{
    DEBUGLOG("handle get missing block utxo {}",utxo);
    DBReader db_reader;

    std::string strBlockHash = "";
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashByTransactionHash(utxo, strBlockHash))
    {
        ERRORLOG("GetBlockHashByTransactionHash fail!");
        return -1;
    }

    std::string blockstr = "";
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockByBlockHash(strBlockHash, blockstr))
    {
        ERRORLOG("GetBlockByBlockHash fail!");
        return -2;
    }
    if(blockstr == "")
    {
        ERRORLOG("blockstr is empty fail!");
        return -3;
    }
    GetBlockByUtxoAck ack;
    ack.set_addr(net_get_self_node_id());
    ack.set_utxo(utxo);
    ack.set_block_raw(blockstr);
    ack.set_msg_id(msg_id);

    net_send_message<GetBlockByUtxoAck>(addr, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}

int HandleBlockByUtxoReq(const std::shared_ptr<GetBlockByUtxoReq> &msg, const MsgData &msgdata)
{
    SendBlockByUtxoAck(msg->utxo(), msg->addr(),msg->msg_id());
    return 0;
}

int HandleBlockByUtxoAck(const std::shared_ptr<GetBlockByUtxoAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int SendBlockByHashReq(const std::map<std::string, bool> &missingHashs)
{
    DEBUGLOG("SendBlockByHashReq Start");
    std::vector<std::string> send_node_ids;

    uint64_t chain_height = 0;
    if(!BlockHelper::obtain_chain_height(chain_height))
    {
        return -1;
    }
    uint64_t self_node_height = 0;
    std::vector<std::string> pledge_addr;
    {
        DBReader db_reader;
        auto status = db_reader.GetBlockTop(self_node_height);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }
        status = db_reader.GetStakeAddress(pledge_addr);
        if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
        {
            return -3;
        }
    }
    
    if (GetUtxoFindNode(10, chain_height, pledge_addr, send_node_ids) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msg_id;
    size_t send_num = send_node_ids.size();
    if (!GLOBALDATAMGRPTR.CreateWait(30, send_num * 0.8, msg_id))
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

    std::string self_node_id = net_get_self_node_id();
    req.set_addr(self_node_id);
    req.set_msg_id(msg_id);

    for (auto &node_id : send_node_ids)
    {
        net_send_message<GetBlockByHashReq>(node_id, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        if(!SyncBlock::check_byzantine(send_num, ret_datas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", send_num, ret_datas.size());
            return -6;
        }
    }

    GetBlockByHashAck ack;
    uint32_t succent_count = 0;
    std::map<std::string, std::pair<std::string, uint32_t>> seek_block_hashes;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        succent_count++;
        for (auto &block : ack.blocks())
        {
            if (seek_block_hashes.end() == seek_block_hashes.find(block.hash()))
            {
                seek_block_hashes[block.hash()].first = std::move(block.block_raw());
                seek_block_hashes[block.hash()].second = 1;
            }
            seek_block_hashes[block.hash()].second++;
        }
    }

    uint32_t verify_num = succent_count / 5 * 4;
    std::vector<std::pair<CBlock,std::string>> seek_blocks;
    for(const auto& it : seek_block_hashes)
    {
        if(it.second.second > verify_num)
        {
            CBlock block;
            if(!block.ParseFromString(it.second.first))
            {
                ERRORLOG("block_raw parse fail!");
                return -7;
            }
            seek_blocks.push_back({block, it.first});
        }
    }

    MagicSingleton<taskPool>::GetInstance()->commit_syncBlock_task(std::bind(&BlockHelper::AddSeekBlock, MagicSingleton<BlockHelper>::GetInstance().get(), seek_blocks));
    return 0;
}

int SendBlockByHashAck(const std::map<std::string, bool> &missingHashs, const std::string &addr, const std::string &msg_id)
{
    DBReader db_reader;
    GetBlockByHashAck ack;
    for(const auto& it : missingHashs)
    {
        std::string strBlockHash = "";
        if(it.second)
        {
            if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashByTransactionHash(it.first, strBlockHash))
            {
                ERRORLOG("GetBlockHashByTransactionHash fail!");
                return -1;
            }
        }
        else
        {
            strBlockHash = it.first;
        }
        std::string blockstr = "";
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockByBlockHash(strBlockHash, blockstr))
        {
            ERRORLOG("GetBlockByBlockHash fail!");
            return -2;
        }
        if(blockstr == "")
        {
            ERRORLOG("blockstr is empty fail!");
            return -3;
        }
        auto block = ack.add_blocks();
        block->set_hash(it.first);
        block->set_tx_or_block(it.second);
        block->set_block_raw(blockstr);
    }
    
    ack.set_addr(net_get_self_node_id());
    ack.set_msg_id(msg_id);

    net_send_message<GetBlockByHashAck>(addr, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0; 
}

int HandleBlockByHashReq(const std::shared_ptr<GetBlockByHashReq> &msg, const MsgData &msgdata)
{
    std::map<std::string, bool> missingHashs;
    for(const auto& it : msg->missinghashs())
    {
        missingHashs[it.hash()] = it.tx_or_block();
    }
    SendBlockByHashAck(missingHashs, msg->addr(), msg->msg_id());
    return 0;
}

int HandleBlockByHashAck(const std::shared_ptr<GetBlockByHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(), msg->SerializeAsString());
    return 0;
}

int BlockHelper::VerifyFlowedBlock(const CBlock& block, BlockStatus* blockStatus)
{
    if(block.version() != 0)
	{
		return -1;
	}
    bool isVerify = true;
    if(block.sign_size() == 0)
    {
        isVerify = false;
    }
    uint64_t block_height = block.height();
    
	std::string block_hash = block.hash();
	if(block_hash.empty())
    {
        return -2;
    }
    
    DBReadWriter db_writer;
	uint64_t node_height = 0;
    if (DBStatus::DB_SUCCESS != db_writer.GetBlockTop(node_height))
    {
        return -3;
    }

    if ( (node_height  > 9) && (node_height - 9 > block_height))
	{
        ERRORLOG("VerifyHeight fail!!,block_height:{}, node_height:{}, isVerify:{}",block_height, node_height, isVerify);
		return -4;
	}
	else if (node_height + 1 < block_height)
	{
        ERRORLOG("VerifyHeight fail!!,block_height:{}, node_height:{}, isVerify:{}",block_height, node_height, isVerify);
		return -5;
	}

    uint64_t chain_height = 0;
    if(!obtain_chain_height(chain_height))
    {
        return -7;
    }

    //Increase the height and time of the block within a certain height without judgment
    if(chain_height > global::ca::kMinUnstakeHeight)
    {
        uint64_t current_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        const static uint64_t stability_time = 60 * 1000000;
        if(block_height < (chain_height - 10) && current_time - block.time() > stability_time)
        {
            DEBUGLOG("broadcast block overtime , block height{}, block hash{}",block_height,block_hash);
            return -8;
        }
    }
    
	std::string strTempHeader;

	DBStatus status = db_writer.GetBlockByBlockHash(block_hash, strTempHeader);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -9;
	}

	if (strTempHeader.size() != 0)
	{
		return -10;
	}

	std::string strPrevHeader;
	status = db_writer.GetBlockByBlockHash(block.prevhash(), strPrevHeader);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -11;
	}

	if (strPrevHeader.size() == 0)
	{
        return -12;
	}
    std::vector<CTransaction> DoubleSpentTransactions;
    Checker::CheckConflict(block, DoubleSpentTransactions);
    if(!DoubleSpentTransactions.empty())
    {
        if(blockStatus != NULL)
        {
            for(const auto& tx : DoubleSpentTransactions)
            {
                auto txStatus = blockStatus->add_txstatus();
                txStatus->set_txhash(tx.hash());
                txStatus->set_status(global::ca::DoubleSpend::SingleBlock);
            }
            
        }
        std::ostringstream filestream;
        printBlock(block,true,filestream);

        std::string test_str = filestream.str();
        DEBUGLOG("DoubleSpentTransactions block --> {}", test_str);
        return -13;
    }
    auto start_t5 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    DEBUGLOG("verifying block {} , isVerify:{}", block_hash.substr(0, 6), isVerify);
	auto ret = ca_algorithm::VerifyBlock(block, false, true, isVerify, blockStatus);
	if (0 != ret)
	{
		ERRORLOG("verify block fail ret:{}:{}:{}", ret, block_height, block_hash);
		return -14;
	}
    
    if(!isVerify){
        auto end_t5 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        auto t5 = end_t5 - start_t5;
        MagicSingleton<TFSbenchmark>::GetInstance()->SetByBlockHash(block.hash(), &t5, 3);
    }
    return 0;
}

int BlockHelper::SaveBlock(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean)
{    
    DBReadWriter* db_writer_ptr = new DBReadWriter();
    ON_SCOPE_EXIT{
        if (db_writer_ptr != nullptr)
        {
            delete db_writer_ptr;
        }
        if (saveType == global::ca::SaveType::Broadcast)
        {
            DEBUGLOG("SAVETEST hash: {} , BlockHelper::SaveBlock end: {}", block.hash(), MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
        }
    };

    int ret = 0;
    std::string block_raw;
    std::string block_hash = block.hash();
    ret = db_writer_ptr->GetBlockByBlockHash(block.hash(), block_raw);
    if (DBStatus::DB_SUCCESS == ret)
    {
        INFOLOG("BlockHelper block {} already in saved , skip",block.hash().substr(0, 6));
        return 0;
    }

    ret = PreSaveProcess(block, saveType, obtainMean);
    if (ret < 0)
    {
        delete db_writer_ptr;
        db_writer_ptr = nullptr;
        return ret;
    }
    DEBUGLOG("PreSaveProcess doubleSpendCheck ret:{}", ret);
    if(ret == 1 || ret == 2) //doubleSpend error
    {
        return 0;
    }
    
    ResetMissingPrehash();
    uint64_t block_height = block.height();
    ret = ca_algorithm::SaveBlock(*db_writer_ptr, block, saveType, obtainMean);
    if (0 != ret)
    {
        delete db_writer_ptr;
        db_writer_ptr = nullptr;
        ERRORLOG("save block ret:{}:{}:{}", ret, block_height, block_hash);
        if (missing_prehash)
        {
            ResetMissingPrehash();
            SyncBlock::SetFastSync(block_height - 1);
            return -4;
        }
        if(!missing_utxos.empty())
        {
            GetMissBlock();
            return -5;
        }
        return -6;
    }
    if(DBStatus::DB_SUCCESS == db_writer_ptr->TransactionCommit())
    {        
        INFOLOG("save block ret:{}:{}:{}", ret, block_height, block_hash);
        auto startTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        PostSaveProcess(block);
        auto endTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp(); 
        postCommitCost += (endTime - startTime);
        postCommitCount++;
    }
    else
    {
        ERRORLOG("Transaction commit fail");
        return -7;
    }
    return 0;
}

bool BlockHelper::VerifyHeight(const CBlock& block, uint64_t ownblockHeight)
{
    DBReader db_reader;

	unsigned int preheight = 0;
	if (DBStatus::DB_SUCCESS != db_reader.GetBlockHeightByBlockHash(block.prevhash(), preheight))
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

        global::ca::TxType tx_type;
        tx_type = (global::ca::TxType)tx.txtype();

        if (global::ca::TxType::kTxTypeUnstake == tx_type || global::ca::TxType::kTxTypeDisinvest == tx_type)
        {
            DBReadWriter db_writer;
            std::vector<std::string> block_hashs;
            uint64_t block_height = block.height();
            if (DBStatus::DB_SUCCESS != db_writer.GetBlockHashsByBlockHeight(block_height, block_hashs))
            {
                ERRORLOG("fail to get block hash at height {}", block_height);
                continue;
            }
            std::vector<std::string> blocks;
            if (DBStatus::DB_SUCCESS != db_writer.GetBlocksByBlockHash(block_hashs, blocks))
            {
                ERRORLOG("fail to get block at height {}", block_height);
                continue;
            }
            
            for (auto &block_raw : blocks)
            {                                                                               
                CBlock height_block;                
                if (!height_block.ParseFromString(block_raw))
                {
                    ERRORLOG("block parse fail!");
                    continue;
                }
                if(height_block.hash() == block.hash())
                {
                    continue;
                }
                for (int i = 0; i < height_block.txs_size(); i++)
                {
                    CTransaction height_tx = height_block.txs(i);
                    bool isNeedAgent = TxHelper::IsNeedAgent(tx);

                    for (int i = (isNeedAgent ? 0 : 1); i < block.sign_size(); ++i)
                    {
                        std::string sign_addr = GetBase58Addr(block.sign(i).pub(), Base58Ver::kBase58Ver_Normal);
                        if(std::find(tx.utxo().owner().begin(), tx.utxo().owner().end(), sign_addr) != tx.utxo().owner().end())
                        {
                            int ret = ca_algorithm::RollBackByHash(height_block.hash());
                            if (ret != 0)
                            {
                                ERRORLOG("rollback hash {} fail, ret: ", height_block.hash(), ret);
                            }
                        }                    
                    }
                }

            }
        }
    }
}

std::pair<doubleSpendType, CBlock> BlockHelper::DealDoubleSpend(const CBlock& block, const CTransaction& tx, const std::string& missing_utxo)
{
    uint64_t block_height = block.height();
    std::string block_hash = block.hash();

    DBReader db_reader;
    uint64_t node_height = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(node_height))
    {
        ERRORLOG("GetBlockTop fail!!!");
        return {doubleSpendType::err,{}};
    }
    
    std::set<std::string> SetOwner(tx.utxo().owner().begin(), tx.utxo().owner().end());
    std::vector<std::string> block_hashes;
    if(block_height > node_height)
    {
        DEBUGLOG("block_height:({}) > node_height:({})", block_height, node_height);
        return {doubleSpendType::err,{}};
    }
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(block_height, node_height, block_hashes))
    {
        ERRORLOG("GetBlockHashesByBlockHeight fail!!!");
        return {doubleSpendType::err,{}};
    }
    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlocksByBlockHash(block_hashes, blocks))
    {
        ERRORLOG("GetBlocksByBlockHash fail!!!");
        return {doubleSpendType::err,{}};
    }

    for (auto &PBlock_str : blocks)
    {
        CBlock PBlock;
        if(PBlock.ParseFromString(PBlock_str))
        {
            for(const auto& PTx : PBlock.txs())
            {
                if((global::ca::TxType)PTx.txtype() != global::ca::TxType::kTxTypeTx)
                {
                    continue;                              
                }
                for (auto &PVin : PTx.utxo().vin())
                {
                    std::string PVinAddr = GetBase58Addr(PVin.vinsign().pub());
                    if(SetOwner.find(PVinAddr) != SetOwner.end())
                    {
                        for (auto & PPrevout : PVin.prevout())
                        {
                            std::string PUtxo = PPrevout.hash();
                            if(missing_utxo == PUtxo)
                            {
                                DEBUGLOG("DoubleSpend, block_height:{}, PBlock.height:{} , block_time:{}, PBlock.time:{}, blockHash:{}, PBlockHash:{}", block_height, PBlock.height() , block.time() , PBlock.time(), block.hash().substr(0,6), PBlock.hash().substr(0,6));
                                //same height doublespend
                                if((block_height == PBlock.height() && block.time() >= PBlock.time()) || block_height > PBlock.height())
                                {
                                    DEBUGLOG("DoubleSpend_blocks.insert(block_hash):{}", block_hash.substr(0,6));
                                    DoubleSpend_blocks.insert({block_hash, block});
                                    checkDoubleBlooming({doubleSpendType::newDoubleSpend, std::move(PBlock)}, block);
                                    return {doubleSpendType::newDoubleSpend,std::move(PBlock)};
                                }
                                else
                                {
                                    DEBUGLOG("DoubleSpend_blocks PBlock roll oldBackHash:{} at newBlockHash:{}", PBlock.hash().substr(0,6), block_hash.substr(0,6));
                                    auto ret = ca_algorithm::RollBackByHash(PBlock.hash());
                                    if (ret != 0)
                                    {
                                        ERRORLOG("PBlock rollback hash {} fail, ret:{}", PBlock.hash(), ret);
                                        return {doubleSpendType::err,{}};
                                    }
                                    checkDoubleBlooming({doubleSpendType::oldDoubleSpend, std::move(PBlock)}, block);
                                    return {doubleSpendType::oldDoubleSpend,std::move(PBlock)};
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    DEBUGLOG("PBlock Not found DoubleSpend_blocks.insert(block_hash):{}", block_hash);
    DoubleSpend_blocks.insert({block_hash, block});
    return {doubleSpendType::invalidDoubleSpend,{}};
}

int BlockHelper::PreSaveProcess(const CBlock& block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean)
{
    uint64_t block_height = block.height();
    std::string block_hash = block.hash();
    if(saveType == global::ca::SaveType::SyncNormal)
    {
        DEBUGLOG("verifying block {}", block_hash.substr(0, 6));
        ResetMissingPrehash();
        auto ret = ca_algorithm::VerifyBlock(block, true, false);
        if (0 != ret)
        {
            ERRORLOG("verify block ret:{}:{}:{}", ret, block_height, block_hash);
            if (missing_prehash)
            {
                ResetMissingPrehash();
                SyncBlock::SetFastSync(block_height - 1);
                return -1;
            }
            if(!missing_utxos.empty())
            {
                GetMissBlock();
                return -2;
            }
            return -3;
        }
    }
    else if(saveType == global::ca::SaveType::Broadcast)
    {
        DBReader db_reader;
        uint64_t node_height = 0;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(node_height))
        {
            return -1;
        }
        if(obtainMean == global::ca::BlockObtainMean::Normal && block_height + 50 < node_height)
        {
            DEBUGLOG("block_height + 50 < node_height");
            return -2;
        }

        if(ca_algorithm::VerifyPreSaveBlock(block) < 0)
        {
            ERRORLOG("Verify PreSave Block fail!");
            return -9;
        }
        
        for (auto& tx : block.txs())
        {
            if (GetTransactionType(tx) != kTransactionType_Tx)
            {
                continue;
            }
            std::string missing_utxo;
            int ret = ca_algorithm::DoubleSpendCheck(tx, false, &missing_utxo);
            if (0 != ret)
            {
                if(ret == -5 || ret == -7 || ret == -8 && !missing_utxo.empty())
                {
                    std::string blockHash;
                    if(db_reader.GetBlockHashByTransactionHash(missing_utxo, blockHash) == DBStatus::DB_SUCCESS)//DoubleSpend
                    {
                        DEBUGLOG("DoubleSpendCheck fail!! <utxo>: {}, ", missing_utxo);
                        auto doubleSpendInfo = DealDoubleSpend(block, tx , missing_utxo);
                        if(doubleSpendInfo.first == doubleSpendType::newDoubleSpend)
                        {
                            return -3;
                        }
                        else if(doubleSpendInfo.first == doubleSpendType::oldDoubleSpend)
                        {
                            DEBUGLOG("oldDoubleSpend,blockHash:{}, txHash:{}", block.hash().substr(0,6), tx.hash().substr(0,10));
                            continue;
                        }
                        else if(doubleSpendInfo.first == doubleSpendType::invalidDoubleSpend)
                        {
                            return -5;
                        }
                        else
                        {
                            return -6;
                        }
                    }
                    else
                    {
                        DEBUGLOG("not found!! <utxo>: {}, ", missing_utxo);
                        uint64_t now_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
                        std::unique_lock<std::mutex> locker(seek_mutex_);
                        missing_blocks.insert({missing_utxo, now_time, 1});
                    }
                }

                auto found = hash_pending_blocks.find(block.hash());
                if(found == hash_pending_blocks.end())
                {
                    hash_pending_blocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp(), block};
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
    for (int i = 0; i < block.txs_size(); i++)
    {
        CTransaction tx = block.txs(i);
        if (GetTransactionType(tx) == kTransactionType_Tx)
        {
            MagicSingleton<VRF>::GetInstance()->removeVrfInfo(tx.hash());
        }
    }
    MagicSingleton<VRF>::GetInstance()->removeVrfInfo(block.hash());
    MagicSingleton<PeerNode>::GetInstance()->set_self_height(block.height());

    // Run http callback
    if (MagicSingleton<CBlockHttpCallback>::GetInstance()->IsRunning())
    {
        MagicSingleton<CBlockHttpCallback>::GetInstance()->AddBlock(block);
    }
    MagicSingleton<CBlockCache>::GetInstance()->Add(block);
}

void BlockHelper::PostSaveProcess(const CBlock &block)
{
    MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapEnd(block.hash());
    MagicSingleton<taskPool>::GetInstance()->commit_ca_task(std::bind(&BlockHelper::PostTransactionProcess, this, block));

    auto found = pending_blocks.find(block.height() + 1);
    if (found != pending_blocks.end())
    {
        auto& blocks = found->second;
        auto target_begin = blocks.lower_bound(block.hash());
        auto target_end = blocks.upper_bound(block.hash());
        for (; target_begin != target_end ; target_begin++)
        {
            DEBUGLOG("pending_blocks Add block height:{}, hash:{}", target_begin->second.height(), target_begin->second.hash());
            SaveBlock(target_begin->second, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::ByPreHash);
        }     
    }
    PostMembershipCancellationProcess(block);
}

int BlockHelper::RollbackBlocks()
{
    if (rollback_blocks.empty())
    {
        return 0;
    }
    auto rollback_top = (--rollback_blocks.end())->first;
    int ret = 0;
    for (auto it = rollback_blocks.rbegin(); it != rollback_blocks.rend(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            DEBUGLOG("roll back {} at height {}", sit->hash(), sit->height());
            ret = ca_algorithm::RollBackByHash(sit->hash());
            if (ret != 0)
            {
                ERRORLOG("rollback hash {} fail, ret: ", sit->hash(), ret);
                return -2;
            }
            
        }
    }
    return 0;
}

void BlockHelper::SetMissingPrehash()
{
    missing_prehash = true;
}

void BlockHelper::ResetMissingPrehash()
{
    missing_prehash = false;
}

void BlockHelper::PushMissUTXO(const std::string& utxo)
{
    std::lock_guard<std::mutex> lock(missing_utxos_mutex);
    missing_utxos.push(utxo);
    if(missing_utxos.size() > max_missing_uxto_size)
    {
        std::stack<std::string>().swap(missing_utxos);
    }
}

bool BlockHelper::GetMissBlock()
{
    std::string utxo;
    {
        std::lock_guard<std::mutex> lock(missing_utxos_mutex);
        if(missing_utxos.empty())
        {
            INFOLOG("utxo is empty!");
            return false;
        }
        utxo = missing_utxos.top();
    }

    auto async_thread = std::thread(SendBlockByUtxoReq, utxo);
	async_thread.detach();
    return true;
}
void BlockHelper::PopMissUTXO()
{
    std::scoped_lock lock(helper_mutex, missing_utxos_mutex);
    if(missing_utxos.empty())
    {
        return;
    }
    missing_utxos.pop();
}

void BlockHelper::Process()
{
    static int count = 0;
    static int broadcast_save_fail_count = 0;
    static bool processing_ = false;
    if(processing_)
    {
        DEBUGLOG("BlockPoll::Process is processing_");
        return;
    }
    processing_ = true;
    std::lock_guard<std::mutex> lock(helper_mutex);
    postCommitCost = 0;
    postCommitCount = 0;
    DBReader db_reader;
    uint64_t node_height = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(node_height))
    {
        return;
    }

    uint64_t now_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();

    ON_SCOPE_EXIT{
        processing_ = false;
        uint64_t newTop = 0;
        DBReader reader;
        if (reader.GetBlockTop(newTop) == DBStatus::DB_SUCCESS)
        {
            if (node_height != newTop)
            {
                NotifyNodeHeightChange();
                DEBUGLOG("NotifyNodeHeightChange update ok.");
            }
        }
        fast_sync_blocks.clear();
        auto begin = pending_blocks.begin();
        std::vector<decltype(begin)> delete_pending_block;
        for(auto iter = begin; iter != pending_blocks.end(); ++iter)
        {
            if (newTop >= iter->first + 10000 )
            {
                delete_pending_block.push_back(iter);
            }

        }

        for (auto pending_iter : delete_pending_block)
        {
            DEBUGLOG("pending_blocks.erase height:{}", pending_iter->first);
            pending_blocks.erase(pending_iter);
        }
        rollback_blocks.clear();
        sync_blocks.clear();
        broadcast_blocks.clear();
        
        for(auto iter = DoubleSpend_blocks.begin(); iter != DoubleSpend_blocks.end();)
        {
            if(now_time >= iter->second.time() + 30 * 1000000ull)
            {
                DEBUGLOG("AAAC DoubleSpend_blocks deleteBlockHash:{}", iter->first);
                DoubleSpend_blocks.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for(auto iter = DuplicateChecker.begin(); iter != DuplicateChecker.end();)
        {
            if(now_time >= iter->second.second + 60 * 1000000)
            {
                DuplicateChecker.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    };


    uint64_t RollbackTimeout = 60 * 1000000;

    std::map<uint64_t, std::vector<std::set<CBlock, CBlockCompare>::iterator>> delete_rollback_blocks;
    for(auto iter = rollback_blocks.begin(); iter != rollback_blocks.end(); ++iter)
    {
        for(auto rollBack = iter->second.begin(); rollBack != iter->second.end(); ++rollBack)
        {
            if(now_time <= RollbackTimeout + rollBack->time())
            {
                delete_rollback_blocks[iter->first].push_back(rollBack);
            }
        }
    }

    for(auto& iter : delete_rollback_blocks)
    {
        if(rollback_blocks.find(iter.first) != rollback_blocks.end())
        {
            for(auto& rollBack : iter.second)
            {
                DEBUGLOG("RollBackBlock No timeout blockHash:{}", rollBack->hash().substr(0,6));// 60S
                rollback_blocks[iter.first].erase(rollBack);
                
            }
        }
        if(rollback_blocks[iter.first].empty())
        {
            DEBUGLOG("rollback_blocks[*{}*].empty()", iter.first);
            rollback_blocks.erase(iter.first);
        }
    }

    int result = RollbackBlocks();
    if(result != 0)
    {
        return;
    }

    uint64_t chain_height = 0;
    if(!obtain_chain_height(chain_height))
    {
        ERRORLOG("fail to get chain height");
        return;
    }

    for(const auto& block : fast_sync_blocks)
    {
        global::ca::BlockObtainMean obtain_mean = global::ca::BlockObtainMean::Normal;
        if (block.height() + 1 == node_height)
        {
            obtain_mean = global::ca::BlockObtainMean::ByPreHash;
        }
        DEBUGLOG("fast_sync_blocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, sync_type, obtain_mean);
        usleep(100000);
        DEBUGLOG("fast_sync save block height: {}\tblock hash: {}\tresult: {}", block.height(), block.hash(), result);
        if(result != 0)
        {
            break;
        }
    }
    for(const auto& block : utxo_missing_blocks)
    {
        DEBUGLOG("utxo_missing_blocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, sync_type, global::ca::BlockObtainMean::ByUtxo);
        if(result != 0)
        {
            if(utxo_missing_blocks.size() > max_missing_block_size)
            {
                utxo_missing_blocks.clear();
            }
            break;
        }
    }
    utxo_missing_blocks.clear();

    for(const auto& block : sync_blocks)
    {
        if(!stop_blocking)
        {
            return;
        }

        DEBUGLOG("chain height: {}, height: {}, sync type: {}", chain_height, block.height(), sync_type);
        DEBUGLOG("sync_blocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, sync_type, global::ca::BlockObtainMean::Normal);
        if(result != 0)
        {
            break;
        }
    }

    for(const auto& block : broadcast_blocks)
    {
        std::string block_raw;
        if (DBStatus::DB_SUCCESS == db_reader.GetBlockByBlockHash(block.hash(), block_raw))
        {
            INFOLOG("block {} already saved", block.hash().substr(0,6));
            continue;
        }
        DEBUGLOG("broadcast_blocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::Normal);
        if(result == 0)
        {
            MagicSingleton<BlockMonitor>::GetInstance()->SendSuccessBlockSituationAck(block);
        }
        else if(result < 0)
        {
            MagicSingleton<BlockMonitor>::GetInstance()->SendFailedBlockSituationAck(block);
            INFOLOG("broadcast_blocks SaveBlock fail!!! result:{} ,BlockHash:{}", result, block.hash().substr(0,6));
        }
    }

    auto begin = hash_pending_blocks.begin();
    auto end = hash_pending_blocks.end();
    std::vector<decltype(begin)> delete_utxo_blocks;
    for(auto iter = begin; iter != end; ++iter)
    {
        if(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp() - iter->second.first > 1 * 60 * 1000000)
        {
            DEBUGLOG("hash_pending_blocks.erase timeout block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            delete_utxo_blocks.push_back(iter);
            continue;
        }
        DEBUGLOG("hash_pending_blocks SaveBlock Hash: {}, height: {}, PreHash:{}", iter->second.second.hash().substr(0, 6), iter->second.second.height(), iter->second.second.prevhash().substr(0, 6));
        int result = SaveBlock(iter->second.second, global::ca::SaveType::Broadcast, global::ca::BlockObtainMean::ByUtxo);
        if(result == 0)
        {
            DEBUGLOG("hash_pending_blocks Add <success> block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            delete_utxo_blocks.push_back(iter);
        }
        else
        {
            DEBUGLOG("hash_pending_blocks Add <fail> block height:{}, hash:{}", iter->second.second.height(), iter->second.second.hash());
        }

    }

    for(auto uxto_block_iter: delete_utxo_blocks)
    {
        hash_pending_blocks.erase(uxto_block_iter);
    }
    
    return;
}

void BlockHelper::SeekBlockThread()
{
    if(missing_blocks.empty())
    {
        DEBUGLOG("missing_blocks.empty() == true");
        return;
    }

    DEBUGLOG("SeekBlockThread start");
    std::map<std::string, bool> missingHashs;
    auto begin = missing_blocks.begin();
    auto end = missing_blocks.end();
    std::vector<decltype(begin)> delete_missing_blocks;

    {
        DBReader db_reader;
        uint64_t now_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        std::unique_lock<std::mutex> locker(seek_mutex_);
        for(auto iter = begin; iter != end; ++iter)
        {
            if(now_time - iter->time_ > 30 * 1000000 || *iter->trigger_count > 3)
            {
                DEBUGLOG("missing_Hash:{}, timeout:({}),*iter->trigger_count:({}),*iter->tx_or_block_:{}", iter->hash_, now_time - iter->time_ > 30 * 1000000, *iter->trigger_count > 3, *iter->tx_or_block_);
                delete_missing_blocks.push_back(iter);
                continue;
            }
            else if(now_time - iter->time_ > 15 * 1000000)
            {
                std::string strBlock;
                if(*iter->tx_or_block_)
                {
                    if (DBStatus::DB_SUCCESS == db_reader.GetBlockHashByTransactionHash(iter->hash_, strBlock))
                    {
                        delete_missing_blocks.push_back(iter);
                        continue;
                    }
                }
                else if(DBStatus::DB_SUCCESS == db_reader.GetBlockByBlockHash(iter->hash_, strBlock))
                {
                    delete_missing_blocks.push_back(iter);
                    continue;
                }

                if(missingHashs.find(iter->hash_) == missingHashs.end())
                {
                    DEBUGLOG("missing_Hash:{},*iter->trigger_count:{},*iter->tx_or_block_:{}", iter->hash_, *iter->trigger_count, *iter->tx_or_block_);
                    missingHashs[iter->hash_] = *(iter->tx_or_block_);
                }
                else
                {
                    //Filtering duplicate hash
                    delete_missing_blocks.push_back(iter);
                }
                *iter->trigger_count = *iter->trigger_count + 1;
            }
            else
            {
                break;
            }
        }

        for(auto iter: delete_missing_blocks)
        {
            DEBUGLOG("missing_blocks.erase_Hash:{}", iter->hash_);
            missing_blocks.erase(iter);
        }
    }

    if(!missingHashs.empty())
    {
        SendBlockByHashReq(missingHashs);
    }
            
}

void BlockHelper::AddSeekBlock(std::vector<std::pair<CBlock,std::string>>& seek_blocks)
{
    std::lock_guard<std::mutex> lock(helper_mutex);
    for(const auto &iter : seek_blocks)
    {
        auto& block = iter.first;
        auto found = hash_pending_blocks.find(block.hash());
        if(found == hash_pending_blocks.end())
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
            hash_pending_blocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp(), block};
        }

        DEBUGLOG("AddSeekBlock missing_block_hash:{}, tx_or_block_hash:{}", block.hash(), iter.second); 
    }
}

void BlockHelper::checkDoubleBlooming(const std::pair<doubleSpendType, CBlock>& doubleSpendInfo, const CBlock &block)
{
    if(doubleSpendInfo.first == doubleSpendType::newDoubleSpend)
    {
        auto found = DuplicateChecker.find(block.hash());
        DEBUGLOG("doubleSpendType:{}, status:{}, blockHash:{}", doubleSpendInfo.first, found->second.first, block.hash().substr(0,6));
        if(found != DuplicateChecker.end() && found->second.first)
        {
            makeTxStatusMsg(doubleSpendInfo.second, block);
            return;
        }
    }  
    else if(doubleSpendInfo.first == doubleSpendType::oldDoubleSpend)
    {
        auto found = DuplicateChecker.find(doubleSpendInfo.second.hash());
        DEBUGLOG("doubleSpendType:{}, status:{}, blockHash:{}", doubleSpendInfo.first, found->second.first, block.hash().substr(0,6));
        if(found != DuplicateChecker.end() && found->second.first)
        {
            makeTxStatusMsg(block, doubleSpendInfo.second);
            return;
        }
    }
    return;
}

void BlockHelper::makeTxStatusMsg(const CBlock &oldBlock, const CBlock &newBlock)
{
    DEBUGLOG("AAAC makeTxStatusMsg oldBlock:{}, newBlock:{}", oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
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
                DEBUGLOG("AAAC makeTxStatusMsg oldBlocktx1:{}, newBlocktx2:{}", tx1.hash().substr(0,10), tx2.hash().substr(0,10));
                auto txStatus = blockStatus.add_txstatus();
                txStatus->set_txhash(tx2.hash());
                txStatus->set_status(global::ca::DoubleSpend::DoubleBlock);
            }
        }
    }

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    blockStatus.set_blockhash(newBlock.hash());
    blockStatus.set_status(-99);
    blockStatus.set_id(net_get_self_node_id());
    std::string destNode = GetBase58Addr(newBlock.sign(0).pub());
    if(destNode != defaultBase58Addr)
    {
        DEBUGLOG("AAAC doProtoBlockStatus, destNode:{}, ret:{}, blockHash:{}", destNode, -99, newBlock.hash().substr(0,6));
        doProtoBlockStatus(blockStatus, destNode);
    }
        
}

void BlockHelper::AddBroadcastBlock(const CBlock& block, bool status)
{

    std::lock_guard<std::mutex> lock_low1(helper_mutex_low1);
    std::lock_guard<std::mutex> lock(helper_mutex);
    
    if(DuplicateChecker.find(block.hash()) != DuplicateChecker.end())
    {
        DEBUGLOG("+++Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
        if(status) DuplicateChecker[block.hash()] = {status, block.time()};
        return;
    }

    if(DoubleSpend_blocks.find(block.hash()) != DoubleSpend_blocks.end())
    {
        DEBUGLOG("DoubleSpend_blocks block_hash:{}", block.hash().substr(0, 6));
        return;
    }

    DEBUGLOG("Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
    DuplicateChecker[block.hash()] = {status, block.time()};

    for (auto it = broadcast_blocks.begin(); it != broadcast_blocks.end(); ++it) 
    {
        auto &curr_block = *it;
        bool ret = Checker::CheckConflict(curr_block, block);
        if(ret)   // Conflicting
        {
            if((curr_block.height() == block.height() && curr_block.time() <= block.time()) || (curr_block.height() < block.height()))
            {
                if(status)
                {
                    MagicSingleton<taskPool>::GetInstance()->commit_ca_task(std::bind(&BlockHelper::makeTxStatusMsg, this, curr_block, block));
                } 
                INFOLOG("block {} has conflict, discard!", block.hash().substr(0,6));
                return;
            }
            else
            {
                auto result = DuplicateChecker.find(curr_block.hash());
                if(result != DuplicateChecker.end() && result->second.first)
                {
                    MagicSingleton<taskPool>::GetInstance()->commit_ca_task(std::bind(&BlockHelper::makeTxStatusMsg, this, block, curr_block));
                }
                INFOLOG("blockHash:{}, deleteBlockHash:{}", block.hash().substr(0,6), curr_block.hash().substr(0,6));
                it = broadcast_blocks.erase(it);
                break;
            }
        }
    }
    DEBUGLOG("broadcast_blocks broadcast_blocks.size:{}", broadcast_blocks.size());
    std::string block_raw;
    DBReader db_reader;

    uint64_t self_node_height;
    auto res = db_reader.GetBlockTop(self_node_height);
    if (DBStatus::DB_SUCCESS != res)
    {
        DEBUGLOG("Get self_node_height error");
        return;
    }

    if (DBStatus::DB_SUCCESS == db_reader.GetBlockByBlockHash(block.prevhash(), block_raw))
    {
        INFOLOG("broadcast_blocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
        MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
        if(block.height() <= self_node_height + 1000)
        {
            broadcast_blocks.insert(block); 
        }

    }
    else
    {
        uint64_t block_height = block.height();
        auto found = pending_blocks.find(block_height);
        if (found == pending_blocks.end())
        {
            pending_blocks[block_height] = {};
        }
        INFOLOG("pending_blocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
        MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
        pending_blocks[block_height].insert({block.prevhash(), block}); 

        uint64_t node_height = 0;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(node_height))
        {
            INFOLOG("GetBlockTop Error");
        }
        if(block.height() > node_height + 3)
        {
            return;
        }

        DEBUGLOG("missing_blocks.insert height:{}, hash:{}, prevhash:{}, ", block.height(), block.hash().substr(0,6), block.prevhash().substr(0,6));
        uint64_t now_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
        MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
        std::unique_lock<std::mutex> locker(seek_mutex_);
        missing_blocks.insert({block.prevhash(), now_time, 0});
    }
}

void BlockHelper::AddSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &sync_block_data, global::ca::SaveType type)
{
    DEBUGLOG("AddSyncBlock sync_block_data.size(): {}", sync_block_data.size());
    std::lock_guard<std::mutex> lock(helper_mutex);
    for(const auto&[key,value]:sync_block_data)
    {
        for(const auto& sit: value)
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(sit.hash());
            sync_blocks.insert(std::move(sit));
        }
    }
    sync_type = type;
}

void BlockHelper::AddFastSyncBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &sync_block_data, global::ca::SaveType type)
{
    std::lock_guard<std::mutex> lock(helper_mutex);
    for (auto it = sync_block_data.begin(); it != sync_block_data.end(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(sit->hash());
            fast_sync_blocks.insert(*sit);
        }
    }
    sync_type = type;
}

void BlockHelper::AddRollbackBlock(const std::map<uint64_t, std::set<CBlock, CBlockCompare>> &rollback_block_data)
{
    std::lock_guard<std::mutex> lock(helper_mutex);
    rollback_blocks = rollback_block_data;
}

void BlockHelper::AddMissingBlock(const CBlock& block)
{
    std::lock_guard<std::mutex> lock(helper_mutex);
    MagicSingleton<TFSbenchmark>::GetInstance()->AddBlockPoolSaveMapStart(block.hash());
    utxo_missing_blocks.push_back(block);
}

bool BlockHelper::obtain_chain_height(uint64_t& chain_height)
{
    std::vector<Node> nodes;
    auto peer_node = MagicSingleton<PeerNode>::GetInstance();
    nodes = peer_node->get_nodelist();
    uint64_t node_amount = nodes.size();
    if (nodes.empty())
    {
        return false;
    }
    std::vector<uint64_t> node_heights;
    for (auto &node : nodes)
    {
        node_heights.push_back(node.height);
    }
    std::sort(node_heights.begin(), node_heights.end());
    const static int malicious_node_tolerated_amount = 25;
    double sample_rate = 0;
    if(node_amount <= 25)
    {
        sample_rate = 0.95;
    }
    else
    {
        sample_rate = static_cast<double>((node_amount - malicious_node_tolerated_amount)) / static_cast<double>(node_amount);
    }
     
    int verify_num = node_heights.size() * sample_rate;
    if (verify_num >= node_heights.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verify_num, node_heights.size());
        return false;
    }
    chain_height = node_heights.at(verify_num);
    
    return true;
}


void BlockHelper::rollback_test()
{
    std::lock_guard<std::mutex> lock(helper_mutex);

    cout << "1.Rollback block from Height" << endl;
    cout << "2.Rollback block from Hash" << endl;
    cout << "0.Quit" << endl;

    int iSwitch = 0;
    cin >> iSwitch;
    switch (iSwitch)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            unsigned int height = 0;
            cout << "Rollback block height: ";
            cin >> height;
            auto ret = ca_algorithm::RollBackToHeight(height);
            if (0 != ret)
            {
                cout << endl
                          << "ca_algorithm::RollBackToHeight:" << ret << endl;
                break;
            }
            MagicSingleton<PeerNode>::GetInstance()->set_self_height();
            break;
        }
        case 2:
        {
            string hash;
            cout << "Rollback block hash: ";
            cin >> hash;
            auto ret = ca_algorithm::RollBackByHash(hash);
            if (0 != ret)
            {
                cout << endl
                          << "ca_algorithm::RollBackByHash:" << ret << endl;
                break;
            }

            break;
        }
        default:
        {
            cout << "Input error !" << endl;
            break;
        }
    }
}
