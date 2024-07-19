#include "rpc_create_transaction.h"

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


int CheckFromaddr(const std::vector<std::string>& fromAddr)
{

	if(fromAddr.empty())
	{
		ERRORLOG("Fromaddr is empty!");		
		return -1;
	}

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

	for(auto& from : fromAddr)
	{
		if (!isValidAddress(from))
		{
			ERRORLOG("Fromaddr is a non  address! from:{}", from);
			return -3;
		}
	}

	return 0;
}


void ReplaceCreateTxTransaction(const std::vector<std::string>& fromAddr,
									const std::map<std::string, int64_t> & toAddr, bool isFindUtxo, const std::string& encodedInfo, txAck * ack)
{
	txAck *ackT = ack;
	MagicSingleton<TFSbenchmark>::GetInstance()->IncreaseTransactionInitiateAmount();

	DBReader dbReader;
    uint64_t height = 0;
	
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ackT->message=" db get top failed!!";
		ackT->code=-1;
		ERRORLOG("db get top failed!!");
        return; 
    }
	//  Check parameters
	height += 1;
	int ret = CheckFromaddr(fromAddr);
	if (ret != 0)
	{
		ackT->message="Check parameters failed!";
		ackT->code=-2;
		ERRORLOG("Check parameters failed! The error code is " + std::to_string(ret-100));
		return; 
	}

	if(toAddr.empty())
	{
		ackT->message="to addr is empty";
		ackT->code=-3;
		ERRORLOG("to addr is empty");
		return;
	}	

	for (auto& addr : toAddr)
	{
		if (!isValidAddress(addr.first))
		{
			ackT->message="To address is not  address!";
			ackT->code=-4;
			ERRORLOG("To address is not  address!");
			return;
		}

		for (auto& from : fromAddr)
		{
			if (addr.first == from)
			{
				ackT->message="from address is same with toaddr";
				ackT->code=-5;
				ERRORLOG("from address is same with toaddr");
				return;
			}
		}
		
		if (addr.second <= 0)
		{
			ackT->message="Value is zero!";
			ackT->code=-6;
			ERRORLOG("Value is zero!");
			return;
		}
	}

	if(encodedInfo.size() > 1024)
	{
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-7;
		ERRORLOG("The information entered exceeds the specified length");
		return;
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
	ret = TxHelper::FindUtxo(fromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ackT->message="FindUtxo failed!";
		ackT->code=-8;
		ERRORLOG("FindUtxo failed! The error code is " + std::to_string(ret-200));
		return; 
	}
	if (setOutUtxos.empty())
	{
		ackT->message="Utxo is empty!";
		ackT->code=-9;
		ERRORLOG("Utxo is empty!");
		return;
	}

	CTransaction outTx;
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
		ackT->message="Tx owner is empty!";
		ackT->code=-10;
		ERRORLOG("Tx owner is empty!");
		return;
	}

	uint32_t n = 0;
	DoubleSpendCache::doubleSpendsuc _usings;
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
				_usings.utxoVec.push_back(utxo.hash);
			}
		}
		vin->set_sequence(n++);
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
		ackT->message="Generate Gas failed!";
		ackT->code=-11;
		ERRORLOG("Generate Gas failed!");
		return;
	}

	TxHelper::vrfAgentType type;
	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::GetTxStartIdentity(height,currentTime,type);
	DEBUGLOG("GetTxStartIdentity currentTime = {} type = {}",currentTime ,type);
	expend +=  gas;
	ackT->gas = std::to_string(gas);
	ackT->time = std::to_string(currentTime);
	//Judge whether utxo is enough
	if(total < expend)
	{
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend)+"gas is " +std::to_string(gas)+"toAddr amount is "+std::to_string(amount);
		ackT->code=-12;
		ERRORLOG("{}", ackT->message);
		return; 
	}
	// fill vout
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


	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion); 
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeTx);
	// Determine whether dropshipping is default or local dropshipping
	Vrf info;
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
	}
	else
	{
		std::string allUtxos;
		for(auto & utxo:setOutUtxos){
			allUtxos+=utxo.hash;
		}
		// Candidate packers are selected based on all utxohashes
		allUtxos += std::to_string(currentTime);
		
		std::string id;
		
    	int ret = GetBlockPackager(id,allUtxos, info);
    	if(ret != 0){
			ackT->message = "Find packager failed";
			ackT->code=-13;
			ERRORLOG("Find packager failed ,error code is " + std::to_string(ret));
			return;
    	}
		outTx.set_identity(id);

	}
	_usings.time = outTx.time();
	if(MagicSingleton<DoubleSpendCache>::GetInstance()->AddFromAddr(std::make_pair(*fromAddr.rbegin(),_usings)) != 0)
	{
		ackT->message = "utxo is using!";
		ackT->code=-14;
		ERRORLOG("utxo is using");
		return;
	}
	DEBUGLOG("GetTxStartIdentity tx time = {}, package = {}", outTx.time(), outTx.identity());
	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);

	ackT->code = 0;
	ackT->message="success";
	ackT->txJson=txJsonString;
	ackT->vrfJson=vrfJsonString;
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return;
}


