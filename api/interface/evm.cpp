#include "ca_TfsHost.hpp"
#include "ca_evmone.h"

#include <cstdint>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include "utils/json.hpp"
#include "utils/console.h"
#include <proto/transaction.pb.h>
#include <db/db_api.h>
#include "ca_transaction.h"
#include "mpt/trie.h"
#include "ca_global.h"
#include "include/logging.h"
#include "utils/ContractUtils.h"
#include "ca_algorithm.h"
#include <future>
#include <chrono>
#include "evm.h"
#include "utils/tmplog.h"



namespace rpc_evm{


int rpc_genVin(const std::vector<std::string>& vecfromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign)
{
    // Find utxo
    std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
    auto ret = TxHelper::FindUtxo(vecfromAddr, TxHelper::kMaxVinSize, total, setOutUtxos);
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
    std::set<std::string> setTxowners;
    // Fill Vin
    for (auto & utxo : setOutUtxos)
    {
        setTxowners.insert(utxo.addr);
    }
    if (setTxowners.empty())
    {
        ERRORLOG(RED "Tx owner is empty!" RESET);
        return -7;
    }
    uint32_t n = txUtxo->vin_size();
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
                DEBUGLOG("----- utxo.hash:{}, utxo.n:{} owner:{}", utxo.hash, utxo.n, owner);
                utxoHashs.push_back(utxo.hash);
            }
        }
        vin->set_sequence(n++);

        if(isSign)
        {
            // std::string serVinHash = getsha256hash(vin->SerializeAsString());
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

int rpc_GenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<transferInfo> &transferrings, int64_t gasCost, 
                  CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign)
{
    DBReader db_reader;
    std::vector<std::string> vecDeployerAddrs;
    auto ret = db_reader.GetAllDeployerAddr(vecDeployerAddrs);
    if (DBStatus::DB_SUCCESS != ret && DBStatus::DB_NOT_FOUND != ret)
    {
        return -3;
    }

    std::map<std::string,map<std::string,uint64_t>> transfersMap;
    for(const auto& iter : transferrings)
    {
        DEBUGLOG("from:{}, to:{}, amount:{}", iter.from, iter.to, iter.amount); 
        if(iter.amount == 0) continue;
        transfersMap[iter.from][iter.to] += iter.amount;
    }

    std::map<std::string, uint64_t> fromBalance;
    for(const auto& iter : transfersMap)
    {
        bool isSign = false;
        std::vector<std::string> vecfromAddr;
        vecfromAddr.push_back(iter.first);

        std::string utxo;
        if(db_reader.GetLatestUtxoByContractAddr(iter.first, utxo) != DBStatus::DB_SUCCESS)
        {
            isSign = true;
        }

        if(!isGenSign)
        {
            isSign = false;
        }

        uint64_t total = 0;
        auto ret =rpc_evm::rpc_genVin(vecfromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
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
        std::vector<std::string> vecfromAddr;
        vecfromAddr.push_back(fromAddr);

        if(!isGenSign)
        {
            isSign = false;
        }

        uint64_t total = 0;
        auto ret =rpc_evm::rpc_genVin(vecfromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
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

    for(auto & iter : transfersMap)
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
        return -11;
    }
    expend += gas;

    if(fromBalance[fromAddr] < expend)
    {
        ERRORLOG("The total cost = {} is less than the cost = {}", fromBalance[fromAddr], expend);
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


int rpc_FillOutTx(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType tx_type,
          const std::vector<transferInfo> &transferrings, const nlohmann::json &jTxInfo,
          uint64_t height, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info_, const uint64_t& contractTip)
{
    if(toAddr.empty())
    {
        return -1;
    }

    if(contractTip != 0 && fromAddr != toAddr)
    {
        return -2;
    }

    outTx.set_type(global::ca::kTxSign);
    nlohmann::json data;
    data["TxInfo"] = jTxInfo;
    std::string s = data.dump();
    outTx.set_data(s);
    
    std::vector<std::string> utxoHashs;
    int ret = rpc_evm::rpc_GenCallOutTx(fromAddr, toAddr, transferrings, gasCost, outTx, contractTip, utxoHashs);
    if(ret < 0)
    {
        ERRORLOG("GenCallOutTx fail !!! ret:{}", ret);
        return -3;
    }

    //ca_algorithm::PrintTx(outTx);

    auto current_time=MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    TxHelper::GetTxStartIdentity(std::vector<std::string>(),height,current_time,type);
    if(type == TxHelper::vrfAgentType_unknow)
    {
    //This indicates that the current node has not met the pledge within 30 seconds beyond the height of 50 and the investment node can initiate the investment operation at this time
        type = TxHelper::vrfAgentType_defalut;
    }

    debugL("type:%s",(int)type);



    outTx.set_time(current_time);
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
        allUtxos += std::to_string(current_time);

        std::string id;
        auto ret = GetBlockPackager(id,allUtxos,info_);
        if(ret != 0){
            ERRORLOG("GetBlockPackager fail ret: {}", ret);
            return ret -= 300;
        }
        outTx.set_identity(id);
    }

    outTx.set_version(0);
    outTx.set_txtype((uint32_t)tx_type);
    outTx.set_consensus(global::ca::kConsensus);

    return 0;
}



int rpc_CreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                                 const std::string &code, uint64_t height,
                                                 const nlohmann::json &contractInfo, CTransaction &outTx,
                                                 TxHelper::vrfAgentType &type, Vrf &info_)
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

    ret = rpc_evm::rpc_FillOutTx(fromAddr,global::ca::kVirtualDeployContractAddr,global::ca::TxType::kTxTypeDeployContract,host.coin_transferrings, jTxInfo ,height,gasCost, outTx, type, info_,0);
    return ret;
}




int rpc_CreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr,
                                               const std::string &txHash,
                                               const std::string &strInput, const std::string &OwnerEvmAddr,
                                               uint64_t height,
                                               CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info_,
											   const uint64_t contractTip,const uint64_t contractTransfer)
{
    std::string strOutput;
    TfsHost host;
    int64_t gasCost = 0;
    int ret = Evmone::CallContract(fromAddr, OwnerEvmAddr, toAddr, txHash, strInput, strOutput, host, gasCost, contractTransfer);
    if (ret != 0)
    {
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

    ret = Evmone::ContractInfoAdd(host, jTxInfo, global::ca::TxType::kTxTypeCallContract);
    if(ret != 0)
    {
        DEBUGLOG("ContractInfoAdd error! ret:{}", ret);
        return -1;
    }

    // ret = Evmone::FillCallOutTx(fromAddr, toAddr, host.coin_transferrings, jTxInfo, height, gasCost, outTx, type,
    //                             info_, contractTip);

    ret=rpc_evm::rpc_FillOutTx(fromAddr, toAddr, global::ca::TxType::kTxTypeCallContract, host.coin_transferrings, jTxInfo, height, gasCost, outTx, type, info_, contractTip);
    if (ret != 0)
    {
        ERRORLOG("FillCallOutTx fail ret: {}", ret);
    }
    return ret;
}



}