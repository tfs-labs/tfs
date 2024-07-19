

#include "api/interface/rpc_tx.h"
#include "api/rpc_error.h"

#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/contract.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/tfs_wasmtime.h"
#include "ca/block_helper.h"
#include "ca/double_spend_cache.h"
#include "ca/block_helper.h"
#include "utils/json.hpp"
#include "utils/console.h"
#include "utils/tmp_log.h"
#include "utils/time_util.h"
#include "utils/string_util.h"
#include "utils/tfs_bench_mark.h"
#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/contract_utils.h"

#include "include/logging.h"

#include "db/db_api.h"
#include "ca/evm/evm_environment.h"
#include "ca/evm/evm_manager.h"
#include "ca.h"

int TxHelper::GetTxUtxoHeight(const CTransaction &tx, uint64_t& txUtxoHeight)
{
	DBReader dbReader;
	bool flag = false;
	txUtxoHeight = 0;
    for(const auto& vin : tx.utxo().vin())
    {
        for (auto & prevout : vin.prevout())
        {
            std::string blockHash;
            if(DBStatus::DB_SUCCESS != dbReader.GetBlockHashByTransactionHash(prevout.hash(), blockHash))
            {
				return -1;
            }
			uint32_t blockHeight;
			if(DBStatus::DB_SUCCESS != dbReader.GetBlockHeightByBlockHash(blockHash, blockHeight))
			{
				return -2;
			}
			if(txUtxoHeight <= blockHeight )
			{
				txUtxoHeight = blockHeight;
				flag = true;
			}
        }
    }
	if(!flag)
	{
		return -3;
	}
	return 0;
}

std::vector<std::string> TxHelper::GetTxOwner(const CTransaction& tx)
{
	std::vector<std::string> address;
	for (int i = 0; i < tx.utxo().vin_size(); i++)
	{
		const CTxInput & txin = tx.utxo().vin(i);
		auto pub = txin.vinsign().pub();
		std::string addr = GenerateAddr(pub);
		auto res = std::find(std::begin(address), std::end(address), addr);
		if (res == std::end(address))
		{
			address.push_back(addr);
		}
	}

	return address;
}

int TxHelper::GetUtxos(const std::string & address, std::vector<TxHelper::Utxo>& utxos)
{
	if (address.empty())
	{
		return -1;
	}
	utxos.clear();

	DBReader dbReader;
	{
		std::vector<std::string> vecUtxoHashs;
		if (DBStatus::DB_SUCCESS != dbReader.GetUtxoHashsByAddress(address, vecUtxoHashs))
		{
			ERRORLOG("GetUtxoHashsByAddress failed!");
			return -2;
		}

		std::sort(vecUtxoHashs.begin(), vecUtxoHashs.end());
		vecUtxoHashs.erase(std::unique(vecUtxoHashs.begin(), vecUtxoHashs.end()), vecUtxoHashs.end()); 

		for (const auto& hash : vecUtxoHashs)
		{
			bool flag = false;
			TxHelper::Utxo utxo;
			utxo.hash = hash;
			utxo.addr = address;
			utxo.value = 0;
			utxo.n = 0; 
			std::string balance ;
			if (dbReader.GetUtxoValueByUtxoHashs(hash, address, balance) != 0)
			{
				ERRORLOG("GetTransactionByHash failed!");
				continue;
			}
			//	If I get the pledged utxo, I will use it together
			std::string underline = "_";
			std::vector<std::string> utxoValues;

			if(balance.find(underline) != std::string::npos)
			{
				StringUtil::SplitString(balance, "_", utxoValues);
				
				for(int i = 0; i < utxoValues.size(); ++i)
				{
					utxo.value += std::stol(utxoValues[i]);
				}
			}
			else
			{
				utxo.value = std::stol(balance);
			}
			if(!flag)
			{
				utxos.push_back(utxo);
			}
		}
	}
	return 0;
}

const uint32_t TxHelper::kMaxVinSize = 1000;
int TxHelper::Check(const std::vector<std::string>& fromAddr,
					uint64_t height
					)
{
	//  Fromaddr cannot be empty
	if(fromAddr.empty())
	{
		ERRORLOG("Fromaddr is empty!");		
		return -1;
	}

	//  Fromaddr cannot have duplicate elements
	std::vector<std::string> tempfromAddr = fromAddr;
	std::sort(tempfromAddr.begin(),tempfromAddr.end());
	auto iter = std::unique(tempfromAddr.begin(),tempfromAddr.end());
	tempfromAddr.erase(iter,tempfromAddr.end());
	if(tempfromAddr.size() != fromAddr.size())
	{
		ERRORLOG("Fromaddr have duplicate elements!");		
		return -2;
	}

	DBReader dbReader;
	std::map<std::string, std::vector<std::string>> identities;

	//  Fromaddr cannot be a non  address
	for(auto& from : fromAddr)
	{
		if (!isValidAddress(from))
		{
			ERRORLOG("Fromaddr is a non  address! from:{}", from);
			return -3;
		}

		std::vector<std::string> utxoHashs;
		if (DBStatus::DB_SUCCESS != dbReader.GetUtxoHashsByAddress(from, utxoHashs))
		{
			ERRORLOG(RED "GetUtxoHashsByAddress failed!" RESET);
			return -4;
		}

		auto found = identities.find(from);
		if (found == identities.end())
		{
			identities[from] = std::vector<std::string>{};
		}

		identities[from] = utxoHashs;
	}

	if (height == 0)
	{
		ERRORLOG("height is zero!");
		return -5;
	}

	return 0;
}


int TxHelper::FindUtxo(const std::vector<std::string>& fromAddr,
						const uint64_t needUtxoAmount,
						uint64_t& total,
						std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare>& setOutUtxos, bool isFindUtxo
						)
{
	//  Count all utxo
	std::vector<TxHelper::Utxo> Utxos;

	DBReader dbReader;
	for (const auto& addr : fromAddr)
	{
		std::vector<std::string> vecUtxoHashs;
		if (DBStatus::DB_SUCCESS != dbReader.GetUtxoHashsByAddress(addr, vecUtxoHashs))
		{
			ERRORLOG("GetUtxoHashsByAddress failed!");
			return -1;
		}
		//  duplicate removal
		std::sort(vecUtxoHashs.begin(), vecUtxoHashs.end());
		vecUtxoHashs.erase(std::unique(vecUtxoHashs.begin(), vecUtxoHashs.end()), vecUtxoHashs.end()); 

		for (const auto& hash : vecUtxoHashs)
		{

			bool flag = false;

			TxHelper::Utxo utxo;
			utxo.hash = hash;
			utxo.addr = addr;
			utxo.value = 0;
			utxo.n = 0; //	At present, the n of utxo is all 0

			std::string balance ;
			if (dbReader.GetUtxoValueByUtxoHashs(hash, addr, balance) != 0)
			{
				ERRORLOG("GetTransactionByHash failed!");
				continue;
			}

			//If I get the pledged utxo, I will use it together
			std::string underline = "_";
			std::vector<std::string> utxoValues;

			if(balance.find(underline) != std::string::npos)
			{
				StringUtil::SplitString(balance, "_", utxoValues);
				
				for(int i = 0; i < utxoValues.size(); ++i)
				{
					utxo.value += std::stol(utxoValues[i]);
				}

			}
			else
			{
				utxo.value = std::stol(balance);
			}

			if(!flag)
			{
				Utxos.push_back(utxo);
			}
		}

	}

	std::sort(Utxos.begin(), Utxos.end(),[](const TxHelper::Utxo & u1, const TxHelper::Utxo & u2){
		return u1.value > u2.value;
	});

	total = 0;

	if(setOutUtxos.size() < needUtxoAmount)
	{
		//  Fill other positions with non-0
		auto it = Utxos.begin();
		while (it != Utxos.end())
		{
			if (setOutUtxos.size() == needUtxoAmount)
			{
				break;
			}
			total += it->value;

			setOutUtxos.insert(*it);
			++it;
		}
	}

	if(isFindUtxo)
	{
		int ret = SendConfirmUtxoHashReq(setOutUtxos);
		if(ret != 0)
		{
			ERRORLOG("SendConfirmUtxoHashReq error ret : {}", ret);
			return -2;
		}
	}

	return 0;
}


