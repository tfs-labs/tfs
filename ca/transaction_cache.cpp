#include <memory>
#include <unordered_map>
#include <future>
#include <chrono>


#include "ca/checker.h"
#include "ca/contract.h"
#include "ca/txhelper.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/transaction_cache.h"
#include "ca/failed_transaction_cache.h"
#include "ca/sync_block.h"

#include "utils/json.hpp"
#include "utils/console.h"
#include "utils/tmp_log.h"
#include "utils/time_util.h"
#include "utils/time_util.h"
#include "utils/tfs_bench_mark.h"
#include "utils/contract_utils.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"

#include "db/db_api.h"
#include "net/unregister_node.h"

#include "common/time_report.h"
#include "common/global_data.h"
#include "ca/evm/evm_manager.h"

class ContractDataCache;

const int TransactionCache::_kBuildInterval = 3 * 1000;
const time_t TransactionCache::_kTxExpireInterval  = 10;
const int TransactionCache::_kBuildThreshold = 1000000;


int CreateBlock(const std::list<CTransaction>& txs, const uint64_t& blockHeight, CBlock& cblock)
{
	cblock.Clear();

	// Fill version
	cblock.set_version(global::ca::kCurrentBlockVersion);

	// Fill time
	uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	cblock.set_time(time);
    DEBUGLOG("block set time ======");

	// Fill height
	uint64_t prevBlockHeight = blockHeight;
	cblock.set_height(prevBlockHeight + 1);

    nlohmann::json storage;
    bool isContractBlock = false;

    packDispatch packdis;
	// Fill tx
	for(auto& tx : txs)
	{
		// Add major transaction
		CTransaction * majorTx = cblock.add_txs();
		*majorTx = tx;
		auto& txHash = tx.hash();
        auto txType = (global::ca::TxType)tx.txtype();
        if (txType == global::ca::TxType::kTxTypeCallContract || txType == global::ca::TxType::kTxTypeDeployContract)
        {
            isContractBlock = true;
            nlohmann::json txStorage;
            if (MagicSingleton<TransactionCache>::GetInstance()->GetContractInfoCache(txHash, txStorage) != 0)
            {
                ERRORLOG("can't find storage of tx {}", txHash);
                return -1;
            }

            std::set<std::string> dirtyContractList;
            if(!MagicSingleton<TransactionCache>::GetInstance()->GetDirtyContractMap(tx.hash(), dirtyContractList))
            {
                ERRORLOG("GetDirtyContractMap fail!!! txHash:{}", tx.hash());
                return -2;
            }
            txStorage["dependentCTx"] = dirtyContractList;

            storage[txHash] = txStorage;
        }
	}

    cblock.set_data(storage.dump());
    // Fill preblockhash
    uint64_t seekPrehashTime = 0;
    std::future_status status;
    auto futurePrehash = MagicSingleton<BlockStroage>::GetInstance()->GetPrehash(prevBlockHeight);
    if(!futurePrehash.valid())
    {
        ERRORLOG("futurePrehash invalid,hight:{}",prevBlockHeight);
        return -2;
    }
    status = futurePrehash.wait_for(std::chrono::seconds(6));
    if (status == std::future_status::timeout) 
    {
        ERRORLOG("seek prehash timeout, hight:{}",prevBlockHeight);
        return -3;
    }
    else if(status == std::future_status::ready) 
    {
        std::string preBlockHash = futurePrehash.get().first;
        if(preBlockHash.empty())
        {
            ERRORLOG("seek prehash <fail>!!!,hight:{},prehash:{}",prevBlockHeight, preBlockHash);
            return -4;
        }
        DEBUGLOG("seek prehash <success>!!!,hight:{},prehash:{},blockHeight:{}",prevBlockHeight, preBlockHash, blockHeight);
        seekPrehashTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        cblock.set_prevhash(preBlockHash);
    }
    
	// Fill merkleroot
	cblock.set_merkleroot(ca_algorithm::CalcBlockMerkle(cblock));
	// Fill hash
	cblock.set_hash(Getsha256hash(cblock.SerializeAsString()));
    DEBUGLOG("blockHash:{}, \n storage:{}", cblock.hash().substr(0,6), storage.dump(4));
    DEBUGLOG("block hash = {} set time ",cblock.hash());
	return 0;
}

int BuildBlock(const std::list<CTransaction>& txs, const uint64_t& blockHeight, bool build_first, bool isConTractTx = false)
{
	if(txs.empty())
	{
		ERRORLOG("Txs is empty!");
		return -1;
	}

	CBlock cblock;
	int ret = CreateBlock(txs, blockHeight, cblock);
    if(ret != 0)
    {
        if(ret == -3 || ret == -4 || ret == -5)
        {
            MagicSingleton<BlockStroage>::GetInstance()->ForceCommitSeekTask(cblock.height() - 1);
        }
        auto tx_sum = cblock.txs_size();
        ERRORLOG("Create block failed! : {},  Total number of transactions : {} ", ret, tx_sum);
		return ret - 100;
    }
	std::string serBlock = cblock.SerializeAsString();
	ca_algorithm::PrintBlock(cblock);

    BlockMsg blockmsg;
    blockmsg.set_version(global::kVersion);
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    blockmsg.set_block(serBlock);

    for(auto &tx : cblock.txs())
    {
        if(GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }
        CTransaction copyTx = tx;
        copyTx.clear_hash();
        copyTx.clear_verifysign();
        std::string txHash = Getsha256hash(copyTx.SerializeAsString());
        MagicSingleton<TFSbenchmark>::GetInstance()->SetTxHashByBlockHash(cblock.hash(), txHash);

        std::pair<std::string,Vrf>  vrfPair;
        if(!MagicSingleton<VRF>::GetInstance()->getVrfInfo(txHash, vrfPair))
        {
            ERRORLOG("getVrfInfo failed! tx hash {}", txHash);
            return -2;
        }
        Vrf *vrfinfo  = blockmsg.add_vrfinfo();
        vrfinfo ->CopyFrom(vrfPair.second);

        uint64_t handleTxHeight =  cblock.height() - 1;
        TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
        if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
        {
            continue;
        }

        if(!MagicSingleton<VRF>::GetInstance()->getTxVrfInfo(txHash, vrfPair))
        {
            ERRORLOG("getTxVrfInfo failed! tx hash {}", txHash);
            return -3;
        }

        Vrf *txvrfinfo  = blockmsg.add_txvrfinfo();
        vrfPair.second.mutable_vrfdata()->set_txvrfinfohash(txHash);
        txvrfinfo ->CopyFrom(vrfPair.second);
    }
    
    BlockMsg _cpMsg = blockmsg;
    _cpMsg.clear_block();

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    std::string _cpMsgHash = Getsha256hash(_cpMsg.SerializeAsString());
	std::string signature;
	std::string pub;
	if (TxHelper::Sign(defaultAddr, _cpMsgHash, signature, pub) != 0)
	{
		return -4;
	}

	CSign * sign = blockmsg.mutable_sign();
	sign->set_sign(signature);
	sign->set_pub(pub);

    auto msg = std::make_shared<BlockMsg>(blockmsg);
	ret = DoHandleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("DoHandleBlock failed The error code is {} ,blockHash:{}",ret, cblock.hash().substr(0,6));
        CBlock cblock;
	    if (!cblock.ParseFromString(msg->block()))
	    {
		    ERRORLOG("fail to serialization!!");
		    return -5;
	    }
        return -6;
    }

	return 0;
}

