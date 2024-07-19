#include "ca.h"

#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <random>
#include <fcntl.h>
#include <filesystem>

#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/contract.h"
#include "ca/dispatchtx.h"
#include "ca/block_helper.h"
#include "ca/transaction.h"
#include "ca/tfs_wasmtime.h"
#include "ca/sync_block.h"
#include "ca/advanced_menu.h"

#include "ca/double_spend_cache.h"
#include "ca/block_http_callback.h"
#include "ca/failed_transaction_cache.h"

#include "utils/bip39.h"
#include "utils/qrcode.h"
#include "utils/console.h"
#include "utils/tmp_log.h"
#include "utils/hex_code.h"
#include "utils/time_util.h"
#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/contract_utils.h"

#include "api/interface/rpc_tx.h"
#include "utils/base64.h"

#include "net/api.h"
#include "net/epoll_mode.h"

#include "proto/interface.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "google/protobuf/util/json_util.h"

#include "db/db_api.h"
#include "ca/evm/evm_manager.h"
#include "api/interface/http_api.h"
#include "../api/rpc_error.h"


bool bStopTx = false;
bool bIsCreateTx = false;
const static uint64_t input_limit = 500000;

int CaStartTimerTask()
{
    // Blocking thread
    global::ca::kBlockPoolTimer.AsyncLoop(100, [](){ MagicSingleton<BlockHelper>::GetInstance()->Process(); });
    
    //SeekBlock Thread
    global::ca::kSeekBlockTimer.AsyncLoop(3 * 1000, [](){ MagicSingleton<BlockHelper>::GetInstance()->SeekBlockThread(); });
    
    //Start patch thread
    MagicSingleton<BlockHelper>::GetInstance()->SeekBlockThread();

    // Block synchronization thread
    MagicSingleton<SyncBlock>::GetInstance()->ThreadStart();
    MagicSingleton<CheckBlocks>::GetInstance()->StartTimer();

    MagicSingleton<FailedTransactionCache>::GetInstance()->_StartTimer();

    MagicSingleton<BlockStroage>::GetInstance();

    MagicSingleton<DoubleSpendCache>::GetInstance();

    return 0;
}

bool CaInit()
{
    RegisterInterface();

    // Register interface with network layer
    RegisterCallback();
    

    // Register HTTP related interfaces
    if(MagicSingleton<Config>::GetInstance()->GetRpc())
    {
        _CaRegisterHttpCallbacks();
    }

    // Start timer task
    CaStartTimerTask();

    // NTP verification
    CheckNtpTime();

    MagicSingleton<TransactionCache>::GetInstance()->Process();

    MagicSingleton<ContractDispatcher>::GetInstance()->Process();

    std::filesystem::create_directory("./contract");

//    TFSC::_wasm_time_init();
    return true;
}

int CaEndTimerTask()
{
    global::ca::kDataBaseTimer.Cancel();
    global::ca::kBlockPoolTimer.Cancel();
    global::ca::kSeekBlockTimer.Cancel();
    return 0;
}

void CaCleanup()
{
    DEBUGLOG("start clean")
    CaEndTimerTask();
    DEBUGLOG("start clean g_heartTimer")
    global::g_heartTimer.Cancel();
    DEBUGLOG("start clean FailedTransactionCache")
    MagicSingleton<FailedTransactionCache>::GetInstance()->StopTimer();
    DEBUGLOG("start clean BlockHelper")
    MagicSingleton<BlockHelper>::GetInstance()->StopSaveBlock();
    DEBUGLOG("start clean TFSbenchmark")
    MagicSingleton<TFSbenchmark>::GetInstance()->Clear();
    DEBUGLOG("start clean TransactionCache")
    MagicSingleton<TransactionCache>::GetInstance()->Stop();
    DEBUGLOG("start clean SyncBlock")
    MagicSingleton<SyncBlock>::GetInstance()->ThreadStop();
    DEBUGLOG("start clean BlockStroage")
    MagicSingleton<BlockStroage>::GetInstance()->StopTimer();
    DEBUGLOG("start clean DoubleSpendCache")
    MagicSingleton<DoubleSpendCache>::GetInstance()->StopTimer();
    DEBUGLOG("start clean EpollMode")
    MagicSingleton<EpollMode>::GetInstance()->EpollStop();
    DEBUGLOG("start clean CBlockHttpCallback")
    MagicSingleton<CBlockHttpCallback>::GetInstance()->Stop();
    DEBUGLOG("start clean PeerNode")
    MagicSingleton<PeerNode>::GetInstance()->StopNodesSwap();
    DEBUGLOG("start clean CheckBlocks")
    MagicSingleton<CheckBlocks>::GetInstance()->StopTimer();
    DEBUGLOG("start clean VRF" )
    MagicSingleton<VRF>::GetInstance()->StopTimer();
    DEBUGLOG("sleep")

    sleep(5);
    DEBUGLOG("start clean DB")
    DBDestory();
    DEBUGLOG("clean finish")
}

void PrintBasicInfo()
{
    using namespace std;
    std::string version = global::kVersion;
    std::string addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();

    uint64_t balance = 0;
    GetBalanceByUtxo(addr, balance);
    DBReader dbReader;

    uint64_t blockHeight = 0;
    dbReader.GetBlockTop(blockHeight);

    std::string ownID = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();

    CaConsole infoColor(kConsoleColor_Green, kConsoleColor_Black, true);
    double b = balance / double(100000000);
    
    cout << "*********************************************************************************" << endl;
    cout << "Version: " << version << endl;
    cout << "addr: " << "0x" + addr << endl;
    cout << "Balance: " << setiosflags(ios::fixed) << setprecision(8) << b << endl;
    cout << "Block top: " << blockHeight << endl;
    cout << "*********************************************************************************" << endl;
  
}

void HandleTransaction()
{
    std::cout << std::endl
              << std::endl;

    std::string strFromAddr;
    std::cout << "input FromAddr :" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }

    std::string strToAddr;
    std::cout << "input toAddress :" << std::endl;
    std::cin >> strToAddr;
    if (strToAddr.substr(0, 2) == "0x") 
    {
        strToAddr = strToAddr.substr(2);
    }

    std::string strAmt;
    std::cout << "input amount :" << std::endl;
    std::cin >> strAmt;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strAmt, pattern))
    {
        std::cout << "input amount error ! " << std::endl;
        return;
    }

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }


    std::vector<std::string> fromAddr;
    fromAddr.emplace_back(strFromAddr);
    uint64_t amount = (std::stod(strAmt) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    std::map<std::string, int64_t> toAddrAmount;
    toAddrAmount[strToAddr] = amount;

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;

    Vrf info;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, encodedInfo, top + 1,  outTx,isNeedAgentFlag,info, isFindUtxo);
    if (ret != 0)
    {
        ERRORLOG("CreateTxTransaction error!! ret:{}", ret);
        return;
    }
    
    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo -> CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        if(outTx.identity() == defaultAddr)
        {
            ret = DoHandleTx(msg,outTx);
        }
        else
        {
            ret = DropshippingTx(msg, outTx, outTx.identity());
        }
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());

    return;
}