static int GetRandomNodeLists(std::vector<Node>& randomNodeLists)
{
	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    auto nodelistsize = nodelist.size();
    if(nodelistsize == 0)
    {
        ERRORLOG("Nodelist Size is empty");
        return -1;
    }
    DEBUGLOG("Nodelist Size {}",nodelistsize);
    
	uint64_t height = 0;
	DBReader db_reader;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(height))
    {
        return -2; 
    }

    std::vector<Node> filterHeightNodeLists;
    for(auto &item : nodelist)
    {
        if(item.height >= height)
        {
            filterHeightNodeLists.push_back(item);
        }
    }
	auto send_num = filterHeightNodeLists.size() >= 100 ? 100 : filterHeightNodeLists.size();
	if(send_num == 0)
	{
		return -3;
	}

	std::default_random_engine e;
  	std::shuffle(filterHeightNodeLists.begin(), filterHeightNodeLists.end(), e);

	for(int i = 0; i < send_num; i++) {
		randomNodeLists.push_back(filterHeightNodeLists[i]);
	}

	return 0;
}

int TxHelper::SendConfirmUtxoHashReq(const std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare>& setOutUtxos){
	std::vector<Node> randomNodeLists;
    int ret = GetRandomNodeLists(randomNodeLists);
	if(ret != 0)
	{
		ERRORLOG("GetRandomNodeLists ret : {}", ret);
		return -1;
	}
    //send_size
    std::string msgId;
    std::map<std::string, uint32_t> successHash;
    if (!GLOBALDATAMGRPTR.CreateWait(5, randomNodeLists.size() * 0.8, msgId))
    {
        ERRORLOG("SendConfirmTransactionReq CreateWait is error");
        return -2;
    }

    GetUtxoHashReq req;
    req.set_version(global::kVersion);
    req.set_msg_id(msgId);

	std::string connectHash;
	for(auto begin = setOutUtxos.begin(); begin != setOutUtxos.end(); ++begin)
	{
		req.add_utxohash(begin->hash);
        successHash.insert(std::make_pair(begin->hash, 0));
	}

    for (auto &node : randomNodeLists)
    {
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, node.address))
        {
            ERRORLOG("SendConfirmTransactionReq AddResNode is error");
            return -3;
        }
        NetSendMessage<GetUtxoHashReq>(node.address, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, ret_datas))
    {
        ERRORLOG("SendConfirmTransactionReq WaitData is error");
        return -4;
    }

    GetUtxoHashAck copyAck;
    std::multimap<std::string, int> txFlagHashs;
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
        }
    }

    for(auto & item : txFlagHashs)
    {
        for(auto & success : successHash)
        {
            if(item.second == 1 && item.first == success.first)
            {
                success.second++;
            }
        }
    }

    for(auto & success : successHash)
    {
        double rate = (double)success.second / (double)(ret_datas.size());

        if(rate - 0.5 < 0)
        {
            ERRORLOG("SendConfirmTransactionReq = {} , rate = {}", success.second, rate);
            return -5;
        }
    }

    return 0;
}

int TxHelper::CreateTxTransaction(const std::vector<std::string>& fromAddr,
									const std::map<std::string, int64_t> & toAddr,
									const std::string& encodedInfo,
									uint64_t height,
									CTransaction& outTx,
									TxHelper::vrfAgentType & type,
									Vrf & info, bool isFindUtxo)
{
	MagicSingleton<TFSbenchmark>::GetInstance()->IncreaseTransactionInitiateAmount();
	//  Check parameters
	int ret = Check(fromAddr, height);
	if (ret != 0)
	{
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		ret -= 100;
		return ret;
	}

	if(toAddr.empty())
	{
		ERRORLOG("to addr is empty");
		return -1;
	}	

	for (auto& addr : toAddr)
	{
		if (!isValidAddress(addr.first))
		{
			ERRORLOG(RED "To address is not  address!" RESET);
			return -2;
		}

		for (auto& from : fromAddr)
		{
			if (addr.first == from)
			{
				ERRORLOG(RED "From address and to address is equal!" RESET);
				return -3;
			}
		}
		
		if (addr.second <= 0)
		{
			ERRORLOG(RED "Value is zero!" RESET);
			return -4;
		}
	}

	
	uint64_t amount = 0;// Transaction fee
	for (auto& i : toAddr)
	{
		amount += i.second;    
	}
	uint64_t expend = amount;

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = FindUtxo(fromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
		ret -= 200;
		return ret;
	}
	if (setOutUtxos.empty())
	{
		ERRORLOG(RED "Utxo is empty!" RESET);
		return -5;
	}

	outTx.Clear();

	CTxUtxo * txUtxo = outTx.mutable_utxo();
	
	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	if (setTxowners.empty())
	{
		ERRORLOG(RED "Tx owner is empty!" RESET);
		return -6;
	}

	DoubleSpendCache::doubleSpendsuc used;
	uint32_t n = 0;
	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
				used.utxoVec.push_back(utxo.hash);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			ERRORLOG("sign fail");
			return -8;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	outTx.set_data("");
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	uint64_t gas = 0;
	std::map<std::string, int64_t> targetAddrs = toAddr;
	targetAddrs.insert(make_pair(*fromAddr.rbegin(), total - expend));
	targetAddrs.insert(make_pair(global::ca::kVirtualBurnGasAddr,gas));
	if(GenerateGas(outTx, targetAddrs.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -9;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

	GetTxStartIdentity(height,currentTime,type);
	DEBUGLOG("GetTxStartIdentity currentTime = {} type = {}",currentTime ,type);
	expend +=  gas;
	
	//Judge whether utxo is enough
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -10;
	}

	// Fill vout
	for(auto & to : toAddr)
	{
		CTxOutput * vout = txUtxo->add_vout();
		vout->set_addr(to.first);
		vout->set_value(to.second);
	}
	CTxOutput * voutFromAddr = txUtxo->add_vout();
	voutFromAddr->set_addr(*fromAddr.rbegin());
	voutFromAddr->set_value(total - expend);
	
	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{		
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			ERRORLOG("addd mutil sign fail");
			return -11;
		}
	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeTx);
	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		std::string identity = TxHelper::GetIdentityNodes();
		if (identity.empty())
        {
            identity = TxHelper::GetEligibleNodes();
        }
		DEBUGLOG("DropshippingTx identity:{}", identity);
		outTx.set_identity(identity);
	}
	else
	{
		std::string allUtxos;
		for(auto & utxo:setOutUtxos)
		{
			allUtxos += utxo.hash;
		}
		std::string id;
		int count = 0;
		while(count < 6)
		{
			count++;

			// Candidate packers are selected based on all utxohashes
			std::string rawData = allUtxos + std::to_string(currentTime);
			int ret = GetBlockPackager(id, rawData, info);
			if(ret != 0)
			{
				if(ret == -6)
				{
					currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
					continue;
				}
				return ret;
			}
			outTx.set_time(currentTime);
			break;
		}
		if(count == 6)
		{
			ERRORLOG("Packager = {} cannot be the transaction initiator", id);
			return -12;
		}
		outTx.set_identity(id);
	}

	DEBUGLOG("GetTxStartIdentity tx time = {}, package = {}", outTx.time(), outTx.identity());
	
	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);
	used.time = outTx.time();


	if(MagicSingleton<DoubleSpendCache>::GetInstance()->AddFromAddr(std::make_pair(*fromAddr.begin(),used)))
	{
		ERRORLOG("utxo is using!");
		return -7;
	}

	INFOLOG( "Transaction Start Time = {}");
	return 0;
}

