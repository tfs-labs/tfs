#include "ca.h"

#include "unistd.h"

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

#include "db/db_api.h"
#include "ca_sync_block.h"
#include "include/net_interface.h"
#include "net/net_api.h"
#include "net/ip_port.h"
#include "utils/qrcode.h"
#include "utils/string_util.h"
#include "utils/util.h"
#include "utils/time_util.h"
#include "utils/bip39.h"
#include "utils/MagicSingleton.h"
#include "utils/hexcode.h"
#include "utils/console.h"

#include "ca_txhelper.h"
#include "ca_test.h"
#include "ca_transaction.h"
#include "ca_global.h"
#include "ca_interface.h"
#include "ca_test.h"
#include "ca_txhelper.h"

#include "ca_block_http_callback.h"
#include "ca_block_http_callback.h"
#include "ca_transaction_cache.h"
#include "api/http_api.h"


#include "ca/ca_AdvancedMenu.h"
#include "ca_blockcache.h"
#include "ca_protomsg.pb.h"
#include "ca_blockhelper.h"
#include "utils/AccountManager.h"
#include "ca_evmone.h"
#include "api/interface/tx.h"
#include "api/interface/evm.h"

bool bStopTx = false;
bool bIsCreateTx = false;

int ca_startTimerTask()
{
    // Blocking thread
    global::ca::kBlockPoolTimer.AsyncLoop(100, [](){ MagicSingleton<BlockHelper>::GetInstance()->Process(); });
    
    //SeekBlock Thread
    global::ca::kSeekBlockTimer.AsyncLoop(3 * 1000, [](){ MagicSingleton<BlockHelper>::GetInstance()->SeekBlockThread(); });
    
    //Start patch thread
    MagicSingleton<BlockHelper>::GetInstance()->SeekBlockThread();

    // Block synchronization thread
    MagicSingleton<SyncBlock>::GetInstance()->ThreadStart();


    MagicSingleton<BlockStroage>::GetInstance();

    return 0;
}

bool ca_init()
{
    RegisterInterface();

    // Register interface with network layer
    RegisterCallback();

    // Register HTTP related interfaces
    if(MagicSingleton<Config>::GetInstance()->GetRpc())
    {
        ca_register_http_callbacks();
    }

    // Start timer task
    ca_startTimerTask();

    // NTP verification
    checkNtpTime();

    MagicSingleton<CtransactionCache>::GetInstance()->process();

    std::filesystem::create_directory("./contract");
    return true;
}


int ca_endTimerTask()
{
    global::ca::kDataBaseTimer.Cancel();
    return 0;
}

void ca_cleanup()
{
    ca_endTimerTask();
    DBDestory();
}

void ca_print_basic_info()
{
    std::string version = global::kVersion;
    std::string base58 = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    uint64_t balance = 0;
    GetBalanceByUtxo(base58, balance);
    DBReader db_reader;

    uint64_t blockHeight = 0;
    db_reader.GetBlockTop(blockHeight);



    std::string ownID = net_get_self_node_id();

    ca_console infoColor(kConsoleColor_Green, kConsoleColor_Black, true);
    double b = balance / double(100000000);
    
    cout << "*********************************************************************************" << endl;
    cout << "Version: " << version << endl;
    cout << "Base58: " << base58 << endl;
    cout << "Balance: " << setiosflags(ios::fixed) << setprecision(8) << b << endl;
    cout << "Block top: " << blockHeight << endl;
    cout << "*********************************************************************************" << endl;
  
}

