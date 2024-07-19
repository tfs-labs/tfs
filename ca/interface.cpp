#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/interface.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/sync_block.h"

#include "ca/transaction_cache.h"

#include "utils/util.h"
#include "utils/time_util.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/contract_utils.h"

#include "db/cache.h"
#include "db/db_api.h"
#include "net/interface.h"
#include "common/global_data.h"
#include "include/scope_guard.h"

/*************************************SDK access block height, utxo, pledge list, investment list, block request*************************************/
int GetSDKAllNeed(const std::shared_ptr<GetSDKReq> & req, GetSDKAck & ack)
{
    ack.set_version(global::kVersion);
    ack.set_type(req->type());
    DEBUGLOG("req type:{}",req->type());
    std::vector<std::string> fromAddr;

    for(int i = 0;i<req->address_size();++i)
    {
        fromAddr.emplace_back(req->address(i));
    }

    if (fromAddr.empty() )
    {
        ack.set_code(-1);
        std::cout<<"request is empty()"<<std::endl;
        ack.set_message("request is empty()");
        return -1;
    }

    for(auto& from : fromAddr)
	{
		if (!isValidAddress(from))
		{
            ack.set_code(-2);
            ack.set_message("request is isValidAddress failed ");
            std::cout<<"request is isValidAddress failed"<<std::endl;
            ERRORLOG("Fromaddr is a non  address!");
            return -2;
		}
	}

    std::vector<Node> nodelist;
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);
    
    std::vector<Node> satisfiedNode;
    for(const auto & node : nodelist)
    {
        //Verification of investment and pledge
        int ret = VerifyBonusAddr(node.address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
        if (stakeTime > 0 && ret == 0)
        {
            satisfiedNode.push_back(node);
        }
    }
   
    std::sort(satisfiedNode.begin(), satisfiedNode.end(), [&](const Node &n1, const Node &n2)
			  { return n1.height < n2.height; });
    uint64_t height  = satisfiedNode[satisfiedNode.size()-1].height;


    for(auto &node :satisfiedNode)
    {
       SDKNodeInfo *nodeinfo =  ack.add_nodeinfo();
       nodeinfo->set_height(node.height);
       nodeinfo->set_listen_ip(node.listenIp);
       nodeinfo->set_listen_port(node.listenPort);
       nodeinfo->set_public_ip(node.publicIp);
       nodeinfo->set_public_port(node.publicPort);
       nodeinfo->set_addr(node.address);
       nodeinfo->set_pub(node.pub);
       nodeinfo->set_sign(node.sign);
       nodeinfo->set_identity(node.identity);
    }


    auto nodeinfolist = ack.nodeinfo();
    std::cout<<"nodeinfolist = "<<nodeinfolist.size()<<std::endl;
    
    if(height == 0)
    {
        ack.set_code(-3);
        ack.set_message("The height is zero.");
        return -3;
    }
    ack.set_height(height);

    //get utxo
    std::vector<TxHelper::Utxo> sdkutxos;
    for(auto& from : fromAddr)
    {
        std::vector<TxHelper::Utxo> sigleaddrutxos;
        int ret = TxHelper::GetUtxos(from, sigleaddrutxos);
        if (ret != 0)
        {
            std::cout<<"TxHelper::GetUtxos"<<std::endl;
            return ret -= 10;
        }
        for(auto &uxto : sigleaddrutxos)
        {
            sdkutxos.emplace_back(uxto);
        }
    }

    for (auto & item : sdkutxos)
    {
        SDKUtxo* utxo = ack.add_utxos();
        utxo->set_address(item.addr);
        utxo->set_hash(item.hash);
        utxo->set_value(item.value);
        utxo->set_n(item.n);
    }
    
    //print utxo
    auto v= ack.utxos();
    for(auto &t : v)
    {
        std::cout<< t.address() << std::endl;
        std::cout<< t.value() << std::endl;
        std::cout<< t.hash() << std::endl;
        std::cout<< t.n() << std::endl;
    }

     //get block
    std::vector<std::string> hashes;
    DBReader dbReader;
	
	if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(height, hashes))
	{
        ack.set_code(-4);
        ack.set_message("Get block hash failed");
        std::cout<<"Get block hash failed"<<std::endl;
		return -4;
	}

	std::vector<CBlock> blocks;
	for (const auto &hash : hashes)
	{
		std::string blockStr;
		dbReader.GetBlockByBlockHash(hash, blockStr);
        ack.add_blocks(blockStr);
	}

    std::vector<std::string> stakeAddresses;
	if (dbReader.GetStakeAddress(stakeAddresses) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get all stake address failed!"<<std::endl;
	}
    for(auto &addr :stakeAddresses)
    {
        ack.add_pledgeaddr(addr);
    }

    std::vector<std::string> utxos;
    if (dbReader.GetStakeAddressUtxo(fromAddr.at(0), utxos) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get stake utxo from address failed!"<<std::endl;
	}
    for(auto &utxo :utxos)
    {
        ack.add_pledgeutxo(utxo);
    }

    for(auto& from : stakeAddresses)
    {
        std::vector<std::string> utxoes;
        auto dbStatus = dbReader.GetStakeAddressUtxo(from, utxoes);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ack.set_code(-4);
            ack.set_message("Get Stake Address Utxo failed");
            std::cout<<"Get Stake Address Utxo failed"<<std::endl;
            continue;
        }

        for (auto & strUtxo: utxoes)
        {
            std::string serTxRaw;
            auto dbStatus = dbReader.GetTransactionByHash(strUtxo, serTxRaw);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get stake tx error");
                std::cout<<"Get stake tx error!"<<std::endl;
                continue;
            }
            SDKPledgeTx *sdkPledgeTx  = ack.add_pledgetx();
            sdkPledgeTx->set_address(from);
            sdkPledgeTx->set_tx(serTxRaw);
            sdkPledgeTx->set_utxo(strUtxo);
            std::cout<<"***************** "<<std::endl;  
        }
    }

    std::cout<<"ack.pledgetx() size = "<<ack.pledgetx().size()<<std::endl;
    for(auto& bonusAddr : fromAddr)
    {
        uint64_t investAmount;
        auto ret = MagicSingleton<BonusAddrCache>::GetInstance()->getAmount(bonusAddr, investAmount);
        if (ret < 0)
        {
            ERRORLOG("invest BonusAddr: {}, ret:{}", bonusAddr, ret);
            std::cout<<"invest BonusAddr: {}, ret:{}"<<std::endl;
            continue;
        }
        SDKBonusamout * sdkbonusamout = ack.add_bonusamount();
        sdkbonusamout->set_address(bonusAddr);
        sdkbonusamout->set_invest_amount(investAmount);
    }
    
    std::vector<std::string> bonusAddr;
	auto status = dbReader.GetBonusAddrByInvestAddr(fromAddr.at(0), bonusAddr);
	if (status == DBStatus::DB_SUCCESS && !bonusAddr.empty())
	{
        std::cout<<"The investor have already invested in a node!"<<std::endl;
	}

    for(auto &addr :bonusAddr)
    {
        ack.add_bonusaddr(addr);
    }

    std::vector<std::string> addresses;
    status = dbReader.GetInvestAddrsByBonusAddr(req->toaddr(), addresses);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
        std::cout<<"Get invest addrs by node failed!!"<<std::endl;
	}

    for(auto &addr :addresses)
    {
        ack.add_investedaddr(addr);;
    }

   
   for (auto &address : addresses)
   {
        std::vector<std::string> utxos;
        if (dbReader.GetBonusAddrInvestUtxosByBonusAddr(req->toaddr(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"GetBonusAddrInvestUtxosByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_bonusaddrinvestutxos(utxo);
            std::string strInvestTx;
            if (DBStatus::DB_SUCCESS != dbReader.GetTransactionByHash(utxo, strInvestTx))
            {
                ERRORLOG("Invest tx not found!");
                std::cout<<"Invest tx not found!"<<std::endl;
                continue;
            }
            SDKBonusTx * adkbonustx = ack.add_bonustx();
            adkbonustx->set_address(address);
            adkbonustx->set_utxo(utxo);
            adkbonustx->set_tx(strInvestTx);
        }
   }


    uint64_t totalCirculation = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetM2(totalCirculation))
    {
        std::cout<<"GetM2 failed!"<<std::endl;
    }

    ack.set_m2(totalCirculation);

    uint64_t Totalinvest=0;
    if (DBStatus::DB_SUCCESS != dbReader.GetTotalInvestAmount(Totalinvest))
    {
        std::cout<<"GetTotalInvestAmount failed!"<<std::endl;
    }
    ack.set_totalinvest(Totalinvest);


    std::map<std::string, double> addr_percent;
    std::unordered_map<std::string, uint64_t> addrSignCnt;
    uint64_t curTime = req->time();
    auto ret = ca_algorithm::GetAbnormalSignAddrListByPeriod(curTime, addr_percent, addrSignCnt);
    if(ret < 0) 
    {
        std::cout<<"GetAbnormalSignAddrListByPeriod failed!"<<std::endl;
    }
   

    std::cout<<"addrSignCnt size = "<<addrSignCnt.size()<<std::endl;
    for(auto& kv:addrSignCnt)
    {
        AbnormalAddrCnt *banorcnt =  ack.add_abnormaladdr_cnt();
        banorcnt->set_address(kv.first);
        banorcnt->set_count(kv.second);
    }
    
    std::vector<std::string> investaddresses;
    status = dbReader.GetInvestAddrsByBonusAddr(fromAddr.at(0), investaddresses);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
        std::cout<<"Get invest addrs by node failed!!"<<std::endl;
	}

    for(auto &Addr :investaddresses)
    {
        ack.add_claiminvestedaddr(Addr);
    }
    
    for (auto &address : investaddresses)
   {
        std::vector<std::string> utxos;
        if (dbReader.GetBonusAddrInvestUtxosByBonusAddr(fromAddr.at(0), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"GetBonusAddrInvestUtxosByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_claimbonusaddrinvestutxos(utxo);
            std::string strInvestTx;
            if (DBStatus::DB_SUCCESS != dbReader.GetTransactionByHash(utxo, strInvestTx))
            {
                std::cout<<"Invest tx not found!"<<std::endl;
                continue;
            }
            
            SDKClaimBonusTx * claimbonustx = ack.add_claimbonustx();
            claimbonustx->set_address(address);
            claimbonustx->set_utxo(utxo);
            claimbonustx->set_tx(strInvestTx);
        }
   }

    std::vector<std::string> claimutxos;
    uint64_t Period = MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);
	
    auto retstatus = dbReader.GetBonusUtxoByPeriod(Period, claimutxos);
    if (retstatus != DBStatus::DB_SUCCESS && retstatus != DBStatus::DB_NOT_FOUND)
	{
		std::cout<<"GetBonusUtxoByPeriod failed!"<<std::endl;
	}

    for(auto utxo = claimutxos.rbegin(); utxo != claimutxos.rend(); utxo++)
    {
        std::string strTx;
        if (DBStatus::DB_SUCCESS != dbReader.GetTransactionByHash(*utxo, strTx) )
        {
            std::cout<<"Invest tx not found!"<<std::endl;
            continue;
        }
        Claimtx * claimtx = ack.add_claimtx();
        claimtx->set_utxo(*utxo);
        claimtx->set_tx(strTx);
    }
    ack.set_code(0);
    ack.set_message("success");
    return 0;
}