void HandleStake()
{
    std::cout << std::endl
              << std::endl;

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.GetAddr();
    std::cout << "stake addr: " << "0x"+ strFromAddr << std::endl;
    std::string strStakeFee;
    std::cout << "Please enter the amount to stake:" << std::endl;
    std::cin >> strStakeFee;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strStakeFee, pattern))
    {
        std::cout << "input stake amount error " << std::endl;
        return;
    }

    std::regex bonus("^(5|6|7|8|9|1[0-9]|20)$");
    std::cout <<"Please input the bonus pumping percentage to stake (5 - 20):" << std::endl;
    std::string strRewardRank;
    std::cin >> strRewardRank;
    if(!std::regex_match(strRewardRank,bonus))
    {
        std::cout << "input the bonus pumping percentage error" << std::endl;
        return;
    }
    
    double commissionRate = std::stold(strRewardRank) / 100;
    if(commissionRate < global::ca::KMinCommissionRate || commissionRate > global::ca::KMaxCommissionRate)
    {
        std::cout << "input the bonus pumping percentage error" << std::endl;
        return;
    }
    std::cout << commissionRate << std::endl;
    
    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    TxHelper::pledgeType pledgeType = TxHelper::pledgeType::kPledgeType_Node;

    uint64_t stakeAmount = std::stod(strStakeFee) * global::ca::kDecimalNum;

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if (TxHelper::CreateStakeTransaction(strFromAddr, stakeAmount, encodedInfo, top + 1,  pledgeType, outTx, outVin,isNeedAgentFlag,info, commissionRate, isFindUtxo) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    auto ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();

    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret=DropshippingTx(msg,outTx);
    }else{
        if(outTx.identity() == defaultAddr)
        {
            ret = DoHandleTx(msg,outTx);
        }
        else
        {
            ret = DropshippingTx(msg, outTx, outTx.identity());
        }
    }

    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void HandleUnstake()
{
    std::cout << std::endl
              << std::endl;

    std::string strFromAddr;
    std::cout << "Please enter unstake addr:" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }

    DBReader dbReader;
    std::vector<std::string> utxos;
    dbReader.GetStakeAddressUtxo(strFromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::cout << "-- Current pledge amount: -- " << std::endl;
    for (auto &utxo : utxos)
    {
        std::string txRaw;
        dbReader.GetTransactionByHash(utxo, txRaw);
        CTransaction tx;
        tx.ParseFromString(txRaw);
        uint64_t value = 0;
        for (auto &vout : tx.utxo().vout())
        {
            if (vout.addr() == global::ca::kVirtualStakeAddr)
            {
                value = vout.value();
                break;
            }
        }
        std::cout << "utxo: " << utxo << " value: " << value << std::endl;
    }
    std::cout << std::endl;

    std::string strUtxoHash;
    std::cout << "utxo:";
    std::cin >> strUtxoHash;

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }
    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if (TxHelper::CreatUnstakeTransaction(strFromAddr, strUtxoHash, encodedInfo, top + 1, outTx, outVin,isNeedAgentFlag,info,isFindUtxo) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    auto ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret=DropshippingTx(msg,outTx);
    }else{
        if(outTx.identity() == defaultAddr)
        {
            ret = DoHandleTx(msg,outTx);
        }
        else
        {
            ret = DropshippingTx(msg, outTx, outTx.identity());
        }
    }
    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void HandleInvest()
{
    std::cout << std::endl
              << std::endl;
    std::cout << "AddrList:" << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }
    if (!isValidAddress(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strToAddr;
    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cin >> strToAddr;
    if (strToAddr.substr(0, 2) == "0x") 
    {
        strToAddr = strToAddr.substr(2);
    }
    if (!isValidAddress(strToAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strInvestFee;
    std::cout << "Please enter the amount to delegate:" << std::endl;
    std::cin >> strInvestFee;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strInvestFee, pattern))
    {
        ERRORLOG("Input invest fee error!");
        std::cout << "Input delegate fee error!" << std::endl;
        return;
    }
    
    TxHelper::InvestType investType = TxHelper::InvestType::kInvestType_NetLicence;
    uint64_t investAmount = std::stod(strInvestFee) * global::ca::kDecimalNum;

    
    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, investAmount, encodedInfo, top + 1,  investType, outTx, outVin,isNeedAgentFlag,info, isFindUtxo);
    if (ret != 0)
    {
        ERRORLOG("Failed to create investment transaction! The error code is:{}", ret);
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret=DropshippingTx(msg,outTx);
    }else{
        if(outTx.identity() == defaultAddr)
        {
            ret = DoHandleTx(msg,outTx);
        }
        else
        {
            ret = DropshippingTx(msg, outTx, outTx.identity());
        }
    }
    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void HandleDisinvest()
{
    std::cout << std::endl
              << std::endl;

    std::cout << "AddrList : " << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }
    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strToAddr;
    std::cout << "Please enter the addr you want to withdraw from:" << std::endl;
    std::cin >> strToAddr;
    if (strToAddr.substr(0, 2) == "0x") 
    {
        strToAddr = strToAddr.substr(2);
    }
    if (!isValidAddress(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dbReader;
    std::vector<std::string> utxos;
    dbReader.GetBonusAddrInvestUtxosByBonusAddr(strToAddr, strFromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::cout << "-- Current delegate amount: -- " << std::endl;
    for (auto &utxo : utxos)
    {
        std::cout << "utxo: " << utxo << std::endl;
    }
    std::cout << std::endl;

    std::string strUtxoHash;
    std::cout << "Please enter the utxo you want to withdraw:";
    std::cin >> strUtxoHash;

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    int ret = TxHelper::CreateDisinvestTransaction(strFromAddr, strToAddr, strUtxoHash, encodedInfo, top + 1, outTx, outVin,isNeedAgentFlag,info, isFindUtxo);
    if (ret != 0)
    {
        ERRORLOG("Create withdraw transaction error!:{}", ret);
        return;
    }
    
    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret=DropshippingTx(msg,outTx);
    }else{
        if(outTx.identity() == defaultAddr)
        {
            ret = DoHandleTx(msg,outTx);
        }
        else
        {
            ret = DropshippingTx(msg, outTx, outTx.identity());
        }
    }

    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void HandleBonus()
{
    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    std::string strFromAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    int ret = TxHelper::CreateBonusTransaction(strFromAddr, encodedInfo, top + 1,  outTx, outVin,isNeedAgentFlag,info, isFindUtxo);
    if (ret != 0)
    {
        ERRORLOG("Failed to create bonus transaction! The error code is:{}", ret);
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }
    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        ret=DropshippingTx(msg,outTx);
    }else{
        ret=DoHandleTx(msg,outTx);
    }

    if (ret != 0)
    {
        ret -= 100;
    }
    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

std::vector<uint8_t> hexStringToBytes(const std::string& hexString) {
    std::vector<uint8_t> bytes;
    std::istringstream stream(hexString);
    std::string byteStr;
    
    while (stream >> std::setw(2) >> byteStr) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
    }

    return bytes;
}

void HandleAccountManger()
{
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::cout << std::endl
              << std::endl;
    while (true)
    {
        std::cout << "0.Exit" << std::endl;
        std::cout << "1. Set Default Account" << std::endl;
        std::cout << "2. Add Account" << std::endl;
        std::cout << "3. Remove " << std::endl;
        std::cout << "4. Import SeedKey" << std::endl;
        std::cout << "5. Export SeedKey" << std::endl;

        std::string strKey;
        std::cout << "Please input your choice: " << std::endl;
        std::cin >> strKey;
        std::regex pattern("^[0-6]$");
        if (!std::regex_match(strKey, pattern))
        {
            std::cout << "Invalid input." << std::endl;
            continue;
        }
        int key = std::stoi(strKey);
        switch (key)
        {
        case 0:
            return;
        case 1:
            HandleSetdefaultAccount();
            break;
        case 2:
            GenKey();
            break;
        case 3:
        {
            std::string addr;
            std::cout << "Please enter the address you want to remove :" << std::endl;
            std::cin >> addr;
            if (addr.substr(0, 2) == "0x") {
                addr = addr.substr(2); 
            }
            std::string confirm;
            std::cout << "Are you sure to delete (Y / N) " << std::endl;
            std::cin >> confirm;
            if(confirm == "Y")
            {
                MagicSingleton<AccountManager>::GetInstance()->DeleteAccount(addr);
            }
            else if(confirm == "N")
            {
                break;
            }
            else
            {
                std::cout << "Invalid input" << std::endl;
            }

            break;
        }
        case 4:
        {
            std::string hexString;
            std::cout << "Please input SeedKey string: ";
            std::cin >> hexString;

            if (hexString.length() != 32) {
                std::cout << "Invalid input. Please enter a 32-character hexadecimal string." << std::endl;
                return ;
            }

            uint8_t byteArray[16] = {0};

            std::stringstream ss;
            for (int i = 0; i < 16; i++) {
                unsigned int byte;
                ss.clear();
                ss.str(hexString.substr(2 * i, 2));
                ss >> std::hex >> byte;
                byteArray[i] = static_cast<uint8_t>(byte);
            }

           
            // std::cout << "Converted byte array: ";
            // for (int i = 0; i < 16; i++) {
            //     std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byteArray[i]) << " ";
            // }
            std::cout << std::endl;
            
            if (MagicSingleton<AccountManager>::GetInstance()->ImportPrivateKeyHex(byteArray) != 0)
            {
                std::cout << "Save PrivateKey failed!" << std::endl;
            }
            break;
        }
        case 5:
            HandleExportPrivateKey();
            break;
        default:
            std::cout << "Invalid input." << std::endl;
            continue;
        }
    }
}

void HandleSetdefaultAccount()
{
    std::string addr;
    std::cout << "Please enter the address you want to set :" << std::endl;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    if(!isValidAddress(addr))
    {
        std::cout << "The address entered is illegal" <<std::endl;
        return;
    }

    Account oldAccount;
    if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(oldAccount) != 0)
    {
        ERRORLOG("not found DefaultKeyBs58Addr  in the _accountList");
        return;
    }

    if (MagicSingleton<AccountManager>::GetInstance()->SetDefaultAccount(addr) != 0)
    {
        ERRORLOG("Set DefaultKeyBs58Addr failed!");
        return;
    }

    Account newAccount;
    if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(newAccount) != 0)
    {
        ERRORLOG("not found DefaultKeyBs58Addr  in the _accountList");
        return;
    }

    if (!isValidAddress(oldAccount.GetAddr()) ||
        !isValidAddress(newAccount.GetAddr()))
    {
        return;
    }

    // update base 58 addr
    NodeAddrChangedReq req;
    req.set_version(global::kVersion);

    NodeSign *oldSign = req.mutable_oldsign();
    oldSign->set_pub(oldAccount.GetPubStr());
    std::string oldSignature;
    if (!oldAccount.Sign(Getsha256hash(newAccount.GetAddr()), oldSignature))
    {
        return;
    }
    oldSign->set_sign(oldSignature);

    NodeSign *newSign = req.mutable_newsign();
    newSign->set_pub(newAccount.GetPubStr());
    std::string newSignature;
    if (!newAccount.Sign(Getsha256hash(oldAccount.GetAddr()), newSignature))
    {
        return;
    }
    newSign->set_sign(newSignature);

    MagicSingleton<PeerNode>::GetInstance()->SetSelfId(newAccount.GetAddr());
    MagicSingleton<PeerNode>::GetInstance()->SetSelfIdentity(newAccount.GetPubStr());
    std::vector<Node> publicNodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    for (auto &node : publicNodes)
    {
        net_com::SendMessage(node, req, net_com::Compress::kCompress_True, net_com::Priority::kPriority_High_2);
    }
    std::cout << "Set Default account success" << std::endl;
}