void handle_transaction()
{
    std::cout << std::endl
              << std::endl;

    std::string strFromAddr;
    std::cout << "input FromAddr :" << std::endl;
    std::cin >> strFromAddr;

    std::string strToAddr;
    std::cout << "input ToAddr :" << std::endl;
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

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgent_flag;

    Vrf info_;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, top + 1,  outTx,isNeedAgent_flag,info_);
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
                outTx.set_hash(getsha256hash(outTx.SerializeAsString()));
            }

            {
                if (TxHelper::AddMutilSign("1QD3H7vyNAGKW3VPEFCvz1BkkqbjLFNaQx", outTx) != 0)
                {
                    return;
                }
                outTx.clear_hash();
                outTx.set_hash(getsha256hash(outTx.SerializeAsString()));
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

    if(isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info -> CopyFrom(info_);
    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_stake()
{
    std::cout << std::endl
              << std::endl;

    Account account;
    EVP_PKEY_free(account.pkey);
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.base58Addr;
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

    TxHelper::PledgeType pledgeType = TxHelper::PledgeType::kPledgeType_Node;

    uint64_t stake_amount = std::stod(strStakeFee) * global::ca::kDecimalNum;

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }


    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    if (TxHelper::CreateStakeTransaction(strFromAddr, stake_amount, top + 1,  pledgeType, outTx, outVin,isNeedAgent_flag,info_) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    int ret=0;
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_unstake()
{
    std::cout << std::endl
              << std::endl;

    std::string strFromAddr;
    std::cout << "Please enter unstake addr:" << std::endl;
    std::cin >> strFromAddr;

    DBReader db_reader;
    std::vector<string> utxos;
    db_reader.GetStakeAddressUtxo(strFromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::cout << "-- Current pledge amount: -- " << std::endl;
    for (auto &utxo : utxos)
    {
        std::string txRaw;
        db_reader.GetTransactionByHash(utxo, txRaw);
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
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    if (TxHelper::CreatUnstakeTransaction(strFromAddr, strUtxoHash, top + 1, outTx, outVin,isNeedAgent_flag,info_) != 0)
    {
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    int ret=0;
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_invest()
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
    uint64_t invest_amount = std::stod(strInvestFee) * global::ca::kDecimalNum;

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, invest_amount, top + 1,  investType, outTx, outVin,isNeedAgent_flag,info_);
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

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_disinvest()
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

    DBReader db_reader;
    std::vector<string> utxos;
    db_reader.GetBonusAddrInvestUtxosByBonusAddr(strToAddr, strFromAddr, utxos);
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
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    int ret = TxHelper::CreateDisinvestTransaction(strFromAddr, strToAddr, strUtxoHash, top + 1, outTx, outVin,isNeedAgent_flag,info_);
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

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_bonus()
{
    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    std::string strFromAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    int ret = TxHelper::CreateBonusTransaction(strFromAddr, top + 1,  outTx, outVin,isNeedAgent_flag,info_);
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

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }
    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
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

void handle_AccountManger()
{
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    std::cout << std::endl
              << std::endl;
    while (true)
    {
        std::cout << "0.Exit" << std::endl;
        std::cout << "1. Set Defalut Account" << std::endl;
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
            handle_SetdefaultAccount();
            break;
        case 2:
            gen_key();
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
            std::string pri_key;
            std::cout << "Please input private key :" << std::endl;
            std::cin >> pri_key;

            if (MagicSingleton<AccountManager>::GetInstance()->ImportPrivateKeyHex(pri_key) != 0)
            {
                std::cout << "Save PrivateKey failed!" << std::endl;
            }
            break;
        }
        case 5:
            handle_export_private_key();
            break;
        default:
            std::cout << "Invalid input." << std::endl;
            continue;
        }
    }
}

void handle_SetdefaultAccount()
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
    EVP_PKEY_free(oldAccount.pkey);
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
    EVP_PKEY_free(newAccount.pkey);
    if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(newAccount) != 0)
    {
        ERRORLOG("not found DefaultKeyBs58Addr  in the _accountList");
        return;
    }

    if (!CheckBase58Addr(oldAccount.base58Addr, Base58Ver::kBase58Ver_Normal) ||
        !CheckBase58Addr(newAccount.base58Addr, Base58Ver::kBase58Ver_Normal))
    {
        return;
    }

    // update base 58 addr
    NodeBase58AddrChangedReq req;
    req.set_version(global::kVersion);

    NodeSign *oldSign = req.mutable_oldsign();
    oldSign->set_pub(oldAccount.pubStr);
    std::string oldSignature;
    if (!oldAccount.Sign(getsha256hash(newAccount.base58Addr), oldSignature))
    {
        return;
    }
    oldSign->set_sign(oldSignature);

    NodeSign *newSign = req.mutable_newsign();
    newSign->set_pub(newAccount.pubStr);
    std::string newSignature;
    if (!newAccount.Sign(getsha256hash(oldAccount.base58Addr), newSignature))
    {
        return;
    }
    newSign->set_sign(newSignature);

    MagicSingleton<PeerNode>::GetInstance()->set_self_id(newAccount.base58Addr);
    MagicSingleton<PeerNode>::GetInstance()->set_self_identity(newAccount.pubStr);
    std::vector<Node> publicNodes = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    for (auto &node : publicNodes)
    {
        net_com::send_message(node, req, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
    }
    std::cout << "Set Default account success" << std::endl;
}
static string readFileIntoString(string filename)
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

static int loop_read(const std::regex&& pattern, std::string_view input_name, std::string& input)
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
    return 0;
}

static int loop_read_file(std::string& input, std::string& output, const std::filesystem::path& filename = "")
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

    output = readFileIntoString(contract_path.string());
    return 0;
}

static int loop_read_json(std::string& input, nlohmann::json& output, const std::filesystem::path& filename = "")
{
    std::string content;
    int ret = loop_read_file(input, content, filename);
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

static int generate_contract_info(nlohmann::json& contract_info)
{
    int ret = 0;

    std::string nContractName;
    std::cout << "Please enter contract name: " << std::endl;
    std::cin >> nContractName;

    uint32_t nContractLanguage;
    std::cout << "Please enter contract language: (0: Solidity) " << std::endl;
    std::cin >> nContractLanguage;

    if (nContractLanguage != 0)
    {
        std::cout << "error contract language choice" << std::endl;
        ret = -1;
        return ret;
    }


    std::string nContractLanguageVersion;
    std::cout << "Please enter contract language version: (0 to skip) "
                 "e.g. 0.5.0" << std::endl;
    std::cin >> nContractLanguageVersion;

    ret = loop_read(std::regex(R"(^(\d+\.){1,2}(\d+)(-[a-zA-Z0-9]+(\.\d+)?)?(\+[a-zA-Z0-9]+(\.\d+)?)?$)"),
                        "contract language version", nContractLanguageVersion);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractStandard;
    std::cout << "Please enter contract Standard:  (0 to skip)\n" << std::endl;
    std::cout << R"(Standard should start with "ERC-", e.g. ERC-20 )" << std::endl;
    std::cin >> nContractStandard;

    ret = loop_read(std::regex(R"(^ERC-\d+$)"),
                    "contract Standard", nContractStandard);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractLogo;
    std::cout << "Please enter contract logo url: (0 to skip) " << std::endl;
    std::cin >> nContractLogo;

    ret = loop_read(std::regex(R"(\b((?:[a-zA-Z0-9]+://)?[^\s]+\.[a-zA-Z]{2,}(?::\d{2,})?(?:/[^\s]*)?))"),
                    "url", nContractLogo);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractSource;
    std::cout << "Please enter contract source code path: (enter 0 use default path ./contract/source.sol) " << std::endl;
    std::cin >> nContractSource;

    std::string source_code;
    ret = loop_read_file(nContractSource, source_code, "source.sol");
    if (ret < 0)
    {
        return -1;
    }

    std::string nContractABI;
    std::cout << "Please enter contract ABI, only accept string in json format: (enter 0 use default path ./contract/abi.json) " << std::endl;
    std::cin >> nContractABI;

    nlohmann::json ABI;
    ret = loop_read_json(nContractABI, ABI, "abi.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractUserDoc;
    std::cout << "Please enter contract user document, only accept string in json format: (enter 0 use default path ./contract/userdoc.json) " << std::endl;
    std::cin >> nContractUserDoc;

    nlohmann::json userDoc;
    ret = loop_read_json(nContractUserDoc, userDoc, "userdoc.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractDevDoc;
    std::cout << "Please enter contract developer document, only accept string in json format: (enter 0 use default path ./contract/devdoc.json)  " << std::endl;
    std::cin >> nContractDevDoc;

    nlohmann::json devDoc;
    ret = loop_read_json(nContractDevDoc, devDoc, "devdoc.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractCompilerVersion;
    std::cout << "Please enter compiler Version: (0 to skip) " << std::endl;
    std::cin >> nContractCompilerVersion;

    ret = loop_read(std::regex(R"(^\d+\.\d+\.\d+.*$)"),
                    "compiler version", nContractCompilerVersion);
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractCompilerOptions;
    std::cout << "Please enter compiler options, only accept string in json format: (enter 0 use default path ./contract/compiler_options.json) " << std::endl;
    std::cin >> nContractCompilerOptions;

    nlohmann::json compilerOptions;
    ret = loop_read_json(nContractCompilerOptions, compilerOptions, "compiler_options.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractSrcMap;
    std::cout << "Please enter contract SrcMap path: (enter 0 use default path ./contract/srcmap.txt) " << std::endl;
    std::cin >> nContractSrcMap;

    std::string srcMap;
    ret = loop_read_file(nContractSrcMap, srcMap, "srcmap.txt");
    if (ret < 0)
    {
        return ret;
    }

    std::string nContractSrcMapRuntime;
    std::cout << "Please enter contract SrcMapRuntime path : (enter 0 use default path ./contract/srcmap_runtime.txt) " << std::endl;
    std::cin >> nContractSrcMapRuntime;

    std::string srcMapRuntime;
    ret = loop_read_file(nContractSrcMapRuntime, srcMapRuntime, "srcmap_runtime.txt");
    if (ret < 0)
    {
        return ret;
    }

    std::string nContractMetadata;
    std::cout << "Please enter contract metadata, only accept string in json format: (enter 0 use default path ./contract/metadata.json) " << std::endl;
    std::cin >> nContractMetadata;

    nlohmann::json metadata;
    ret = loop_read_json(nContractMetadata, metadata, "metadata.json");
    if (ret != 0)
    {
        return ret;
    }

    std::string nContractOther;
    std::cout << "Please enter other info you wanna preservation, only accept string in json format: (enter 0 use default path ./contract/otherdata.json) " << std::endl;
    std::cin >> nContractOther;

    nlohmann::json otherData;
    ret = loop_read_json(nContractOther, otherData, "otherdata.json");
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


void handle_deploy_contract()
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

    DBReader data_reader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }

    uint32_t nContractType;
    std::cout << "Please enter contract type: (0: EVM) " << std::endl;
    std::cin >> nContractType;

    nlohmann::json contract_info;
    int ret = generate_contract_info(contract_info);
    if (ret != 0)
    {
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    if(nContractType == 0)
    {

        std::string nContractPath;
        std::cout << "Please enter contract path : (enter 0 use default path ./contract/contract) " << std::endl;
        std::cin >> nContractPath;

        std::string code;
        ret = loop_read_file(nContractPath, code, "contract");
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
        std::string OwnerEvmAddr = evm_utils::generateEvmAddr(launchAccount.pubStr);
        ret = TxHelper::CreateEvmDeployContractTransaction(strFromAddr, OwnerEvmAddr, code, top + 1, contract_info,
                                                           outTx,
                                                           isNeedAgent_flag,
                                                           info_);
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

	if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void handle_call_contract()
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

    DBReader data_reader;
    std::vector<std::string> vecDeployers;
    data_reader.GetAllDeployerAddr(vecDeployers);
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
    data_reader.GetDeployUtxoByDeployerAddr(strToAddr, vecDeployUtxos);
    std::cout << "=====================deployed utxos=====================" << std::endl;
    for(auto& deploy_utxo : vecDeployUtxos)
    {
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

    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }


    CTransaction outTx;
    CTransaction tx;
    std::string tx_raw;
    if (DBStatus::DB_SUCCESS != data_reader.GetTransactionByHash(strTxHash, tx_raw))
    {
        ERRORLOG("get contract transaction failed!!");
        return ;
    }
    if(!tx.ParseFromString(tx_raw))
    {
        ERRORLOG("contract transaction parse failed!!");
        return ;
    }
    

    nlohmann::json data_json = nlohmann::json::parse(tx.data());
    nlohmann::json tx_info = data_json["TxInfo"].get<nlohmann::json>();
    int vm_type = tx_info["VmType"].get<int>();
 
    int ret = 0;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    if (vm_type == global::ca::VmType::EVM)
    {
        Account launchAccount;
        if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(strFromAddr, launchAccount) != 0)
        {
            ERRORLOG("Failed to find account {}", strFromAddr);
            return;
        }
        std::string OwnerEvmAddr = evm_utils::generateEvmAddr(launchAccount.pubStr);
        ret = TxHelper::CreateEvmCallContractTransaction(strFromAddr, strToAddr, strTxHash, strInput,
                                                         OwnerEvmAddr, top + 1,
                                                         outTx, isNeedAgent_flag, info_);
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

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info -> CopyFrom(info_);

    }

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void handle_export_private_key()
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
    EVP_PKEY_free(account.pkey);
    MagicSingleton<AccountManager>::GetInstance()->FindAccount(addr, account);

    file << "Please use Courier New font to view" << std::endl
         << std::endl;

    file << "Base58 addr: " << addr << std::endl;
    std::cout << "Base58 addr: " << addr << std::endl;

    char out_data[1024] = {0};
    int data_len = sizeof(out_data);
    mnemonic_from_data((const uint8_t *)account.priStr.c_str(), account.priStr.size(), out_data, data_len);
    file << "Mnemonic: " << out_data << std::endl;
    std::cout << "Mnemonic: " << out_data << std::endl;

    std::string strPriHex = Str2Hex(account.priStr);
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


    ca_console redColor(kConsoleColor_Red, kConsoleColor_Black, true);
    std::cout << redColor.color() << "You can also view above in file:" << fileName << " of current directory." << redColor.reset() << std::endl;
    return;
}

int get_chain_height(unsigned int &chainHeight)
{
    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        return -1;
    }
    chainHeight = top;
    return 0;
}

void net_register_chain_height_callback()
{
    net_callback::register_chain_height_callback(get_chain_height);
}

/**
 * @description: Registering Callbacks
 * @param {*}
 * @return {*}
 */
void RegisterCallback()
{
    syncBlock_register_callback<FastSyncGetHashReq>(HandleFastSyncGetHashReq);
    syncBlock_register_callback<FastSyncGetHashAck>(HandleFastSyncGetHashAck);
    syncBlock_register_callback<FastSyncGetBlockReq>(HandleFastSyncGetBlockReq);
    syncBlock_register_callback<FastSyncGetBlockAck>(HandleFastSyncGetBlockAck);

    syncBlock_register_callback<SyncGetSumHashReq>(HandleSyncGetSumHashReq);
    syncBlock_register_callback<SyncGetSumHashAck>(HandleSyncGetSumHashAck);
    syncBlock_register_callback<SyncGetHeightHashReq>(HandleSyncGetHeightHashReq);
    syncBlock_register_callback<SyncGetHeightHashAck>(HandleSyncGetHeightHashAck);
    syncBlock_register_callback<SyncGetBlockReq>(HandleSyncGetBlockReq);
    syncBlock_register_callback<SyncGetBlockAck>(HandleSyncGetBlockAck);

    syncBlock_register_callback<SyncFromZeroGetSumHashReq>(HandleFromZeroSyncGetSumHashReq);
    syncBlock_register_callback<SyncFromZeroGetSumHashAck>(HandleFromZeroSyncGetSumHashAck);
    syncBlock_register_callback<SyncFromZeroGetBlockReq>(HandleFromZeroSyncGetBlockReq);
    syncBlock_register_callback<SyncFromZeroGetBlockAck>(HandleFromZeroSyncGetBlockAck);

    syncBlock_register_callback<GetBlockByUtxoReq>(HandleBlockByUtxoReq);
    syncBlock_register_callback<GetBlockByUtxoAck>(HandleBlockByUtxoAck);

    syncBlock_register_callback<GetBlockByHashReq>(HandleBlockByHashReq);
    syncBlock_register_callback<GetBlockByHashAck>(HandleBlockByHashAck);

    syncBlock_register_callback<SeekPreHashByHightReq>(HandleSeekGetPreHashReq);
    syncBlock_register_callback<SeekPreHashByHightAck>(HandleSeekGetPreHashAck);

    // PCEnd correlation
    tx_register_callback<TxMsgReq>(HandleTx); // PCEnd transaction flow

    
    saveBlock_register_callback<BuildBlockBroadcastMsg>(HandleBuildBlockBroadcastMsg); // Building block broadcasting

    block_register_callback<BlockMsg>(HandleBlock);      // PCEnd transaction flow
    block_register_callback<BuildBlockBroadcastMsgAck>(HandleAddBlockAck);

    ca_register_callback<MultiSignTxReq>(HandleMultiSignTxReq);

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



        DBReader db_reader;
        uint64_t top = 0;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
        {
            ERRORLOG("db get top failed!!");
            continue;
        }

        CTransaction outTx;
        TxHelper::vrfAgentType isNeedAgent_flag;
        Vrf info_;
        int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, top + 1,  outTx,isNeedAgent_flag,info_);
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
        
        if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf){
            Vrf * new_info=txMsg.mutable_vrfinfo();
            new_info->CopyFrom(info_);

        }


        auto msg = make_shared<TxMsgReq>(txMsg);

        std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
        if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr){

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

int checkNtpTime()
{
    // Ntp check
    int64_t getNtpTime = MagicSingleton<TimeUtil>::GetInstance()->getNtpTimestamp();
    int64_t getLocTime = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();

    int64_t tmpTime = abs(getNtpTime - getLocTime);

    std::cout << "UTC Time: " << MagicSingleton<TimeUtil>::GetInstance()->formatUTCTimestamp(getLocTime) << std::endl;
    std::cout << "Ntp Time: " << MagicSingleton<TimeUtil>::GetInstance()->formatUTCTimestamp(getNtpTime) << std::endl;

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

std::string handle__deploy_contract_rpc(void * arg){

    deploy_contract_req * req_=(deploy_contract_req *)arg;

    int ret;
    std::string strFromAddr=req_->addr;
   
    if (!CheckBase58Addr(strFromAddr))
    {
       return "base58 error!";
    }

    DBReader data_reader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
       return "db get top failed!!";
    }

    uint32_t nContractType=std::stoi(req_->nContractType);

    contract_info info_t;
    if(info_t.paseFromJson(req_->info)==false){
        return "contract_info pase fail";
    }

    nlohmann::json contract_info_(info_t.paseToString());
   

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    if(nContractType == 0)
    {

        std::string code=req_->contract;
        std::string strInput=req_->data;

        if (strInput == "0")
        {
            strInput.clear();
        }
        else if(strInput.substr(0, 2) == "0x")
        {
            strInput = strInput.substr(2);
            code += strInput;
        }
        Base64 base_;
        std::string pubstr=base_.Decode(req_->pubstr.c_str(),req_->pubstr.size());
        std::string OwnerEvmAddr = evm_utils::generateEvmAddr(pubstr);
      
        ret = interface_evm::CreateEvmDeployContractTransaction(strFromAddr, OwnerEvmAddr, code, top + 1, contract_info_,
                                                           outTx,
                                                           isNeedAgent_flag,
                                                           info_);
        if(ret != 0)
        {
           return "Failed to create DeployContract transaction! The error code is:" + std::to_string(ret);
        }        
    }
    else
    {
        return "unknow error";
    }

	TxMsgReq txMsg;
	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

	if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if(isNeedAgent_flag==TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {
        ret = DropshippingTx(msg,outTx);
    }
    else
    {
        ret = DoHandleTx(msg,outTx);
    }

    return std::to_string(ret);
}


std::string handle__call_contract_rpc(void * arg){


return std::string("");
}
