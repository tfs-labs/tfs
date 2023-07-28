#include "ca_interface.h"
#include "include/net_interface.h"
#include "include/ScopeGuard.h"
#include "utils/ReturnAckCode.h"
#include "utils/util.h"
#include "utils/MagicSingleton.h"
#include "ca/ca_global.h"
#include "db/db_api.h"
#include "db/cache.h"
#include "ca/ca_txhelper.h"
#include "ca/ca_algorithm.h"
#include "utils/time_util.h"
#include "ca_transaction.h"
#include "utils/AccountManager.h"
#include "common/global_data.h"

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
		if (!CheckBase58Addr(from))
		{
            ack.set_code(-2);
            ack.set_message("request is CheckBase58Addr failed ");
            std::cout<<"request is CheckBase58Addr failed"<<std::endl;
            ERRORLOG("Fromaddr is a non base58 address!");
            return -2;
		}
	}

    std::vector<Node> nodelist;
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);
    
    std::vector<Node> SatisfiedNode;
    for(const auto & node : nodelist)
    {
        //Verification of investment and pledge
        int ret = VerifyBonusAddr(node.base58address);
        int64_t stake_time = ca_algorithm::GetPledgeTimeByAddr(node.base58address, global::ca::StakeType::kStakeType_Node);
        if (stake_time > 0 && ret == 0)
        {
            SatisfiedNode.push_back(node);
        }
    }
   
    std::sort(SatisfiedNode.begin(), SatisfiedNode.end(), [&](const Node &n1, const Node &n2)
			  { return n1.height < n2.height; });
    uint64_t height  = SatisfiedNode[SatisfiedNode.size()-1].height;


    for(auto &node :SatisfiedNode)
    {
       SDKNodeInfo *nodeinfo =  ack.add_nodeinfo();
       nodeinfo->set_height(node.height);
       nodeinfo->set_listen_ip(node.listen_ip);
       nodeinfo->set_listen_port(node.listen_port);
       nodeinfo->set_public_ip(node.public_ip);
       nodeinfo->set_public_port(node.public_port);
       nodeinfo->set_base58addr(node.base58address);
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
    DBReader db_reader;
	
	if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(height, hashes))
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
		db_reader.GetBlockByBlockHash(hash, blockStr);
        ack.add_blocks(blockStr);
	}

    std::vector<std::string> Stakeaddresses;
	if (db_reader.GetStakeAddress(Stakeaddresses) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get all stake address failed!"<<std::endl;
	}
    for(auto &addr :Stakeaddresses)
    {
        ack.add_pledgeaddr(addr);
    }

    std::vector<std::string> utxos;
    if (db_reader.GetStakeAddressUtxo(fromAddr.at(0), utxos) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get stake utxo from address failed!"<<std::endl;
	}
    for(auto &utxo :utxos)
    {
        ack.add_pledgeutxo(utxo);
    }

    for(auto& from : Stakeaddresses)
    {
        std::vector<string> utxoes;
        auto db_status = db_reader.GetStakeAddressUtxo(from, utxoes);
        if (DBStatus::DB_SUCCESS != db_status)
        {
            ack.set_code(-4);
            ack.set_message("Get Stake Address Utxo failed");
            std::cout<<"Get Stake Address Utxo failed"<<std::endl;
            continue;
        }

        for (auto & strUtxo: utxoes)
        {
            std::string serTxRaw;
            auto db_status = db_reader.GetTransactionByHash(strUtxo, serTxRaw);
            if (DBStatus::DB_SUCCESS != db_status)
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
    for(auto& BonusAddr : fromAddr)
    {
        uint64_t invest_amount;
        auto ret = MagicSingleton<BounsAddrCache>::GetInstance()->get_amount(BonusAddr, invest_amount);
        if (ret < 0)
        {
            ERRORLOG("invest BonusAddr: {}, ret:{}", BonusAddr, ret);
            std::cout<<"invest BonusAddr: {}, ret:{}"<<std::endl;
            continue;
        }
        SDKBonusamout * sdkbonusamout = ack.add_bonusamount();
        sdkbonusamout->set_address(BonusAddr);
        sdkbonusamout->set_invest_amount(invest_amount);
    }
    
    std::vector<string> BonusAddr;
	auto status = db_reader.GetBonusAddrByInvestAddr(fromAddr.at(0), BonusAddr);
	if (status == DBStatus::DB_SUCCESS && !BonusAddr.empty())
	{
        std::cout<<"The investor have already invested in a node!"<<std::endl;
	}

    for(auto &bonusAddr :BonusAddr)
    {
        ack.add_bonusaddr(bonusAddr);
    }

    std::vector<string> addresses;
    status = db_reader.GetInvestAddrsByBonusAddr(req->toaddr(), addresses);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
        std::cout<<"Get invest addrs by node failed!!"<<std::endl;
	}

    for(auto &Addr :addresses)
    {
        ack.add_investedaddr(Addr);;
    }

   
   for (auto &address : addresses)
   {
        std::vector<string> utxos;
        if (db_reader.GetBonusAddrInvestUtxosByBonusAddr(req->toaddr(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"GetBonusAddrInvestUtxosByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_bonusaddrinvestutxos(utxo);
            std::string strInvestTx;
            if (DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(utxo, strInvestTx))
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


    uint64_t TotalCirculation = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetM2(TotalCirculation))
    {
        std::cout<<"GetM2 failed!"<<std::endl;
    }

    ack.set_m2(TotalCirculation);

    uint64_t Totalinvest=0;
    if (DBStatus::DB_SUCCESS != db_reader.GetTotalInvestAmount(Totalinvest))
    {
        std::cout<<"GetTotalInvestAmount failed!"<<std::endl;
    }
    ack.set_totalinvest(Totalinvest);


    std::map<std::string, uint64_t> abnormal_addr_list;
    std::unordered_map<std::string, uint64_t> addr_sign_cnt;
    uint64_t cur_time = req->time();
    auto ret = ca_algorithm::GetAbnormalSignAddrListByPeriod(cur_time, abnormal_addr_list, addr_sign_cnt);
    if(ret < 0) 
    {
        std::cout<<"GetAbnormalSignAddrListByPeriod failed!"<<std::endl;
    }
   

    std::cout<<"addr_sign_cnt size = "<<addr_sign_cnt.size()<<std::endl;
    for(auto& kv:addr_sign_cnt)
    {
        AbnormalAddrCnt *banorcnt =  ack.add_abnormaladdr_cnt();
        banorcnt->set_address(kv.first);
        banorcnt->set_count(kv.second);
    }
    
    std::vector<string> investaddresses;
    status = db_reader.GetInvestAddrsByBonusAddr(fromAddr.at(0), investaddresses);
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
        std::vector<string> utxos;
        if (db_reader.GetBonusAddrInvestUtxosByBonusAddr(fromAddr.at(0), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"GetBonusAddrInvestUtxosByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_claimbonusaddrinvestutxos(utxo);
            std::string strInvestTx;
            if (DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(utxo, strInvestTx))
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
    uint64_t Period = MagicSingleton<TimeUtil>::GetInstance()->getPeriod(cur_time);
	
    auto retstatus = db_reader.GetBonusUtxoByPeriod(Period, claimutxos);
    if (retstatus != DBStatus::DB_SUCCESS && retstatus != DBStatus::DB_NOT_FOUND)
	{
		std::cout<<"GetBonusUtxoByPeriod failed!"<<std::endl;
	}

    for(auto utxo = claimutxos.rbegin(); utxo != claimutxos.rend(); utxo++)
    {
        std::string strTx;
        if (DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(*utxo, strTx) )
        {
            cout<<"Invest tx not found!"<<endl;
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
int HandleGetSDKAllNeedReq(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetSDKAllNeedReq();
    GetSDKAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetSDKAck>(msgdata, errInfo, ack, ret);
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

    DBReader db_reader;
    std::vector<std::string> hashes;
	uint64_t top = req->height();
    uint64_t block_height = top;
	if(top >= global::ca::kMinUnstakeHeight)
	{
		block_height = top - 10;
	}
	
	if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(block_height, hashes))
	{
        ack.set_code(-2);
        ack.set_message("Get block hash failed");
		return -2;
	}

	std::vector<CBlock> blocks;
	for (const auto &hash : hashes)
	{
		std::string blockStr;
		db_reader.GetBlockByBlockHash(hash, blockStr);
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
            blockitem->add_addr(GetBase58Addr(block.sign(i).pub())) ;
        }
    }
    {
        std::vector<std::string> block_hashes;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashesByBlockHeight(top, top, block_hashes))
        {
            ERRORLOG("can't GetBlockHashesByBlockHeight");
            return false;
        }

        std::vector<CBlock> blocks_time;
        for (auto &hash : block_hashes)
        {
            std::string blockStr;
            if(DBStatus::DB_SUCCESS != db_reader.GetBlockByBlockHash(hash, blockStr))
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
            blocks_time.push_back(block);
        }

        std::sort(blocks_time.begin(), blocks_time.end(), [](const CBlock& x, const CBlock& y){ return x.time() < y.time(); });
        ack.set_timestamp(blocks_time.at(blocks_time.size()-1).time());
	
    }

    ack.set_code(0);
    ack.set_message("success");
    ack.set_height(block_height);
	return 0;
}
int HandleGetBlockReq(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgdata)
{

    auto errInfo = GetBlockReqCode();
    GetBlockAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetBlockAck>(msgdata, errInfo, ack, ret); 
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

    if (!CheckBase58Addr(addr))
    {
        return -2;
    }

    DBReader db_reader;

    uint64_t blockHeight = 0;
    db_reader.GetBlockTop(blockHeight);

	std::vector<std::string> addr_utxo_hashs;
    DBStatus db_status = db_reader.GetUtxoHashsByAddress(addr, addr_utxo_hashs);
    if (DBStatus::DB_SUCCESS != db_status)
    {
        if (db_status == DBStatus::DB_NOT_FOUND)
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
	for (auto utxo_hash : addr_utxo_hashs)
	{
		if (DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(utxo_hash, txRaw))
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
												std::make_pair(-2, "base58 addr invalid"), 
												std::make_pair(-3, "search balance not found"),
                                                std::make_pair(-4, "get tx failed"),
                                                std::make_pair(-5, "GetTransactionByHash failed!"),
                                                std::make_pair(-6, "parse tx failed"),
												};

	return errInfo;												
}
int HandleGetBalanceReq(const std::shared_ptr<GetBalanceReq>& req, const MsgData& msgdata)
{
    auto errInfo = GetBalanceReqCode();
    GetBalanceAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        ReturnAckCode<GetBalanceAck>(msgdata, errInfo, ack, ret);
        
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

    ack.set_address(MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
    ack.set_ip(IpPort::ipsz(selfNode.public_ip));

    DBReader db_reader;
	uint64_t height = 0;
    DBStatus db_status = db_reader.GetBlockTop(height);
    if (DBStatus::DB_SUCCESS != db_status)
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
int HandleGetNodeInfoReqReq(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgdata)
{
    auto errInfo = GetNodeInfoReqCode();
    GetNodeInfoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        ReturnAckCode<GetNodeInfoAck>(msgdata, errInfo, ack, ret);
        
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

	if (!CheckBase58Addr(addr))
    {
        return -2;
    }

    std::vector<string> utxoes;
    DBReader db_reader;
    auto db_status = db_reader.GetStakeAddressUtxo(addr, utxoes);
    if (DBStatus::DB_SUCCESS != db_status)
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
        db_status = db_reader.GetTransactionByHash(strUtxo, serTxRaw);
        if (DBStatus::DB_SUCCESS != db_status)
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
        db_status = db_reader.GetBlockHashByTransactionHash(tx.hash(), strBlockHash);
        if (DBStatus::DB_SUCCESS != db_status)
        {
            ERRORLOG("Get stake block hash error");
            continue;
        }

        std::string serBlock;
        db_status = db_reader.GetBlockByBlockHash(strBlockHash, serBlock);
        if (db_status != 0)
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
            nlohmann::json data_json = nlohmann::json::parse(tx.data());
            pItem->set_detail(data_json["TxInfo"]["StakeType"].get<std::string>());
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
												std::make_pair(-2, "base58 addr invalid"), 
												std::make_pair(-3, "Get Stake utxo error"),
                                                std::make_pair(-4, "No stake"),
												};

	return errInfo;												
}
int HandleGetStakeListReq(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgdata)
{
	auto errInfo = GetStakeListReqCode();
    GetStakeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetStakeListAck>(msgdata, errInfo, ack, ret);
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

	if (!CheckBase58Addr(addr))
    {
        return -2;
    }

    std::vector<std::string> utxoes;
    std::vector<std::string> bonusAddrs;

    DBReader db_reader;
    auto db_status = db_reader.GetBonusAddrByInvestAddr(addr, bonusAddrs);
    if (DBStatus::DB_SUCCESS != db_status)
    {
        return -3;
    }

    for (auto & bonusAddr : bonusAddrs)
    {
        db_status = db_reader.GetBonusAddrInvestUtxosByBonusAddr(bonusAddr, addr, utxoes);
        if (DBStatus::DB_SUCCESS != db_status)
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
            db_status = db_reader.GetTransactionByHash(strUtxo, serTxRaw);
            if (DBStatus::DB_SUCCESS != db_status)
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
            db_status = db_reader.GetBlockHashByTransactionHash(tx.hash(), strBlockHash);
            if (DBStatus::DB_SUCCESS != db_status)
            {
                ERRORLOG("Get pledge block hash error");
                continue;
            }

            std::string serBlock;
            db_status = db_reader.GetBlockByBlockHash(strBlockHash, serBlock);
            if (db_status != 0)
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
                nlohmann::json data_json = nlohmann::json::parse(tx.data());
                pItem->set_detail(data_json["TxInfo"].dump());
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
												std::make_pair(-2, "base58 addr invalid !"), 
												std::make_pair(-3, "GetBonusAddr failed !"),
												std::make_pair(-4, "GetBonusAddrInvestUtxos failed !"),
                                                std::make_pair(-5, "No Invest"),
												};

	return errInfo;												
}

int HandleGetInvestListReq(const std::shared_ptr<GetInvestListReq>& req, const MsgData & msgdata)
{
	auto errInfo = GetInvestListReqCode();
    GetInvestListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetInvestListAck>(msgdata, errInfo, ack, ret);
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

    string address = req->address();
    if (address.empty() || !CheckBase58Addr(address))
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
												std::make_pair(-1,  "The addr is empty / Base58 addr invalid"), 
												std::make_pair(-12, "GetUtxos : GetUtxoHashsByAddress failed !"),
												};

	return errInfo;												
}
int HandleGetUtxoReq(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetUtxoReqCode();
    GetUtxoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetUtxoAck>(msgdata, errInfo, ack, ret);
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
    
    string address = req->addr();
    if (address.empty() || !CheckBase58Addr(address))
    {
        return -1;
    }

    ack.set_addr(address);

    DBReader db_reader;
    std::vector<std::string> addresses;
    auto db_status = db_reader.GetInvestAddrsByBonusAddr(address,addresses);
    if (db_status != DBStatus::DB_SUCCESS)
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
        db_status = db_reader.GetBonusAddrInvestUtxosByBonusAddr(address,addr,utxos);
        if (db_status != DBStatus::DB_SUCCESS)
        {
            return -4;
        }
        for (auto &item : utxos) 
        {
            std::string strTxRaw;
            if (DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(item, strTxRaw))
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
												std::make_pair(-1, "The addr is empty / Base58 addr invalid"), 
												std::make_pair(-2, "GetInvestAddrsByBonusAddr failed !"), 
												std::make_pair(-3, "No Invested addrs !"),
                                                std::make_pair(-4, "GetBonusAddrInvestUtxos failed !"),
                                                std::make_pair(-5, "GetTransactionByHash failed !"),
												};

	return errInfo;												
}
int HandleGetAllInvestAddressReq(const std::shared_ptr<GetAllInvestAddressReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetAllInvestAddressReqCode();
    GetAllInvestAddressAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetAllInvestAddressAck>(msgdata, errInfo, ack, ret);
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
    
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);

    for (auto &node : nodelist)
	{
        StakeNode* pStakeNode =  ack.add_list();
        pStakeNode->set_addr(node.base58address);
        pStakeNode->set_name(node.name);
        pStakeNode->set_height(node.height);
        pStakeNode->set_identity(node.identity);
        pStakeNode->set_logo(node.logo);
        pStakeNode->set_ip(string(IpPort::ipsz(node.public_ip)) );
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
int HandleGetAllStakeNodeListReq(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetAllStakeNodeListReqCode();
    GetAllStakeNodeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetAllStakeNodeListAck>(msgdata, errInfo, ack, ret);
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

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if (!CheckBase58Addr(defaultAddr))
    {
        return -1;
    }

    std::map<std::string, uint64_t> abnormal_addr_list;
    std::unordered_map<std::string, uint64_t> addr_sign_cnt;
    uint64_t cur_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    ca_algorithm::GetAbnormalSignAddrListByPeriod(cur_time, abnormal_addr_list, addr_sign_cnt);
    if(abnormal_addr_list.find(defaultAddr) != abnormal_addr_list.end())
    {
        return -2;
    }

    for (auto & item : addr_sign_cnt)
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
                                                std::make_pair(-4, "BounsdAddrs < 1 || BounsdAddrs > 999"),
                                                std::make_pair(-5, "GetBonusAddrInvestUtxosByBonusAddr failed !"),
                                                std::make_pair(-6, "GetTransactionByHash failed !"),
                                                std::make_pair(-7, "Parse tx failed !"),
                                                std::make_pair(-8, "Total amount invested < 5000 !"),
                                                std::make_pair(-9, "GetSignUtxoByAddr failed !"),
												};

	return errInfo;												
}
int HandleGetSignCountListReq(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetSignCountListReqCode();
    GetSignCountListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetSignCountListAck>(msgdata, errInfo, ack, ret);
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
    DBReader db_reader;
    uint64_t height = 0;
    if(db_reader.GetBlockTop(height) != DBStatus::DB_SUCCESS)
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
int HandleGetHeightReq(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetHeightReqCode();
    GetHeightAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetHeightAck>(msgdata, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetHeightReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 10000;
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

    if (!CheckBase58Addr(addr))
    {
        return -2;
    }

    ack.set_bonusaddr(addr);
    uint64_t cur_time=MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    std::map<std::string, uint64_t> values;
    int ret = ca_algorithm::CalcBonusValue(cur_time, addr, values);
    if (ret != 0)
    {
        return ret -= 10;
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
                                                std::make_pair(-2, "base58 is invalid !"),
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

int HandleGetBonusListReq(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetBonusListReqCode();
    GetBonusListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetBonusListAck>(msgdata, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetBonusListReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 1000;
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

int SendCheckTxReq(const std::shared_ptr<IsOnChainReq>& msg,  IsOnChainAck & ack)
{
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();

    std::string msg_id;
    std::map<std::string, uint32_t> successHash;
    int send_num = nodelist.size();
    if (!GLOBALDATAMGRPTR.CreateWait(10, send_num * 0.8, msg_id))
    {
        return -1;
    }

    CheckTxReq req;
    req.set_version(global::kVersion);
    req.set_msg_id(msg_id);
    for(auto & hash : msg->txhash())
    {
        req.add_txhash(hash);
        successHash.insert(std::make_pair(hash, 0));
    }

    for (auto &node : nodelist)
    {
        net_send_message<CheckTxReq>(node.base58address, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msg_id, ret_datas))
    {
        return -2;
    }


    CheckTxAck copyAck;

    std::multimap<std::string, int> tx_flag_hashs;

    for (auto &ret_data : ret_datas)
    {
        copyAck.Clear();
        if (!copyAck.ParseFromString(ret_data))
        {
            continue;
        }
        
        for(auto & flag_hash : copyAck.flaghash())
        {
            tx_flag_hashs.insert(std::make_pair(flag_hash.hash(),flag_hash.flag()));
        }
    }



    for(auto & item : tx_flag_hashs)
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

    ack.set_version(global::kVersion);
    ack.set_time(msg->time());

    for(auto & success : successHash)
    {
        SuccessRate * alsuc = ack.add_percentage();

        alsuc->set_hash(success.first);
        double rate = (double)success.second / (double)(send_num * 0.8);
        DEBUGLOG("SendCheckTxReq = {} , == {}, rate = ", success.second, send_num, rate);
        alsuc->set_rate(rate);

    }


    return 0;
}

std::map<int32_t, std::string> GetOnChianReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get  List Success"), 
												std::make_pair(-1, "version unCompatible"), 
												};
	return errInfo;												
}

int HandleIsOnChainReq(const std::shared_ptr<IsOnChainReq>& req, const MsgData & msgdata)
{
   
    auto errInfo = GetOnChianReqCode();
    IsOnChainAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<IsOnChainAck>(msgdata, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = SendCheckTxReq(req, ack);


    return ret;

}

int GetRestInvestAmountReqImpl(const std::shared_ptr<GetRestInvestAmountReq>& msg,  GetRestInvestAmountAck & ack)
{
    // The node to be invested can only be invested by 999 people at most
    uint64_t invest_amount = 0;
    DBReader db_reader;
    std::vector<string> addresses;
    auto status = db_reader.GetInvestAddrsByBonusAddr(msg->base58(), addresses);
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

    // The node to be invested can only be be invested 100000 DON at most
    uint64_t sum_invest_amount = 0;
    for (auto &address : addresses)
    {
        std::vector<string> utxos;
        if (db_reader.GetBonusAddrInvestUtxosByBonusAddr(msg->base58(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetBonusAddrInvestUtxosByBonusAddr failed!");
            return -3;
        }

        for (const auto &utxo : utxos)
        {
            std::string strTx;
            if (db_reader.GetTransactionByHash(utxo, strTx) != DBStatus::DB_SUCCESS)
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
                    sum_invest_amount += vout.value();
                    break;
                }
            }
        }
    }
    invest_amount = (65000 * global::ca::kDecimalNum) - sum_invest_amount;
    ack.set_base58(msg->base58());
    ack.set_amount(invest_amount);

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

int HandleGetRestInvestAmountReq(const std::shared_ptr<GetRestInvestAmountReq>& req, const MsgData & msgdata)
{
    auto errInfo = GetRestInvestAmountReqCode();
    GetRestInvestAmountAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        ReturnAckCode<GetRestInvestAmountAck>(msgdata, errInfo, ack, ret);
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
    ca_register_callback<GetBlockReq>(HandleGetBlockReq);
    ca_register_callback<GetBalanceReq>(HandleGetBalanceReq);
    ca_register_callback<GetNodeInfoReq>(HandleGetNodeInfoReqReq);
	ca_register_callback<GetStakeListReq>(HandleGetStakeListReq);
	ca_register_callback<GetInvestListReq>(HandleGetInvestListReq);
    ca_register_callback<GetUtxoReq>(HandleGetUtxoReq);
    ca_register_callback<GetAllInvestAddressReq>(HandleGetAllInvestAddressReq);
    ca_register_callback<GetAllStakeNodeListReq>(HandleGetAllStakeNodeListReq);
    ca_register_callback<GetSignCountListReq>(HandleGetSignCountListReq);
    ca_register_callback<GetHeightReq>(HandleGetHeightReq);
    ca_register_callback<GetBonusListReq>(HandleGetBonusListReq);
    ca_register_callback<GetSDKReq>(HandleGetSDKAllNeedReq);
    ca_register_callback<IsOnChainReq>(HandleIsOnChainReq);
    ca_register_callback<GetRestInvestAmountReq>(HandleGetRestInvestAmountReq);

}