static std::string ReadFileIntoString(std::string filename)
{
	std::ifstream ifile(filename);
	std::ostringstream buf;
	char ch;
	while(buf&&ifile.get(ch))
    {
        buf.put(ch);
    }
	return buf.str();
}

static int LoopRead(const std::regex&& pattern, std::string_view input_name, std::string& input)
{
    if (input == "0")
    {
        input.clear();
    }
    else
    {
        while(!std::regex_match(input, pattern))
        {
            input.clear();
            std::cout << "invalid " <<  input_name << ", please enter again :(0 to skip, 1 to exit)" << std::endl;
            std::cin >> input;

            if (input == "0")
            {
                input.clear();
                return 0;
            }
            if (input == "1")
            {
                return -1;
            }
        }
    }
    if(input.size() > input_limit)
    {
        std::cout << "Input cannot exceed " << input_limit << " characters" << std::endl;
        return -2;
    }
    return 0;
}

static int LoopReadFile(std::string& input, std::string& output, const std::filesystem::path& filename = "")
{
    std::filesystem::path contract_path;
    bool raise_info = false;
    if (input == "0")
    {
        contract_path = std::filesystem::current_path() / "contract" / filename;
    }
    else
    {
        contract_path = input;
        raise_info = true;
    }

    if (raise_info)
    {
        while(!exists(contract_path))
        {
            input.clear();
            std::cout << contract_path << " doesn't exist! please enter again: (0 to skip, 1 to exit)" << std::endl;
            std::cin >> input;
            if (input == "0")
            {
                return 1;
            }
            if (input == "1")
            {
                return -1;
            }
            contract_path = input;
        }
    }
    else
    {
        if (!exists(contract_path))
        {
            return 1;
        }
    }

    output = ReadFileIntoString(contract_path.string());
    if(output.size() > input_limit)
    {
        std::cout << "Input cannot exceed " << input_limit << " characters" << std::endl;
        return -2;
    }
    return 0;
}