std::map<int32_t, std::string> GetSDKAllNeedReq()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0, "GetSDKAllNeedReq Success"), 

											 };
	return errInfo;												
}
//SDK access block height, utxo, pledge list, investment list, block request
int HandleGetSDKAllNeedReq(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgData)
{
    auto errInfo = GetSDKAllNeedReq();
    GetSDKAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetSDKAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetSDKAllNeed(req, ack);
    if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;
}

std::map<int32_t, std::string> GetBlockReqCode()
{
	std::map<int32_t, std::string> errInfo = {  
                                                std::make_pair(-1, "The version is wrong"),
												std::make_pair(-12, "By block height failure"), 
												};

	return errInfo;												
}
int GetBlockReqImpl(const std::shared_ptr<GetBlockReq>& req, GetBlockAck & ack)
{
	ack.set_version(global::kVersion);

    DBReader dbReader;
    std::vector<std::string> hashes;
	uint64_t top = req->height();
    uint64_t blockHeight = top;
	if(top >= global::ca::kMinUnstakeHeight)
	{
		blockHeight = top - 10;
	}
	
	if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(blockHeight, hashes))
	{
        ack.set_code(-2);
        ack.set_message("Get block hash failed");
		return -2;
	}

	std::vector<CBlock> blocks;
	for (const auto &hash : hashes)
	{
		std::string blockStr;
		dbReader.GetBlockByBlockHash(hash, blockStr);
		CBlock block;
		block.ParseFromString(blockStr);
		blocks.push_back(block);
	}
	std::sort(blocks.begin(), blocks.end(), [](const CBlock &x, const CBlock &y)
			  { return x.time() < y.time(); });

    
    for(const auto &block:blocks)
    {
        BlockItem *blockitem = ack.add_list();
        blockitem->set_blockhash(block.hash());
        for(int i = 0; i<block.sign().size();  ++i)
        {
            blockitem->add_addr(GenerateAddr(block.sign(i).pub())) ;
        }
    }
    {
        std::vector<std::string> blockHashes;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(top, top, blockHashes))
        {
            ERRORLOG("can't GetBlockHashesByBlockHeight");
            return false;
        }

        std::vector<CBlock> blocksTime;
        for (auto &hash : blockHashes)
        {
            std::string blockStr;
            if(DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(hash, blockStr))
            {
                ERRORLOG("GetBlockByBlockHash error block hash = {} ", hash);
                return false;
            }

            CBlock block;
            if(!block.ParseFromString(blockStr))
            {
                ERRORLOG("block parse from string fail = {} ", blockStr);
                return false;
            }
            blocksTime.push_back(block);
        }

        std::sort(blocksTime.begin(), blocksTime.end(), [](const CBlock& x, const CBlock& y){ return x.time() < y.time(); });
        ack.set_timestamp(blocksTime.at(blocksTime.size()-1).time());
	
    }

    ack.set_code(0);
    ack.set_message("success");
    ack.set_height(blockHeight);
	return 0;
}
int HandleGetBlockReq(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgData)
{

    auto errInfo = GetBlockReqCode();
    GetBlockAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetBlockAck>(msgData, errInfo, ack, ret); 
    };
    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = GetBlockReqImpl(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return 0;
}
/*************************************Get the balance*************************************/
int GetBalanceReqImpl(const std::shared_ptr<GetBalanceReq>& req, GetBalanceAck & ack)
{
	ack.set_version(global::kVersion);

    std::string addr = req->address();
    if(addr.size() == 0)
    {
        return -1;
    } 

    if (!isValidAddress(addr))
    {
        return -2;
    }

    DBReader dbReader;

    uint64_t blockHeight = 0;
    dbReader.GetBlockTop(blockHeight);

	std::vector<std::string> addrUtxoHashs;
    DBStatus dbStatus = dbReader.GetUtxoHashsByAddress(addr, addrUtxoHashs);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        if (dbStatus == DBStatus::DB_NOT_FOUND)
        {
            return -3;
        }
        else 
        {
            return -4;
        }
    }
	
	uint64_t balance = 0;
	std::string txRaw;
	CTransaction tx;
	for (auto utxo_hash : addrUtxoHashs)
	{
		if (DBStatus::DB_SUCCESS != dbReader.GetTransactionByHash(utxo_hash, txRaw))
		{
			return -5;
		}
		if (!tx.ParseFromString(txRaw))
		{
            return -6;
		}
		for (auto &vout : tx.utxo().vout())
		{
			if (vout.addr() == addr)
			{
				balance += vout.value();
			}
		}
	}

    ack.set_address(addr);
    ack.set_balance(balance);
    ack.set_height(blockHeight);
    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> GetBalanceReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Amount Success"), 
												std::make_pair(-1, "addr is empty"), 
												std::make_pair(-2, " addr invalid"), 
												std::make_pair(-3, "search balance not found"),
                                                std::make_pair(-4, "get tx failed"),
                                                std::make_pair(-5, "GetTransactionByHash failed!"),
                                                std::make_pair(-6, "parse tx failed"),
												};

	return errInfo;												
}
int HandleGetBalanceReq(const std::shared_ptr<GetBalanceReq>& req, const MsgData& msgData)
{
    auto errInfo = GetBalanceReqCode();
    GetBalanceAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        ReturnAckCode<GetBalanceAck>(msgData, errInfo, ack, ret);
        
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = GetBalanceReqImpl(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;    
}
/*************************************Get node information*************************************/
int GetNodeInfoReqImpl(const std::shared_ptr<GetNodeInfoReq>& req, GetNodeInfoAck & ack)
{
	ack.set_version(global::kVersion);

    ack.set_address(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
    
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    ack.set_ip(IpPort::IpSz(selfNode.publicIp));

    DBReader dbReader;
	uint64_t height = 0;
    DBStatus dbStatus = dbReader.GetBlockTop(height);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -1;
    }

    ack.set_height(height);
    ack.set_ver(global::kVersion);

    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> GetNodeInfoReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Node Info Success"), 
												std::make_pair(-1, "Invalid Version"), 
												std::make_pair(-11, "Get Top Failed"),
												std::make_pair(-12, "Get Gas Failed"),
												};

	return errInfo;												
}
int HandleGetNodeInfoReqReq(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgData)
{
    auto errInfo = GetNodeInfoReqCode();
    GetNodeInfoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        ReturnAckCode<GetNodeInfoAck>(msgData, errInfo, ack, ret);
        
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = GetNodeInfoReqImpl(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;
}
/*************************************Stake list*************************************/
int GetStakeListReqImpl(const std::shared_ptr<GetStakeListReq>& req, GetStakeListAck & ack)
{
	ack.set_version(global::kVersion);

	std::string addr = req->addr();
    if (addr.length() == 0)
    {
        return -1;
    }

	if (!isValidAddress(addr))
    {
        return -2;
    }

    std::vector<std::string> utxoes;
    DBReader dbReader;
    auto dbStatus = dbReader.GetStakeAddressUtxo(addr, utxoes);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -3;
    }

    if (utxoes.size() == 0)
    {
        return -4;
    }

    reverse(utxoes.begin(), utxoes.end());

    for (auto & strUtxo: utxoes)
    {
        std::string serTxRaw;
        dbStatus = dbReader.GetTransactionByHash(strUtxo, serTxRaw);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ERRORLOG("Get stake tx error");
            continue;
        }

        CTransaction tx;
        tx.ParseFromString(serTxRaw);
        if(tx.utxo().vout_size() != 3)
        {
            ERRORLOG("invalid tx");
            continue;
        }

        if (tx.hash().length() == 0)
        {
            ERRORLOG("Get stake tx error");
            continue;
        }

        std::string strBlockHash;
        dbStatus = dbReader.GetBlockHashByTransactionHash(tx.hash(), strBlockHash);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ERRORLOG("Get stake block hash error");
            continue;
        }

        std::string serBlock;
        dbStatus = dbReader.GetBlockByBlockHash(strBlockHash, serBlock);
        if (dbStatus != 0)
        {
            ERRORLOG("Get stake block error");
            continue;
        }

        CBlock block;
        block.ParseFromString(serBlock);

        if (block.hash().empty())
        {
            ERRORLOG("Block error");
            continue;
        }

        std::vector<std::string> txOwnerVec(tx.utxo().owner().begin(), tx.utxo().owner().end());
		if (txOwnerVec.size() == 0)
        {
            continue;
        }

        StakeItem * pItem = ack.add_list();
        
        pItem->set_blockhash(block.hash());
        pItem->set_blockheight(block.height());
        pItem->set_utxo(strUtxo);
        pItem->set_time(tx.time());

        pItem->set_fromaddr(txOwnerVec[0]);

        for (int i = 0; i < tx.utxo().vout_size(); i++)
        {
            CTxOutput txout = tx.utxo().vout(i);
            if (txout.addr() == global::ca::kVirtualStakeAddr)
            {
                pItem->set_toaddr(txout.addr());
                pItem->set_amount(txout.value());
                break;
            }
        }

        if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
        {
            nlohmann::json dataJson = nlohmann::json::parse(tx.data());
            pItem->set_detail(dataJson["TxInfo"]["StakeType"].get<std::string>());
        }
    }

    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> GetStakeListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Stake List Success"), 
												std::make_pair(-1, "addr is empty !"), 
												std::make_pair(-2, " addr invalid"), 
												std::make_pair(-3, "Get Stake utxo error"),
                                                std::make_pair(-4, "No stake"),
												};

	return errInfo;												
}
int HandleGetStakeListReq(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgData)
{
	auto errInfo = GetStakeListReqCode();
    GetStakeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetStakeListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

	ret = GetStakeListReqImpl(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}
	return ret;
}

