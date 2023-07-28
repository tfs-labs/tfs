#include "ca_evmone.h"

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

static int
ExecuteByEvmone(const evmc_message &msg, const evmc::bytes &code, TfsHost &host, std::string &strOutput,
                    int64_t &gasCost)
{

    struct evmc_vm* pvm = evmc_create_evmone();
    if (!pvm)
    {
        return -1;
    }
    if (!evmc_is_abi_compatible(pvm))
    {
        return -2;
    }
    evmc::VM vm{pvm};

    auto async_execute = [&vm](Host& host, evmc_revision rev, const evmc_message& msg, const uint8_t* code, size_t code_size)
    {
        return vm.execute(host, rev, msg, code, code_size);
    };
    std::future<evmc::result> future_result = std::async(std::launch::async, async_execute, std::ref(host), EVMC_MAX_REVISION, std::ref(msg), code.data(), code.size());

    std::future_status status = future_result.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::timeout)
    {
        ERRORLOG(RED "Evmone execution failed timeout!" RESET); 
        return -3;
    }
    evmc::result result = future_result.get();
    DEBUGLOG("Evmone execution Result: {}", result.status_code);
    if (result.status_code != EVMC_SUCCESS)
	{
		ERRORLOG(RED "Evmone execution failed! gas_left:{}" RESET, result.gas_left);  
        strOutput = std::string_view(reinterpret_cast<const char *>(result.output_data), result.output_size);
	    DEBUGLOG("Output: {}", strOutput);
		return -4;
	}
    gasCost = msg.gas - result.gas_left;
	strOutput = std::move(evmc::hex({result.output_data, result.output_size}));
    
	DEBUGLOG("Output: {}", strOutput);
    return 0;    
}

int Evmone::DeployContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &code_str,
                           std::string &strOutput, TfsHost &host, int64_t &gasCost)
{
    // code
    const auto code = evmc::from_hex(code_str);

    // msg
    evmc_address&& evmAddr = evm_utils::stringToEvmAddr(OwnerEvmAddr);
    evmc::address create_address = {{0,1,2}};
    evmc_message create_msg{};
    create_msg.kind = EVMC_CREATE;
    create_msg.recipient = create_address;
    create_msg.sender = evmAddr;
    uint64_t balance = 0;
    GetBalanceByUtxo(fromAddr, balance);
    create_msg.gas = balance;

    struct evmc_tx_context tx_context = {
        .tx_origin = evmAddr
    };
    host.tx_context = tx_context;

    int ret = ExecuteByEvmone(create_msg, code, host, strOutput, gasCost);
    DEBUGLOG("evm execute ret: {}", ret);
    return ret;
}

