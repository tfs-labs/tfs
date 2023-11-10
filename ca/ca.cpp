#include "ca.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <random>
#include <map>
#include <array>
#include <fcntl.h>
#include <thread>
#include <shared_mutex>
#include <iomanip>
#include <filesystem>

#include "proto/interface.pb.h"
#include "api/interface/base64.h"
#include "google/protobuf/util/json_util.h"
#include "unistd.h"
#include "db/db_api.h"
#include "sync_block.h"
#include "net/interface.h"
#include "net/api.h"
#include "net/ip_port.h"
#include "utils/json.hpp"
#include "utils/qrcode.h"
#include "utils/string_util.h"
#include "utils/util.h"
#include "utils/time_util.h"
#include "utils/bip39.h"
#include "utils/magic_singleton.h"
#include "utils/hex_code.h"
#include "utils/console.h"
#include "test.h"
#include "transaction.h"
#include "global.h"
#include "interface.h"
#include "txhelper.h"
#include "block_http_callback.h"
#include "transaction_cache.h"
#include "api/http_api.h"
#include "ca/advanced_menu.h"
#include "block_cache.h"
#include "ca_protomsg.pb.h"
#include "block_helper.h"
#include "utils/account_manager.h"
#include "evmone.h"
#include "api/interface/tx.h"
#include "utils/tmp_log.h"
#include "double_spend_cache.h"
#include "../net/epoll_mode.h"
#include "ca/failed_transaction_cache.h"
#include "db/cache.h"
#include "api/interface/evm.h"

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
        CaRegisterHttpCallbacks();
    }

    // Start timer task
    CaStartTimerTask();

    // NTP verification
    CheckNtpTime();

    MagicSingleton<TransactionCache>::GetInstance()->Process();

    std::filesystem::create_directory("./contract");
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
    CaEndTimerTask();
    
//    MagicSingleton<DBCache>::GetInstance()->StopTimer();
    MagicSingleton<FailedTransactionCache>::GetInstance()->StopTimer();
    MagicSingleton<BlockHelper>::GetInstance()->StopSaveBlock();
    MagicSingleton<TFSbenchmark>::GetInstance()->Clear();
    MagicSingleton<TransactionCache>::GetInstance()->Stop();
    MagicSingleton<SyncBlock>::GetInstance()->ThreadStop();
    MagicSingleton<BlockStroage>::GetInstance()->StopTimer();
    MagicSingleton<DoubleSpendCache>::GetInstance()->StopTimer();
    MagicSingleton<EpollMode>::GetInstance()->EpollStop();
    MagicSingleton<CBlockHttpCallback>::GetInstance()->Stop();
    MagicSingleton<PeerNode>::GetInstance()->StopNodesSwap();
    MagicSingleton<CheckBlocks>::GetInstance()->StopTimer();

    sleep(5);
    DBDestory();
}