TransactionCache::TransactionCache()
{
    _buildTimer.AsyncLoop(
        _kBuildInterval, 
        [=](){ _blockBuilder.notify_one(); }
        );
}

uint64_t TransactionCache::getBlockCount()
{
    std::unique_lock<std::mutex> locker(_transactionCacheMutex);
    return _transactionCache.rbegin()->GetTxUtxoHeight();
}

int TransactionCache::AddCache(CTransaction& transaction, const std::shared_ptr<TxMsgReq>& msg)
{
    auto txType = (global::ca::TxType)transaction.txtype();
    bool isContractTransaction = txType == global::ca::TxType::kTxTypeCallContract || txType == global::ca::TxType::kTxTypeDeployContract;
    if (isContractTransaction)
    {
        std::unique_lock<std::mutex> locker(_contractCacheMutex);
        if(Checker::CheckConflict(transaction, _contractCache))
        {
            DEBUGLOG("DoubleSpentTransactions, txHash:{}", transaction.hash());
            return -1;
        }
        _contractCache.push_back({transaction, msg->txmsginfo().nodeheight(), false});
    }
    else
    {
        std::unique_lock<std::mutex> locker(_transactionCacheMutex);
        if(Checker::CheckConflict(transaction, _transactionCache))
        {
            DEBUGLOG("DoubleSpentTransactions, txHash:{}", transaction.hash());
            return -2;
        }
        _transactionCache.push_back({*msg, transaction, msg->txmsginfo().txutxoheight()});
        if (_transactionCache.size() >= _kBuildThreshold)
        {
            _blockBuilder.notify_one();
        }
    }
    return 0;
}

bool TransactionCache::Process()
{
    _transactionCacheBuildThread = std::thread(std::bind(&TransactionCache::_TransactionCacheProcessingFunc, this));
    _transactionCacheBuildThread.detach();
    return true;
}

void TransactionCache::Stop(){
    _threadRun=false;
}