int TxHelper::CreateStakeTransaction(const std::string & fromAddr,
										uint64_t stakeAmount,
										const std::string& encodedInfo,
										uint64_t height,
										TxHelper::pledgeType pledgeType,
										CTransaction & outTx,
										std::vector<TxHelper::Utxo> & outVin,
										TxHelper::vrfAgentType &type ,Vrf & information, double commissionRate, bool isFindUtxo)
{
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = Check(vecfromAddr, height);
	if(ret != 0)
	{
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		ret -= 100;
		return ret;
	}

	if (!isValidAddress(fromAddr)) 
	{
		ERRORLOG(RED "From address invlaid!" RESET);
		return -1;
	}

	if(stakeAmount == 0 )
	{
		ERRORLOG(RED "Stake amount is zero !" RESET);
		return -2;		
	}
	
	if(stakeAmount < 1000 * global::ca::kDecimalNum)
	{
		std::cout << "The pledge amount must be greater than 1000 !" << std::endl;
		return -3;
	}

	std::string strStakeType;
	if (pledgeType == TxHelper::pledgeType::kPledgeType_Node)
	{
		strStakeType = global::ca::kStakeTypeNet;
	}
	else
	{
		ERRORLOG(RED "Unknown stake type!" RESET);
		return -4;
	}

	DBReader dbReader;
	std::vector<std::string> stakeUtxos;
    auto dbret = dbReader.GetStakeAddressUtxo(fromAddr,stakeUtxos);
	if(dbret != DBStatus::DB_NOT_FOUND)
	{
		std::cout << "There has been a pledge transaction before !" << std::endl;
		return -5;
	}
	uint64_t expend = stakeAmount;
	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = FindUtxo(vecfromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
		ret -= 200;
		return ret;
	}

	if (setOutUtxos.empty())
	{
		ERRORLOG(RED "Utxo is empty!" RESET);
		return -6;
	}

	outTx.Clear();
	CTxUtxo * txUtxo = outTx.mutable_utxo();
	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	
	if (setTxowners.size() != 1)
	{
		ERRORLOG(RED "Tx owner is invalid!" RESET);
		return -7;
	}

	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			return -8;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	nlohmann::json txInfo;
	txInfo["StakeType"] = strStakeType;
	txInfo["StakeAmount"] = stakeAmount;
	txInfo["CommissionRate"] = commissionRate;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	//  Calculate total expenditure
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::kVirtualStakeAddr, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total - expend));
	toAddr.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas));
	
	if(GenerateGas(outTx, toAddr.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -9;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	GetTxStartIdentity(height,currentTime,type);
	expend += gas;
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -10;
	}

	CTxOutput * vout = txUtxo->add_vout(); 
	vout->set_addr(global::ca::kVirtualStakeAddr);
	vout->set_value(stakeAmount);

	CTxOutput * voutFromAddr = txUtxo->add_vout();
	voutFromAddr->set_addr(fromAddr);
	voutFromAddr->set_value(total - expend);

	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{	
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			return -11;
		}
	}
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeStake);

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		std::string identity = TxHelper::GetIdentityNodes();
		if (identity.empty())
        {
            identity = TxHelper::GetEligibleNodes();
        }
		DEBUGLOG("DropshippingTx identity:{}", identity);
		outTx.set_identity(identity);
	}
	else
	{
		// Select dropshippers
		std::string allUtxos;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		allUtxos += std::to_string(currentTime);
		
		std::string id;
    	int ret= GetBlockPackager(id,allUtxos,information);
    	if(ret!=0){
        	return ret;
    	}
		outTx.set_identity(id);
		
	}

	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);
	return 0;

}

int TxHelper::CreatUnstakeTransaction(const std::string& fromAddr,
										const std::string& utxoHash,
										const std::string& encodedInfo,
										uint64_t height,
										CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin
										,TxHelper::vrfAgentType &type ,Vrf & information, bool isFindUtxo)
{
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = Check(vecfromAddr, height);
	if(ret != 0)
	{
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		ret -= 100;
		return ret;
	}

	if (!isValidAddress(fromAddr))
	{
		ERRORLOG(RED "FromAddr is not normal  addr." RESET);
		return -1;
	}


	uint64_t stakeAmount = 0;
	ret = IsQualifiedToUnstake(fromAddr, utxoHash, stakeAmount);
	if(ret != 0)
	{
		ERRORLOG(RED "FromAddr is not qualified to unstake! The error code is {}." RESET, ret);
		ret -= 200;
		return ret;
	}	

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	//  The number of utxos to be searched here needs to be reduced by 1 \
	because a VIN to be redeem is from the pledged utxo, so just look for 99
	ret = FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo); 
	if (ret != 0)
	{
		ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
		ret -= 300;
		return ret;
	}

	if (setOutUtxos.empty())
	{
		ERRORLOG(RED "Utxo is empty!" RESET);
		return -2;
	}

	outTx.Clear();

	CTxUtxo * txUtxo = outTx.mutable_utxo();
	
	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	if (setTxowners.empty())
	{
		ERRORLOG(RED "Tx owner is empty!" RESET);
		return -3;
	}

	{
		//  Fill vin
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);

		std::string serVinHash = Getsha256hash(txin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(fromAddr, serVinHash, signature, pub) != 0)
		{
			return -4;
		}

		CSign * vinSign = txin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}


	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 1;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			return -5;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	nlohmann::json txInfo;
	txInfo["UnstakeUtxo"] = utxoHash;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	
	outTx.set_version(global::ca::kCurrentTransactionVersion);

	uint64_t gas = 0;
	//The filled quantity only participates in the calculation and does not affect others
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::kVirtualStakeAddr, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total));
	toAddr.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas));
	
	
	if(GenerateGas(outTx, toAddr.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -6;
	}

	//  Calculate total expenditure
	uint64_t gasTtoal =  gas;

	uint64_t cost = 0;// Packing fee

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	GetTxStartIdentity(height,currentTime,type);

	uint64_t expend = gas;
	// Judge whether there is enough money
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -7;
	}

	//  Fill vout
	CTxOutput* txoutToAddr = txUtxo->add_vout();
	txoutToAddr->set_addr(fromAddr);      	//  Release the pledge to my account number
	txoutToAddr->set_value(stakeAmount);

	txoutToAddr = txUtxo->add_vout();
	txoutToAddr->set_addr(fromAddr);  		//  Give myself the rest
	txoutToAddr->set_value(total - expend);

	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{	
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			return -8;
		}
	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeUnstake);

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		std::string identity = TxHelper::GetIdentityNodes();
		if (identity.empty())
        {
            identity = TxHelper::GetEligibleNodes();
        }
		DEBUGLOG("DropshippingTx identity:{}", identity);
		outTx.set_identity(identity);
	}
	else{
		
		// Select dropshippers
		std::string allUtxos = utxoHash;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		allUtxos += std::to_string(currentTime);
		
		std::string id;
    	int ret= GetBlockPackager(id,allUtxos,information);
    	if(ret!=0){
        	return ret;
    	}
		outTx.set_identity(id);
	}

	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);

	return 0;
}