/*************************************List of investments*************************************/
int GetInvestListReqImpl(const std::shared_ptr<GetInvestListReq>& req, GetInvestListAck & ack)
{
	ack.set_version(global::kVersion);
        
    std::string addr = req->addr();
    if (addr.length() == 0)
    {
        return -1;
    }

	if (!isValidAddress(addr))
    {
        return -2;
    }

    std::vector<std::string> utxoes;
    std::vector<std::string> bonusAddrs;

    DBReader dbReader;
    auto dbStatus = dbReader.GetBonusAddrByInvestAddr(addr, bonusAddrs);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -3;
    }

    for (auto & bonusAddr : bonusAddrs)
    {
        dbStatus = dbReader.GetBonusAddrInvestUtxosByBonusAddr(bonusAddr, addr, utxoes);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            return -4;
        }

        if (utxoes.size() == 0)
        {
            return -5;
        }

        reverse(utxoes.begin(), utxoes.end());

        for (auto & strUtxo: utxoes)
        {
            std::string serTxRaw;
            dbStatus = dbReader.GetTransactionByHash(strUtxo, serTxRaw);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get invest tx error");
                continue;
            }

            CTransaction tx;
            tx.ParseFromString(serTxRaw);
            if(tx.utxo().vout_size() != 3)
            {
                ERRORLOG("invalid tx");
                continue;
            }

            if (tx.hash().length() == 0)
            {
                ERRORLOG("Get invest tx error");
                continue;
            }

            std::string strBlockHash;
            dbStatus = dbReader.GetBlockHashByTransactionHash(tx.hash(), strBlockHash);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get pledge block hash error");
                continue;
            }

            std::string serBlock;
            dbStatus = dbReader.GetBlockByBlockHash(strBlockHash, serBlock);
            if (dbStatus != 0)
            {
                ERRORLOG("Get invest block error");
                continue;
            }

            CBlock block;
            block.ParseFromString(serBlock);

            if (block.hash().empty())
            {
                ERRORLOG("Block error");
                continue;
            }
            
            std::vector<std::string> txOwnerVec(tx.utxo().owner().begin(), tx.utxo().owner().end());
            if (txOwnerVec.size() == 0)
            {
                continue;
            }

            InvestItem * pItem = ack.add_list();
            
            pItem->set_blockhash(block.hash());
            pItem->set_blockheight(block.height());
            pItem->set_utxo(strUtxo);
            pItem->set_time(tx.time());

            pItem->set_fromaddr(txOwnerVec[0]);

            for (int i = 0; i < tx.utxo().vout_size(); i++)
            {
                CTxOutput txout = tx.utxo().vout(i);
                if (txout.addr() == global::ca::kVirtualInvestAddr)
                {
                    pItem->set_toaddr(txout.addr());
                    pItem->set_amount(txout.value());
                    break;
                }
            }

            if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
            {
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                pItem->set_detail(dataJson["TxInfo"].dump());
            }
        }
    }

    ack.set_code(0);
    ack.set_message("success");


    return 0;
}