int
Evmone::CallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                     const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost, const uint64_t& contractTransfer)
{
    // check whether the addr has deployed the tx hash
    DBReader data_reader;
    std::vector<std::string> vecDeployHashs;
    auto ret = data_reader.GetDeployUtxoByDeployerAddr(strDeployer, vecDeployHashs);
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
    std::string ContractAddress = evm_utils::generateContractAddr(strDeployer + strDeployHash);
    std::cout<<"ContractAddress:" << ContractAddress << std::endl;
    std::string deployHash;
    if(data_reader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetContractDeployUtxoByContractAddr failed!");
        return -3;
    }
    std::string txRaw;
    if(data_reader.GetTransactionByHash(deployHash, txRaw) != DBStatus::DB_SUCCESS)
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
        nlohmann::json data_json = nlohmann::json::parse(deployTx.data());
        nlohmann::json tx_info = data_json["TxInfo"].get<nlohmann::json>();
        strCode = tx_info["Output"].get<std::string>();
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
    evmc_address&& evmAddr = evm_utils::stringToEvmAddr(OwnerEvmAddr);
    evmc_message msg{};
    msg.kind = EVMC_CALL;
    msg.input_data = input.data();
    msg.input_size = input.size();
    msg.recipient = evm_utils::stringToEvmAddr(evm_utils::generateEvmAddr(strDeployer + strDeployHash));
    msg.sender = evmAddr;
    uint64_t balance = 0;
    GetBalanceByUtxo(fromAddr, balance);
    msg.gas = balance;

    dev::u256 value = contractTransfer;
    if(value > 0)
    {
       dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
       memcpy(msg.value.bytes, &by[0], by.size() * sizeof(uint8_t));
    }

    host.coin_transferrings.emplace_back(evm_utils::EvmAddrToBase58(msg.sender), evm_utils::EvmAddrToBase58(msg.recipient), contractTransfer);
    struct evmc_tx_context tx_context = {
        .tx_origin = evmAddr
    };
    host.tx_context = tx_context;

    // host
    std::string strPrevTxHash;
	ret = data_reader.GetLatestUtxoByContractAddr(ContractAddress, strPrevTxHash);
    if(ret != DBStatus::DB_SUCCESS)
    {
		ERRORLOG("GetLatestUtxoByContractAddr failed!");
        return -8;        
    }

    CTransaction PrevTx;
    std::string tx_raw;
	ret = data_reader.GetTransactionByHash(strPrevTxHash, tx_raw);

    if(ret != DBStatus::DB_SUCCESS)    
    {
		ERRORLOG("GetTransactionByHash failed!");
        return -9;   
    }

    if(!PrevTx.ParseFromString(tx_raw))
    {
		ERRORLOG("parse failed!");
        return -10;   
    }
    
	std::string rootHash;
    try
    {
        nlohmann::json jPrevData = nlohmann::json::parse(PrevTx.data());
        nlohmann::json jPrevStorage = jPrevData["TxInfo"]["Storage"];
        if(!jPrevStorage.is_null())
        {
            auto tx_type = (global::ca::TxType)PrevTx.txtype();
            if(tx_type == global::ca::TxType::kTxTypeDeployContract)
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

void Evmone::getStorage(const TfsHost& host, nlohmann::json& jStorage, std::set<address>& dirtyContract)
{
    for(const auto &account : host.accounts)
    {
        std::pair<std::string, std::string> rootHash;
        std::map<std::string, std::string> dirtyhash;
        std::shared_ptr<trie> root = account.second.StorageRoot;
        root->save();
        root->GetBlockStorage(rootHash, dirtyhash);

        if(!dirtyhash.empty())
        {
            dirtyContract.insert(account.first);
        }

        if(rootHash.first.empty())
        {
            continue;
        }
        jStorage[root->mContractAddr + "_" + "rootHash"] = rootHash.first;
        if (!rootHash.second.empty())
        {
            jStorage[root->mContractAddr + "_" + rootHash.first] = rootHash.second;
        }
        
        for(const auto& it : dirtyhash)
        {
            jStorage[root->mContractAddr + "_" + it.first] = it.second;
        }
    }
}

void Evmone::getSelfdestructs(const TfsHost& host, nlohmann::json& jSelfdestructs)
{
    DBReader data_reader;
    for(auto &it : host.recorded_selfdestructs)
    {
        std::string ContractAddress = evm_utils::EvmAddrToBase58(evmc::hex({it.selfdestructed.bytes,sizeof(it.selfdestructed.bytes)}));
        std::string deployHash;
        if(data_reader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
        {
            DEBUGLOG("GetContractDeployUtxoByContractAddr failed!, base58Addr:{}", ContractAddress);
            return;
        }
        jSelfdestructs.push_back({ContractAddress, deployHash});
    }
}

int Evmone::ContractInfoAdd(const TfsHost& host, nlohmann::json& jTxInfo, global::ca::TxType TxType)
{
    nlohmann::json jStorage;
    std::set<address> dirtyContract;
    getStorage(host, jStorage, dirtyContract);
    jTxInfo["Storage"] = jStorage;

    DBReader data_reader;
    std::map<std::string,std::string> items;
    evmc::address create_address = {{0,1,2}};
    for(auto &account : host.accounts)
    {
        if(TxType == global::ca::TxType::kTxTypeDeployContract && account.first == create_address)
        {
            continue;
        }

        if(dirtyContract.find(account.first) == dirtyContract.end())
        {
            continue;
        }

        std::string strPrevTxHash;
        std::string callAddress = account.second.StorageRoot->mContractAddr;
        if (data_reader.GetLatestUtxoByContractAddr(callAddress, strPrevTxHash) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetLatestUtxo of ContractAddr {} fail", callAddress);
            return -1;
        }
        items[callAddress] = strPrevTxHash;
    }
    jTxInfo["PrevHash"] = items;

    for(auto &it : host.recorded_logs)
    {
        nlohmann::json logmap;
        logmap["creator"] = evm_utils::EvmAddrToString(it.creator);
        logmap["data"] = evmc::hex({it.data.data(), it.data.size()});
        for(auto& topic : it.topics)
        {
            logmap["topics"].push_back(evmc::hex({topic.bytes, sizeof(topic.bytes)}));
        }
        jTxInfo["log"].push_back(logmap);
    }

    std::map<std::string,std::string> destruct_items;
    for(auto &it : host.recorded_selfdestructs)
    {
        std::string ContractAddress = evm_utils::EvmAddrToBase58(evmc::hex({it.selfdestructed.bytes,sizeof(it.selfdestructed.bytes)}));
        std::string deployHash;
        if(data_reader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
        {
            DEBUGLOG("GetContractDeployUtxoByContractAddr failed!, base58Addr:{}", ContractAddress);
            return -1;
        }
        destruct_items[ContractAddress] = deployHash;

    }
    jTxInfo["selfdestructs"] = destruct_items;
    return 0;
}

int Evmone::genVin(const std::vector<std::string>& vecfromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign)
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
            std::string serVinHash = getsha256hash(vin->SerializeAsString());
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
        else
        {
            vin->set_contractaddr(owner);
        }

    }
    return 0;
}

static int
FillOutTx(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType tx_type,
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
    int ret = Evmone::GenCallOutTx(fromAddr, toAddr, transferrings, gasCost, outTx, contractTip, utxoHashs);
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

    if (TxHelper::AddMutilSign(fromAddr, outTx) != 0)
    {
        return -11;
    }
    


    outTx.set_time(current_time);
    //Determine whether dropshipping is default or local dropshipping
    if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
    {
        outTx.set_identity(MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
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

    std::string txHash = getsha256hash(outTx.SerializeAsString());
    outTx.set_hash(txHash);

    return 0;
}

int Evmone::GenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
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
        auto ret = genVin(vecfromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
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
        auto ret = genVin(vecfromAddr, outTx.mutable_utxo(), utxoHashs, total, isSign);
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

int Evmone::VerifyUtxo(const CTransaction& tx, const CTransaction& callOutTx)
{
    if(tx.utxo().vout_size() != callOutTx.utxo().vout_size())
    {
        return -1;
    }
    std::string srcStr;
    for(const auto& vout : tx.utxo().vout())
    {
        DEBUGLOG("----- vout.addr:{}, vout.amount:{}", vout.addr(), vout.value());
        srcStr += vout.SerializeAsString();
    }

    std::string detStr;
    for(const auto& vout : callOutTx.utxo().vout())
    {
        detStr += vout.SerializeAsString();
    }

    if(srcStr != detStr)
    {
        return -2;
    }
    return 0;
}

int Evmone::FillCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                          const std::vector<transferInfo> &transferrings,
                          const nlohmann::json &jTxInfo, uint64_t height, int64_t gasCost, CTransaction &outTx,
                          TxHelper::vrfAgentType &type, Vrf &info_, const uint64_t& contractTip)
{
    if (!CheckBase58Addr(toAddr))
    {
        ERRORLOG("Fromaddr is a non base58 address!");
        return -1;
    }

    return FillOutTx(fromAddr, toAddr,
                     global::ca::TxType::kTxTypeCallContract, transferrings, jTxInfo, height, gasCost, outTx, type,
                     info_, contractTip);
}

int Evmone::FillDeployOutTx(const std::string &fromAddr, const std::string &toAddr,
                            const std::vector<transferInfo> &transferrings,
                            const nlohmann::json &jTxInfo, int64_t gasCost, uint64_t height, CTransaction &outTx,
                            TxHelper::vrfAgentType &type, Vrf &info_)
{
    return FillOutTx(fromAddr, toAddr,
                     global::ca::TxType::kTxTypeDeployContract, transferrings, jTxInfo, height, gasCost, outTx, type,
                     info_, 0);
}

void test_address_mapping()
{
    Account defaultAccount;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultAccount);
    std::cout<< "strFromAddr:" << defaultAccount.GetBase58() <<std::endl;
    std::cout<< "EvmAddress:" << evm_utils::getEvmAddr(defaultAccount.GetPubStr()) << std::endl;
}