int TxHelper::CreateInvestTransaction(const std::string & fromAddr,
										const std::string& toAddr,
										uint64_t investAmount,
										const std::string& encodedInfo,
										uint64_t height,
										TxHelper::InvestType investType,
										CTransaction & outTx,
										std::vector<TxHelper::Utxo> & outVin
										,TxHelper::vrfAgentType &type ,Vrf & information, bool isFindUtxo)
{
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = Check(vecfromAddr, height);
	if(ret != 0)
	{
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		ret -= 100;
		return ret;
	}

	//  Neither fromaddr nor toaddr can be a virtual account
	if (!isValidAddress(fromAddr))
	{
		ERRORLOG(RED "FromAddr is not normal  addr." RESET);
		return -1;
	}

	if (!isValidAddress(toAddr))
	{
		ERRORLOG(RED "To address is not  address!" RESET);
		return -2;
	}

	if(investAmount < 35 * global::ca::kDecimalNum){
		ERRORLOG("Investment amount exceeds the limit!");
		return -3;
	}

	ret = CheckInvestQualification(fromAddr, toAddr, investAmount);
	if(ret != 0)
	{
		ERRORLOG(RED "FromAddr is not qualified to invest! The error code is {}." RESET, ret);
		ret -= 200;
		return ret;
	}	
	std::string strinvestType;
	if (investType ==  TxHelper::InvestType::kInvestType_NetLicence)
	{
		strinvestType = global::ca::kInvestTypeNormal;
	}
	else
	{
		ERRORLOG(RED "Unknown invest type!" RESET);
		return -4;
	}
	
	//  Find utxo
	uint64_t total = 0;
	uint64_t expend = investAmount;

	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = FindUtxo(vecfromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
		ret -= 300;
		return ret;
	}
	if (setOutUtxos.empty())
	{
		ERRORLOG(RED "Utxo is empty!" RESET);
		return -5;
	}

	outTx.Clear();

	CTxUtxo * txUtxo = outTx.mutable_utxo();
	
	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	if (setTxowners.empty())
	{
		ERRORLOG(RED "Tx owner is empty!" RESET);
		return -6;
	}

	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			return -7;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	nlohmann::json txInfo;
	txInfo["InvestType"] = strinvestType;
	txInfo["BonusAddr"] = toAddr;
	txInfo["InvestAmount"] = investAmount;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	uint64_t gas = 0;	
	//  Calculate total expenditure
	std::map<std::string, int64_t> toAddrs;
	toAddrs.insert(std::make_pair(global::ca::kVirtualStakeAddr, investAmount));
	toAddrs.insert(std::make_pair(fromAddr, total - expend));
	toAddrs.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas));
	
	if(GenerateGas(outTx, toAddrs.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -8;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	GetTxStartIdentity(height,currentTime,type);
	expend += gas;
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -9;
	}

	CTxOutput * vout = txUtxo->add_vout(); 
	vout->set_addr(global::ca::kVirtualInvestAddr);
	vout->set_value(investAmount);

	CTxOutput * voutFromAddr = txUtxo->add_vout();
	voutFromAddr->set_addr(fromAddr);
	voutFromAddr->set_value(total - expend);

	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{	
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			return -10;
		}
	}
	
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeInvest);

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		std::string identity = TxHelper::GetIdentityNodes();
		if (identity.empty())
        {
            identity = TxHelper::GetEligibleNodes();
        }
		DEBUGLOG("DropshippingTx identity:{}", identity);
		outTx.set_identity(identity);
	}
	else{
		
		// Select dropshippers
		std::string allUtxos;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		allUtxos += std::to_string(currentTime);
		
		std::string id;
    	int ret= GetBlockPackager(id,allUtxos,information);
    	if(ret!=0){
        	return ret;
    	}
		outTx.set_identity(id);
	}

	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);

	return 0;
}

int TxHelper::CreateDisinvestTransaction(const std::string& fromAddr,
										const std::string& toAddr,
										const std::string& utxoHash,
										const std::string& encodedInfo,
										uint64_t height,
										CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin
										,TxHelper::vrfAgentType &type ,Vrf & information, bool isFindUtxo)
{
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = Check(vecfromAddr, height);
	if(ret != 0)
	{
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		ret -= 100;
		return ret;
	}

	if (!isValidAddress(fromAddr))
	{
		ERRORLOG(RED "FromAddr is not normal  addr." RESET);
		return -1;
	}

	if (!isValidAddress(toAddr))
	{
		ERRORLOG(RED "To address is not  address!" RESET);
		return -2;
	}

	uint64_t investedAmount = 0;
	if(IsQualifiedToDisinvest(fromAddr, toAddr, utxoHash, investedAmount) != 0)
	{
		ERRORLOG(RED "FromAddr is not qualified to divest!." RESET);
		return -3;
	}

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	//  The utxo quantity sought here needs to be reduced by 1
	ret = FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo); 
	if (ret != 0)
	{
		ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
		ret -= 300;
		return ret;
	}
	if (setOutUtxos.empty())
	{
		ERRORLOG(RED "Utxo is empty!" RESET);
		return -4;
	}

	outTx.Clear();

	CTxUtxo * txUtxo = outTx.mutable_utxo();

	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	if (setTxowners.empty())
	{
		ERRORLOG(RED "Tx owner is empty!" RESET);
		return -5;
	}

	{
		//  Fill vin
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);

		std::string serVinHash = Getsha256hash(txin->SerializeAsString());
		std::string signature;
		std::string pub;
		ret = TxHelper::Sign(fromAddr, serVinHash, signature, pub);
		if (ret != 0)
		{
			ERRORLOG("invest utxoHash Sign error:{}", ret);
			return -6;
		}

		CSign * vinSign = txin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 1;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			return -7;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}

	nlohmann::json txInfo;
	txInfo["BonusAddr"] = toAddr;
	txInfo["DisinvestUtxo"] = utxoHash;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	//  Calculate total expenditure
	std::map<std::string, int64_t> targetAddrs;
	targetAddrs.insert(std::make_pair(global::ca::kVirtualStakeAddr, investedAmount));
	targetAddrs.insert(std::make_pair(fromAddr, total ));
	targetAddrs.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas ));
	
	
	if(GenerateGas(outTx, targetAddrs.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -8;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	GetTxStartIdentity(height,currentTime,type);
	uint64_t expend = gas;
	
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -9;
	}	

	// Fill vout
	CTxOutput* txoutToAddr = txUtxo->add_vout();
	txoutToAddr->set_addr(fromAddr);      //  Give my account the money I withdraw
	txoutToAddr->set_value(investedAmount);

	txoutToAddr = txUtxo->add_vout();
	txoutToAddr->set_addr(fromAddr);  	  //  Give myself the rest
	txoutToAddr->set_value(total - expend);

	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{	
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			return -10;
		}
	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeDisinvest);

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		std::string identity = TxHelper::GetIdentityNodes();
		if (identity.empty())
        {
            identity = TxHelper::GetEligibleNodes();
        }
		DEBUGLOG("DropshippingTx identity:{}", identity);
		outTx.set_identity(identity);
	}
	else
	{	
		// Select dropshippers
		std::string allUtxos = utxoHash;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		allUtxos += std::to_string(currentTime);
		
		std::string id;
    	int ret= GetBlockPackager(id,allUtxos,information);
    	if(ret!=0){
        	return ret;
    	}
		outTx.set_identity(id);
	}

	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);


	return 0;
}