std::map<int32_t, std::string> GetInvestListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0, "Get Invest List Success"), 
												std::make_pair(-1, "addr is empty !"), 
												std::make_pair(-2, " addr invalid !"), 
												std::make_pair(-3, "GetBonusAddr failed !"),
												std::make_pair(-4, "GetBonusAddrInvestUtxos failed !"),
                                                std::make_pair(-5, "No Invest"),
												};

	return errInfo;												
}

int HandleGetInvestListReq(const std::shared_ptr<GetInvestListReq>& req, const MsgData & msgData)
{
	auto errInfo = GetInvestListReqCode();
    GetInvestListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetInvestListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

	ret = GetInvestListReqImpl(req, ack); 
	if (ret != 0)
	{
		return ret -= 10;
	}
	return ret;
}
/*************************************Query UTXO*************************************/
int GetUtxoReqImpl(const std::shared_ptr<GetUtxoReq>& req, GetUtxoAck & ack)
{
    ack.set_version(global::kVersion);

    std::string address = req->address();
    if (address.empty() || !isValidAddress(address))
    {
        return -1;
    }

    ack.set_address(address);

    std::vector<TxHelper::Utxo> utxos;
    int ret = TxHelper::GetUtxos(address, utxos);
    if (ret != 0)
    {
        return ret -= 10;
    }

    for (auto & item : utxos)
    {
        Utxo* utxo = ack.add_utxos();
        utxo->set_hash(item.hash);
        utxo->set_value(item.value);
        utxo->set_n(item.n);
    }
    
    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> GetUtxoReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0,  "Get Utxo Success"), 
												std::make_pair(-1,  "The addr is empty /  addr invalid"), 
												std::make_pair(-12, "GetUtxos : GetUtxoHashsByAddress failed !"),
												};

	return errInfo;												
}
int HandleGetUtxoReq(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgData)
{
    auto errInfo = GetUtxoReqCode();
    GetUtxoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetUtxoAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetUtxoReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Query all investment accounts and amounts on the investee node*************************************/
int GetAllInvestAddressReqImpl(const std::shared_ptr<GetAllInvestAddressReq>& req, GetAllInvestAddressAck & ack)
{
    ack.set_version(global::kVersion);
    
    std::string address = req->addr();
    if (address.empty() || !isValidAddress(address))
    {
        return -1;
    }

    ack.set_addr(address);

    DBReader dbReader;
    std::vector<std::string> addresses;
    auto dbStatus = dbReader.GetInvestAddrsByBonusAddr(address,addresses);
    if (dbStatus != DBStatus::DB_SUCCESS)
    {
        return -2;
    }

    if(addresses.size() == 0)
    {
        return -3;
    }

    for(auto& addr : addresses)
    {
        std::vector<std::string> utxos;
	    uint64_t total = 0;
        dbStatus = dbReader.GetBonusAddrInvestUtxosByBonusAddr(address,addr,utxos);
        if (dbStatus != DBStatus::DB_SUCCESS)
        {
            return -4;
        }
        for (auto &item : utxos) 
        {
            std::string strTxRaw;
            if (DBStatus::DB_SUCCESS != dbReader.GetTransactionByHash(item, strTxRaw))
            {
                return -5;
            }
            CTransaction utxoTx;
            utxoTx.ParseFromString(strTxRaw);
            for (auto &vout : utxoTx.utxo().vout())
            {
                if (vout.addr() == global::ca::kVirtualInvestAddr)
                {
                    total += vout.value();
                }
            }
        }
        
        InvestAddressItem * item = ack.add_list();
        item->set_addr(addr);
        item->set_value(total);
    }
    ack.set_code(0);
    ack.set_message("success");
    return 0;
}

std::map<int32_t, std::string> GetAllInvestAddressReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get AllInvestAddress Success"), 
												std::make_pair(-1, "The addr is empty /  addr invalid"), 
												std::make_pair(-2, "GetInvestAddrsByBonusAddr failed !"), 
												std::make_pair(-3, "No Invested addrs !"),
                                                std::make_pair(-4, "GetBonusAddrInvestUtxos failed !"),
                                                std::make_pair(-5, "GetTransactionByHash failed !"),
												};

	return errInfo;												
}
int HandleGetAllInvestAddressReq(const std::shared_ptr<GetAllInvestAddressReq>& req, const MsgData & msgData)
{
    auto errInfo = GetAllInvestAddressReqCode();
    GetAllInvestAddressAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetAllInvestAddressAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetAllInvestAddressReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 10;
    }

    return 0;
}