static int LoopReadJson(std::string& input, nlohmann::json& output, const std::filesystem::path& filename = "")
{
    std::string content;
    int ret = LoopReadFile(input, content, filename);
    if (ret < 0)
    {
        return -1;
    }
    else if (ret > 0)
    {
        output = "";
        return 0;
    }

    try
    {
        output = nlohmann::json::parse(content);
        return 0;
    }
    catch (...)
    {
        std::cout << "json parse fail, enter 0 to skip : (other key to exit)";
        std::cin >> input;
        if (input == "0")
        {
            output = "";
            return 0;
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

void HandleDeployContract()
{
        std::cout << std::endl
              << std::endl;

    std::cout << "AddrList : " << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }
    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dataReader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!")
        return ;
    }

    uint32_t nContractType = 0;
//    std::cout << "Please enter contract type: (0: EVM  1:WASM) " << std::endl;
//    std::cin >> nContractType;
//    if(nContractType != 0 && nContractType != 1)
//    {
//        std::cout << "The contract type was entered incorrectly" << std::endl;
//        return;
//    }

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txInfo);

    std::string encodedInfo = Base64Encode(txInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;
    int ret = 0;
    if(nContractType == 0)
    {
        std::string nContractPath;
        std::cout << "Please enter contract path : (enter 0 use default path ./contract/contract) " << std::endl;
        std::cin >> nContractPath;
        std::string code;
        if (LoopReadFile(nContractPath, code, "contract") != 0)
        {
            return;
        }

        if(code.empty())
        {
            return;
        }

        std::cout << "code :" << code << std::endl;
        std::string strInput;
        std::cout << "Please enter input data (enter 0 to skip):" << std::endl;
        std::cin >> strInput;

        if (strInput == "0")
        {
            strInput.clear();
        }
        else if(strInput.substr(0, 2) == "0x")
        {
            strInput = strInput.substr(2);
            code += strInput;
        }

        std::string contractTransferStr;
        uint64_t contractTransfer = 0;

        auto result = Evmone::GetLatestContractAddress(strFromAddr);
        if (!result.has_value())
        {
            return;
        }
        std::string contractAddress = result.value();
        ret = TxHelper::CreateEvmDeployContractTransaction(top + 1, outTx, dirtyContract, isNeedAgentFlag,
                                                           info, code, strFromAddr, contractTransfer, contractAddress,
                                                           encodedInfo, isFindUtxo);
        if(ret != 0)
        {
            ERRORLOG("Failed to create DeployContract transaction! The error code is:{}", ret)
            return;
        }        
    }
//    else if(nContractType == 1)
//    {
//        std::string nContractPath;
//        std::cout << "Please enter contract path : " << std::endl;
//        std::cin >> nContractPath;
//
//        std::string extension = ".wasm";
//        if (nContractPath.length() < extension.length() || nContractPath.substr(nContractPath.length() - extension.length()) != extension) {
//            std::cout << "The file does not have a .wasm extension." << std::endl;
//            return;
//        }
//
//        std::string code = ReadFileIntoString(nContractPath);
//        if(code.empty())
//        {
//            return;
//        }
//
//        if(code.size() > input_limit)
//        {
//            std::cout << "Input cannot exceed " << input_limit << " characters" << std::endl;
//            return;
//        }
//        Account launchAccount;
//        if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(strFromAddr, launchAccount) != 0)
//        {
//            ERRORLOG("Failed to find account {}", strFromAddr);
//            return;
//        }
//
//        std::string ownerWasmAddr = launchAccount.GetAddr();
//        ret = TxHelper::CreateWasmDeployContractTransaction(strFromAddr, ownerWasmAddr, code, top + 1, outTx,
//                                                           isNeedAgentFlag,
//                                                           info);
//        if(ret != 0)
//        {
//            ERRORLOG("Failed to create DeployWasmContract transaction! The error code is:{}", ret);
//            return;
//        }
//
//        int sret=SigTx(outTx, strFromAddr);
//        if(sret!=0){
//            errorL("sig fial %s",sret);
//            return ;
//        }
//        std::string txHash = Getsha256hash(outTx.SerializeAsString());
//        outTx.set_hash(txHash);

//    }
    else
    {
        return;
    }

    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::kVersion);
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    // uint64_t localTxUtxoHeight;
    // ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    // if(ret != 0)
    // {
    //     ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
    //     return;
    // }

    // txMsgInfo->set_txutxoheight(localTxUtxoHeight);
    if(nContractType == 0)
    {
        for (const auto& addr : dirtyContract)
        {
            txMsgInfo->add_contractstoragelist(addr);
        }
    }

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);

    }

    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf )
    {
        ret = DropCallShippingTx(msg,outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash())
}

void HandleCallContract()
{

    std::cout << std::endl
              << std::endl;

    std::cout << "AddrList : " << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }
    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    uint32_t nContractType = 0;
//    std::cout << "Please enter contract type: (0: EVM  1: WASM) " << std::endl;
//    std::cin >> nContractType;

    DBReader dataReader;
    std::vector<std::string> vecDeployers;
    if(nContractType == 0)
    {
        dataReader.GetAllEvmDeployerAddr(vecDeployers);
    }