int TxHelper::CreateBonusTransaction(const std::string& addr,
										const std::string& encodedInfo,
										uint64_t height,
										CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,
										TxHelper::vrfAgentType &type,
										Vrf & information, bool isFindUtxo)
{
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(addr);
	int ret = Check(vecfromAddr, height);
	if(ret != 0)
	{
		ERRORLOG("Check parameters failed");
		ret -= 100;
		return ret;
	}

	if (!isValidAddress(addr))
	{
		ERRORLOG(RED "Default is not normal  addr." RESET);
		return -1;
	}

	DBReader dbReader; 
	std::vector<std::string> utxos;
	uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ret = CheckBonusQualification(addr, curTime);
	if(ret != 0)
	{
		ERRORLOG(RED "Not allowed to Bonus!" RESET);
		return ret - 200;
	}

	std::map<std::string, uint64_t> companyDividend;
    ret=ca_algorithm::CalcBonusValue(curTime, addr, companyDividend);
	if(ret < 0)
	{
		ERRORLOG("Failed to obtain the amount claimed by the investor ret:({})",ret);
		ret-=300;
		return ret;
	}

	uint64_t expend = 0;
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ERRORLOG("TxHelper CreatUnstakeTransaction: FindUtxo failed");
		ret -= 400;
		return ret;
	}
	if (setOutUtxos.empty())
	{
		ERRORLOG("TxHelper CreatUnstakeTransaction: utxo is zero");
		return -2;
	}
	outTx.Clear();
	CTxUtxo * txUtxo = outTx.mutable_utxo();
	//  Fill Vin
	std::set<std::string> setTxowners;
	for (auto & utxo : setOutUtxos)
	{
		setTxowners.insert(utxo.addr);
	}
	if (setTxowners.empty())
	{
		ERRORLOG(RED "Tx owner is empty!" RESET);
		return -3;
	}

	for (auto & owner : setTxowners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : setOutUtxos)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		std::string serVinHash = Getsha256hash(vin->SerializeAsString());
		std::string signature;
		std::string pub;
		if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
		{
			return -4;
		}

		CSign * vinSign = vin->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
	}
	// Fill data
	uint64_t tempCosto=0;
	uint64_t tempNodeDividend=0;
	uint64_t tempTotalClaim=0;

	double tempcommissionRate;
	int rt = ca_algorithm::GetCommissionPercentage(addr, tempcommissionRate);
	if(rt != 0)
	{
		ERRORLOG("GetCommissionPercentage error:{}", rt);
        return -100;
	}
	for(auto company : companyDividend)
	{
		tempCosto=company.second*tempcommissionRate+0.5;
		tempNodeDividend+=tempCosto;
		std::string addr = company.first;
		uint64_t award = company.second - tempCosto;
		tempTotalClaim+=award;		
	}
	tempTotalClaim += tempNodeDividend;

	nlohmann::json txInfo;
	txInfo["BonusAmount"] = tempTotalClaim;
	txInfo["BonusAddrList"] = companyDividend.size() + 2;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	// calculation gas
	uint64_t gas = 0;
	std::map<std::string, int64_t> toAddrs;
	for(const auto & item : companyDividend)
	{
		toAddrs.insert(make_pair(item.first, item.second));
	}
	toAddrs.insert(std::make_pair(global::ca::kVirtualStakeAddr, total - expend));
	toAddrs.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas));

	if(GenerateGas(outTx, toAddrs.size(), gas) != 0)
	{
		ERRORLOG(" gas = 0 !");
		return -5;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	GetTxStartIdentity(height,currentTime,type);
	expend = gas;
	if(total < expend)
	{
		ERRORLOG("The total cost = {} is less than the cost = {}", total, expend);
		return -6;
	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeBonus);
	// Fill vout
	uint64_t costo=0;
	uint64_t nodeDividend=0;
	uint64_t totalClaim=0;
	std::cout << YELLOW << "Claim addr : Claim Amount" << RESET << std::endl;
	std::cout << "Commission: " << tempcommissionRate << std::endl;
	for(auto company : companyDividend)
	{
		costo=company.second*tempcommissionRate+0.5;
		nodeDividend+=costo;
		std::string addr = company.first;
		uint64_t award = company.second - costo;
		totalClaim+=award;
		CTxOutput* txoutToAddr = txUtxo->add_vout();	
		txoutToAddr->set_addr(addr); 
		txoutToAddr->set_value(award);		
		std::cout << company.first << ":" << std::fixed << std::setprecision(8) << static_cast<double>(company.second) / global::ca::kDecimalNum << std::endl;	
	}

	CTxOutput* txoutToAddr = txUtxo->add_vout();
	txoutToAddr->set_addr(addr);
	txoutToAddr->set_value(total - expend + nodeDividend);

	CTxOutput * voutBurn = txUtxo->add_vout();
	voutBurn->set_addr(global::ca::kVirtualBurnGasAddr);
	voutBurn->set_value(gas);


	std::cout << addr << ":" << nodeDividend << std::endl;
	totalClaim+=nodeDividend;
	if(totalClaim == 0)
	{
		ERRORLOG("The claim amount is 0");
		return -7;
	}

	std::string serUtxoHash = Getsha256hash(txUtxo->SerializeAsString());
	for (auto & owner : setTxowners)
	{	
		if (TxHelper::AddMutilSign(owner, outTx) != 0)
		{
			return -8;
		}
	}

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
	}
	else{

		// Select dropshippers
		std::string allUtxos;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		allUtxos += std::to_string(currentTime);
		
		std::string id;
    	int ret= GetBlockPackager(id,allUtxos,information);
    	if(ret!=0){
        	return ret;
    	}
		outTx.set_identity(id);
	}
	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);

	return 0;
}


//int TxHelper::CreateWasmDeployContractTransaction(const std::string &fromaddr, const std::string &OwnerWasmAddr,
//                                            const std::string &code, uint64_t height,
//                                            CTransaction &outTx,TxHelper::vrfAgentType &type, Vrf &information)
//{
//    std::string strOutput;
//	int64_t gasCost = 0;
//	int ret = Wasmtime::DeployWasmContract(fromaddr,code,strOutput,gasCost);
//	if(ret != 0)
//	{
//		ERRORLOG("Wasmone failed to deploy contract! ret : {}", ret);
//        return -1;
//	}
//
//	nlohmann::json jTxInfo;
//    jTxInfo["Version"] = 0;
//    jTxInfo["OwnerWasmAddr"] = OwnerWasmAddr;
//    jTxInfo[Evmone::contractVirtualMachineKeyName] = global::ca::VmType::WASM;
//    jTxInfo[Evmone::contractInputKeyName] = Str2Hex(code);
//    jTxInfo[Evmone::contractOutputKeyName] = strOutput;
//
//	ret = Wasmtime::FillWasmOutTx(fromaddr,global::ca::kVirtualDeployContractAddr, global::ca::TxType::kTxTypeDeployContract ,jTxInfo,  height, gasCost, outTx, type, information,0);
//	if(ret != 0)
//	{
//		ERRORLOG("FillWasmOutTx fail ret :{}", ret);
//		return -2;
//	}
//	return 0;
//}