/*************************************Get all investable nodes*************************************/
int GetAllStakeNodeListReqImpl(const std::shared_ptr<GetAllStakeNodeListReq>& req, GetAllStakeNodeListAck & ack)
{
    ack.set_version(global::kVersion);

	std::vector<Node> nodelist;
    
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    if(tmp.empty())
    {
        ack.set_code(-1);
        ack.set_message("peerlist is empty");
    }
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);

    for (auto &node : nodelist)
	{
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
        if(stakeTime < 0)
        {
            continue;
        }

        StakeNode* pStakeNode =  ack.add_list();
        pStakeNode->set_addr("0x"+node.address);
        pStakeNode->set_name(node.name);
        pStakeNode->set_height(node.height);
        pStakeNode->set_identity(node.identity);
        pStakeNode->set_logo(node.logo);
        pStakeNode->set_ip(std::string(IpPort::IpSz(node.publicIp)) );
        pStakeNode->set_version(node.ver);
	}
    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> GetAllStakeNodeListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get GetAllStakeNode List Success"), 
												std::make_pair(-1, "GetStakeAddress failed !"), 
												std::make_pair(-12, "GetStakeAddressUtxo failed !"), 
												std::make_pair(-21, "No failure of the transaction."),
                                                std::make_pair(-24, "GetBonusAddrInvestUtxos failed !"),
                                                std::make_pair(-25, "GetTransactionByHash failed !"),
												};

	return errInfo;												
}
int HandleGetAllStakeNodeListReq(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgData)
{
    auto errInfo = GetAllStakeNodeListReqCode();
    GetAllStakeNodeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetAllStakeNodeListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetAllStakeNodeListReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Get a list of signatures*************************************/
int GetSignCountListReqImpl(const std::shared_ptr<GetSignCountListReq>& req, GetSignCountListAck & ack)
{
    ack.set_version(global::kVersion);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (!isValidAddress(defaultAddr))
    {
        return -1;
    }

    std::map<std::string, double> addr_percent;
    std::unordered_map<std::string, uint64_t> addrSignCnt;
    uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    ca_algorithm::GetAbnormalSignAddrListByPeriod(curTime, addr_percent, addrSignCnt);
    // if(abnormalAddrList.find(defaultAddr) != abnormalAddrList.end())
    // {
    //     return -2;
    // }

    for (auto & item : addrSignCnt)
    {
        SignCount* Sign_list =  ack.add_list();
        Sign_list->set_addr(item.first);
        Sign_list->set_count(item.second);
    }

    ack.set_code(0);
    ack.set_message("success");
    return 0;
}

std::map<int32_t, std::string> GetSignCountListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get SignCountList List Success"), 
												std::make_pair(-1, "defaultAddr is invalid !"), 
												std::make_pair(-2, "GetBonusaddr failed !"), 
												std::make_pair(-3, "GetInvestAddrsByBonusAddr failed !"),
                                                std::make_pair(-4, "BonusdAddrs < 1 || BonusdAddrs > 999"),
                                                std::make_pair(-5, "GetBonusAddrInvestUtxosByBonusAddr failed !"),
                                                std::make_pair(-6, "GetTransactionByHash failed !"),
                                                std::make_pair(-7, "Parse tx failed !"),
                                                std::make_pair(-8, "Total amount invested < 5000 !"),
                                                std::make_pair(-9, "GetSignUtxoByAddr failed !"),
												};

	return errInfo;												
}
int HandleGetSignCountListReq(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgData)
{
    auto errInfo = GetSignCountListReqCode();
    GetSignCountListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetSignCountListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetSignCountListReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 10;
    }

    return 0;
}