//    else if(nContractType == 1)
//    {
//        dataReader.GetAllWasmDeployerAddr(vecDeployers);
//    }

    std::cout << "=====================deployers======================" << std::endl;
    for(auto& deployer : vecDeployers)
    {
        std::cout << "deployer: " << "0x"+deployer << std::endl;
    }
    std::cout << "=====================deployers======================" << std::endl;
    std::string strToAddr;
    std::cout << "Please enter to addr:" << std::endl;
    std::cin >> strToAddr;
    if (strToAddr.substr(0, 2) == "0x") 
    {
        strToAddr = strToAddr.substr(2);
    }
    if(!isValidAddress(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;        
    }

//    std::string strTxHash;
    std::string contractAddress;
    if(nContractType == 0)
    {
        std::vector<std::string> contractAddresses;
        dataReader.GetContractAddrByDeployerAddr(strToAddr, contractAddresses);
        std::cout << "=====================contract addresses=====================" << std::endl;
        for(auto& contractAddress: contractAddresses)
        {
            std::string code;
            if(dataReader.GetContractCodeByContractAddr(contractAddress, code) != DBStatus::DB_SUCCESS)
            {
                continue;
            }
            std::cout << "contract address: " << "0x"+contractAddress << std::endl;
        }
        std::cout << "=====================contract addresses=====================" << std::endl;

        std::cout << "Please enter contract address:" << std::endl;
        std::cin >> contractAddress;
        if (contractAddress.substr(0, 2) == "0x") 
        {
            contractAddress = contractAddress.substr(2);
        }
    }
//    else if(nContractType == 1)
//    {
//        std::vector<std::string> vecDeployUtxos;
//        dataReader.GetContractAddrByDeployerAddr(strToAddr, vecDeployUtxos);
//        std::cout << "=====================deployed utxos=====================" << std::endl;
//        for(auto& deploy_utxo : vecDeployUtxos)
//        {
//            std::string ContractAddress = evm_utils::GenerateContractAddr(strToAddr + deploy_utxo);
//            std::string deployHash;
//            if(dataReader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
//            {
//                continue;
//            }
//            std::cout << "deployed utxo: " << deploy_utxo << std::endl;
//        }
//        std::cout << "=====================deployed utxos=====================" << std::endl;
//
//
//        std::cout << "Please enter tx hash:" << std::endl;
//        std::cin >> strTxHash;
//    }


    
    std::string strInput, contractFunName;
    if(nContractType == 0)
    {
        std::cout << "Please enter args:" << std::endl;
        std::cin >> strInput;
        if(strInput.substr(0, 2) == "0x")
        {
            strInput = strInput.substr(2);
        }
    }
//    else if(nContractType == 1)
//    {
//        std::cout << "=====================deployed contract function name=====================" << std::endl;
//        std::cout << "Please enter contract function name: " << std::endl;
//        std::cin >> contractFunName;
//
//        int param;
//        std::cout << "Please enter 1 or 0 (1 code has parameters, 0 means no parameters)" << std::endl;
//        std::cin >> param;
//        if(param == 0){
//            strInput = "";
//        }
//        else if(param == 1){
//            std::cout << "Please enter args (Multiple parameters are separated by underscores)" << std::endl;
//            std::cin >> strInput;
//        }
//        else {
//            std::cout << "Parameter input error !" << std::endl;
//            return;
//        }
//    }


    std::string contractTipStr;
    std::cout << "input contract tip amount :" << std::endl;
    std::cin >> contractTipStr;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(contractTipStr, pattern))
    {
        std::cout << "input contract tip error ! " << std::endl;
        return;
    }

    std::string contractTransferStr;
    uint64_t contractTransfer;
    if(nContractType == 0)
    {
        std::cout << "input contract transfer amount :" << std::endl;
        std::cin >> contractTransferStr;
        if (!std::regex_match(contractTransferStr, pattern))
        {
            std::cout << "input contract transfer error ! " << std::endl;
            return;
        }
        contractTransfer = (std::stod(contractTransferStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    }

    std::string input;
    std::cout << "Whether to look for network-wide utxo status (1 means yes, 0 means no)" << std::endl;
    std::cin >> input;

    bool isFindUtxo = false;
    std::regex pattern1("^(0|1)$");
    if(!std::regex_match(input, pattern1))
    {
        std::cout << "Error in parameter input" << std::endl;
        return;
    }
    else
    {
        isFindUtxo = input == "1" ? true : false;
    }

    std::string txDescriptionInfo;
    std::cout << "Enter a description of the transaction, no more than 1 kb" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, txDescriptionInfo);

    std::string encodedInfo = Base64Encode(txDescriptionInfo);
    if(encodedInfo.size() > 1024){
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    uint64_t contractTip = (std::stod(contractTipStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!")
        return ;
    }


    CTransaction outTx;

    int ret = 0;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;
    if (nContractType == 0)
    {
        ret = TxHelper::CreateEvmCallContractTransaction(strFromAddr, strToAddr, strInput,encodedInfo,top + 1,
                                                         outTx, isNeedAgentFlag, info, contractTip, contractTransfer,
                                                         dirtyContract, contractAddress, isFindUtxo);
        if(ret != 0)
        {
            ERRORLOG("Create call contract transaction failed! ret:{}", ret);        
            return;
        }
    }
    else
    {
        return;
    }

    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::kVersion);
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    // uint64_t localTxUtxoHeight;
    // ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    // if(ret != 0)
    // {
    //     ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
    //     return;
    // }

    // txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    std::cout << "size = " << dirtyContract.size() << std::endl;
    for (const auto& addr : dirtyContract)
    {
        std::cout << "addr = " << "0x"+addr << std::endl;
        txMsgInfo->add_contractstoragelist(addr);
    }

//    if(nContractType == 1)
//    {
//        std::string contractAddress = evm_utils::GenerateContractAddr(strToAddr + strTxHash);
//        txMsgInfo->add_contractstoragelist(contractAddress);
//    }

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);
    }

    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        ret = DropCallShippingTx(msg,outTx);
    }
}


// std::string bytesToHexString(uint8_t* data, size_t length) {
//     std::ostringstream oss;
//     oss << std::hex << std::setfill('0');
//     for (size_t i = 0; i < length; ++i) {
//         oss << std::setw(2) << static_cast<int>(data[i]);
//     }
//     return oss.str();
// }

std::string seedToHexString(const uint8_t seed[], size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');  
    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(seed[i]);
    }
    return oss.str();  
}

void HandleExportPrivateKey()
{
    // 1 private key, 2 annotation, 3 QR code
    std::cout << std::endl
              << std::endl;
    
    std::string addr;
    std::cout << "please input the addr you want to export" << std::endl;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") {
             addr = addr.substr(2); 
        }
    
    std::string fileName = addr + ".txt";
    std::ofstream file;
    file.open(fileName);

    Account account;
    if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(addr, account))
    {
        return;
    }

    file << "Please use Courier New font to view" << std::endl
         << std::endl;

    file << "addr: " << "0x" + addr << std::endl;
    std::cout << "addr: " << "0x" + addr << std::endl;


    char outData[1024] = {0};
    int dataLen = sizeof(outData);

    mnemonic_from_data(account.GetSeed(), 16, outData, dataLen);
    file << "Mnemonic: " << outData << std::endl;
    std::cout << "Mnemonic: " << outData << std::endl;
    
    uint8_t* seed=account.GetSeed();
    uint8_t seedGet[PrimeSeedNum];
    for (int i = 0; i < PrimeSeedNum; i++) {
        seedGet[i] = static_cast<uint8_t>(seed[i]);
    }
    std::string strSeedHex = seedToHexString(seedGet, PrimeSeedNum);
    //std::string strPriHex = Str2Hex(account.GetPriStr());
    file << "seed key: " << strSeedHex << std::endl;
    std::cout << "seed key: " << strSeedHex << std::endl;

    file << "QRCode:";
    std::cout << "QRCode:";

    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(5)];
    qrcode_initText(&qrcode, qrcodeData, 5, ECC_MEDIUM, strSeedHex .c_str());

    file << std::endl
         << std::endl;
    std::cout << std::endl
              << std::endl;

    for (uint8_t y = 0; y < qrcode.size; y++)
    {
        file << "        ";
        std::cout << "        ";
        for (uint8_t x = 0; x < qrcode.size; x++)
        {
            file << (qrcode_getModule(&qrcode, x, y) ? "\u2588\u2588" : "  ");
            std::cout << (qrcode_getModule(&qrcode, x, y) ? "\u2588\u2588" : "  ");
        }

        file << std::endl;
        std::cout << std::endl;
    }

    file << std::endl
         << std::endl
         << std::endl
         << std::endl
         << std::endl
         << std::endl;
    std::cout << std::endl
              << std::endl
              << std::endl
              << std::endl
              << std::endl
              << std::endl;


    CaConsole redColor(kConsoleColor_Red, kConsoleColor_Black, true);
    std::cout << redColor.Color() << "You can also view above in file:" << fileName << " of current directory." << redColor.Reset() << std::endl;
    return;
}