void PrintBasicInfo()
{
    std::string version = global::kVersion;
    std::string base58 = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    uint64_t balance = 0;
    GetBalanceByUtxo(base58, balance);
    DBReader dbReader;

    uint64_t blockHeight = 0;
    dbReader.GetBlockTop(blockHeight);

    std::string ownID = NetGetSelfNodeId();

    CaConsole infoColor(kConsoleColor_Green, kConsoleColor_Black, true);
    double b = balance / double(100000000);
    
    cout << "*********************************************************************************" << endl;
    cout << "Version: " << version << endl;
    cout << "Base58: " << base58 << endl;
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

    std::string strToAddr;
    std::cout << "input toAddress :" << std::endl;
    std::cin >> strToAddr;

    std::string strAmt;
    std::cout << "input amount :" << std::endl;
    std::cin >> strAmt;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strAmt, pattern))
    {
        std::cout << "input amount error ! " << std::endl;
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
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, top + 1,  outTx,isNeedAgentFlag,info);
    if (ret != 0)
    {
        ERRORLOG("CreateTxTransaction error!! ret:{}", ret);
        return;
    }
    
    { //  Test code, need to be adjusted separately
        if (fromAddr.size() == 1 && CheckBase58Addr(fromAddr[0], Base58Ver::kBase58Ver_MultiSign))
        {

            {
                if (TxHelper::AddMutilSign("1BKJq6f73jYZBnRSH3rZ7bP7Ro2oYkY7me", outTx) != 0)
                {
                    return;
                }
                outTx.clear_hash();
                outTx.set_hash(Getsha256hash(outTx.SerializeAsString()));
            }

            {
                if (TxHelper::AddMutilSign("1QD3H7vyNAGKW3VPEFCvz1BkkqbjLFNaQx", outTx) != 0)
                {
                    return;
                }
                outTx.clear_hash();
                outTx.set_hash(Getsha256hash(outTx.SerializeAsString()));
            }

            std::shared_ptr<MultiSignTxReq> req = std::make_shared<MultiSignTxReq>();
            req->set_version(global::kVersion);
            req->set_txraw(outTx.SerializeAsString());

            MsgData msgdata;
            int ret = HandleMultiSignTxReq(req, msgdata);

            return;
        }
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo -> CopyFrom(info);
    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
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
    std::string strFromAddr = account.GetBase58();
    std::cout << "stake addr: " << strFromAddr << std::endl;
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
    
    double commission = std::stold(strRewardRank) / 100;
    if(commission < global::ca::KMinBonusPumping || commission > global::ca::KMaxBonusPumping)
    {
        std::cout << "input the bonus pumping percentage error" << std::endl;
        return;
    }
    std::cout << commission << std::endl;
   

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
    if (TxHelper::CreateStakeTransaction(strFromAddr, stakeAmount, top + 1,  pledgeType, outTx, outVin,isNeedAgentFlag,info, commission) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    int ret=0;
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void HandleUnstake()
{
    std::cout << std::endl
              << std::endl;

    std::string strFromAddr;
    std::cout << "Please enter unstake addr:" << std::endl;
    std::cin >> strFromAddr;

    DBReader dbReader;
    std::vector<string> utxos;
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
    if (TxHelper::CreatUnstakeTransaction(strFromAddr, strUtxoHash, top + 1, outTx, outVin,isNeedAgentFlag,info) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    int ret=0;
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void HandleInvest()
{
    std::cout << std::endl
              << std::endl;
    std::cout << "AddrList:" << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (!CheckBase58Addr(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strToAddr;
    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cin >> strToAddr;
    if (!CheckBase58Addr(strToAddr))
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
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, investAmount, top + 1,  investType, outTx, outVin,isNeedAgentFlag,info);
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
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void HandleDisinvest()
{
    std::cout << std::endl
              << std::endl;

    std::cout << "AddrList : " << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::string strFromAddr;
    std::cout << "Please enter your addr:" << std::endl;
    std::cin >> strFromAddr;
    if (!CheckBase58Addr(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strToAddr;
    std::cout << "Please enter the addr you want to withdraw from:" << std::endl;
    std::cin >> strToAddr;
    if (!CheckBase58Addr(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dbReader;
    std::vector<string> utxos;
    dbReader.GetBonusAddrInvestUtxosByBonusAddr(strToAddr, strFromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::cout << "======================================= Current delegate amount: =======================================" << std::endl;
    for (auto &utxo : utxos)
    {
        std::cout << "Utxo: " << utxo << std::endl;
    }
    std::cout << "======================================================================================================" << std::endl;

    std::string strUtxoHash;
    std::cout << "Please enter the utxo you want to withdraw:";
    std::cin >> strUtxoHash;

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
    int ret = TxHelper::CreateDisinvestTransaction(strFromAddr, strToAddr, strUtxoHash, top + 1, outTx, outVin,isNeedAgentFlag,info);
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
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void HandleBonus()
{
    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    std::string strFromAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    int ret = TxHelper::CreateBonusTransaction(strFromAddr, top + 1,  outTx, outVin,isNeedAgentFlag,info);
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
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }
    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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
        std::cout << "4. Import PrivateKey" << std::endl;
        std::cout << "5. Export PrivateKey" << std::endl;

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
            std::string priKey;
            std::cout << "Please input private key :" << std::endl;
            std::cin >> priKey;

            if (MagicSingleton<AccountManager>::GetInstance()->ImportPrivateKeyHex(priKey) != 0)
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
    
    if(!CheckBase58Addr(addr, Base58Ver::kBase58Ver_Normal))
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

    if (!CheckBase58Addr(oldAccount.GetBase58(), Base58Ver::kBase58Ver_Normal) ||
        !CheckBase58Addr(newAccount.GetBase58(), Base58Ver::kBase58Ver_Normal))
    {
        return;
    }

    // update base 58 addr
    NodeBase58AddrChangedReq req;
    req.set_version(global::kVersion);

    NodeSign *oldSign = req.mutable_oldsign();
    oldSign->set_pub(oldAccount.GetPubStr());
    std::string oldSignature;
    if (!oldAccount.Sign(Getsha256hash(newAccount.GetBase58()), oldSignature))
    {
        return;
    }
    oldSign->set_sign(oldSignature);

    NodeSign *newSign = req.mutable_newsign();
    newSign->set_pub(newAccount.GetPubStr());
    std::string newSignature;
    if (!newAccount.Sign(Getsha256hash(oldAccount.GetBase58()), newSignature))
    {
        return;
    }
    newSign->set_sign(newSignature);

    MagicSingleton<PeerNode>::GetInstance()->SetSelfId(newAccount.GetBase58());
    MagicSingleton<PeerNode>::GetInstance()->SetSelfIdentity(newAccount.GetPubStr());
    std::vector<Node> publicNodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    for (auto &node : publicNodes)
    {
        net_com::SendMessage(node, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
    }
    std::cout << "Set Default account success" << std::endl;
}

static string ReadFileIntoString(string filename)
{
	ifstream ifile(filename);
	ostringstream buf;
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

static int GenerateContractInfo(nlohmann::json& contract_info)
{
    int ret = 0;

    std::string nContractName;
    std::cout << "Please enter contract name: " << std::endl;
    std::cin >> nContractName;

    if(nContractName.size() > input_limit)
    {
        std::cout << "Input cannot exceed " << input_limit << " characters" << std::endl;
        return -1;
    }

    uint32_t nContractLanguage;
    std::cout << "Please enter contract language: (0: Solidity) " << std::endl;
    std::cin >> nContractLanguage;

    if (nContractLanguage != 0)
    {
        std::cout << "error contract language choice" << std::endl;
        ret = -2;
        return ret;
    }


    std::string nContractLanguageVersion;
    std::cout << "Please enter contract language version: (0 to skip) "
                 "e.g. 0.5.0" << std::endl;
    std::cin >> nContractLanguageVersion;

    ret = LoopRead(std::regex(R"(^(\d+\.){1,2}(\d+)(-[a-zA-Z0-9]+(\.\d+)?)?(\+[a-zA-Z0-9]+(\.\d+)?)?$)"),
                        "contract language version", nContractLanguageVersion);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractStandard;
    std::cout << "Please enter contract Standard:  (0 to skip)\n" << std::endl;
    std::cout << R"(Standard should start with "ERC-", e.g. ERC-20 )" << std::endl;
    std::cin >> nContractStandard;

    ret = LoopRead(std::regex(R"(^ERC-\d+$)"),
                    "contract Standard", nContractStandard);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractLogo;
    std::cout << "Please enter contract logo url: (0 to skip) " << std::endl;
    std::cin >> nContractLogo;

    ret = LoopRead(std::regex(R"(\b((?:[a-zA-Z0-9]+://)?[^\s]+\.[a-zA-Z]{2,}(?::\d{2,})?(?:/[^\s]*)?))"),
                    "url", nContractLogo);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractSource;
    std::cout << "Please enter contract source code path: (enter 0 use default path ./contract/source.sol) " << std::endl;
    std::cin >> nContractSource;

    std::string source_code;
    ret = LoopReadFile(nContractSource, source_code, "source.sol");
    if (ret < 0)
    {
        return -3;
    }

    std::string nContractABI;
    std::cout << "Please enter contract ABI, only accept string in json format: (enter 0 use default path ./contract/abi.json) " << std::endl;
    std::cin >> nContractABI;

    nlohmann::json ABI;
    ret = LoopReadJson(nContractABI, ABI, "abi.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractUserDoc;
    std::cout << "Please enter contract user document, only accept string in json format: (enter 0 use default path ./contract/userdoc.json) " << std::endl;
    std::cin >> nContractUserDoc;

    nlohmann::json userDoc;
    ret = LoopReadJson(nContractUserDoc, userDoc, "userdoc.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractDevDoc;
    std::cout << "Please enter contract developer document, only accept string in json format: (enter 0 use default path ./contract/devdoc.json)  " << std::endl;
    std::cin >> nContractDevDoc;

    nlohmann::json devDoc;
    ret = LoopReadJson(nContractDevDoc, devDoc, "devdoc.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractCompilerVersion;
    std::cout << "Please enter compiler Version: (0 to skip) " << std::endl;
    std::cin >> nContractCompilerVersion;

    ret = LoopRead(std::regex(R"(^\d+\.\d+\.\d+.*$)"),
                    "compiler version", nContractCompilerVersion);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractCompilerOptions;
    std::cout << "Please enter compiler options, only accept string in json format: (enter 0 use default path ./contract/compiler_options.json) " << std::endl;
    std::cin >> nContractCompilerOptions;

    nlohmann::json compilerOptions;
    ret = LoopReadJson(nContractCompilerOptions, compilerOptions, "compiler_options.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractSrcMap;
    std::cout << "Please enter contract SrcMap path: (enter 0 use default path ./contract/srcmap.txt) " << std::endl;
    std::cin >> nContractSrcMap;

    std::string srcMap;
    ret = LoopReadFile(nContractSrcMap, srcMap, "srcmap.txt");
    if (ret < 0)
    {
        return ret;
    }

    std::string nContractSrcMapRuntime;
    std::cout << "Please enter contract SrcMapRuntime path : (enter 0 use default path ./contract/srcmap_runtime.txt) " << std::endl;
    std::cin >> nContractSrcMapRuntime;

    std::string srcMapRuntime;
    ret = LoopReadFile(nContractSrcMapRuntime, srcMapRuntime, "srcmap_runtime.txt");
    if (ret < 0)
    {
        return ret;
    }

    std::string nContractMetadata;
    std::cout << "Please enter contract metadata, only accept string in json format: (enter 0 use default path ./contract/metadata.json) " << std::endl;
    std::cin >> nContractMetadata;

    nlohmann::json metadata;
    ret = LoopReadJson(nContractMetadata, metadata, "metadata.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractOther;
    std::cout << "Please enter other info you wanna preservation, only accept string in json format: (enter 0 use default path ./contract/otherdata.json) " << std::endl;
    std::cin >> nContractOther;

    nlohmann::json otherData;
    ret = LoopReadJson(nContractOther, otherData, "otherdata.json");
    if (ret != 0)
    {
        return ret;
    }

    contract_info["name"] = nContractName;
    contract_info["language"] = "Solidity";
    contract_info["languageVersion"] = nContractLanguageVersion;
    contract_info["standard"] = nContractStandard;
    contract_info["logo"] = nContractLogo;
    contract_info["source"] = source_code;
    contract_info["ABI"] = ABI;
    contract_info["userDoc"] = userDoc;
    contract_info["developerDoc"] = devDoc;
    contract_info["compilerVersion"] = nContractCompilerVersion;
    contract_info["compilerOptions"] = compilerOptions;
    contract_info["srcMap"] = srcMap;
    contract_info["srcMapRuntime"] = srcMapRuntime;
    contract_info["metadata"] = metadata;
    contract_info["other"] = otherData;

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
    if (!CheckBase58Addr(strFromAddr))
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

    uint32_t nContractType;
    std::cout << "Please enter contract type: (0: EVM) " << std::endl;
    std::cin >> nContractType;

    nlohmann::json contract_info;
    int ret = GenerateContractInfo(contract_info);
    if (ret != 0)
    {
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if(nContractType == 0)
    {

        std::string nContractPath;
        std::cout << "Please enter contract path : (enter 0 use default path ./contract/contract) " << std::endl;
        std::cin >> nContractPath;

        std::string code;
        ret = LoopReadFile(nContractPath, code, "contract");
        if (ret != 0)
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
        Account launchAccount;
        if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(strFromAddr, launchAccount) != 0)
        {
            ERRORLOG("Failed to find account {}", strFromAddr);
            return;
        }
        std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(launchAccount.GetPubStr());
        ret = TxHelper::CreateEvmDeployContractTransaction(strFromAddr, ownerEvmAddr, code, top + 1, contract_info,
                                                           outTx,
                                                           isNeedAgentFlag,
                                                           info);
        if(ret != 0)
        {
            ERRORLOG("Failed to create DeployContract transaction! The error code is:{}", ret);
            return;
        }        
    }
    else
    {
        return;
    }

	TxMsgReq txMsg;
	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

	if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
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
    if (!CheckBase58Addr(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dataReader;
    std::vector<std::string> vecDeployers;
    dataReader.GetAllDeployerAddr(vecDeployers);
    std::cout << "=====================deployers=====================" << std::endl;
    for(auto& deployer : vecDeployers)
    {
        std::cout << "deployer: " << deployer << std::endl;
    }
    std::cout << "=====================deployers=====================" << std::endl;
    std::string strToAddr;
    std::cout << "Please enter to addr:" << std::endl;
    std::cin >> strToAddr;
    if(!CheckBase58Addr(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;        
    }

    std::vector<std::string> vecDeployUtxos;
    dataReader.GetDeployUtxoByDeployerAddr(strToAddr, vecDeployUtxos);
    std::cout << "=====================deployed utxos=====================" << std::endl;
    for(auto& deploy_utxo : vecDeployUtxos)
    {
        std::string ContractAddress = evm_utils::GenerateContractAddr(strToAddr + deploy_utxo);
        std::string deployHash;
        if(dataReader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
        {
            continue;
        }
        std::cout << "deployed utxo: " << deploy_utxo << std::endl;
    }
    std::cout << "=====================deployed utxos=====================" << std::endl;
    std::string strTxHash;
    std::cout << "Please enter tx hash:" << std::endl;
    std::cin >> strTxHash;
    
    std::string strInput;
    std::cout << "Please enter args:" << std::endl;
    std::cin >> strInput;
    if(strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

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
    std::cout << "input contract transfer amount :" << std::endl;
    std::cin >> contractTransferStr;
    if (!std::regex_match(contractTransferStr, pattern))
    {
        std::cout << "input contract transfer error ! " << std::endl;
        return;
    }
    uint64_t contractTip = (std::stod(contractTipStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t contractTransfer = (std::stod(contractTransferStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }


    CTransaction outTx;
    CTransaction tx;
    std::string txRaw;
    if (DBStatus::DB_SUCCESS != dataReader.GetTransactionByHash(strTxHash, txRaw))
    {
        ERRORLOG("get contract transaction failed!!");
        return ;
    }
    if(!tx.ParseFromString(txRaw))
    {
        ERRORLOG("contract transaction parse failed!!");
        return ;
    }
    

    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
    int vmType = txInfo["VmType"].get<int>();
 
    int ret = 0;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if (vmType == global::ca::VmType::EVM)
    {
        Account launchAccount;
        if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(strFromAddr, launchAccount) != 0)
        {
            ERRORLOG("Failed to find account {}", strFromAddr);
            return;
        }
        std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(launchAccount.GetPubStr());
        ret = TxHelper::CreateEvmCallContractTransaction(strFromAddr, strToAddr, strTxHash, strInput,
                                                         ownerEvmAddr, top + 1,
                                                         outTx, isNeedAgentFlag, info, contractTip, contractTransfer);
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

    TxMsgReq txMsg;
	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo -> CopyFrom(info);

    }

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void HandleExportPrivateKey()
{
    std::cout << std::endl
              << std::endl;
    // 1 private key, 2 annotation, 3 QR code
    std::string fileName("account_private_key.txt");
    ofstream file;
    file.open(fileName);
    std::string addr;
    std::cout << "please input the addr you want to export" << std::endl;
    std::cin >> addr;

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->FindAccount(addr, account);

    file << "Please use Courier New font to view" << std::endl
         << std::endl;

    file << "Base58 addr: " << addr << std::endl;
    std::cout << "Base58 addr: " << addr << std::endl;

    char outData[1024] = {0};
    int dataLen = sizeof(outData);
    mnemonic_from_data((const uint8_t *)account.GetPriStr().c_str(), account.GetPriStr().size(), outData, dataLen);
    file << "Mnemonic: " << outData << std::endl;
    std::cout << "Mnemonic: " << outData << std::endl;

    std::string strPriHex = Str2Hex(account.GetPriStr());
    file << "Private key: " << strPriHex << std::endl;
    std::cout << "Private key: " << strPriHex << std::endl;

    file << "QRCode:";
    std::cout << "QRCode:";

    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(5)];
    qrcode_initText(&qrcode, qrcodeData, 5, ECC_MEDIUM, strPriHex.c_str());

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

void net_register_chain_height_callback()
{
    net_callback::RegisterChainHeightCallback(GetChainHeight);
}

/**
 * @description: Registering Callbacks
 * @param {*}
 * @return {*}
 */
void RegisterCallback()
{
    SyncBlockRegisterCallback<FastSyncGetHashReq>(HandleFastSyncGetHashReq);
    SyncBlockRegisterCallback<FastSyncGetHashAck>(HandleFastSyncGetHashAck);
    SyncBlockRegisterCallback<FastSyncGetBlockReq>(HandleFastSyncGetBlockReq);
    SyncBlockRegisterCallback<FastSyncGetBlockAck>(HandleFastSyncGetBlockAck);

    SyncBlockRegisterCallback<SyncGetSumHashReq>(HandleSyncGetSumHashReq);
    SyncBlockRegisterCallback<SyncGetSumHashAck>(HandleSyncGetSumHashAck);
    SyncBlockRegisterCallback<SyncGetHeightHashReq>(HandleSyncGetHeightHashReq);
    SyncBlockRegisterCallback<SyncGetHeightHashAck>(HandleSyncGetHeightHashAck);
    SyncBlockRegisterCallback<SyncGetBlockReq>(HandleSyncGetBlockReq);
    SyncBlockRegisterCallback<SyncGetBlockAck>(HandleSyncGetBlockAck);

    SyncBlockRegisterCallback<SyncFromZeroGetSumHashReq>(HandleFromZeroSyncGetSumHashReq);
    SyncBlockRegisterCallback<SyncFromZeroGetSumHashAck>(HandleFromZeroSyncGetSumHashAck);
    SyncBlockRegisterCallback<SyncFromZeroGetBlockReq>(HandleFromZeroSyncGetBlockReq);
    SyncBlockRegisterCallback<SyncFromZeroGetBlockAck>(HandleFromZeroSyncGetBlockAck);

    SyncBlockRegisterCallback<SyncNodeHashReq>(HandleSyncNodeHashReq);
    SyncBlockRegisterCallback<SyncNodeHashAck>(HandleSyncNodeHashAck);

    SyncBlockRegisterCallback<GetBlockByUtxoReq>(HandleBlockByUtxoReq);
    SyncBlockRegisterCallback<GetBlockByUtxoAck>(HandleBlockByUtxoAck);

    SyncBlockRegisterCallback<GetBlockByHashReq>(HandleBlockByHashReq);
    SyncBlockRegisterCallback<GetBlockByHashAck>(HandleBlockByHashAck);

    SyncBlockRegisterCallback<SeekPreHashByHightReq>(HandleSeekGetPreHashReq);
    SyncBlockRegisterCallback<SeekPreHashByHightAck>(HandleSeekGetPreHashAck);

    SyncBlockRegisterCallback<GetCheckSumHashReq>(HandleGetCheckSumHashReq);
    SyncBlockRegisterCallback<GetCheckSumHashAck>(HandleGetCheckSumHashAck);

    // PCEnd correlation
    TxRegisterCallback<TxMsgReq>(HandleTx); // PCEnd transaction flow

    
    SaveBlockRegisterCallback<BuildBlockBroadcastMsg>(HandleBuildBlockBroadcastMsg); // Building block broadcasting

    BlockRegisterCallback<BlockMsg>(HandleBlock);      // PCEnd transaction flow
    BlockRegisterCallback<BuildBlockBroadcastMsgAck>(HandleAddBlockAck);
    

    CaRegisterCallback<MultiSignTxReq>(HandleMultiSignTxReq);
    CaRegisterCallback<BlockStatus>(HandleBlockStatusMsg); //retransmit

    net_register_chain_height_callback();
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
            DEBUGLOG("Illegal account. from base58addr is null !");
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
        int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, top + 1,  outTx,isNeedAgentFlag,info);
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
        txMsgInfo->set_height(top);
        
        if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf){
            Vrf * newInfo=txMsg.mutable_vrfinfo();
            newInfo->CopyFrom(info);

        }


        auto msg = make_shared<TxMsgReq>(txMsg);

        std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
        if(isNeedAgentFlag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr){

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

// std::string handle__deploy_contract_rpc(void * arg,void *ack){

//     deploy_contract_req * req=(deploy_contract_req *)arg;

//     int ret;
//     std::string strFromAddr=req->addr;
   
//     if (!CheckBase58Addr(strFromAddr))
//     {
//        return "base58 error!";
//     }

//     DBReader dataReader;
//     uint64_t top = 0;
// 	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
//     {
//        return "db get top failed!!";
//     }

//     uint32_t nContractType=std::stoi(req->nContractType);

//     contract_info infoT;
//     if(infoT.paseFromJson(req->info)==false){
//         return "contract_info pase fail";
//     }

//     nlohmann::json contract_info_(infoT.paseToString());
   

//     CTransaction outTx;
//     TxHelper::vrfAgentType isNeedAgentFlag;
//     Vrf info;
//     if(nContractType == 0)
//     {

//         std::string code=req->contract;
//         std::string strInput=req->data;

//         if (strInput == "0")
//         {
//             strInput.clear();
//         }
//         else if(strInput.substr(0, 2) == "0x")
//         {
//             strInput = strInput.substr(2);
//             code += strInput;
//         }
//         Base64 base_;
//         std::string pubstr=base_.Decode(req->pubstr.c_str(),req->pubstr.size());
//         std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(pubstr);
      
//         // ret = interface_evm::CreateEvmDeployContractTransaction(strFromAddr, ownerEvmAddr, code, top + 1, contract_info_,
//         //                                                    outTx,
//         //                                                    isNeedAgentFlag,
//         //                                                    info,ack);
//         if(ret != 0)
//         {
//            return "Failed to create DeployContract transaction! The error code is:" + std::to_string(ret);
//         }        
//     }
//     else
//     {
//         return "unknow error";
//     }
//     return std::to_string(ret);
// }


// std::string handle__call_contract_rpc(void * arg,void *ack){

//     call_contract_req * retT=(call_contract_req*)arg;

//     std::string strFromAddr=retT->addr;
    
//     if (!CheckBase58Addr(strFromAddr))
//     {
//         return DSTR"Input addr error!" ;
//     }

//     DBReader dataReader;
//     std::string strToAddr=retT->deployer;
    
//     if(!CheckBase58Addr(strToAddr))
//     {
//         return DSTR "Input addr error!";
             
//     }

//     std::string strTxHash=retT->deployutxo;
//     std::string strInput=retT->args;
    
//     if(strInput.substr(0, 2) == "0x")
//     {
//         strInput = strInput.substr(2);
//     }

//     uint64_t top = 0;
// 	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
//     {
//         return DSTR"db get top failed!!";
        
//     }

//     CTransaction outTx;
//     CTransaction tx;
//     std::string txRaw;
//     if (DBStatus::DB_SUCCESS != dataReader.GetTransactionByHash(strTxHash, txRaw))
//     {
//         return DSTR"get contract transaction failed!!";
//     }
//     if(!tx.ParseFromString(txRaw))
//     {
//         return DSTR"contract transaction parse failed!!";
//     }
    
//     nlohmann::json dataJson = nlohmann::json::parse(tx.data());
//     nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
//     int vmType = txInfo["VmType"].get<int>();
 
//     int ret = 0;
//      std::pair<int,std::string> ret_evm;
//     TxHelper::vrfAgentType isNeedAgentFlag;
//     Vrf info;
//     if (vmType == global::ca::VmType::EVM)
//     {
//         Base64 base_;
//         std::string pubstr_=base_.Decode(retT->pubstr.c_str(),retT->pubstr.size());
//         std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(pubstr_);
//         ret_evm = interface_evm::ReplaceCreateEvmCallContractTransaction(strFromAddr, strToAddr, strTxHash, strInput,
//                                                          ownerEvmAddr, top + 1,
//                                                          outTx, isNeedAgentFlag, info,ack);
//         if(ret_evm.first != 0)
//         {
//            return DSTR"Create call contract transaction failed! ret:"+ret_evm.second;        
            
//         }
//     }
//     else
//     {
//         return DSTR"unkown";
//     }
//     return std::to_string(ret);
// }



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
    int ret=0;
    std::string strFromAddr=req->addr;
    if (!CheckBase58Addr(strFromAddr))
    {
        return DSTR"base58 error!";
    }

    DBReader dataReader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
        return DSTR"db get top failed!!";
    }

    uint32_t nContractType=std::stoi(req->nContractType);
    

     contract_info infoT;
   

    nlohmann::json infoTObj=infoT.paseToJsonObj(req->info);
    

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if(nContractType == 0)
    {

       
       
        std::string code=req->contract;
        
        if(code.empty())
        {
            return DSTR"code is empty!";
        }
        // std::cout << "code :" << code << std::endl;
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
        //Account launchAccount;
        Base64 base_;
        std::string pubstr=base_.Decode(req->pubstr.c_str(),req->pubstr.size());
        std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(pubstr);

        ret = rpc_evm::RpcCreateEvmDeployContractTransaction(strFromAddr, ownerEvmAddr, code, top + 1, infoTObj,
                                                           outTx,
                                                           isNeedAgentFlag,
                                                           info);
        if(ret != 0)
        {
            return DSTR Sutil::Format("Failed to create DeployContract transaction! The error code is:%s", ret);
            
        }        
    }
    else
    {
        return DSTR "unknow error";
    }
    tx_ack *ack_t=(tx_ack*)ack;
    RPC_contrack_uitl(outTx);
    std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);
	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrfJsonString;
	ack_t->ErrorCode="0";
	ack_t->height=std::to_string(top);
	ack_t->txType=std::to_string((int)isNeedAgentFlag);

    return "0";
}





std::string RpcCallContract(void * arg,void *ack)
{
    call_contract_req * retT=(call_contract_req*)arg;

    bool isToChain=(retT->istochain=="true" ? true:false);
    std::string strFromAddr=retT->addr;

  
    if (!CheckBase58Addr(strFromAddr))
    {
        return DSTR "base58 addr error!";
    }


    std::string strTxHash=retT->deployutxo;
    std::string strInput=retT->args;
    std::string strToAddr=retT->deployer;

    DBReader dataReader;
    
    if(!CheckBase58Addr(strToAddr))
    {
        return DSTR "base58 addr error!";
    }

   
    if(strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

    std::string contractTipStr=retT->tip;
    
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(contractTipStr, pattern))
    {
        return DSTR "input contract tip error ! ";
        
    }

    std::string contractTransferStr=retT->money;
    // std::cout << "input contract transfer amount :" << std::endl;
    // std::cin >> contractTransferStr;
    if (!std::regex_match(contractTransferStr, pattern))
    {
       return DSTR"input contract transfer error ! ";
      
    }
    uint64_t contractTip = (std::stod(contractTipStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t contractTransfer = (std::stod(contractTransferStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top))
    {
       return DSTR"db get top failed!!";
       
    }


    CTransaction outTx;
    CTransaction tx;
    std::string txRaw;
    if (DBStatus::DB_SUCCESS != dataReader.GetTransactionByHash(strTxHash, txRaw))
    {
        return DSTR"get contract transaction failed!!";
    }
    if(!tx.ParseFromString(txRaw))
    {
        return DSTR"contract transaction parse failed!!";
      
    }
    

    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
    int vmType = txInfo["VmType"].get<int>();
 
    int ret = 0;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    if (vmType == global::ca::VmType::EVM)
    {
        Base64 base;
        std::string pubstr=base.Decode(retT->pubstr.c_str(),retT->pubstr.size());
        std::string ownerEvmAddr = evm_utils::GenerateEvmAddr(pubstr);
        ret = rpc_evm::RpcCreateEvmCallContractTransaction(strFromAddr, strToAddr, strTxHash, strInput,
                                                         ownerEvmAddr, top + 1,
                                                         outTx, isNeedAgentFlag, info, contractTip, contractTransfer,isToChain);
        if(ret != 0)
        {
            return DSTR Sutil::Format("Create call contract transaction failed! ret: %s", ret);          
        }
    }
    else
    {
        return DSTR"not EVM";
    }
    
    //errorL("VVVVVVVRRRRFDATA:%s",info.data());

    tx_ack *ack_t=(tx_ack*)ack;
    RPC_contrack_uitl(outTx);
    std::string txJsonString;
	std::string vrfJsonString;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(info,&vrfJsonString);
	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrfJsonString;
    //errorL("vrfString:%s",ack_t->vrfJson);
	ack_t->ErrorCode="0";
	ack_t->height=std::to_string(top);
	ack_t->txType=std::to_string((int)isNeedAgentFlag);

    return "0";
    
}