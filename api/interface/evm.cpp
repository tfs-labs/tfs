#include "tfs_host.hpp"

#include <cstdint>
#include <future>
#include <chrono>

#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include <proto/transaction.pb.h>
#include <db/db_api.h>

#include "utils/json.hpp"
#include "evmone.h"
#include "utils/console.h"
#include "transaction.h"
#include "mpt/trie.h"
#include "global.h"
#include "include/logging.h"
#include "utils/contract_utils.h"
#include "algorithm.h"
#include "evm.h"
#include "utils/tmp_log.h"
#include "rpc_error.h"

namespace rpc_evm{
int RpcGenVin(const std::vector<std::string>& vecFromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign)
{
    // Find utxo
    std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
    auto ret = TxHelper::FindUtxo(vecFromAddr, TxHelper::kMaxVinSize, total, setOutUtxos);
    if (ret != 0)
    {
        ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
        ret -= 100;
        return ret;
    }
    if (setOutUtxos.empty())
    {
        ERRORLOG(RED "Utxo is empty!" RESET);
        return -6;
    }
    std::set<std::string> setTxOwners;
    // Fill Vin
    for (auto & utxo : setOutUtxos)
    {
        setTxOwners.insert(utxo.addr);
    }
    if (setTxOwners.empty())
    {
        ERRORLOG(RED "Tx owner is empty!" RESET);
        return -7;
    }
    uint32_t n = txUtxo->vin_size();
    for (auto & owner : setTxOwners)
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
                DEBUGLOG("----- utxo.hash:{}, utxo.n:{} owner:{}", utxo.hash, utxo.n, owner);
                utxoHashs.push_back(utxo.hash);
            }
        }
        vin->set_sequence(n++);

        if(isSign)
        {
            // std::string serVinHash = Getsha256hash(vin->SerializeAsString());
            // std::string signature;
            // std::string pub;
            // if (TxHelper::Sign(owner, serVinHash, signature, pub) != 0)
            // {
            //     return -8;
            // }

            // CSign * vinSign = vin->mutable_vinsign();
            // vinSign->set_sign(signature);
            // vinSign->set_pub(pub);
        }
        else
        {
            vin->set_contractaddr(owner);
        }

    }
    return 0;
}

int RpcGenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<TransferInfo> &transFerrings, int64_t gasCost, 
                  CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign)
{
    DBReader dbReader;
    std::vector<std::string> vecDeployerAddrs;
    auto ret = dbReader.GetAllDeployerAddr(vecDeployerAddrs);
    if (DBStatus::DB_SUCCESS != ret && DBStatus::DB_NOT_FOUND != ret)
    {
        return -3;
    }

    std::map<std::string,map<std::string,uint64_t>> transFersMap;
    for(const auto& iter : transFerrings)
    {
        DEBUGLOG("from:{}, to:{}, amount:{}", iter.from, iter.to, iter.amount); 
        if(iter.amount == 0) continue;
        transFersMap[iter.from][iter.to] += iter.amount;
    }

    std::map<std::string, uint64_t> fromBalance;
    for(const auto& iter : transFersMap)
    {
        bool isSign = false;
        std::vector<std::string> vecFromAddr;
        vecFromAddr.push_back(iter.first);

        std::string utxo;
        if(dbReader.GetLatestUtxoByContractAddr(iter.first, utxo) != DBStatus::DB_SUCCESS)
        {
            isSign = true;
        }

        if(!isGenSign)
        {
            isSign = false;
        }

        uint64_t total = 0;
        auto ret =rpc_evm::RpcGenVin(vecFromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
        if(ret < 0)
        {
            ERRORLOG("genVin fail!!! ret:{}",ret);
            return -4;
        }
        fromBalance[iter.first] = total;
    }
    
    uint64_t expend =  gasCost + contractTip;
    auto found = fromBalance.find(fromAddr);
    if(found == fromBalance.end())
    {
        bool isSign = true;
        std::vector<std::string> vecFromAddr;
        vecFromAddr.push_back(fromAddr);

        if(!isGenSign)
        {
            isSign = false;
        }

        uint64_t total = 0;
        auto ret =rpc_evm::RpcGenVin(vecFromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
        if(ret < 0)
        {
            ERRORLOG("genVin fail!!! ret:{}",ret);
            return -5;
        }
        fromBalance[fromAddr] = total;
    }

    for(auto& vin : fromBalance)
    {
        DEBUGLOG("----- vin.addr:{}, vin.amount:{}", vin.first, vin.second);
    }

    std::multimap<std::string, int64_t> targetAddrs;

    CTxUtxo * txUtxo = outTx.mutable_utxo();
    CTxOutput * vout = txUtxo->add_vout();
    vout->set_addr(global::ca::kVirtualDeployContractAddr);
    vout->set_value(gasCost);
    targetAddrs.insert({global::ca::kVirtualDeployContractAddr, gasCost});

    if(contractTip != 0)
    {
        CTxOutput * voutToAddr = txUtxo->add_vout();
        voutToAddr->set_addr(toAddr);
        voutToAddr->set_value(contractTip);
        targetAddrs.insert({toAddr, contractTip});
    }

    for(auto & iter : transFersMap)
    {
        auto& balance = fromBalance[iter.first];
        for(const auto& toaddr : iter.second)
        {
            CTxOutput * vout = txUtxo->add_vout();
            vout->set_addr(toaddr.first);
            vout->set_value(toaddr.second);
            targetAddrs.insert({toaddr.first, toaddr.second});

            if(balance < toaddr.second)
            {
                return -10;
            }

            balance -= toaddr.second;
        }
        if(iter.first == fromAddr)
        {
            continue;
        } 
        CTxOutput * vout = txUtxo->add_vout();
        vout->set_addr(iter.first);
        vout->set_value(balance);
        targetAddrs.insert({iter.first, balance});
    }

    targetAddrs.insert({global::ca::kVirtualBurnGasAddr, 0});
    targetAddrs.insert({fromAddr, 0});
    uint64_t gas = 0;
    if(GenerateGas(outTx, targetAddrs.size(), gas) != 0)
    {
        ERRORLOG(" gas = 0 !");
        return -9;
    }

    if (contractTip != 0 && contractTip < gas)
    {
        ERRORLOG("contractTip {} < gas {}" , contractTip, gas);
        SetRpcError("-72018", Sutil::Format("contractTip %s < gas %s", contractTip,gas));
        return -11;
    }
    expend += gas;

    if(fromBalance[fromAddr] < expend)
    {
        ERRORLOG("The total cost = {} is less than the cost = {}", fromBalance[fromAddr], expend);
        SetRpcError("-72013", Sutil::Format("The total cost = %s is less than the cost = %s", fromBalance[fromAddr], expend));
        return -10;
    }

    fromBalance[fromAddr] -= expend;

    CTxOutput * voutFromAddr = txUtxo->add_vout();
    voutFromAddr->set_addr(fromAddr);
    voutFromAddr->set_value(fromBalance[fromAddr]);
    
    CTxOutput * vout_burn = txUtxo->add_vout();
    vout_burn->set_addr(global::ca::kVirtualBurnGasAddr);
    vout_burn->set_value(gas);
    return 0;
}


int RpcFillOutTx(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType txType,
          const std::vector<TransferInfo> &transFerrings, const nlohmann::json &jTxInfo,
          uint64_t height, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info, const uint64_t& contractTip)
{
    if(toAddr.empty())
    {
        return -1;
    }

    if(contractTip != 0 && fromAddr == toAddr)
    {
        debugL("tip:%s,fromAddr:%s,toAddr:%s",contractTip,fromAddr,toAddr);
        return -2;
    }

    outTx.set_type(global::ca::kTxSign);
    nlohmann::json data;
    data["TxInfo"] = jTxInfo;
    std::string s = data.dump();
    outTx.set_data(s);
    
    std::vector<std::string> utxoHashs;
    int ret = rpc_evm::RpcGenCallOutTx(fromAddr, toAddr, transFerrings, gasCost, outTx, contractTip, utxoHashs);
    if(ret < 0)
    {
        ERRORLOG("GenCallOutTx fail !!! ret:{}", ret);
        return -3;
    }

    //ca_algorithm::PrintTx(outTx);

    auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    TxHelper::GetTxStartIdentity(std::vector<std::string>(),height,currentTime,type);
    if(type == TxHelper::vrfAgentType_unknow)
    {
    //This indicates that the current node has not met the pledge within 30 seconds beyond the height of 50 and the investment node can initiate the investment operation at this time
        type = TxHelper::vrfAgentType_defalut;
    }

    debugL("type:%s",(int)type);



    outTx.set_time(currentTime);
    //Determine whether dropshipping is default or local dropshipping
    if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
    {
        outTx.set_identity(TxHelper::GetEligibleNodes());
        //outTx.set_identity(MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    }
    else
    {
        //Select dropshippers
        std::string allUtxos;
        for(auto & utxoHash : utxoHashs){
            allUtxos += utxoHash;
        }
        allUtxos += std::to_string(currentTime);

        std::string id;
        auto ret = GetBlockPackager(id,allUtxos,info);
        if(ret != 0){
            ERRORLOG("GetBlockPackager fail ret: {}", ret);
            return ret -= 300;
        }
        outTx.set_identity(id);
    }

    outTx.set_version(0);
    outTx.set_txtype((uint32_t)txType);
    outTx.set_consensus(global::ca::kConsensus);

    return 0;
}



int RpcCreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                                 const std::string &code, uint64_t height,
                                                 const nlohmann::json &contractInfo, CTransaction &outTx,
                                                 TxHelper::vrfAgentType &type, Vrf &info)
{
    std::string strOutput;
    TfsHost host;
    int64_t gasCost = 0;
    int ret = Evmone::DeployContract(fromAddr, OwnerEvmAddr, code, strOutput, host, gasCost);
    if (ret != 0)
    {
        ERRORLOG("Evmone failed to deploy contract!");
        ret -= 10;
        return ret;
    }

    nlohmann::json jTxInfo;
    jTxInfo["Version"] = 0;
    jTxInfo["OwnerEvmAddr"] = OwnerEvmAddr;
    jTxInfo["VmType"] = global::ca::VmType::EVM;
    jTxInfo["Code"] = code;
    jTxInfo["Output"] = strOutput;
    jTxInfo["Info"] = contractInfo;

    ret = Evmone::ContractInfoAdd(host, jTxInfo, global::ca::TxType::kTxTypeDeployContract);
    if(ret != 0)
    {
        DEBUGLOG("ContractInfoAdd error! ret:{}", ret);
        return -1;
    }

    ret = rpc_evm::RpcFillOutTx(fromAddr,global::ca::kVirtualDeployContractAddr,global::ca::TxType::kTxTypeDeployContract,host.coin_transferrings, jTxInfo ,height,gasCost, outTx, type, info,0);
    return ret;
}




int RpcCreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr,
                                               const std::string &txHash,
                                               const std::string &strInput, const std::string &OwnerEvmAddr,
                                               uint64_t height,
                                               CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info,
											   const uint64_t contractTip,const uint64_t contractTransfer,bool istochain)
{
    std::string strOutput;
    TfsHost host;
    int64_t gasCost = 0;
     int ret=0;
    if(istochain){
        ret = Evmone::CallContract(fromAddr, OwnerEvmAddr, toAddr, txHash, strInput, strOutput, host, gasCost, contractTransfer);
        if (ret != 0) {
            SetRpcError("-72019", Sutil::Format("Evmone failed to call contract! %s",ret));
            ERRORLOG("Evmone failed to call contract!");
            ret -= 10;
            return ret;
        }

        nlohmann::json jTxInfo;
        jTxInfo["Version"] = 0;
        jTxInfo["OwnerEvmAddr"] = OwnerEvmAddr;
        jTxInfo["VmType"] = global::ca::VmType::EVM;
        jTxInfo["DeployerAddr"] = toAddr;
        jTxInfo["DeployHash"] = txHash;
        jTxInfo["Input"] = strInput;
        jTxInfo["Output"] = strOutput;
        jTxInfo["contractTip"] = contractTip;
        jTxInfo["contractTransfer"] = contractTransfer;

        ret = Evmone::ContractInfoAdd(host, jTxInfo,
                                      global::ca::TxType::kTxTypeCallContract);
        if (ret != 0) {
            DEBUGLOG("ContractInfoAdd error! ret:{}", ret);
            return -1;
        }

        // ret = Evmone::FillCallOutTx(fromAddr, toAddr,
        // host.coin_transferrings, jTxInfo, height, gasCost, outTx, type,
        //                             info, contractTip);

        ret = rpc_evm::RpcFillOutTx(fromAddr, toAddr,
                                     global::ca::TxType::kTxTypeCallContract,
                                     host.coin_transferrings, jTxInfo, height,
                                     gasCost, outTx, type, info, contractTip);
        if (ret != 0) {
            ERRORLOG("FillCallOutTx fail ret: {}", ret);
        }
        return ret;

    }else{
        ret = RpcECallContract(fromAddr, OwnerEvmAddr, toAddr, txHash, strInput, strOutput, host, gasCost, contractTransfer);

        if (ret != 0) {
            SetRpcError("-72019", Sutil::Format("Evmone failed to call contract! %s",ret));
            ERRORLOG("Evmone failed to call contract!");
            ret -= 10;
            return ret;
        }

        nlohmann::json jTxInfo;
        jTxInfo["Version"] = 0;
        jTxInfo["OwnerEvmAddr"] = OwnerEvmAddr;
        jTxInfo["VmType"] = global::ca::VmType::EVM;
        jTxInfo["DeployerAddr"] = toAddr;
        jTxInfo["DeployHash"] = txHash;
        jTxInfo["Input"] = strInput;
        jTxInfo["Output"] = strOutput;
        jTxInfo["contractTip"] = contractTip;
        jTxInfo["contractTransfer"] = contractTransfer;

        outTx.set_data(jTxInfo.dump());

        return ret;

    }
    return 0;
     
    
}


int RpcECallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                     const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost, const uint64_t& contractTransfer)
{
    // check whether the addr has deployed the tx hash
    DBReader dataReader;
    std::vector<std::string> vecDeployHashs;
    auto ret = dataReader.GetDeployUtxoByDeployerAddr(strDeployer, vecDeployHashs);
    if(ret != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetDeployUtxoByDeployerAddr failed!");
        return -1;
    }
    auto iter = std::find(vecDeployHashs.cbegin(), vecDeployHashs.cend(), strDeployHash);
    if(iter == vecDeployHashs.cend())
    {
        ERRORLOG("Transaction has not been deployed at this address!");
        return -2;
    }
    std::string ContractAddress = evm_utils::GenerateContractAddr(strDeployer + strDeployHash);
    std::cout<<"ContractAddress:" << ContractAddress << std::endl;
    std::string deployHash;
    if(dataReader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetContractDeployUtxoByContractAddr failed!");
        return -3;
    }
    std::string txRaw;
    if(dataReader.GetTransactionByHash(deployHash, txRaw) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetTransactionByHash failed!");
        return -4;
    }
    CTransaction deployTx;
    if(!deployTx.ParseFromString(txRaw))
    {
        ERRORLOG("Transaction Parse failed!");
        return -5;
    }

    std::string strCode;
    evmc::bytes code;
    evmc::bytes input;
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(deployTx.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
        strCode = txInfo["Output"].get<std::string>();
        if(strCode.empty())
        {
            return -6;
        }
        code = evmc::from_hex(strCode);
        input = evmc::from_hex(strInput);

    }
    catch(const std::exception& e)
    {
        ERRORLOG("can't parse deploy contract transaction");
        return -7;
    }
    // msg
    evmc_address&& evmAddr = evm_utils::StringToEvmAddr(OwnerEvmAddr);
    evmc_message msg{};
    msg.kind = EVMC_CALL;
    msg.input_data = input.data();
    msg.input_size = input.size();
    msg.recipient = evm_utils::StringToEvmAddr(evm_utils::GenerateEvmAddr(strDeployer + strDeployHash));
    msg.sender = evmAddr;
    uint64_t balance = 100000;
    //GetBalanceByUtxo(fromAddr, balance);
    msg.gas = balance;

    dev::u256 value = contractTransfer;
    if(value > 0)
    {
       dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
       memcpy(msg.value.bytes, &by[0], by.size() * sizeof(uint8_t));
    }

    host.coin_transferrings.emplace_back(evm_utils::EvmAddrToBase58(msg.sender), evm_utils::EvmAddrToBase58(msg.recipient), contractTransfer);
    struct evmc_tx_context TxContext = 
    {
        .tx_origin = evmAddr
    };
    host.tx_context = TxContext;

    // host
    std::string strPrevTxHash;
	ret = dataReader.GetLatestUtxoByContractAddr(ContractAddress, strPrevTxHash);
    if(ret != DBStatus::DB_SUCCESS)
    {
		ERRORLOG("GetLatestUtxoByContractAddr failed!");
        return -8;        
    }

    CTransaction prevTx;
    std::string transactionRaw;
	ret = dataReader.GetTransactionByHash(strPrevTxHash, transactionRaw);

    if(ret != DBStatus::DB_SUCCESS)    
    {
		ERRORLOG("GetTransactionByHash failed!");
        return -9;   
    }

    if(!prevTx.ParseFromString(transactionRaw))
    {
		ERRORLOG("parse failed!");
        return -10;   
    }
    
	std::string rootHash;
    try
    {
        nlohmann::json jPrevData = nlohmann::json::parse(prevTx.data());
        nlohmann::json jPrevStorage = jPrevData["TxInfo"]["Storage"];
        if(!jPrevStorage.is_null())
        {
            auto txType = (global::ca::TxType)prevTx.txtype();
            if(txType == global::ca::TxType::kTxTypeDeployContract)
            {
                rootHash = jPrevStorage[std::string("_") + "rootHash"].get<std::string>();
            }
            else
            {
                rootHash = jPrevStorage[ContractAddress + "_" + "rootHash"].get<std::string>();
            }
        }
        
    }
    catch(...)
    {
		ERRORLOG("Parsing failed!");  
        return -11;
    }

    host.accounts[msg.recipient].CreateTrie(rootHash, ContractAddress);
    host.accounts[msg.recipient].set_code(code);
    int res = ExecuteByEvmone(msg, code, host, strOutput, gasCost);
    DEBUGLOG("evm execute ret: {}", res);
    return res;
}


}