int TxHelper::MakeEvmDeployContractTransaction(uint64_t height, CTransaction &outTx,
                                               std::vector<std::string> &dirtyContract,
                                               TxHelper::vrfAgentType &type, Vrf &information, EvmHost &host,
                                               evmc_message &message, bool isFindUtxo, const std::string& encodedInfo,
                                               std::function<int(CTransaction &,std::string &)> signFunction)
{
    std::string sender = evm_utils::EvmAddrToString(message.sender);
    std::string recipient = evm_utils::EvmAddrToString(message.recipient);
    nlohmann::json jTxInfo;
    jTxInfo[Evmone::contractVersionKeyName] = 0;
    jTxInfo[Evmone::contractSenderKeyName] = sender;
    jTxInfo[Evmone::contractVirtualMachineKeyName] = global::ca::VmType::EVM;
  	jTxInfo[Evmone::contractRecipientKeyName] = recipient;
    jTxInfo[Evmone::contractInputKeyName] = evm_utils::BytesToString(host.code) ;

    jTxInfo[Evmone::contractBlockTimestampKeyName] = host.tx_context.block_timestamp;
    jTxInfo[Evmone::contractBlockPrevRandaoKeyName] = evm_utils::EvmcUint256beToUint32(host.tx_context.block_prev_randao);

    Evmone::GetCalledContract(host, dirtyContract);
    int ret = Evmone::FillDeployOutTx(sender ,global::ca::kVirtualDeployContractAddr,host.coin_transferrings,
										jTxInfo, host.gas_cost, height, outTx, type, information, encodedInfo, isFindUtxo);
	if (ret != 0)
    {
		ERRORLOG("FillDeployOutTx fail ret : {}", ret);
		return ret - 1000;
	}

	if(signFunction != nullptr)
	{
		ret = signFunction(outTx, sender);
		if (ret!= 0){
			errorL("sig fial %s", ret);
			return -2;
		}
		std::string txHash = Getsha256hash(outTx.SerializeAsString());
		outTx.set_hash(txHash);
	}

    return 0;
}

int
TxHelper::MakeEvmCallContractTransaction(uint64_t height, CTransaction &outTx, std::vector<std::string> &dirtyContract,
                                         TxHelper::vrfAgentType &type, Vrf &information, EvmHost &host,
                                         evmc_message &message, const std::string &strInput, const uint64_t contractTip,
                                         const uint64_t contractTransfer, const std::string &toAddr, bool isFindUtxo,
										 const std::string& encodedInfo,
                                         std::function<int(CTransaction &,std::string &)> signFunction)
{
    std::string sender = evm_utils::EvmAddrToString(message.sender);
    std::string recipient = evm_utils::EvmAddrToString(message.recipient);
    nlohmann::json jTxInfo;
    jTxInfo[Evmone::contractVersionKeyName] = 0;
    jTxInfo[Evmone::contractSenderKeyName] = sender;
    jTxInfo[Evmone::contractRecipientKeyName] = recipient;
    jTxInfo[Evmone::contractVirtualMachineKeyName] = global::ca::VmType::EVM;
    jTxInfo[Evmone::contractInputKeyName] = strInput;

    jTxInfo[Evmone::contractDonationKeyName] = contractTip;
    jTxInfo[Evmone::contractTransferKeyName] = contractTransfer;
    jTxInfo[Evmone::contractDeployerKeyName] = toAddr;

	if(signFunction == nullptr)
	{
    	jTxInfo[Evmone::contractOutputKeyName] = host.output;
	}

    jTxInfo[Evmone::contractBlockTimestampKeyName] = host.tx_context.block_timestamp;
    jTxInfo[Evmone::contractBlockPrevRandaoKeyName] = evm_utils::EvmcUint256beToUint32(host.tx_context.block_prev_randao);

    Evmone::GetCalledContract(host, dirtyContract);
    int ret = Evmone::FillCallOutTx(sender, toAddr, host.coin_transferrings, jTxInfo, height, host.gas_cost, outTx, type,
                                information, contractTip,encodedInfo,isFindUtxo);
    if (ret != 0)
    {
        ERRORLOG("FillCallOutTx fail ret: {}", ret);
        return ret - 1000; 
    }

	if(signFunction != nullptr)
	{
		ret = signFunction(outTx, sender);
		if (ret!= 0){
			errorL("sig fial %s", ret);
			return -2;
		}
		std::string txHash = Getsha256hash(outTx.SerializeAsString());
		outTx.set_hash(txHash);
	}
    return 0;
}



int TxHelper::CreateEvmDeployContractTransaction(uint64_t height, CTransaction &outTx,
                                                 std::vector<std::string> &dirtyContract,
                                                 TxHelper::vrfAgentType &type, Vrf &information, std::string &code,
                                                 const std::string &from, uint64_t transfer, const std::string &to,
												 const std::string& encodedInfo, bool isFindUtxo,bool isRpc)
{

	if(!isRpc)
	{
		Account launchAccount;
		if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(from, launchAccount) != 0)
		{
			std::cout<<RED << "Failed to find account:"<<from << RESET << std::endl;
			ERRORLOG("Failed to find account {}", from);
			return -1;
		}
	}

    EvmHost host;
    code.erase(std::remove_if(code.begin(), code.end(), ::isspace), code.end());
    host.code = evm_utils::StringTobytes(code);
    if (code.empty())
    {
        ERRORLOG("fail to convert contract code to hex format");
        return -2;
    }

    int64_t blockTimestamp = evm_environment::CalculateBlockTimestamp(3);
    if (blockTimestamp < 0)
    {
        return -3;
    }
	int64_t blockPrevRandao = evm_environment::CalculateBlockPrevRandao(from);
	if (blockPrevRandao < 0)
	{
		return -4;
	}

    int64_t blockNumber = 0;
    try
    {
        blockNumber = Util::Unsign64toSign64(height);
    }
    catch (const std::exception& e)
    {
        ERRORLOG("{}", e.what())
        return -5;
    }

    if (blockNumber < 0)
    {
        return -6;
    }

    evmc_message message{};
    int ret = Evmone::DeployContract(from, host, message, to, blockTimestamp,
                                     blockPrevRandao, blockNumber);
    if (ret != 0)
    {
		SetRpcError("-300",Sutil::Format("Evmone failed to call contract! error:%s , output:%s", ret-100, host.outputError));
        ERRORLOG("failed to deploy contract! The error code is:{}", ret);
        return -300;
    }

	int (*ptr)(CTransaction&, const std::string&);
	if(isRpc)
	{
		ptr = nullptr;
	}
	else
	{
		ptr = SigTx;
	}

    ret = TxHelper::MakeEvmDeployContractTransaction(height,
                                                     outTx, dirtyContract,
                                                     type,
                                                     information, host, message, isFindUtxo, encodedInfo, ptr);
    if(ret != 0)
    {
        ERRORLOG("Failed to create DeployContract transaction! The error code is:{}", ret);
        return ret - 10000;
    }
    return 0;
}