void ReplaceCreateStakeTransaction(const std::string & fromAddr, uint64_t stakeAmount, int32_t pledgeType, txAck* ack, double commissionRate, bool isFindUtxo, const std::string& encodedInfo)
{
	txAck *ack_t = ack;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ack_t->code = -2;
        ack_t->message = "Failed to get the current block height";
		return;
    }

	//  Check parameters
	height += 1;
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = TxHelper::Check(vecfromAddr, height);
	if(ret != 0)
	{
		ack_t->code = -3;
        ack_t->message = "Check parameters failed! The error code is " + std::to_string(ret-100);
		return;
	}

	if (!isValidAddress(fromAddr)) 
	{
		ack_t->code = -4;
        ack_t->message = "From address invlaid!";
		return;
	}

	if(stakeAmount == 0 )
	{
		ack_t->code = -5;
        ack_t->message = "Stake amount is zero !";
		return;	
	}
	
	if(stakeAmount < global::ca::kMinStakeAmt)
	{
		ack_t->code = -6;
        ack_t->message = "The pledge amount must be greater than " + std::to_string(global::ca::kMinStakeAmt);
		return;
	}

	TxHelper::pledgeType pledgeType_ = (TxHelper::pledgeType)pledgeType;
	std::string strStakeType;
	if (pledgeType_ == TxHelper::pledgeType::kPledgeType_Node)
	{
		strStakeType = global::ca::kStakeTypeNet;
	}
	else
	{
		ack_t->code = -7;
        ack_t->message = "Unknown stake type!";
		return;
	}

	std::vector<std::string> stakeUtxos;
    auto dbret = dbReader.GetStakeAddressUtxo(fromAddr,stakeUtxos);
	if(dbret == DBStatus::DB_SUCCESS)
	{
		ack_t->code = -8;
        ack_t->message = "There has been a pledge transaction before !";
		return;
	}

	if(encodedInfo.size() > 1024){
		ack_t->code = -9;
        ack_t->message = "The information entered exceeds the specified length";
		return;
	}

	uint64_t expend = stakeAmount;

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
		ack_t->code = -10;
        ack_t->message = "FindUtxo failed! The error code is " + std::to_string(ret-200);
		return; 
	}

	if (setOutUtxos.empty())
	{
		ack_t->code = -11;
        ack_t->message = "Utxo is empty!";
		return;
	}

	CTransaction outTx;
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
		ack_t->code = -12;
        ack_t->message = "Tx owner is invalid!";
		return;
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
	}

	nlohmann::json txInfo;
	txInfo["StakeType"] = strStakeType;
	txInfo["StakeAmount"] = stakeAmount;
	if(commissionRate < global::ca::KMinCommissionRate || commissionRate > global::ca::KMaxCommissionRate)
    {
		ack_t->code = -13;
        ack_t->message = "The commission ratio is not in the range";

		return;
    }
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
		ack_t->code = -14;
        ack_t->message = "The gas charge cannot be 0";
		return;
	}

	auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::GetTxStartIdentity(height,currentTime,type);

	expend += gas;
	ack_t->gas = std::to_string(gas);
	ack_t->time = std::to_string(currentTime);

	//Judge whether utxo is enough
	if(total < expend)
	{
		ack_t->code = -15;
        ack_t->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return; 
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

	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeStake);	
	//Determine whether dropshipping is default or local dropshipping
	Vrf info;
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
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
		
    	int ret= GetBlockPackager(id,allUtxos,info);
    	if(ret!=0){
			ack_t->code = -16;
        	ack_t->message = "Failed to get the packager, error code is " + std::to_string(ret);
			return;
    	}
		outTx.set_identity(id);
	}

	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);

	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrfJsonString;
	ack_t->code=0;
	ack_t->message = "success";
	ack_t->height=std::to_string(height-1);
	ack_t->txType=std::to_string((int)type);

	return;
}