/*************************************Calculate the commission*************************************/
int GetHeightReqImpl(const std::shared_ptr<GetHeightReq>& req, GetHeightAck & ack)
{
    ack.set_version(global::kVersion);
    DBReader dbReader;
    uint64_t height = 0;
    if(dbReader.GetBlockTop(height) != DBStatus::DB_SUCCESS)
    {
        return -1;
    }
    ack.set_height(height);
    
    return 0;
}

std::map<int32_t, std::string> GetHeightReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(    0, " GetHeight Success "),
                                                std::make_pair(   -1, " Get Block Height fail")
												};

	return errInfo;												
}
int HandleGetHeightReq(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgData)
{
    auto errInfo = GetHeightReqCode();
    GetHeightAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetHeightAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetHeightReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Check the current claim amount*************************************/
int GetBonusListReqImpl(const std::shared_ptr<GetBonusListReq> & req, GetBonusListAck & ack)
{
    ack.set_version(global::kVersion);

    std::string addr = req->bonusaddr();
    if(addr.size() == 0)
    {
        return -1;
    } 

    if (!isValidAddress(addr))
    {
        return -2;
    }

    ack.set_bonusaddr(addr);
    uint64_t curTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::map<std::string, uint64_t> values;
    int ret = ca_algorithm::CalcBonusValue(curTime, addr, values);
    if (ret != 0)
    {
        ERRORLOG("CalcBonusValue RET : {}", ret);
        return -3;
    }

    for (auto & bonus : values)
    {
        BonusItem * item = ack.add_list();
        item->set_addr(bonus.first);
        item->set_value(bonus.second);
    }

    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> GetBonusListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Stake List Success !"), 
                                                std::make_pair(-1, "addr is empty !"),
                                                std::make_pair(-2, "addr is invalid !"),
                                                std::make_pair(-11, "addr is AbnormalSignAddr !"),
                                                std::make_pair(-111, "GetInvestAddrsByBonusAddr failed !"),
                                                std::make_pair(-112, "GetBonusAddrInvestUtxosByBonusAddr failed!"),
                                                std::make_pair(-113, "GetTransactionByHash failed !"),
                                                std::make_pair(-114, "Parse Transaction failed !"),
                                                std::make_pair(-115, "GetInvestAndDivestUtxoByAddr failed !"),
                                                std::make_pair(-116, "GetTransactionByHash failed !"),
                                                std::make_pair(-117, "Parse Transaction failed !"),
                                                std::make_pair(-118, "Unknown transaction type !"),
                                                std::make_pair(-119, "mpInvestAddr2Amount is empty !"),
                                                std::make_pair(-211, "GetM2 failed !"),
                                                std::make_pair(-212, "GetBonusUtxo failed !"),
                                                std::make_pair(-213, "GetTransactionByHash failed !"),
                                                std::make_pair(-214, "Parse Transaction failed !"),
                                                std::make_pair(-311, "GetTotalInvestAmount failed !"),
                                                std::make_pair(-312, "GetAllInvestUtxo failed !"),
                                                std::make_pair(-313, "GetTransactionByHash failed !"),
                                                std::make_pair(-314, "Parse Transaction failed !"),

											};

	return errInfo;												
}

int HandleGetBonusListReq(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgData)
{
    auto errInfo = GetBonusListReqCode();
    GetBonusListAck ack;
    int ret = 0;
    ON_SCOPE_EXIT{
        ReturnAckCode<GetBonusListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return -1;
	}

    ret = GetBonusListReqImpl(req, ack);
    if (ret != 0)
    {
        ERRORLOG("GetBonusListReqImpl ret : {}", ret);
        return -2;
    }

    return 0;
}