int TxHelper::CreateEvmCallContractTransaction(const std::string &from, const std::string &toAddr, const std::string &strInput,
 									const std::string& encodedInfo,
                                     uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                     Vrf &information,
                                     const uint64_t contractTip, const uint64_t contractTransfer,
                                     std::vector<std::string> &dirtyContract, const std::string &to,
                                     bool isFindUtxo ,bool isRpc )
{
	if(!isRpc)
	{
		Account launchAccount;
		if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(from, launchAccount) != 0)
		{
			ERRORLOG("Failed to find account {}", from);
			return -1;
		}
	}


    EvmHost host;
    evmc::bytes contractCode;
    if (evm_utils::GetContractCode(to, contractCode) != 0)
    {
        ERRORLOG("fail to get contract code");
        return -2;
    }
    host.code = contractCode;
    int64_t blockTimestamp = evm_environment::CalculateBlockTimestamp(3);
    if (blockTimestamp < 0)
    {
        return -3;
    }
	
	int64_t blockPrevRandao = evm_environment::CalculateBlockPrevRandao(from);
	if (blockPrevRandao < 0)
	{
		return -4;
	}

    int64_t blockNumber = 0;
    try
    {
        blockNumber = Util::Unsign64toSign64(height);
    }
    catch (const std::exception& e)
    {
        ERRORLOG("{}", e.what())
        return -5;
    }

    if (blockNumber < 0)
    {
        return -6;
    }
	
    evmc_message message{};
    int ret = Evmone::CallContract(from, strInput, host,
                                   contractTransfer, to, message, blockTimestamp, blockPrevRandao, blockNumber);
    if (ret != 0)
    {
		SetRpcError("-300",Sutil::Format("Evmone failed to call contract! error:%s , output:%s", ret-100, host.outputError));
        ERRORLOG("Evmone failed to call contract! ret : {}", ret);
        return - 300;
    }

	int (*ptr)(CTransaction&, const std::string&);
	if(isRpc)
	{
		ptr = nullptr;
	}
	else
	{
		ptr = SigTx;
	}


    ret = MakeEvmCallContractTransaction(height, outTx, dirtyContract, type, information, host, message, strInput,
                                         contractTip, contractTransfer, toAddr, isFindUtxo, encodedInfo, ptr);
    if (ret != 0)
    {
        ERRORLOG("Failed to create CallContract transaction! The error code is:{}", ret);
        return ret - 10000;
    }

    return 0;
}




int TxHelper::RPC_CreateEvmCallContractTransaction(const std::string &from, const std::string &toAddr,
                                               const std::string &strInput,
                                               uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                               Vrf &information,
                                               const uint64_t contractTip, const uint64_t contractTransfer,
                                               std::vector<std::string> &dirtyContract, const std::string &to,
                                               bool isFindUtxo,bool isRpc)

{
	EvmHost host;
    evmc::bytes contractCode;
	if (evm_utils::GetContractCode(to, contractCode) != 0)
    {
		SetRpcError("-9",Sutil::Format("fail to get contract code"));
        ERRORLOG("fail to get contract code");
        return -1;
    }
    host.code = contractCode;

	int64_t blockTimestamp = evm_environment::CalculateBlockTimestamp(3);
    if (blockTimestamp < 0)
    {
		SetRpcError("-10",Sutil::Format("Calculate block timestamp failed!"));
        return -2;
    }

	int64_t blockPrevRandao = 64;
	if (blockPrevRandao < 0)
	{
		return -3;
	}

	int64_t blockNumber = 0;
    try
    {
        blockNumber = Util::Unsign64toSign64(height);
    }
    catch (const std::exception& e)
    {
		SetRpcError("-13",Sutil::Format("%s",e.what()));
        ERRORLOG("{}", e.what())
        return -4;
    }

    if (blockNumber < 0)
    {
        return -5;
    }

	evmc_message message{};
    int ret = Evmone::RPC_CallContract(from, strInput, host,
                                   contractTransfer, to, message, blockTimestamp, blockPrevRandao, blockNumber);
    if (ret != 0)
    {
		SetRpcError("-300",Sutil::Format("Evmone failed to call contract! %s , output:%s", ret-100, host.outputError));
        ERRORLOG("Evmone failed to call contract! ret : {}", ret);
        return -300;
    }

	{
		std::string sender = evm_utils::EvmAddrToString(message.sender);
		std::string recipient = evm_utils::EvmAddrToString(message.recipient);
		nlohmann::json jTxInfo;
		jTxInfo[Evmone::contractVersionKeyName] = 0;
		jTxInfo[Evmone::contractSenderKeyName] = sender;
		jTxInfo[Evmone::contractRecipientKeyName] = recipient;
		jTxInfo[Evmone::contractVirtualMachineKeyName] = global::ca::VmType::EVM;
		jTxInfo[Evmone::contractInputKeyName] = strInput;

		jTxInfo[Evmone::contractDonationKeyName] = contractTip;
		jTxInfo[Evmone::contractTransferKeyName] = contractTransfer;
		jTxInfo[Evmone::contractDeployerKeyName] = toAddr;

		jTxInfo[Evmone::contractOutputKeyName] = host.output;
	
		jTxInfo[Evmone::contractBlockTimestampKeyName] = host.tx_context.block_timestamp;
		jTxInfo[Evmone::contractBlockPrevRandaoKeyName] = evm_utils::EvmcUint256beToUint32(host.tx_context.block_prev_randao);

		outTx.set_type(global::ca::kTxSign);
		nlohmann::json data;
		data["TxInfo"] = jTxInfo;
		std::string s = data.dump();
		outTx.set_data(s);		
	}

	return 0;
}



//int TxHelper::CreateWasmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr, const std::string &txHash,
//                                    const std::string &strInput, uint64_t height,CTransaction &outTx, TxHelper::vrfAgentType &type,
//                                    Vrf &information,const uint64_t contractTip, const std::string &contractFunName)
//{
//	std::string strOutput;
//	int64_t gasCost = 0;
//	int ret = Wasmtime::CallWasmContract(fromAddr, toAddr, txHash, strInput, contractFunName, strOutput, gasCost);
//	if(ret != 0)
//	{
//		ERRORLOG("Wasmone failed to deploy contract!", ret);
//        return -1;
//	}
//
//	nlohmann::json jTxInfo;
//
//	jTxInfo["Version"] = 0;
//    jTxInfo["VmType"] = global::ca::VmType::WASM;
//    jTxInfo["DeployerAddr"] = toAddr;
//    jTxInfo["DeployHash"] = txHash;
//    jTxInfo["Input"] = strInput;
//    jTxInfo["Output"] = strOutput;
//	jTxInfo["contractTip"] = contractTip;
//	jTxInfo["contractFunName"] = contractFunName;
//
//	ret = Wasmtime::FillWasmOutTx(fromAddr, toAddr, global::ca::TxType::kTxTypeCallContract, jTxInfo,  height, gasCost, outTx, type, information, contractTip);
//	if (ret != 0)
//    {
//        ERRORLOG("FillWasmOutTx fail ret: {}", ret);
//		return -2;
//    }
//	return 0;
//}

int TxHelper::AddMutilSign(const std::string & addr, CTransaction &tx)
{
	if (!isValidAddress(addr))
	{
		return -1;
	}

	CTxUtxo * txUtxo = tx.mutable_utxo();
	CTxUtxo copyTxUtxo = *txUtxo;
	copyTxUtxo.clear_multisign();

	std::string serTxUtxo = Getsha256hash(copyTxUtxo.SerializeAsString());
	std::string signature;
	std::string pub;
	if(TxHelper::Sign(addr, serTxUtxo, signature, pub) != 0)
	{
		return -2;
	}

	CSign * multiSign = txUtxo->add_multisign();
	multiSign->set_sign(signature);
	multiSign->set_pub(pub);

	return 0;
}