int TransactionCache::GetBuildBlockHeight(std::vector<TransactionEntity>& txcache)
{
    int maxUtxoHeight = 0;
    for(const auto& item : txcache)
    {
        auto TxUtxoHeight = item.GetTxUtxoHeight();
        if(maxUtxoHeight < TxUtxoHeight)
        {
            maxUtxoHeight = TxUtxoHeight;
        }
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::map<std::string, uint64_t> satisfiedAddrs;
    for(auto & node : nodelist)
    {
        //Verification of investment and pledge
        int ret = VerifyBonusAddr(node.address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
        if (stakeTime > 0 && ret == 0)
        {
            satisfiedAddrs[node.address] = node.height;
        }
    }

    if (satisfiedAddrs.size() < global::ca::kNeed_node_threshold && (maxUtxoHeight < global::ca::kMinUnstakeHeight))
	{
		for(auto & node : nodelist)
        {
            if(satisfiedAddrs.find(node.address) == satisfiedAddrs.end())
            {
                satisfiedAddrs[node.address] = node.height;
            }
        }
	}
	else if(satisfiedAddrs.size() < global::ca::kNeed_node_threshold)
	{
		DEBUGLOG("Insufficient qualified signature nodes satisfiedAddrs.size:{}", satisfiedAddrs.size());
		return -1;
	}


    std::map<uint64_t, uint64_t> heightCount;
    uint64_t heightMatchNum = 0;
    std::map<uint64_t, uint64_t> heightFrequency;

    for(auto &it : satisfiedAddrs)
    {
        if(it.second >= maxUtxoHeight)
        {        
            heightMatchNum++;
        }
        heightFrequency[it.second]++;
    }
    
    uint64_t cumulativeCount = 0;
    for(auto it = heightFrequency.rbegin(); it != heightFrequency.rend(); it++)
    {
        cumulativeCount += it->second; // Add the frequency of this height to the cumulative count
        heightCount[it->first] = cumulativeCount; 
        DEBUGLOG("heightFrequency, hegiht:{}, sum:{}", it->first, it->second);
    }

    if(heightMatchNum < satisfiedAddrs.size() * 0.75)
    {
        DEBUGLOG("heightMatchNum:{}, satisfiedAddrs.size:{}, {}, maxUtxoHeight:{}", heightMatchNum, satisfiedAddrs.size(), satisfiedAddrs.size() * 0.75, maxUtxoHeight);
        return -2;
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
    }

    if(top >= maxUtxoHeight)
    {
        auto it = heightCount.find(top);
        DEBUGLOG("top:{}, it->second:{}, satisfiedAddrs.size:{}", top, it->second, satisfiedAddrs.size());
        if(it != heightCount.end() && it->second >= satisfiedAddrs.size() * 0.75)
        {
            DEBUGLOG("top top top top top");
            return top;
        }
    }
 
    // std::pair<uint64_t, uint64_t> heightPair = {0,0};
    // for(const auto& it : heightCount)
    // {
    //     if(heightPair.second < it.second)
    //     {
    //         heightPair = it;
    //     }
    // }

    // DEBUGLOG("heightPair.first:{}, maxUtxoHeight:{}", heightPair.first, maxUtxoHeight);
    // if(heightPair.first >= maxUtxoHeight)
    // {
    //     return heightPair.first;
    // }
    // else
    // {
    //     return maxUtxoHeight;
    // }



    for(auto item = heightCount.rbegin(); item != heightCount.rend(); item++)
    {
        DEBUGLOG("item.first:{}, maxUtxoHeight:{}, {}", item->first, maxUtxoHeight, satisfiedAddrs.size() * 0.75);
        if(item->first < maxUtxoHeight)
        {
            return -3;
        }
        if(item->second >= satisfiedAddrs.size() * 0.75)
        {
            return item->first;
        }
    }
    return -4;
}

void TransactionCache::_TransactionCacheProcessingFunc()
{
    while (_threadRun)
    {
        std::unique_lock<std::mutex> locker(_transactionCacheMutex);
        _blockBuilder.wait(locker);
        //std::vector<cacheIter> emptyHeightCache;

        //std::list<CTransaction> buildTxs;
        if(_transactionCache.empty())
        {
            continue;
        }

        ClearBuildTxs();
        int buildHeight = GetBuildBlockHeight(_transactionCache);
        if(buildHeight < 0)
        {
            _transactionCache.clear();
            ERRORLOG("GetBuildBlockHeight fail!!! ret:{}", buildHeight);
            continue;
        }
        MagicSingleton<BlockStroage>::GetInstance()->CommitSeekTask(buildHeight);

        std::map<std::string, std::future<int>> taskResults;
        for(auto& txs : _transactionCache)
        {
            auto& tx = txs.GetTx();
            auto& txMsg = txs.GetTxMsg();
            
            auto task = std::make_shared<std::packaged_task<int()>>([tx, &txMsg, buildHeight] 
            {
                CTransaction copyTx = tx;
                auto ret = UpdateTxMsg(copyTx, std::make_shared<TxMsgReq>(txMsg), buildHeight);
                if (0 != ret)
                {
                    return ret;
                }
                DEBUGLOG("UpdateTxMsg end txhash:{}, tx.verifysize:{}",copyTx.hash(), copyTx.verifysign_size());
                MagicSingleton<TransactionCache>::GetInstance()->_AddBuildTx(copyTx);
                return 0;
            });

            try
            {
                taskResults[tx.hash()] = task->get_future();
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            MagicSingleton<TaskPool>::GetInstance()->CommitWorkTask([task](){(*task)();});
        }

        bool flag = false;
        for (auto& res : taskResults)
        {
           auto ret = res.second.get();
           if(ret != 0)
           {
                flag = true;
                ERRORLOG("UpdateTxMsg failed txHash:{}, ret:{}", res.first, ret);
           }
        }

        if(flag)
        {
            _transactionCache.clear();
            continue; 
        }

        auto ret = BuildBlock(_GetBuildTxs(), buildHeight, false);
        if(ret != 0)
        {
            ERRORLOG("{} build block fail", ret);
            if(ret == -103 || ret == -104 || ret == -105)
            {
                continue;
            }
            
            _transactionCache.clear();
            std::cout << "block packaging fail" << std::endl;
            continue;
        }
        std::cout << "block successfully packaged" << std::endl;
        _transactionCache.clear();

        locker.unlock();
    }
}

void TransactionCache::ProcessContract(int64_t topTransactionHeight)
{
    std::scoped_lock locker(_contractCacheMutex, _contractInfoCacheMutex, _dirtyContractMapMutex);
    //_ExecuteContracts();
    std::list<CTransaction> buildTxs;

    for(const auto& txEntity : _contractCache)
    {
        buildTxs.push_back(txEntity.GetTransaction());
    }
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("GetBlockTop error!");
        std::cout << "block packaging fail" << std::endl;
        return;
    }
    if (top > topTransactionHeight)
    {
        MagicSingleton<BlockStroage>::GetInstance()->CommitSeekTask(top);
        topTransactionHeight = top;
        DEBUGLOG("top:{} > topTransactionHeight:{}", top, topTransactionHeight);
    }
    if (buildTxs.empty())
    {
        DEBUGLOG("buildTxs.empty()");
        return;
    }

    ON_SCOPE_EXIT{
        removeExpiredEntriesFromDirtyContractMap();
        _contractCache.clear();
        _contractInfoCache.clear();
    };

    std::list<std::pair<std::string, std::string>> contractTxPreHashList;
    if(_GetContractTxPreHash(buildTxs,contractTxPreHashList) != 0)
    {
        ERRORLOG("_GetContractTxPreHash fail");
        return;
    }
    if(contractTxPreHashList.empty())
    {
        DEBUGLOG("contractTxPreHashList empty");
    }
    else
    {
        auto ret = _newSeekContractPreHash(contractTxPreHashList);
        if ( ret != 0)
        {
            ERRORLOG("{} _newSeekContractPreHash fail", ret);
            return;
        }
    }

    auto ret = BuildBlock(buildTxs, topTransactionHeight, false,true);
    if(ret != 0)
    {
        ERRORLOG("{} build block fail", ret);
        std::cout << "block packaging fail" << std::endl;
    }
    else
    {
        std::cout << "block successfully packaged" << std::endl;
    }
    DEBUGLOG("FFF 555555555");
    return;
}

std::pair<int, std::string>
TransactionCache::_ExecuteContracts(const std::map<std::string, CTransaction> &dependentContractTxMap,
                                    int64_t blockNumber)
{
    uint64_t StartTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    ContractDataCache contractDataCache;
    std::map<std::string, std::string> contractPreHashCache;
    for (auto iterPair : dependentContractTxMap)
    {
        uint64_t StartTime1 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        auto& tx = iterPair.second;
        auto txType = (global::ca::TxType)tx.txtype();
        if ( (txType != global::ca::TxType::kTxTypeCallContract && txType != global::ca::TxType::kTxTypeDeployContract)
            || _AddContractInfoCache(tx, contractPreHashCache, &contractDataCache, blockNumber + 1) != 0)
        {
            //_contractCache.erase(iter)
            return {-1,tx.hash()};
        }
        uint64_t StartTime2 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        DEBUGLOG("FFF _ExecuteContracts HHHHHHHHHH txHash:{}, time:{}", tx.hash().substr(0,6), (StartTime2 - StartTime1) / 1000000.0);
    }
    uint64_t EndTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    DEBUGLOG("FFF _ExecuteContracts HHHHHHHHHH txSize:{}, Time:{}",dependentContractTxMap.size(), (EndTime - StartTime) / 1000000.0);
    return {0,""};
}

bool TransactionCache::_VerifyDirtyContract(const std::string &transactionHash, const std::vector<std::string> &calledContract)
{
    auto found = _dirtyContractMap.find(transactionHash);
    if (found == _dirtyContractMap.end())
    {
        ERRORLOG("dirty contract not found hash: {}", transactionHash);
        return false;
    }
    std::set<std::string> calledContractSet(calledContract.begin(), calledContract.end());
    std::vector<std::string> result;
    std::set_difference(calledContractSet.begin(), calledContractSet.end(),
                        found->second.second.begin(), found->second.second.end(),
                        std::back_inserter(result));
    if (!result.empty())
    {
        for (const auto& addr : calledContract)
        {
            ERRORLOG("executed {}", addr);
        }
        for (const auto& addr : found->second.second)
        {
            ERRORLOG("found {}", addr);
        }
        for (const auto& addr : result)
        {
            ERRORLOG("result {}", addr);
        }
        ERRORLOG("dirty contract doesn't match execute result, tx hash: {}", transactionHash);
        return false;
    }
    return true;
}

int TransactionCache::_AddContractInfoCache(const CTransaction &transaction,
                                            std::map<std::string, std::string> &contractPreHashCache,
                                            ContractDataCache *contractDataCache, int64_t blockNumber)
{
    auto txType = (global::ca::TxType)transaction.txtype();
    if (txType != global::ca::TxType::kTxTypeCallContract && txType != global::ca::TxType::kTxTypeDeployContract)
    {
        return 0;
    }

    bool isMultiSign = IsMultiSign(transaction);
    std::string fromAddr;
    int ret = ca_algorithm::GetCallContractFromAddr(transaction, isMultiSign, fromAddr);
    if (ret != 0)
    {
        ERRORLOG("GetCallContractFromAddr fail ret: {}", ret);
        return -1;
    }

    std::string OwnerEvmAddr;
    global::ca::VmType vmType;

    std::string code;
    std::string input;
    std::string deployerAddr;
//    std::string deployHash;
    std::string destAddr;
//    std::string contractFunName;
    uint64_t contractTransfer;
    std::string contractAddress;
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        if(txInfo.find(Evmone::contractSenderKeyName) != txInfo.end())
        {
            OwnerEvmAddr = txInfo[Evmone::contractSenderKeyName].get<std::string>();
        }
        vmType = txInfo[Evmone::contractVirtualMachineKeyName].get<global::ca::VmType>();

        if (txType == global::ca::TxType::kTxTypeCallContract)
        {

            input = txInfo[Evmone::contractInputKeyName].get<std::string>();
            
//            if(vmType == global::ca::VmType::WASM)
//            {
//                deployerAddr = txInfo["DeployerAddr"].get<std::string>();
//                deployHash = txInfo["DeployHash"].get<std::string>();
//                contractFunName = txInfo["contractFunName"].get<std::string>();
//            }
            if(vmType == global::ca::VmType::EVM)
            {
                contractTransfer = txInfo[Evmone::contractTransferKeyName].get<uint64_t>();
                contractAddress = txInfo[Evmone::contractRecipientKeyName].get<std::string>();
            }
        }
        else if (txType == global::ca::TxType::kTxTypeDeployContract)
        {
            code = txInfo[Evmone::contractInputKeyName].get<std::string>();
            contractAddress = txInfo[Evmone::contractRecipientKeyName].get<std::string>();
        }

    }
    catch (...)
    {
        ERRORLOG("json parse fail");
        return -2;
    }
              
    int64_t gasCost = 0;
    nlohmann::json jTxInfo;
    std::string expectedOutput;
    std::vector<std::string> calledContract;
    if(vmType == global::ca::VmType::EVM)
    {
        destAddr = OwnerEvmAddr;
        if(destAddr != fromAddr)
        {
            ERRORLOG("fromAddr {}  is not equal to detAddr {} ", fromAddr, destAddr);
            return -3;
        }

        ret = evm_environment::VerifyEvmParameters(transaction);
        if (ret < 0)
        {
            ERRORLOG("verify evm parameters fail, ret: {}", ret)
            return -99;
        }
        EvmHost host(contractDataCache);
        if(txType == global::ca::TxType::kTxTypeDeployContract)
        {
            ret = Evmone::VerifyContractAddress(fromAddr, contractAddress);
            if (ret < 0)
            {
                ERRORLOG("verify contract address fail, ret: {}", ret)
                return ret - 100;
            }
            DEBUGLOG("888Output: {}", code);

            host.code = evm_utils::StringTobytes(code);
            if (code.empty())
            {
                ERRORLOG("fail to convert contract code to hex format");
                return -21;
            }
            int64_t blockTimestamp = evm_environment::GetBlockTimestamp(transaction);
            if (blockTimestamp < 0)
            {
                return -5;
            }
            DEBUGLOG("777Output: {}", evmc::hex({host.code.data(), host.code.size()}) );
            int64_t blockPrevRandao = evm_environment::GetBlockPrevRandao(transaction);
            if (blockPrevRandao < 0)
            {
                return -6;
            }
            evmc_message message{};

            ret = Evmone::DeployContract(fromAddr, host, message,
                                         contractAddress, blockTimestamp, blockPrevRandao, blockNumber);
            if(ret != 0)
            {
                ERRORLOG("VM failed to deploy contract!, ret {}", ret);
                return ret - 100;
            }
            gasCost = host.gas_cost;
            expectedOutput = host.output;
            DEBUGLOG("111Output: {}", expectedOutput);

        }
        else if(txType == global::ca::TxType::kTxTypeCallContract)
        {
            evmc::bytes contractCode;
            if (evm_utils::GetContractCode(contractAddress, contractCode) != 0)
            {
                ERRORLOG("fail to get contract code");
                return -2;
            }
            host.code = contractCode;

            int64_t blockTimestamp = evm_environment::GetBlockTimestamp(transaction);
            if (blockTimestamp < 0)
            {
                return -5;
            }

            int64_t blockPrevRandao = evm_environment::GetBlockPrevRandao(transaction);
            if (blockPrevRandao < 0)
            {
                return -6;
            }
            evmc_message message{};
            ret = Evmone::CallContract(fromAddr, input, host, contractTransfer, contractAddress,
                                       message, blockTimestamp, blockPrevRandao, blockNumber);
            if(ret != 0)
            {
                ERRORLOG("VM failed to call contract!, ret {}", ret);
                return ret - 200;
            }
            gasCost = host.gas_cost;
            expectedOutput = host.output;
        }

        ret = Evmone::ContractInfoAdd(host, transaction.hash(), txType, transaction.version(), jTxInfo,
                                    contractPreHashCache);
        if(ret != 0)
        {
            ERRORLOG("ContractInfoAdd fail ret: {}", ret);
            return -4;
        }

        Evmone::GetCalledContract(host, calledContract);
   
        if(host.contractDataCache != nullptr) host.contractDataCache->set(jTxInfo[Evmone::contractStorageKeyName]);
        else return -7;
    }
//    else if(vmType == global::ca::VmType::WASM)
//    {
//        destAddr = transaction.utxo().owner(0);
//        if(destAddr != fromAddr)
//        {
//            ERRORLOG("fromAddr {}  is not equal to detAddr {} ", fromAddr, destAddr);
//            return -5;
//        }
//        if(txType == global::ca::TxType::kTxTypeDeployContract)
//        {
//            std::string hexCode = Hex2Str(code);
//            ret = Wasmtime::DeployWasmContract(fromAddr, hexCode, expectedOutput, gasCost);
//            if(ret != 0)
//            {
//                ERRORLOG("WASM failed to deploy contract!");
//                return ret - 300;
//            }
//        }
//        else if(txType == global::ca::TxType::kTxTypeCallContract)
//        {
//            ret = Wasmtime::CallWasmContract(fromAddr, deployerAddr, deployHash, input, contractFunName, expectedOutput, gasCost);
//            if(ret != 0)
//            {
//                ERRORLOG("WASM failed to call contract!");
//                return ret - 400;
//            }
//        }
//        ret = Wasmtime::ContractInfoAdd(transaction.hash(), jTxInfo, txType, transaction.version(), contractPreHashCache);
//        if(ret != 0)
//        {
//            ERRORLOG("Wasmtime ContractInfoAdd fail! ret {}", ret);
//            return -6;
//        }
//        Wasmtime::GetCalledContract(calledContract);
//    }

    if (!_VerifyDirtyContract(transaction.hash(), calledContract))
    {
        ERRORLOG("_VerifyDirtyContract fail");
        return -7;
    }
    DEBUGLOG("999Output: {}", expectedOutput);
    jTxInfo[Evmone::contractOutputKeyName] = expectedOutput;
    DEBUGLOG("888Output: {}", jTxInfo.dump(4));
    AddContractInfoCache(transaction.hash(), jTxInfo, transaction.time());
    return 0;
}

void TransactionCache::AddContractInfoCache(const std::string& transactionHash, const nlohmann::json& jTxInfo, const uint64_t& txtime)
{
    std::unique_lock<std::shared_mutex> locker(_contractInfoCacheMutex);
    _contractInfoCache[transactionHash] = {jTxInfo, txtime};
    return;
}

int TransactionCache::GetContractInfoCache(const std::string& transactionHash, nlohmann::json& jTxInfo)
{
    auto found = _contractInfoCache.find(transactionHash);
    if (found == _contractInfoCache.end())
    {
        return -1;
    }

    jTxInfo = found->second.first;
    return 0;
}

std::string TransactionCache::GetContractPrevBlockHash()
{
    return _preContractBlockHash;
}

bool TransactionCache::HasContractPackingPermission(const std::string& addr, uint64_t transactionHeight, uint64_t time)
{
    std::string packingAddr;
    if (CalculateThePackerByTime(time, transactionHeight, packingAddr, *std::make_unique<std::string>(), *std::make_unique<std::string>()) != 0)
    {
        return false;
    }
    DEBUGLOG("time: {}, height: {}, packer {}", time, transactionHeight, packingAddr);
    return packingAddr == addr;
}

void TransactionCache::ContractBlockNotify(const std::string& blockHash)
{
    if (_preContractBlockHash.empty())
    {
        return;
    }
    if (blockHash == _preContractBlockHash)
    {
        _contractPreBlockWaiter.notify_one();
    }
}

std::string
TransactionCache::GetAndUpdateContractPreHash(const std::string &contractAddress, const std::string &transactionHash,
                                              std::map<std::string, std::string> &contractPreHashCache)
{
    std::string strPrevTxHash;
    auto found = contractPreHashCache.find(contractAddress);
    if (found == contractPreHashCache.end())
    {
        DBReader dbReader;
        if (dbReader.GetLatestUtxoByContractAddr(contractAddress, strPrevTxHash) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetLatestUtxo of ContractAddr {} fail", contractAddress);
            return "";
        }
        if (strPrevTxHash.empty())
        {
            return "";
        }
    }
    else
    {
        strPrevTxHash = found->second;
    }
    contractPreHashCache[contractAddress] = transactionHash;

    return strPrevTxHash;
}

void TransactionCache::SetDirtyContractMap(const std::string& transactionHash, const std::set<std::string>& dirtyContract)
{
    std::unique_lock locker(_dirtyContractMapMutex);
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    _dirtyContractMap[transactionHash]= {currentTime, dirtyContract};

}

bool TransactionCache::GetDirtyContractMap(const std::string& transactionHash, std::set<std::string>& dirtyContract)
{
    auto found = _dirtyContractMap.find(transactionHash);
    if(found != _dirtyContractMap.end())
    {
        dirtyContract = found->second.second;
        return true;
    }
    
    return false;
}

void TransactionCache::removeExpiredEntriesFromDirtyContractMap()
{
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    for(auto iter = _dirtyContractMap.begin(); iter != _dirtyContractMap.end();)
    {
        if(nowTime >= iter->second.first + 60 * 1000000ull)
        {
            DEBUGLOG("remove txHash:{}", iter->first);
            _dirtyContractMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

void TransactionCache::_AddBuildTx(const CTransaction &transaction)
{
    std::unique_lock<std::mutex> locker(_buildTxsMutex);
    buildTxs.push_back(transaction);
}
const std::list<CTransaction>& TransactionCache::_GetBuildTxs()
{
    std::unique_lock<std::mutex> locker(_buildTxsMutex);
    return buildTxs;
}
void TransactionCache::ClearBuildTxs()
{
    std::unique_lock<std::mutex> locker(_buildTxsMutex);
    buildTxs.clear();
}

int GetContractPrehashFindNode(uint32_t num, uint64_t selfNodeHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &sendNodeIds)
{
    int ret = 0;
    if ((ret = SyncBlock::GetSyncNodeSimplify(num, selfNodeHeight, pledgeAddr, sendNodeIds)) != 0)
    {
        ERRORLOG("get seek node fail, ret:{}", ret);
        return -1;
    }
    return 0;
}


int TransactionCache::HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata)
{
    std::unique_lock<std::mutex> locker(_threadMutex);
    DEBUGLOG("111111111111 HHHHHHHHHH");
    // todo verify version
    // todo verify sign
    auto cSign = msg->sign();
    auto pub = cSign.pub();
    auto signature = cSign.sign();

    ContractPackagerMsg cp_msg = *msg;
    cp_msg.clear_sign();
	std::string message = Getsha256hash(cp_msg.SerializeAsString());
    Account account;
    if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(pub, account) == false){
        ERRORLOG(RED " HandleContractPackagerMsg Get public key from bytes failed!" RESET);
        return -1;
    }
    if(account.Verify(message, signature) == false)
    {
        ERRORLOG(RED "HandleBuildBlockBroadcastMsg Public key verify sign failed!" RESET);
        return -2;
    }

    // todo verify vrf
    Vrf vrfInfo = msg->vrfinfo();
    std::string hash;
    int range;
    uint64_t verifyHeight;

    const VrfData& vrfData = vrfInfo.vrfdata();
	hash = vrfData.hash();
	range = vrfData.range();
    verifyHeight = vrfData.height();

	EVP_PKEY *pkey = nullptr;
	if (!GetEDPubKeyByBytes(vrfInfo.vrfsign().pub(), pkey))
	{
		ERRORLOG(RED "HandleBuildBlockBroadcastMsg Get public key from bytes failed!" RESET);
		return -4;
	}

    std::string contractHash;
    for (const TxMsgReq& txMsg : msg->txmsgreq())
    {
        const TxMsgInfo& txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx()))
        {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }
        contractHash += transaction.hash();
    }
    std::string input = Getsha256hash(contractHash);
    DEBUGLOG("HandleContractPackagerMsg input : {} , hash : {}", input,hash);
	std::string result = hash;
	std::string proof = vrfInfo.vrfsign().sign();
	if (MagicSingleton<VRF>::GetInstance()->VerifyVRF(pkey, input, result, proof) != 0)
	{
		ERRORLOG(RED "HandleBuildBlockBroadcastMsg Verify VRF Info fail" RESET);
		return -5;
	}

    std::vector<Node> _vrfNodelist;
    for(auto & item : msg->vrfdatasource().vrfnodelist())
    {
        Node x;
        x.address = item;
        _vrfNodelist.push_back(x);
    }

    DEBUGLOG("2222222222222 HHHHHHHHHH");

    auto ret = verifyVrfDataSource(_vrfNodelist,verifyHeight);
    if(ret != 0)
    {
        ERRORLOG("verifyVrfDataSource fail ! ,ret:{}", ret);
        return -6;
    }

    Node node;
	if (!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(msgdata.fd, node))
	{
		return -7;
	}
    DEBUGLOG("dispatchNodeAddr:{}", node.address);
    double randNum = MagicSingleton<VRF>::GetInstance()->GetRandNum(result);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    ret = VerifyContractPackNode(node.address, randNum, defaultAddr,_vrfNodelist);
    if(ret != 0)
    {
        ERRORLOG("VerifyContractPackNode ret  {}", ret);
        return -8;
    }
    DEBUGLOG("333333333333333 HHHHHHHHHH,txSize:{}", msg->txmsgreq().size());
    std::set<CTransaction> ContractTxs;

    std::map<int64_t, std::vector<TxMsgReq>> blockTimestampGroupedTx;

    for (const TxMsgReq& txMsg : msg->txmsgreq())
    {
        const TxMsgInfo &txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx())) {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }
        int64_t blockTimestamp = evm_environment::GetBlockTimestamp(transaction);
        if (blockTimestamp <= 0)
        {
            ERRORLOG("block timestamp less than 0, tx: {}", transaction.hash());
            continue;
        }
        DEBUGLOG("time: {}, tx: {}", blockTimestamp, transaction.hash());
        blockTimestampGroupedTx[blockTimestamp].push_back(txMsg);
    }

    for (const auto& [blockTimestamp, txMsgReqs] : blockTimestampGroupedTx)
    {

        DealContractTransaction(txMsgReqs);
    }
}