int GetChainHeight(unsigned int &chainHeight)
{
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        return -1;
    }
    chainHeight = top;
    return 0;
}

void net_register_callback()
{
    net_callback::RegisterChainHeightCallback(GetChainHeight);
    net_callback::RegisterCalculateChainHeightCallback(MagicSingleton<BlockHelper>::GetInstance()->ObtainChainHeight);
}

/**
 * @description: Registering Callbacks
 * @param {*}
 * @return {*}
 */
void RegisterCallback()
{
    net_register_callback();
}

void TestCreateTx(const std::vector<std::string> &addrs, const int &sleepTime)
{
    if (addrs.size() < 2)
    {
        std::cout << "Insufficient number of accounts" << std::endl;
        return;
    }
#if 0
    bIsCreateTx = true;
    while (1)
    {
        if (bStopTx)
        {
            break;
        }
        int intPart = rand() % 10;
        double decPart = (double)(rand() % 100) / 100;
        double amount = intPart + decPart;
        std::string amountStr = std::to_string(amount);

        std::cout << std::endl << std::endl << std::endl << "=======================================================================" << std::endl;
        CreateTx("1vkS46QffeM4sDMBBjuJBiVkMQKY7Z8Tu", "18RM7FNtzDi41QEU5rAnrFdVaGBHvhTTH1", amountStr.c_str(), NULL, 6, "0.01");
        std::cout << "=====Transaction initiator:1vkS46QffeM4sDMBBjuJBiVkMQKY7Z8Tu" << std::endl;
        std::cout << "=====Transaction recipient:18RM7FNtzDi41QEU5rAnrFdVaGBHvhTTH1" << std::endl;
        std::cout << "=====Transaction amount:" << amountStr << std::endl;
        std::cout << "=======================================================================" << std::endl << std::endl << std::endl << std::endl;

        sleep(sleepTime);
    }
    bIsCreateTx = false;

#endif

#if 1
    bIsCreateTx = true;

    for (int i = 0; i < addrs.size(); i++)
    {
        if (bStopTx)
        {
            std::cout << "Close the deal!" << std::endl;
            break;
        }
        int intPart = rand() % 10;
        double decPart = (double)(rand() % 100) / 100;
        std::string amountStr = std::to_string(intPart  + decPart );


        std::string from, to;
        if (i >= addrs.size() - 1)
        {
            from = addrs[addrs.size() - 1];
            to = addrs[0];
            i = 0;
        }
        else
        {
            from = addrs[i];
            to = addrs[i + 1];
        }
        if (from != "")
        {
            if (!MagicSingleton<AccountManager>::GetInstance()->IsExist(from))
            {
                DEBUGLOG("Illegal account.");
                continue;
            }
        }
        else
        {
            DEBUGLOG("Illegal account. from addr is null !");
            continue;
        }

        std::cout << std::endl
                  << std::endl
                  << std::endl
                  << "=======================================================================" << std::endl;

        std::vector<std::string> fromAddr;
        fromAddr.emplace_back(from);
        std::map<std::string, int64_t> toAddrAmount;
        uint64_t amount = (stod(amountStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
        if(amount == 0)
        {
            std::cout << "aomunt = 0" << std::endl;
            DEBUGLOG("aomunt = 0");
            continue;
        }
        toAddrAmount[to] = amount;



        DBReader dbReader;
        uint64_t top = 0;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
        {
            ERRORLOG("db get top failed!!");
            continue;
        }

        CTransaction outTx;
        TxHelper::vrfAgentType isNeedAgentFlag;
        Vrf info;
        std::string encodedInfo = "";
        int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, encodedInfo, top + 1,  outTx,isNeedAgentFlag,info, false);
        if (ret != 0)
        {
            ERRORLOG("CreateTxTransaction error!!");
            continue;
        }


        TxMsgReq txMsg;
        txMsg.set_version(global::kVersion);
        TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
        txMsgInfo->set_type(0);
        txMsgInfo->set_tx(outTx.SerializeAsString());
        txMsgInfo->set_nodeheight(top);

        uint64_t localTxUtxoHeight;
        ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
        if(ret != 0)
        {
            ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
            return;
        }

        txMsgInfo->set_txutxoheight(localTxUtxoHeight);
        
        if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf){
            Vrf * newInfo=txMsg.mutable_vrfinfo();
            newInfo->CopyFrom(info);

        }


        auto msg = std::make_shared<TxMsgReq>(txMsg);

        std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
        if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr){

            ret=DropshippingTx(msg,outTx);
        }else{
            ret=DoHandleTx(msg,outTx);
         }
        DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());

        std::cout << "=====Transaction initiator:" << from << std::endl;
        std::cout << "=====Transaction recipient:" << to << std::endl;
        std::cout << "=====Transaction amount:" << amountStr << std::endl;
        std::cout << "=======================================================================" << std::endl
                  << std::endl
                  << std::endl
                  << std::endl;

        usleep(sleepTime);
    }
    bIsCreateTx = false;
#endif
}

void ThreadStart()
{
    std::vector<std::string> addrs;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    int sleepTime = 8;
    std::thread th(TestCreateTx, addrs, sleepTime);
    th.detach();
}

int CheckNtpTime()
{
    // Ntp check
    int64_t getNtpTime = MagicSingleton<TimeUtil>::GetInstance()->GetNtpTimestamp();
    int64_t getLocTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

    int64_t tmpTime = abs(getNtpTime - getLocTime);

    std::cout << "UTC Time: " << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(getLocTime) << std::endl;
    std::cout << "Ntp Time: " << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(getNtpTime) << std::endl;

    if (tmpTime <= 1000000)
    {
        DEBUGLOG("ntp timestamp check success");
        return 0;
    }
    else
    {
        DEBUGLOG("ntp timestamp check fail");
        std::cout << "time check fail" << std::endl;
        return -1;
    }
}

void RPC_contrack_uitl(CTransaction & tx){
    tx.clear_hash();
    std::set<std::string> Miset;
	Base64 base;
	auto txUtxo = tx.mutable_utxo();
	int index = 0;
	auto vin = txUtxo->mutable_vin();
	for (auto& owner : txUtxo->owner()) {

		Miset.insert(owner);
		auto vin_t = vin->Mutable(index);
		vin_t->clear_vinsign();
		index++;
	}
	for (auto& owner : Miset) {
		CTxUtxo* txUtxo = tx.mutable_utxo();
		CTxUtxo copyTxUtxo = *txUtxo;
		copyTxUtxo.clear_multisign();
        txUtxo->clear_multisign();
	}
	
}