int TxHelper::AddVerifySign(const std::string & addr, CTransaction &tx)
{
	if (!isValidAddress(addr))
	{
		ERRORLOG("illegal address {}", addr);
		return -1;
	}

	CTransaction copyTx = tx;

	copyTx.clear_hash();
	copyTx.clear_verifysign();

	std::string serTx = copyTx.SerializeAsString();
	if(serTx.empty())
	{
		ERRORLOG("fail to serialize trasaction");
		return -2;
	}

	std::string message = Getsha256hash(serTx);

	std::string signature;
	std::string pub;
	int ret = TxHelper::Sign(addr, message, signature, pub);
	if (ret != 0)
	{
		ERRORLOG("fail to sign message, ret :{}", ret);
		return -3;
	}

	DEBUGLOG("-------------------add verify sign addr = {} --------------------------",addr);

	CSign * verifySign = tx.add_verifysign();
	verifySign->set_sign(signature);
	verifySign->set_pub(pub);
	
	return 0;
}

int TxHelper::Sign(const std::string & addr, 
					const std::string & message, 
                    std::string & signature, 
					std::string & pub)
{
	if (addr.empty() || message.empty())
	{
		return -1;
	}

	Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(addr ,account) != 0)
	{
		ERRORLOG("account {} doesn't exist", addr);
		return -2;
	}

	if(!account.Sign(message,signature))
	{
		return -3;
	}

	pub = account.GetPubStr();
	return 0;
}

bool TxHelper::IsNeedAgent(const std::vector<std::string> & fromAddr)
{
	bool isNeedAgent = true;
	for(auto& owner : fromAddr)
	{
		//  If the transaction owner cannot be found in all accounts of the node, it indicates that it is issued on behalf
		if (owner == MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr())
		{
			isNeedAgent = false;
		}
	}

	return isNeedAgent;

}

bool TxHelper::IsNeedAgent(const CTransaction &tx)
{
	if(std::find(tx.utxo().owner().begin(), tx.utxo().owner().end(),tx.identity()) == tx.utxo().owner().end())
	{
		return true;
	}
	
	return false;
}

bool TxHelper::CheckTxTimeOut(const uint64_t & txTime, const uint64_t & timeout,const uint64_t & preHeight)
{
	if(txTime <= 0)
	{
		ERRORLOG("tx time = {} ", txTime);
		return false;
	}
    DBReader dbReader;
    std::vector<std::string> blockHashes;
	auto ret = dbReader.GetBlockHashesByBlockHeight(preHeight, preHeight, blockHashes);
    if (DBStatus::DB_SUCCESS != ret)
    {
		if(DBStatus::DB_NOT_FOUND != ret)
		{
			ERRORLOG("can't GetBlockHashesByBlockHeight, DBStatus::DB_NOT_FOUND");
			return false;
		}
		uint64_t top = 0;
    	dbReader.GetBlockTop(top);
        ERRORLOG("can't GetBlockHashesByBlockHeight, fail!!!,top:{}, preHeight:{}",top, preHeight);
        return false;
    }

    std::vector<CBlock> blocks;

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
        blocks.push_back(block);
    }

	std::sort(blocks.begin(), blocks.end(), [](const CBlock& x, const CBlock& y){ return x.time() < y.time(); });
	CBlock resultBlock = blocks[blocks.size() - 1];

	if(resultBlock.time() <= 0)
	{
		ERRORLOG("block time = {}  ", resultBlock.time());
		return false;
	}

	uint64_t resultTime = abs(int64_t(txTime - resultBlock.time()));
    if (resultTime > timeout * 1000000)
    {
		DEBUGLOG("vrf Issuing transaction More than 30 seconds time = {}, tx time= {}, top = {} ", resultTime, txTime, preHeight);
        return true;
    }
    return false;
}

TxHelper::vrfAgentType TxHelper::GetVrfAgentType(const CTransaction &tx, uint64_t &preHeight)
{
	//The type of contract-related transactions is set to vrf
	global::ca::TxType txType = (global::ca::TxType)tx.txtype();
	if (global::ca::TxType::kTxTypeDeployContract == txType || global::ca::TxType::kTxTypeCallContract == txType)
	{
		return TxHelper::vrfAgentType::vrfAgentType_vrf;
	}

	std::vector<std::string> owners(tx.utxo().owner().begin(), tx.utxo().owner().end());
	//The transaction time is verified to distinguish the agent type of the transaction
	if(TxHelper::CheckTxTimeOut(tx.time(),global::ca::TxTimeoutMin, preHeight) == false)
	{	
		if(std::find(owners.begin(), owners.end(), tx.identity()) == owners.end())
		{
			return TxHelper::vrfAgentType::vrfAgentType_vrf;
		}
		return TxHelper::vrfAgentType::vrfAgentType_defalut;
	}
	else
	{
		if(std::find(owners.begin(), owners.end(), tx.identity()) == owners.end())
		{
			return TxHelper::vrfAgentType::vrfAgentType_local;
		}
		else
		{
			return TxHelper::vrfAgentType::vrfAgentType_defalut;
		}
	}
	DEBUGLOG("GetVrfAgentType tx vrf agent type is vrfAgentType_defalut");
	return TxHelper::vrfAgentType::vrfAgentType_defalut;
}

void TxHelper::GetTxStartIdentity(const uint64_t &height, const uint64_t &currentTime, TxHelper::vrfAgentType &type)
{
	uint64_t preHeight = height -1;
	//The transaction time is verified to distinguish the agent type of the transaction
	if(CheckTxTimeOut(currentTime, global::ca::TxTimeoutMin, preHeight) == true)
	{
		type = vrfAgentType_defalut;
		return;
	}
	else
	{
		type = vrfAgentType_vrf;
		return;
	}
}

std::string TxHelper::GetIdentityNodes()
{
	uint64_t chainHeight = 0;
    if(!BlockHelper::ObtainChainHeight(chainHeight))
    {
        return "";
    }
	std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(chainHeight <= global::ca::kMinUnstakeHeight)
    {
		return defaultAddr;
	}
	
	int ret = VerifyBonusAddr(defaultAddr);
	int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(defaultAddr, global::ca::StakeType::kStakeType_Node);
	if (stakeTime > 0 && ret == 0)
	{
		return defaultAddr;
	}
	return "";
}

std::string TxHelper::GetEligibleNodes(){
	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<std::string> result_node;
    for (const auto &node : nodelist)
    {
        int ret = VerifyBonusAddr(node.address);

        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
        if (stakeTime > 0 && ret == 0)
        {
            result_node.push_back(node.address);
        }
    }
	auto getNextNumber=[&](int limit) ->int {
	  	std::random_device seed;
	 	std::ranlux48 engine(seed());
	 	std::uniform_int_distribution<int> u(0, limit-1);
	 	return u(engine);
	};
    if(result_node.size()==0){
		ERRORLOG("GetEligibleNodes:result_node is empty");
        return "";
    }

    
	DEBUGLOG("GetEligibleNodes: node size: {}",result_node.size());
	int rumdom=getNextNumber(result_node.size());
	DEBUGLOG("GetEligibleNodes: rumdom: {}",rumdom);
    
	return result_node[rumdom];
}