void ReplaceCreatUnstakeTransaction(const std::string& fromAddr, const std::string& utxoHash, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ack_t = ack;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ack_t->code = -1;
		ack_t->message = "Failed to get the current highest block height";
        return; 
    }

	height += 1;
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = TxHelper::Check(vecfromAddr, height);
	if(ret != 0)
	{
		ack_t->code = -2;
		ack_t->message = "-2 Check parameters failed! The error code is " + std::to_string(ret-100);
        return; 
	}

	if (!isValidAddress(fromAddr))
	{
		ack_t->code = -3;
		ack_t->message = "FromAddr is not normal addr.";
		return;
	}

	uint64_t stakeAmount = 0;
	ret = IsQualifiedToUnstake(fromAddr, utxoHash, stakeAmount);
	if(ret != 0)
	{
		auto error = GetRpcError();
        ack_t->code = std::stoi(error.first);
        ack_t->message = error.second;
        RpcErrorClear();
		return;
	}	

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	//  The number of utxos to be searched here needs to be reduced by 1 \
	because a VIN to be redeem is from the pledged utxo, so just look for 99
	ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo); 
	if (ret != 0)
	{
		ack_t->code = -4;
		ack_t->message = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		return;
	}

	if (setOutUtxos.empty())
	{
		ack_t->code = -5;
		ack_t->message = "Utxo is empty";
		return;
	}

	CTransaction outTx;
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
		ack_t->code = -6;
		ack_t->message = "Tx owner is empty";
		return;
	}

	if(encodedInfo.size() > 1024){
		ack_t->code = -7;
		ack_t->message = "The information entered exceeds the specified length";
		return;
	}
	{
		//  Fill vin
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);
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
	// 	The filled quantity only participates in the calculation and does not affect others
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::kVirtualStakeAddr, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total));
	toAddr.insert(std::make_pair(global::ca::kVirtualBurnGasAddr, gas));
	
	
	if(GenerateGas(outTx, toAddr.size(), gas) != 0)
	{
		ack_t->code = -9;
		ack_t->message = "The gas charge cannot be 0";
		return;
	}

	//  Calculate total expenditure
	uint64_t gasTtoal =  gas;

	uint64_t cost = 0;// Packing fee

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::GetTxStartIdentity(height,currentTime,type);

	uint64_t expend = gas;
	ack_t->gas = std::to_string(gas);
	ack_t->time = std::to_string(currentTime);

	//	Judge whether there is enough money
	if(total < expend)
	{
		ack_t->code = -8;
		ack_t->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return;
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

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeUnstake);

	// Determine whether dropshipping is default or local dropshipping
	Vrf info;
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
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

    	int ret= GetBlockPackager(id,allUtxos,info);
    	if(ret!=0){
			ack_t->code = -9;
			ack_t->message = "GetBlockPackager error , error code is " + std::to_string(ret);
			return;
    	}
		outTx.set_identity(id);
	}

	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);

	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrfJsonString;
	ack_t->code=0;
	ack_t->message = "success";
	ack_t->height=std::to_string(height-1);
	ack_t->txType=std::to_string((int)type);

}