int TransactionCache::DealContractTransaction(const std::vector<TxMsgReq> &txMsgReqs) {
    packDispatch packdis;
    std::map<std::string, std::future<int>> txTaskResults;
    int64_t topTransactionHeight = 0;

    for (const TxMsgReq& txMsg : txMsgReqs)
    {
        const TxMsgInfo& txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx()))
        {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }

        if (txMsgInfo.nodeheight() > topTransactionHeight)
        {
            topTransactionHeight = txMsgInfo.nodeheight();
        }
        DEBUGLOG("tx:{} height {}", transaction.hash(), txMsgInfo.nodeheight());
        DEBUGLOG("SetDirtyContractMap, txHash:{}", transaction.hash());
        SetDirtyContractMap(transaction.hash(), {txMsgInfo.contractstoragelist().begin(), txMsgInfo.contractstoragelist().end()});


        std::vector<std::string> dependentAddress(txMsgInfo.contractstoragelist().begin(), txMsgInfo.contractstoragelist().end());
        packdis.Add(transaction.hash(),dependentAddress,transaction);

        auto task = std::make_shared<std::packaged_task<int()>>(
                [txMsg, txMsgInfo, transaction] {
                    std::string dispatcherAddr = transaction.identity();
                    if(!HasContractPackingPermission(dispatcherAddr, txMsgInfo.nodeheight(), transaction.time()))
                    {
                        ERRORLOG("HasContractPackingPermission fail!!!, txHash:{}", transaction.hash().substr(0,6));
                        return -1;
                    }

                    int ret = DoHandleTx(std::make_shared<TxMsgReq>(txMsg), *std::make_unique<CTransaction>());
                    if (ret != 0)
                    {
                        ERRORLOG("DoHandleTx fail ret: {}, tx hash : {}", ret, transaction.hash());
                        return ret;
                    }
                    return 0;
                });
        try
        {
            txTaskResults[transaction.hash()] = task->get_future();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        MagicSingleton<TaskPool>::GetInstance()->CommitWorkTask([task](){(*task)();});
    }
    DEBUGLOG("block height will be {}", topTransactionHeight);
    DEBUGLOG("3333333333333444 HHHHHHHHHH");

    std::map<uint32, std::future<std::pair<int, std::string>>> dependentContractTxTaskResults;
    std::vector<std::future<std::pair<int, std::string>>> nondependentContractTxTaskResults;

    std::map<uint32_t, std::map<std::string, CTransaction>> dependentContractTxMap;
    std::map<std::string, CTransaction> nondependentContractTxMap;
    packdis.GetDependentData(dependentContractTxMap, nondependentContractTxMap);
    DEBUGLOG("44444444444 HHHHHHHHHH");
    auto processDependentContract = [&dependentContractTxTaskResults](const uint32_t N, std::map<std::string, CTransaction> ContractTxs, int64_t blockNumber) {
        auto task = std::make_shared<std::packaged_task<std::pair<int, std::string>()>>(
            [ContractTxs, blockNumber] {
                return MagicSingleton<TransactionCache>::GetInstance()->_ExecuteContracts(ContractTxs, blockNumber);
            });

        if (task)
            dependentContractTxTaskResults[N] =task->get_future();
        else
            return -10;

        MagicSingleton<TaskPool>::GetInstance()->CommitTxTask([task]() { (*task)(); });

        return 0;
    };

    for(const auto& iter : dependentContractTxMap)
    {
        DEBUGLOG("dependentContractTxMap HHHHHHHHHH first:{}, second size:{}", iter.first, iter.second.size());
        processDependentContract(iter.first, iter.second, topTransactionHeight);
    }
    DEBUGLOG("555555555555 HHHHHHHHHH");

    DEBUGLOG("nondependentContractTxMap HHHHHHHHHH size:{}", nondependentContractTxMap.size());
    for(const auto& iter : nondependentContractTxMap)
    {
        std::map<std::string, CTransaction> ContractTxs;
        ContractTxs.emplace(iter.first, iter.second);

        auto task = std::make_shared<std::packaged_task<std::pair<int, std::string>()>>(
        [ContractTxs, topTransactionHeight]
        {
            return MagicSingleton<TransactionCache>::GetInstance()->_ExecuteContracts(ContractTxs, topTransactionHeight);
        });

        if(task)    nondependentContractTxTaskResults.push_back(task->get_future());
        else    return -10;

        MagicSingleton<TaskPool>::GetInstance()->CommitBlockTask([task](){(*task)();});
    }
    DEBUGLOG("66666666666 HHHHHHHHHH");
    std::map<std::string, int> txRes;
    for (auto& res : txTaskResults)
    {
        if (res.second.valid()) 
        {
            auto retPair = res.second.get();
            txRes[res.first] = retPair;
        }
    }
    DEBUGLOG("77777777777 HHHHHHHHHH");

    std::set<uint32_t> failDependent;
    for (auto& res : txRes)
    {
        if(res.second != 0)
        {
            ERRORLOG("failDependent txHash:{}, ret:{}", res.first, res.second);
            for (auto iter = dependentContractTxMap.begin(); iter != dependentContractTxMap.end();) 
            {
                if (iter->second.find(res.first) != iter->second.end()) 
                {
                    RemoveContractInfoCacheTransaction(iter->second);
                    failDependent.insert(iter->first);
                    ERRORLOG("verify tx fail !!! delete contract tx:{}", res.first);
                    iter->second.erase(res.first);
                }

                if (iter->second.empty()) 
                {
                    iter = dependentContractTxMap.erase(iter);
                } 
                else 
                {
                    ++iter;
                }
            }
            if(nondependentContractTxMap.find(res.first) != nondependentContractTxMap.end())
            {
                std::map<std::string, CTransaction> contractTxs;
                contractTxs[res.first] = {};
                RemoveContractInfoCacheTransaction(contractTxs);
            }
        }
    }

    for(const auto& iter : failDependent)
    {
        dependentContractTxTaskResults.erase(iter);
        processDependentContract(iter, dependentContractTxMap[iter], topTransactionHeight);
    }

    std::map<std::string, int> depenDentCTxRes;
    for (auto& res : dependentContractTxTaskResults)
    {
        if (res.second.valid()) 
        {
            auto retPair = res.second.get();
            depenDentCTxRes[retPair.second] = retPair.first;
        }
    }
    DEBUGLOG("888888888888 HHHHHHHHHH");

    std::map<std::string, int> nondepenDentCTxRes;
    for (auto& res : nondependentContractTxTaskResults)
    {
        if(res.valid())
        {
            auto retPair = res.get();
            nondepenDentCTxRes[retPair.second] = retPair.first;
        }
        
    }
    DEBUGLOG("999999999999 HHHHHHHHHH");

    for (auto& res : depenDentCTxRes)
    {
        if(res.second != 0)
        {
            ERRORLOG("faildepenDentCTxRes txHash:{}, ret:{}", res.first, res.second);
            for(const auto& iter : dependentContractTxMap)
            {
                if(iter.second.find(res.first) != iter.second.end())
                {
                    RemoveContractsCacheTransaction(iter.second);
                }
            }
            ERRORLOG("_ExecuteDependentContractTx fail!!!, txHash:{}", res.first);
        }
    }

    for (auto& res : nondepenDentCTxRes)
    {
        if(res.second != 0)
        {
            ERRORLOG("nondepenDentCTxRes txHash:{}, ret:{}", res.first, res.second);
            std::map<std::string, CTransaction> contractTxs;
            contractTxs[res.first] = {};
            RemoveContractsCacheTransaction(contractTxs);
            ERRORLOG("_ExecuteNonDependentContractTx fail!!!, txHash:{}", res.first);
        }
    }
    
    DEBUGLOG("1010101010101 HHHHHHHHHH");
    ProcessContract(topTransactionHeight);
    DEBUGLOG("Packager HandleContractPackagerMsg 005 ");
    return 0;
}