std::map<int32_t, std::string> GetReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get  List Success"), 
												std::make_pair(-1, "version unCompatible"), 
												};
	return errInfo;												
}

int SendConfirmTransactionReq(const std::shared_ptr<ConfirmTransactionReq>& msg,   ConfirmTransactionAck & ack)
{

    auto getRandomNumbers = [&](int limit) -> std::vector<int> {
        std::random_device seed;
        std::ranlux48 engine(seed());
        std::uniform_int_distribution<int> u(0, limit - 1);
        
        std::set<int> uniqueNumbers;
        while (uniqueNumbers.size() < 10) {
            int randomNum = u(engine);
            uniqueNumbers.insert(randomNum);
        }
        
        std::vector<int> result(uniqueNumbers.begin(), uniqueNumbers.end());
        return result;
    };

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    auto nodelistsize = nodelist.size();
    if(nodelistsize == 0)
    {
        ERRORLOG("Nodelist Size is empty");
        ack.set_message("Nodelist size is empty");
        return -1;
    }
    DEBUGLOG("Nodelist Size {}",nodelistsize);
    
    std::vector<Node> FilterHeightNodeList;
    for(auto &item : nodelist)
    {
        if(item.height >= msg->height())
        {
            FilterHeightNodeList.push_back(item);
        }
    }
    auto send_num = FilterHeightNodeList.size();
    if(send_num == 0)
    {
        ERRORLOG("FilterHeightNodeList is empty");
        ack.set_message("FilterHeightNodeList size is empty");
        return -2;
    }
    DEBUGLOG("FilterHeightNodeList {}",send_num);
    ack.set_send_size(send_num);
    //send_size
    std::string msgId;
    std::map<std::string, uint32_t> successHash;
    if (!GLOBALDATAMGRPTR.CreateWait(10, send_num * 0.8, msgId))
    {
        ERRORLOG("SendConfirmTransactionReq CreateWait is error");
        ack.set_message("CreateWait error");
        return -3;
    }

    CheckTxReq req;
    req.set_version(global::kVersion);
    req.set_msg_id(msgId);
    for(auto & hash : msg->txhash())
    {
        req.add_txhash(hash);
        successHash.insert(std::make_pair(hash, 0));
    }

    auto needResVec = getRandomNumbers(FilterHeightNodeList.size());
    int index = 0;
    std::vector<std::string> needResNode;
    for (auto &node : FilterHeightNodeList)
    {   
        if(std::find(needResVec.begin(), needResVec.end(), index++) != needResVec.end())
        {
            req.set_isresponse(1);
            needResNode.push_back(node.address);
        }else{
            req.set_isresponse(0);
        }
        
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, node.address))
        {
            ERRORLOG("SendConfirmTransactionReq AddResNode is error");
            ack.set_message(" AddResNode error");
            return -4;
        }
        NetSendMessage<CheckTxReq>(node.address, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, ret_datas))
    {
        ERRORLOG("SendConfirmTransactionReq WaitData is error");
        ack.set_message(" WaitData error");
        return -5;
    }

    CheckTxAck copyAck;
    std::multimap<std::string, int> txFlagHashs;
    std::vector<std::string> txRawVec;

    auto findMostFrequentElement = [](const std::vector<std::string>& vec) {
        std::unordered_map<std::string, int> occurrenceMap;

        for (const std::string& element : vec) {
            occurrenceMap[element]++;
        }

        std::string mostFrequentElement;
        int maxOccurrences = 0;

        for (const auto& entry : occurrenceMap) {
            if (entry.second > maxOccurrences) {
                mostFrequentElement = entry.first;
                maxOccurrences = entry.second;
            }
        }

        return std::make_pair(mostFrequentElement, maxOccurrences);
    };

    for (auto &ret_data : ret_datas)
    {
        copyAck.Clear();
        if (!copyAck.ParseFromString(ret_data))
        {
            continue;
        }
        
        for(auto & flag_hash : copyAck.flaghash())
        {
            txFlagHashs.insert(std::make_pair(flag_hash.hash(),flag_hash.flag()));

            if(std::find(needResNode.begin(),needResNode.end(),copyAck.addr()) != needResNode.end() && flag_hash.flag() == 1)
            {
                txRawVec.push_back(copyAck.tx());
            }
        }
    }
    
    for(auto & item : txFlagHashs)
    {
        for(auto & success : successHash)
        {
            if(item.second == 1 && item.first == success.first)
            {
                success.second ++;
                DEBUGLOG("success.second size = {}", success.second);
            }
        }
    }

    auto commonSize = findMostFrequentElement(txRawVec);
    ack.set_tx(commonSize.first);
    ack.set_version(global::kVersion);
    ack.set_time(msg->time());

    for(auto & success : successHash)
    {
        SuccessRate * alsuc = ack.add_percentage();

        alsuc->set_hash(success.first);
        double rate = (double)success.second / (double)(ret_datas.size());
        DEBUGLOG("SendConfirmTransactionReq = {} , == {}, rate = ", success.second, send_num, rate);
        alsuc->set_rate(rate);
    }
    ack.set_received_size(ret_datas.size());
    return 0;
}
 
std::map<int32_t, std::string> GetOnChianReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get  List Success"), 
												std::make_pair(-1, "version unCompatible"), 
												};
	return errInfo;												
}

int HandleConfirmTransactionReq(const std::shared_ptr<ConfirmTransactionReq>& req, const MsgData & msgData)
{
    auto errInfo = GetOnChianReqCode();
    ConfirmTransactionAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<ConfirmTransactionAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = SendConfirmTransactionReq(req, ack);
    return ret;
}