void ReplaceCreateInvestTransaction(const std::string & fromAddr,
								const std::string& toAddr,uint64_t investAmount, int32_t investType, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	TxHelper::vrfAgentType type;
	RpcErrorClear();
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ackT->code = -1;
		ackT->message = "Failed to get the current highest block height";
        return; 

    }

	height+=1;
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = TxHelper::Check(vecfromAddr, height);
	if(ret != 0)
	{	
		ackT->code = -2;
		ackT->message = "Check parameters failed";
		return;

	}

	//  Neither fromaddr nor toaddr can be a virtual account
	if (!isValidAddress(fromAddr))
	{
		ackT->code = -3;
		ackT->message = "The initiator address is incorrect";
		return;

	}

	if (!isValidAddress(toAddr))
	{
		ackT->code = -4;
		ackT->message = "The receiver address is incorrect";
		return;
	}

	if(investAmount < 35 * global::ca::kDecimalNum){
        ackT->code = -5;
		ackT->message = "investment amount less 35 ";
		return;
	}

	ret = CheckInvestQualification(fromAddr, toAddr, investAmount);
	if(ret != 0)
	{
		auto error = GetRpcError();
        ackT->code = std::stoi(error.first) - 100;
        ackT->message = error.second;
        RpcErrorClear();
        return;
	}	
	std::string strinvestType;
	TxHelper::InvestType locaInvestType = (TxHelper::InvestType)investType;
	if (locaInvestType ==  TxHelper::InvestType::kInvestType_NetLicence)
	{
		strinvestType = global::ca::kInvestTypeNormal;
	}
	else
	{
		ackT->code = -6;
		ackT->message = "Unknown invest type";
		return;
	}

	if(encodedInfo.size() > 1024){
		ackT->code = -7;
		ackT->message = "The information entered exceeds the specified length";
		return;
	}
	
	//  Find utxo
	uint64_t total = 0;
	uint64_t expend = investAmount;

	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{
	    ackT->code = -8;
		ackT->message = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		return;
	}

	if (setOutUtxos.empty())
	{
		ackT->code = -9;
		ackT->message = "Utxo is empty";
		return;

	}

	CTransaction outTx;
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
		ackT->code = -10;
		ackT->message = "Tx owner is empty";
		return;

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
		ackT->code = -11;
		ackT->message = "gas cannot be 0";
		return;

	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::GetTxStartIdentity(height,currentTime,type);

	expend += gas;
	ackT->gas = std::to_string(gas);
	ackT->time = std::to_string(currentTime);

	if(total < expend)
	{
		ackT->code = -12;
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return; 
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
	
	
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeInvest);

	// Determine whether dropshipping is default or local dropshipping
	Vrf info;
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
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
		
    	int ret= GetBlockPackager(id,allUtxos,info);
    	if(ret!=0){
        	ackT->code = -13;
			ackT->message = "GetBlockPackager error";
			return;

    	}
		outTx.set_identity(id);
	}

	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrfJsonString;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

}


void ReplaceCreateDisinvestTransaction(const std::string& fromAddr,
                                    const std::string& toAddr, const std::string& utxoHash, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	TxHelper::vrfAgentType type;
	CTransaction outTx;
    Vrf information;
	RpcErrorClear();
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ackT->message = "Failed to get the current highest block height";
        ackT->code = -1;
        return;
    }

	height += 1;
	//  Check parameters
	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(fromAddr);
	int ret = TxHelper::Check(vecfromAddr, height);
	if(ret != 0)
	{
		ackT->message = "Check parameters failed The error code is " + std::to_string(ret-100);
        ackT->code = -2;
		return;

	}

	if (!isValidAddress(fromAddr))
	{
		ackT->message = "FromAddr is not normal addr.";
        ackT->code = -3;	
		return;

	}

	if (!isValidAddress(toAddr))
	{
		ackT->message = "To address is not normal addr.";
        ackT->code = -4;		
		return;

	}

	uint64_t investedAmount = 0;
	if(IsQualifiedToDisinvest(fromAddr, toAddr, utxoHash, investedAmount) != 0)
	{
		auto error = GetRpcError();
        ackT->code = std::stoi(error.first) - 100;
        ackT->message = error.second;
        RpcErrorClear();
		return;
	}
	if(encodedInfo.size() > 1024){
		ackT->message = "The information entered exceeds the specified length";
	    ackT->code = -5;
		return;
	}

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	//  The utxo quantity sought here needs to be reduced by 1
	ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo); 
	if (ret != 0)
	{
		ackT->message = "FindUtxo failed The error code is " + std::to_string(ret-300);
        ackT->code = -6;		
		return;

	}
	if (setOutUtxos.empty())
	{
		ackT->message = "Utxo is empty";
        ackT->code = -7;	
		return;

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
		ackT->message = "Tx owner is empty";
        ackT->code = -8;	
		return;

	}

	{
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);
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
		ackT->message = "gas cannot be 0";
        ackT->code = -9;	
		return;

	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::GetTxStartIdentity(height,currentTime,type);


	uint64_t expend = gas;
	ackT->gas = std::to_string(gas);
	ackT->time = std::to_string(currentTime);
	
	if(total < expend)
	{
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
        ackT->code = -10;	
		return;
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

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::kCurrentTransactionVersion);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeDisinvest);

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
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
			ackT->message = "GetBlockPackager error";
        	ackT->code = -11;	
			return;

    	}
		outTx.set_identity(id);
	}

	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(information,&vrfJsonString);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrfJsonString;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

}