int TransactionCache::_GetContractTxPreHash(const std::list<CTransaction>& txs, std::list<std::pair<std::string, std::string>>& contractTxPreHashList)
{
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> contractTxPreHashMap;
    for(auto& tx : txs)
	{
        if (global::ca::TxType::kTxTypeDeployContract == (global::ca::TxType)tx.txtype())
        {
            continue;
        }
        auto txHash = tx.hash();
        nlohmann::json txStorage;
        if (MagicSingleton<TransactionCache>::GetInstance()->GetContractInfoCache(txHash, txStorage) != 0)
        {
            ERRORLOG("can't find storage of tx {}", txHash);
            return -1;
        }

        for(auto &it : txStorage[Evmone::contractPreHashKeyName].items())
        {
            contractTxPreHashMap[txHash].push_back({it.key(), it.value()});
        }
	}
    DBReader dbReader;
    for(auto& iter : contractTxPreHashMap)
    {
        for(auto& preHashPair : iter.second)
        {
            if(contractTxPreHashMap.find(preHashPair.second) != contractTxPreHashMap.end())
            {
                continue;
            }
            std::string DBContractPreHash;
            if (DBStatus::DB_SUCCESS != dbReader.GetLatestUtxoByContractAddr(preHashPair.first, DBContractPreHash))
            {
                ERRORLOG("GetLatestUtxoByContractAddr fail !!! ContractAddr:{}", preHashPair.first);
                return -2;
            }
            if(DBContractPreHash != preHashPair.second)
            {
                ERRORLOG("DBContractPreHash:({}) != preHashPair.second:({})", DBContractPreHash, preHashPair.second);
                return -3;
            }
            contractTxPreHashList.push_back(preHashPair);
        }
    }
    return 0;
}