std::string RpcDeployContract(void * arg,void *ack)
{
    deploy_contract_req * req=(deploy_contract_req *)arg;
    contractAck *ack_t=(contractAck*)ack;

   
    std::string strFromAddr = req->addr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }

    if (!isValidAddress(strFromAddr))
    {
        ack_t->code = -1;
        ack_t->message = "Input addr error!";
        errorL("Input addr error!");
        return "-1";
    }

    DBReader dataReader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ack_t->code = -2;
        ack_t->message = "db get top failed!";
        ERRORLOG("db get top failed!");
        return "-2";
    }

    uint32_t nContractType=std::stoi(req->nContractType);
    nlohmann::json contract_info=nlohmann::json::parse(req->info);

    bool isFindUtxoFlag = req->isFindUtxo;
    
    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;
    std::string encodedInfo = Base64Encode(req->txInfo);
    if(encodedInfo.size() > 1024){
        ack_t->code = -3;
        ack_t->message = "The information entered exceeds the specified length";
        ERRORLOG("The information entered exceeds the specified length!");
        return "-3";
    }
    if(nContractType == 0)
    {
        std::string code=req->contract;
        std::string strInput=req->data;

        if (strInput == "0")
        {
            strInput.clear();
        }
        else if(strInput.substr(0, 2) == "0x")
        {
            strInput = strInput.substr(2);
            code += strInput;
        }

//        Base64 base_;
//        std::string ownerEvmAddr = GenerateAddr(base_.Decode(req->pubstr.c_str(), req->pubstr.size()));
        auto result = Evmone::GetLatestContractAddress(strFromAddr);
        if (!result.has_value())
        {
            ack_t->code = -4;
            ack_t->message = "Get contractaddress failed!";
            ERRORLOG("Get contractaddress failed!");
            return "-4";
        }
        std::string contractAddress = result.value();
        int ret = TxHelper::CreateEvmDeployContractTransaction(top + 1, outTx, dirtyContract, isNeedAgentFlag,
                                                               info, code, strFromAddr, 0, contractAddress, 
                                                               encodedInfo, isFindUtxoFlag, true);
        if(ret != 0)
        {
            ack_t->code = ret - 2000;
            ack_t->message = "Failed to create DeployContract transaction!";
            return "Failed to create DeployContract transaction!";
        }        
    }
    else
    {
        ack_t->code = -5;
        ack_t->message = "nContractType Invalid";
        ERRORLOG("nContractType Invalid");
        return "-5";
    }

    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::kVersion);
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_nodeheight(top);

    for (const auto& addr : dirtyContract)
    {
        txMsgInfo->add_contractstoragelist(addr);
    }

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);

    }


    std::string contracJs;
    std::string txJs;
    google::protobuf::util::MessageToJsonString(ContractMsg,&contracJs);
    google::protobuf::util::MessageToJsonString(outTx,&txJs);
    ack_t->contractJs=contracJs;
    ack_t->txJson=txJs;
    return "0";
}

std::string RpcCallContract(void * arg,void *ack)
{
    call_contract_req * req=(call_contract_req*)arg;
    contractAck *ack_t = (contractAck*)ack;

    bool isToChain=(req->istochain=="true" ? true:false);
    std::string strFromAddr = req->addr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }
    if (!isValidAddress(strFromAddr))
    {
        ack_t->code = -1;
        ack_t->message = "from addr error!";
	    ERRORLOG("Input addr error!");
        return "from addr error!";
    }

    DBReader dataReader;
    std::vector<std::string> vecDeployers;
    std::string strToAddr=req->deployer;
    if(strToAddr.substr(0,2) == "0x")
    {
        strToAddr = strToAddr.substr(2);
    }
    if(!isValidAddress(strToAddr))
    {
        ack_t->code = -2;
        ack_t->message = "to addr error!";
        ERRORLOG("to addr error!");
        return "to addr error!"; 
    }

    
    std::string strTxHash=req->deployutxo;
    if(strTxHash.substr(0, 2) == "0x")
    {
        strTxHash = strTxHash.substr(2);
    }

    std::string strInput=req->args;
    if(strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

    std::string _contractAddress =  req->contractAddress;
    if(_contractAddress.substr(0, 2) == "0x")
    {
        _contractAddress = _contractAddress.substr(2);
    }

    std::string contractTipStr=req->tip;
   
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(contractTipStr, pattern))
    {
        ack_t->code = -3;
        ack_t->message = "input contract tip error !";
        ERRORLOG("input contract tip error ! " );
        return "input contract tip error !";
    }

    std::string contractTransferStr=req->money;
    
    if (!std::regex_match(contractTransferStr, pattern))
    {
        ack_t->code = -4;
        ack_t->message = "money regex match error";
        ERRORLOG("regex match error");
        return "regex match error";
    }
    
    bool isFindUtxoFlag = req->isFindUtxo;

    uint64_t contractTip = (std::stod(contractTipStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t contractTransfer = (std::stod(contractTransferStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ack_t->code = -5;
        ack_t->message = "db get top failed!!";
        ERRORLOG("db get top failed!!");
        return "db get top failed!!";
    }

    std::string encodedInfo = Base64Encode(req->txInfo);
    if(encodedInfo.size() > 1024){
        ack_t->code = -6;
        ack_t->message = "The information entered exceeds the specified length";
        ERRORLOG("The information entered exceeds the specified length");
        return "The information entered exceeds the specified length";
    }

    CTransaction outTx;
    CTransaction tx;
    std::string txRaw;
    if (DBStatus::DB_SUCCESS != dataReader.GetTransactionByHash(strTxHash, txRaw))
    {
        ack_t->code = -7;
        ack_t->message = "get contract transaction failed!!";
        ERRORLOG("get contract transaction failed!!");
        return "get contract transaction failed!!";
    }
    if(!tx.ParseFromString(txRaw))
    {
        ack_t->code = -8;
        ack_t->message = "contract transaction parse failed!!";
        ERRORLOG("contract transaction parse failed!!");
        return "contract transaction parse failed!!";
    }
    
    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
    int vmType = txInfo[Evmone::contractVirtualMachineKeyName].get<int>();
 
    int ret = 0;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;
    if (vmType == global::ca::VmType::EVM)
    {
       
        Base64 base_;
        std::string ownerEvmAddr = GenerateAddr(base_.Decode(req->pubstr.c_str(), req->pubstr.size()));
        if(isToChain)
        {
            ret = MagicSingleton<TxHelper>::GetInstance()->CreateEvmCallContractTransaction(strFromAddr, strToAddr,
                                                                                    strInput, encodedInfo, 
                                                                                    top + 1,
                                                                                    outTx, isNeedAgentFlag,
                                                                                    info, contractTip,
                                                                                    contractTransfer,
                                                                                    dirtyContract,
                                                                                    _contractAddress,
                                                                                    isFindUtxoFlag,true);
            if(ret != 0)
            {
                ack_t->code = ret - 2000;
                ack_t->message = "Create call contract transaction failed!";
                ERRORLOG("Create call contract transaction failed! ret:{}", ret);        
                return "Create call contract transaction failed!";
            }


        }
        else
        {
            ret = MagicSingleton<TxHelper>::GetInstance()->RPC_CreateEvmCallContractTransaction(strFromAddr, strToAddr,
                                                                    strInput,
                                                                    top + 1,
                                                                    outTx, isNeedAgentFlag,
                                                                    info, contractTip,
                                                                    contractTransfer,
                                                                    dirtyContract,
                                                                    _contractAddress,
                                                                    isFindUtxoFlag,true);
            if(ret != 0)
            {
                ack_t->code = ret - 2000;
                ack_t->message = "Create call contract transaction failed!";
                ERRORLOG("Create call contract transaction failed! ret:{}", ret );        
                return "Create call contract transaction failed!";
            }
        }
    }
    else
    {
	    ack_t->code = -8;
	    ack_t->message = "VmType is not EVM";
        return "VmType is not EVM";
    }

    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::kVersion);
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_nodeheight(top);

    std::cout << "size = " << dirtyContract.size() << std::endl;
    for (const auto& addr : dirtyContract)
    {
        std::cout << "addr = " << "0x"+addr << std::endl;
        txMsgInfo->add_contractstoragelist(addr);
    }

  
    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);

    }

    std::string txJsonString;
	std::string contractJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(ContractMsg,&contractJsonString);
	ack_t->contractJs=contractJsonString;
    ack_t->txJson=txJsonString;
    return "0";
    
}