void ReplaceCreateBonusTransaction(const std::string& addr, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	TxHelper::vrfAgentType type;
	CTransaction outTx;
	Vrf information;
	uint64_t height;

	DBReader dbReader; 
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(height))
    {
		ackT->code = -1;	
		ackT->message = "Failed to get the current highest block height";
        return;
    }

	std::vector<std::string> vecfromAddr;
	vecfromAddr.push_back(addr);
	int ret = TxHelper::Check(vecfromAddr, height);
	if(ret != 0)
	{
		ackT->code = -2;	
		ackT->message = "Check parameters failed"+std::to_string(ret);
		return;
	}

	if (!isValidAddress(addr))
	{
		ackT->code = -3;	
		ackT->message = "The bouns address is incorrect";
		return;
	}

	if(encodedInfo.size() > 1024){
		ackT->code = -4;	
		ackT->message = "The information entered exceeds the specified length";
		return;
	}

	std::vector<std::string> utxos;
	uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ret = CheckBonusQualification(addr, curTime);
	if(ret != 0)
	{
        auto error = GetRpcError();
        ackT->code = std::stoi(error.first) - 100;
        ackT->message = error.second;
        RpcErrorClear();
		return;
	}

	std::map<std::string, uint64_t> companyDividend;
    ret=ca_algorithm::CalcBonusValue(curTime, addr, companyDividend);
	if(ret < 0)
	{
		ackT->code = -5;	
		ackT->message = "Failed to obtain the amount claimed by the investor, ret"+std::to_string(ret);
		return;
	}

	uint64_t expend = 0;
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
	ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize - 1, total, setOutUtxos, isFindUtxo);
	if (ret != 0)
	{	
		ackT->code = -6;	
		ackT->message = "FindUtxo failed"+std::to_string(ret);
		return;
	}
	if (setOutUtxos.empty())
	{
		ackT->code = -7;	
		ackT->message = "utxo is zero";
		return;		
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
		ackT->code = -8;	
		ackT->message = "Tx owner is empty";
		return;		
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
	}

	// Fill data
	uint64_t tempCosto=0;
	uint64_t tempNodeDividend=0;
	uint64_t tempTotalClaim=0;
	double tempcommissionRate;
	int rt = ca_algorithm::GetCommissionPercentage(addr, tempcommissionRate);
	if(rt != 0)
	{
		ackT->code = -9;	
		ackT->message = "GetCommissionPercentage error"+std::to_string(rt);
		return;	
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
		ackT->code = -10;	
		ackT->message = "The gas charge cannot be 0";
		return;
	}

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::GetTxStartIdentity(height,currentTime,type);

	expend = gas;
	ackT->gas = std::to_string(gas);
	ackT->time = std::to_string(currentTime);

	if(total < expend)
	{
		ackT->code = -11;	
		ackT->message = "The total cost = {} is less than the cost = {}"+std::to_string(total);
		return;
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
		ackT->code = -12;	
		ackT->message = "The claim amount is 0";
		return;
	}

	// Determine whether dropshipping is default or local dropshipping
	if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	{
		outTx.set_identity(TxHelper::GetEligibleNodes());
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
			ackT->code = -13;	
			ackT->message = "GetBlockPackager fail"+std::to_string(ret);
			return;
    	}
		outTx.set_identity(id);
	}

	std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(information,&vrfJsonString);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrfJsonString;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

}

int SendMessage(CTransaction & outTx,int height,Vrf &info,TxHelper::vrfAgentType type){
	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);


	TxMsgReq txMsg;
	txMsg.set_version(global::kVersion);
	TxMsgInfo* txMsgInfo = txMsg.mutable_txmsginfo();
	txMsgInfo->set_type(0);
	txMsgInfo->set_tx(outTx.SerializeAsString());
	txMsgInfo->set_nodeheight(height);

    uint64_t localTxUtxoHeight;
    auto ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return -1;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

	if (type == TxHelper::vrfAgentType::vrfAgentType_vrf) {
		Vrf* new_info = txMsg.mutable_vrfinfo();
		new_info->CopyFrom(info);
		
	}
	auto msg = std::make_shared<TxMsgReq>(txMsg);
	std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (type == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DropshippingTx(msg, outTx);
    }
	return ret;
}