bool TransactionCache::RemoveContractInfoCacheTransaction(const std::map<std::string, CTransaction>& contractTxs)
{
    std::shared_lock<std::shared_mutex> locker(_contractInfoCacheMutex);
    auto it = _contractInfoCache.begin();
    while (it != _contractInfoCache.end()) 
    {
        if (contractTxs.find(it->first) != contractTxs.end()) 
        {
            DEBUGLOG("txHash:{}", it->first);
            it = _contractInfoCache.erase(it); 
        } 
        else 
        {
            ++it;
        }
    }
    return true;
}

bool TransactionCache::RemoveContractsCacheTransaction(const std::map<std::string, CTransaction>& contractTxs) {
    std::unique_lock<std::mutex> locker(_contractCacheMutex);
    auto it = _contractCache.begin();
    while (it != _contractCache.end()) {
        if (contractTxs.find(it->GetTransaction().hash()) != contractTxs.end()) {
            it = _contractCache.erase(it); 
        } else {
            ++it;
        }
    }
    return true;
}

int _HandleSeekContractPreHashReq(const std::shared_ptr<newSeekContractPreHashReq> &msg, const MsgData &msgdata)
{
    newSeekContractPreHashAck ack;
    ack.set_version(msg->version());
    ack.set_msg_id(msg->msg_id());
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    Node node;
	if (!MagicSingleton<PeerNode>::GetInstance()->FindNodeByFd(msgdata.fd, node))
	{
        ERRORLOG("FindNodeByFd fail !!!, seekId:{}", node.address);
		return -1;
	}

    DBReader dbReader;
    if(msg->seekroothash_size() >= 200)
    {
        ERRORLOG("msg->seekroothash_size:({}) >= 200", msg->seekroothash_size());
        return -2;
    }
    for(auto& preHashPair : msg->seekroothash())
    {
        std::string DBContractPreHash;
        if (DBStatus::DB_SUCCESS != dbReader.GetLatestUtxoByContractAddr(preHashPair.contractaddr(), DBContractPreHash))
        {
            ERRORLOG("GetLatestUtxoByContractAddr fail !!!");
            return -3;
        }
        if(DBContractPreHash != preHashPair.roothash())
        {
            DEBUGLOG("DBContractPreHash:({}) != roothash:({}) seekId:{}", DBContractPreHash, preHashPair.roothash(), node.address);
            std::string strPrevBlockHash;
            if(dbReader.GetBlockHashByTransactionHash(DBContractPreHash, strPrevBlockHash) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("GetBlockHashByTransactionHash failed!");
                return -4;
            }
            std::string blockRaw;
            if(dbReader.GetBlockByBlockHash(strPrevBlockHash, blockRaw) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("GetBlockByBlockHash failed!");
                return -5;
            }
            auto seekContractBlock = ack.add_seekcontractblock();
            seekContractBlock->set_contractaddr(preHashPair.contractaddr());
            seekContractBlock->set_roothash(strPrevBlockHash);
            seekContractBlock->set_blockraw(blockRaw);
        }
    }

    NetSendMessage<newSeekContractPreHashAck>(node.address, ack, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    return 0;
}
int _HandleSeekContractPreHashAck(const std::shared_ptr<newSeekContractPreHashAck> &msg, const MsgData &msgdata)
{
    GLOBALDATAMGRPTR.AddWaitData(msg->msg_id(),msg->self_node_id(),msg->SerializeAsString());
    return 0;
}

int _newSeekContractPreHash(const std::list<std::pair<std::string, std::string>> &contractTxPreHashList)
{
    DEBUGLOG("_newSeekContractPreHash.............");
    uint64_t chainHeight;
    if(!MagicSingleton<BlockHelper>::GetInstance()->ObtainChainHeight(chainHeight))
    {
        DEBUGLOG("ObtainChainHeight fail!!!");
    }
    uint64_t selfNodeHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.GetBlockTop(selfNodeHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            DEBUGLOG("GetBlockTop fail!!!");

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
    std::vector<std::string> sendNodeIds;
    if (GetPrehashFindNode(pledgeAddr.size(), chainHeight, pledgeAddr, sendNodeIds) != 0)
    {
        ERRORLOG("get sync node fail");
    }

    if(sendNodeIds.size() == 0)
    {
        DEBUGLOG("sendNodeIds {}",sendNodeIds.size());
        return -2;
    }

    //send_size
    std::string msgId;
    if (!GLOBALDATAMGRPTR.CreateWait(3, sendNodeIds.size() * 0.8, msgId))
    {
        return -3;
    }

    newSeekContractPreHashReq req;
    req.set_version(global::kVersion);
    req.set_msg_id(msgId);

    for(auto &item : contractTxPreHashList)
    {
        preHashPair * _hashPair = req.add_seekroothash();
        _hashPair->set_contractaddr(item.first);
        _hashPair->set_roothash(item.second);
        DEBUGLOG("req contractAddr:{}, contractTxHash:{}", item.first, item.second);
    }
    
    for (auto &node : sendNodeIds)
    {
        if(!GLOBALDATAMGRPTR.AddResNode(msgId, node))
        {
            return -4;
        }
        NetSendMessage<newSeekContractPreHashReq>(node, req, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_1);
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR.WaitData(msgId, ret_datas))
    {
        return -5;
    }

    newSeekContractPreHashAck ack;
    std::map<std::string, std::set<std::string>> blockHashMap;
    std::map<std::string, std::pair<std::string, std::string>> testMap;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        for(auto& iter : ack.seekcontractblock())
        {
            blockHashMap[ack.self_node_id()].insert(iter.blockraw());
            testMap[iter.blockraw()] = {iter.contractaddr(), iter.roothash()};
        }
    }

    std::unordered_map<std::string , int> countMap;
    for (auto& iter : blockHashMap) 
    {
        for(auto& iter_second : iter.second)
        {
            countMap[iter_second]++;
        }
        
    }

    DBReader dbReader;
    std::vector<std::pair<CBlock,std::string>> seekBlocks;
    for (const auto& iter : countMap) 
    {
        double rate = double(iter.second) / double(blockHashMap.size());
        auto test_iter = testMap[iter.first];
        if(rate < 0.66)
        {
            ERRORLOG("rate:({}) < 0.66, contractAddr:{}, contractTxHash:{}", rate, test_iter.first, test_iter.second);
            continue;
        }

        CBlock block;
        if(!block.ParseFromString(iter.first))
        {
            continue;
        }
        std::string blockStr;
        if(dbReader.GetBlockByBlockHash(block.hash(), blockStr) != DBStatus::DB_SUCCESS)
        {
            seekBlocks.push_back({block, block.hash()});
            DEBUGLOG("rate:({}) < 0.66, contractAddr:{}, contractTxHash:{}, blockHash:{}", rate, test_iter.first, test_iter.second, block.hash());
            MagicSingleton<BlockHelper>::GetInstance()->AddSeekBlock(seekBlocks);
        }
    }

    uint64_t timeOut = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp() + 2 * 1000000;
    uint64_t currentTime;
    bool flag;
    do
    {
        flag = true;
        for(auto& it : seekBlocks)
        {
            std::string blockRaw;
            if(dbReader.GetBlockByBlockHash(it.second, blockRaw) != DBStatus::DB_SUCCESS)
            {
                flag = false;
                break;
            }
        }
        if(flag)
        {
            DEBUGLOG("find block successfuly ");
            return 0;
        }
        sleep(1);
        currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    }while(currentTime < timeOut && !flag);
    return -6;
}

int HandleContractPackagerMsg(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata)
{
    MagicSingleton<TransactionCache>::GetInstance()->HandleContractPackagerMsg(msg, msgdata);
    return 0;
}