int GetRestInvestAmountReqImpl(const std::shared_ptr<GetRestInvestAmountReq>& msg,  GetRestInvestAmountAck & ack)
{
    // The node to be invested can only be invested by 999 people at most
    uint64_t investAmount = 0;
    DBReader dbReader;
    std::vector<std::string> addresses;
    auto status = dbReader.GetInvestAddrsByBonusAddr(msg->addr(), addresses);
    if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
    {
        ERRORLOG("Get invest addrs by node failed!" );
        return -1;
    }
    if (addresses.size() + 1 > 999)
    {
        ERRORLOG("The account number to be invested have been invested by 999 people!" );
        return -2;
    }

    // The node to be invested can only be be invested 100000 at most
    uint64_t sumInvestAmount = 0;
    for (auto &address : addresses)
    {
        std::vector<std::string> utxos;
        if (dbReader.GetBonusAddrInvestUtxosByBonusAddr(msg->addr(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetBonusAddrInvestUtxosByBonusAddr failed!");
            return -3;
        }

        for (const auto &utxo : utxos)
        {
            std::string strTx;
            if (dbReader.GetTransactionByHash(utxo, strTx) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("GetTransactionByHash failed!");
                return -4;
            }

            CTransaction tx;
            if (!tx.ParseFromString(strTx))
            {
                ERRORLOG("Failed to parse transaction body!");
                return -5;
            }
            for (auto &vout : tx.utxo().vout())
            {
                if (vout.addr() == global::ca::kVirtualInvestAddr)
                {
                    sumInvestAmount += vout.value();
                    break;
                }
            }
        }
    }
    investAmount = (65000 * global::ca::kDecimalNum) - sumInvestAmount;
    ack.set_addr(msg->addr());
    ack.set_amount(investAmount);

    return 0;
}

std::map<int32_t, std::string> GetRestInvestAmountReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "success"), 
												std::make_pair(-1, "Get invest addrs by node failed!"), 
												std::make_pair(-2, "The account number to be invested have been invested by 999 people!"), 
												std::make_pair(-3, "GetBonusAddrInvestUtxosByBonusAddr failed!"), 
												std::make_pair(-4, "GetTransactionByHash failed!"), 
												std::make_pair(-5, "Failed to parse transaction body!")
												};
	return errInfo;												
}

int HandleGetRestInvestAmountReq(const std::shared_ptr<GetRestInvestAmountReq>& req, const MsgData & msgData)
{
    auto errInfo = GetRestInvestAmountReqCode();
    GetRestInvestAmountAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetRestInvestAmountAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
    {
        return ret = -1;
    }

    ret = GetRestInvestAmountReqImpl(req,ack); 

    return ret;
}

void RegisterInterface()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetBlockReq>(HandleGetBlockReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetBalanceReq>(HandleGetBalanceReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetNodeInfoReq>(HandleGetNodeInfoReqReq);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetStakeListReq>(HandleGetStakeListReq);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetInvestListReq>(HandleGetInvestListReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetUtxoReq>(HandleGetUtxoReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetAllInvestAddressReq>(HandleGetAllInvestAddressReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetAllStakeNodeListReq>(HandleGetAllStakeNodeListReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetSignCountListReq>(HandleGetSignCountListReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetHeightReq>(HandleGetHeightReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetBonusListReq>(HandleGetBonusListReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetSDKReq>(HandleGetSDKAllNeedReq);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<ConfirmTransactionReq>(HandleConfirmTransactionReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<GetRestInvestAmountReq>(HandleGetRestInvestAmountReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<MultiSignTxReq>(HandleMultiSignTxReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->CaRegisterCallback<BlockStatus>(HandleBlockStatusMsg); //retransmit

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<FastSyncGetHashReq>(HandleFastSyncGetHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<FastSyncGetHashAck>(HandleFastSyncGetHashAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<FastSyncGetBlockReq>(HandleFastSyncGetBlockReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<FastSyncGetBlockAck>(HandleFastSyncGetBlockAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetSumHashReq>(HandleSyncGetSumHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetSumHashAck>(HandleSyncGetSumHashAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetHeightHashReq>(HandleSyncGetHeightHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetHeightHashAck>(HandleSyncGetHeightHashAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetBlockReq>(HandleSyncGetBlockReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncGetBlockAck>(HandleSyncGetBlockAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncFromZeroGetSumHashReq>(HandleFromZeroSyncGetSumHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncFromZeroGetSumHashAck>(HandleFromZeroSyncGetSumHashAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncFromZeroGetBlockReq>(HandleFromZeroSyncGetBlockReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncFromZeroGetBlockAck>(HandleFromZeroSyncGetBlockAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncNodeHashReq>(HandleSyncNodeHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SyncNodeHashAck>(HandleSyncNodeHashAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetBlockByUtxoReq>(HandleBlockByUtxoReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetBlockByUtxoAck>(HandleBlockByUtxoAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetBlockByHashReq>(HandleBlockByHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetBlockByHashAck>(HandleBlockByHashAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SeekPreHashByHightReq>(HandleSeekGetPreHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<SeekPreHashByHightAck>(HandleSeekGetPreHashAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetCheckSumHashReq>(HandleGetCheckSumHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SyncBlockRegisterCallback<GetCheckSumHashAck>(HandleGetCheckSumHashAck);

    // PCEnd correlation
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<TxMsgReq>(HandleTx); // PCEnd transaction flow
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<ContractTxMsgReq>(HandleContractTx);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<ContractPackagerMsg>(HandleContractPackagerMsg);
    
    MagicSingleton<ProtobufDispatcher>::GetInstance()->SaveBlockRegisterCallback<BuildBlockBroadcastMsg>(HandleBuildBlockBroadcastMsg); // Building block broadcasting

    MagicSingleton<ProtobufDispatcher>::GetInstance()->BlockRegisterCallback<BlockMsg>(HandleBlock);      // PCEnd transaction flow
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BlockRegisterCallback<newSeekContractPreHashReq>(_HandleSeekContractPreHashReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->BlockRegisterCallback<newSeekContractPreHashAck>(_HandleSeekContractPreHashAck);

}