int SigTx(CTransaction &tx,const std::string & addr){
    std::set<std::string> Miset;
	Base64 base_;
	auto txUtxo = tx.mutable_utxo();
	int index = 0;
	auto vin = txUtxo->mutable_vin();
	for (auto& owner : txUtxo->owner()) {
		Miset.insert(owner);
		auto vin_t = vin->Mutable(index);
        if(!vin_t->contractaddr().empty()){
            index++;
            continue;
        }
		std::string serVinHash = Getsha256hash(vin_t->SerializeAsString());
		std::string signature;
		std::string pub;

        if(TxHelper::Sign(addr, serVinHash, signature, pub)){
            return -1;
        }
		CSign* vinSign = vin_t->mutable_vinsign();
		vinSign->set_sign(signature);
		vinSign->set_pub(pub);
		index++;
	}

    for (auto &owner : Miset) {
        CTxUtxo *txUtxo = tx.mutable_utxo();
        CTxUtxo copyTxUtxo = *txUtxo;
        copyTxUtxo.clear_multisign();
        std::string serTxUtxo = Getsha256hash(copyTxUtxo.SerializeAsString());
        std::string signature;
        std::string pub;

        if (TxHelper::Sign(addr, serTxUtxo, signature, pub)) {
            return -2;
        }
        CSign *multiSign = txUtxo->add_multisign();
        multiSign->set_sign(signature);
        multiSign->set_pub(pub);
    }

    return 0;
}

void HandleMultiDeployContract(const std::string &strFromAddr)
{

    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dataReader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;

    std::string nContractPath = "0";
    std::string code;
    int ret = LoopReadFile(nContractPath, code, "contract");
    if (ret != 0)
    {
        return;
    }

    if(code.empty())
    {
        return;
    }

    auto result = Evmone::GetLatestContractAddress(strFromAddr);
    if (!result.has_value())
    {
        return;
    }
    std::string encodedInfo = "";
    std::string contractAddress = result.value();
    ret = TxHelper::CreateEvmDeployContractTransaction(top + 1, outTx, dirtyContract, isNeedAgentFlag,
                                                       info, code, strFromAddr, 0, contractAddress,
                                                       encodedInfo, false);
    if(ret != 0)
    {
        ERRORLOG("Failed to create DeployContract transaction! The error code is:{}", ret);
        return;
    }
    
    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::kVersion);
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    // uint64_t localTxUtxoHeight;
    // ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    // if(ret != 0)
    // {
    //     ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
    //     return;
    // }

    // txMsgInfo->set_txutxoheight(localTxUtxoHeight);
    for (const auto& addr : dirtyContract)
    {
        txMsgInfo->add_contractstoragelist(addr);
    }

    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo = txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);
    }

    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf )
    {
        ret = DropCallShippingTx(msg, outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
    std::cout << "Transaction result : " << ret << std::endl;
}


void CreateAutomaticDeployContract()
{
    std::vector<std::string> accounts;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(accounts);

    uint64_t time;
    std::cout << "please input time seconds >:";
    std::cin >> time;

    for(auto &fromAddr :accounts)
    {
        HandleMultiDeployContract(fromAddr);
        sleep(time);
    } 
}

void printJson()
{
    std::multimap<std::string,std::string> _deployMap;
    std::string fileName = "print_Delopy_addr_utxo.txt";
    std::ofstream filestream;
    filestream.open(fileName);
    if (!filestream)
    {
        std::cout << "Open file failed!" << std::endl;
        return;
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    for(int i = 0 ; i <= top ; ++i)
    {
        std::vector<std::string> hashes;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(i,hashes))
        {
            ERRORLOG("db get top failed!!");
            return;
        }

        for(auto &hash : hashes)
        {
            std::string blockStr;
            if (DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(hash,blockStr))
            {
                ERRORLOG("db get top failed!!");
                return;
            }
            CBlock block;
            block.ParseFromString(blockStr);
            for(auto & ContractTx : block.txs())
            {
                if ((global::ca::TxType)ContractTx.txtype() != global::ca::TxType::kTxTypeDeployContract)
                {
                    continue;
                }

                std::string fromAddr = ContractTx.utxo().owner(0);
                std::string contractAddress;
                try
                {
                    nlohmann::json dataJson = nlohmann::json::parse(ContractTx.data());
                    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
                    contractAddress = txInfo[Evmone::contractRecipientKeyName].get<std::string>();
                }
                catch (const std::exception& e)
                {
                    std::cout << RED << "fail to parse contract address of tx " << ContractTx.hash() << RESET << std::endl;
                    continue;
                }

                _deployMap.insert({fromAddr,contractAddress});
            }
        }
    }

    std::string arg;
    std::cout << "print arg :";
    std::cin >> arg;
    nlohmann::json addr_utxo;
    for(auto & item : _deployMap)
    {
        nlohmann::json addr;
        addr["deployer"] =  item.first;
        addr["contractAddresses"] =  item.second;
        addr["arg"] = arg;
        addr["money"] = "0";
        addr_utxo["addr_utxo"].push_back(addr);
    }

    filestream << addr_utxo.dump();
    filestream.close();
}

std::string remove0xPrefix(std::string str) 
{
    if (str.length() > 2 && str.substr(0, 2) == "0x") {
        return str.substr(2);
    }
    return str;
}

std::string addHexPrefix(std::string hexStr) {
    if(hexStr.empty()) {
        return hexStr;
    }
  return "0x" + hexStr;
}