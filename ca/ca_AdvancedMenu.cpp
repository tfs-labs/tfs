#include "ca_AdvancedMenu.h"

#include <sys/time.h>
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"

#include <regex>
#include <iomanip>

#include "net/net_api.h"
#include "net/peer_node.h"
#include "net/socket_buf.h"
#include "include/logging.h"
#include "include/ScopeGuard.h"
#include "utils/time_util.h"
#include "utils/qrcode.h"
#include "common/time_report.h"
#include "common/global_data.h"
#include "utils/MagicSingleton.h"
#include "ca/ca_test.h"
#include "ca/ca_global.h"
#include "ca/ca_transaction.h"
#include "ca/ca_interface.h"
#include "utils/hexcode.h"

#include "ca/ca_txhelper.h"
#include "utils/bip39.h"
#include "ca/ca.h"
#include "ca/ca_sync_block.h"
#include "ca/ca_blockcache.h"
#include "ca/ca_blockcache.h"
#include "ca/ca_algorithm.h"

#include "utils/console.h"
#include "ca/ca_blockcache.h"

#include "utils/AccountManager.h"
#include "ca/ca_evmone.h"
#include "utils/ContractUtils.h"
#include "utils/Cycliclist.hpp"
#include "utils/TFSbenchmark.h"
#include "ca_blockhelper.h"

void gen_key()
{
    std::cout << "Please enter the number of accounts to be generated: ";
    int num = 0;
    std::cin >> num;
    if (num <= 0)
    {
        return;
    }

    for (int i = 0; i != num; ++i)
    {
        Account acc(Base58Ver::kBase58Ver_Normal);
        MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
        MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.GetBase58());
    }

    std::cout << "Successfully generated account " << std::endl;
}

void rollback()
{
    MagicSingleton<BlockHelper>::GetInstance()->rollback_test();
}

void GetStakeList()
{
    DBReader db_reader;
    std::vector<std::string> addresses;
    db_reader.GetStakeAddress(addresses);
    std::cout << "StakeList :" << std::endl;
    for (auto &it : addresses)
    {
        std::cout << it << std::endl;
    }
}
int GetBounsAddrInfo()
{
    DBReader db_reader;
    std::vector<std::string> addresses;
    std::vector<std::string> bonusAddrs;
    db_reader.GetBonusaddr(bonusAddrs);
    for (auto &bonusAddr : bonusAddrs)
    {
        std::cout << YELLOW << "BonusAddr: " << bonusAddr << RESET << std::endl;
        auto ret = db_reader.GetInvestAddrsByBonusAddr(bonusAddr, addresses);
        if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
        {
            return -1;
        }

        uint64_t sum_invest_amount = 0;
        std::cout << "InvestAddr:" << std::endl;
        for (auto &address : addresses)
        {
            std::cout << address << std::endl;
            std::vector<string> utxos;
            ret = db_reader.GetBonusAddrInvestUtxosByBonusAddr(bonusAddr, address, utxos);
            if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
            {
                return -2;
            }

            uint64_t invest_amount = 0;
            for (const auto &hash : utxos)
            {
                std::string tx_raw;
                if (db_reader.GetTransactionByHash(hash, tx_raw) != DBStatus::DB_SUCCESS)
                {
                    return -3;
                }
                CTransaction tx;
                if (!tx.ParseFromString(tx_raw))
                {
                    return -4;
                }
                for (int i = 0; i < tx.utxo().vout_size(); i++)
                {
                    if (tx.utxo().vout(i).addr() == global::ca::kVirtualInvestAddr)
                    {
                        invest_amount += tx.utxo().vout(i).value();
                        break;
                    }
                }
            }
            sum_invest_amount += invest_amount;
        }
        std::cout << "total invest amount :" << sum_invest_amount << std::endl;
    }
    return 0;
}

void send_message_to_user()
{
    if (net_com::input_send_one_message() == 0)
        DEBUGLOG("send one msg Succ.");
    else
        DEBUGLOG("send one msg Fail.");
}

void show_my_k_bucket()
{
    std::cout << "The K bucket is being displayed..." << std::endl;
    auto nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    MagicSingleton<PeerNode>::GetInstance()->print(nodelist);
}

void kick_out_node()
{
    std::string id;
    std::cout << "input id:" << std::endl;
    std::cin >> id;
    MagicSingleton<PeerNode>::GetInstance()->delete_node(id);
    std::cout << "Kick out node succeed!" << std::endl;
}

void test_echo()
{

    std::string message;
    std::cout << "please input message:" << std::endl;
    std::cin >> message;

    EchoReq echoReq;
    echoReq.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
    echoReq.set_message(message);
    bool isSucceed = net_com::broadcast_message(echoReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
    if (isSucceed == false)
    {
        ERRORLOG(":broadcast EchoReq failed!");
        return;
    }
}

void print_req_and_ack()
{
    double total = .0f;
    std::cout << "------------------------------------------" << std::endl;
    for (auto &item : global::reqCntMap)
    {
        total += (double)item.second.second;
        std::cout.precision(3);
        std::cout << item.first << ": " << item.second.first << " size: " << (double)item.second.second / 1024 / 1024 << " MB" << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total: " << total / 1024 / 1024 << " MB" << std::endl;
}

void menu_blockinfo()
{
    while (true)
    {
        DBReader reader;
        uint64_t top = 0;
        reader.GetBlockTop(top);

        std::cout << std::endl;
        std::cout << "Height: " << top << std::endl;
        std::cout << "1.Get the total number of transactions \n"
                     "2.Get transaction block details\n"
                     "5.Get device password \n"
                     "6.Set device password\n"
                     "7.Get device private key\n"
                     "0.Exit \n";

        std::string strKey;
        std::cout << "please input your choice:";
        std::cin >> strKey;

        std::regex pattern("^[0-7]$");
        if (!std::regex_match(strKey, pattern))
        {
            std::cout << "Input invalid." << std::endl;
            return;
        }
        int key = std::stoi(strKey);
        switch (key)
        {
        case 0:
            return;
           

        case 2:
            get_tx_block_info(top);
            break;
       

        default:
            std::cout << "Invalid input." << std::endl;
            continue;
        }

        sleep(1);
    }
}

void get_tx_block_info(uint64_t &top)
{
    auto amount = to_string(top);
    std::string input_s, input_e;
    uint64_t start, end;

    std::cout << "amount: " << amount << std::endl;
    std::cout << "pleace input start: ";
    std::cin >> input_s;
    if (input_s == "a" || input_s == "pa")
    {
        input_s = "0";
        input_e = amount;
    }
    else
    {
        if (std::stoul(input_s) > std::stoul(amount))
        {
            std::cout << "input > amount" << std::endl;
            return;
        }
        std::cout << "pleace input end: ";
        std::cin >> input_e;
        if (std::stoul(input_s) < 0 || std::stoul(input_e) < 0)
        {
            std::cout << "params < 0!!" << endl;
            return;
        }
        if (std::stoul(input_s) > std::stoul(input_e))
        {
            input_s = input_e;
        }
        if (std::stoul(input_e) > std::stoul(amount))
        {
            input_e = std::to_string(top);
        }
    }
    start = std::stoul(input_s);
    end = std::stoul(input_e);

    std::cout << "Print to screen[0] or file[1] ";
    uint64_t nType = 0;
    std::cin >> nType;
    if (nType == 0)
    {
        printRocksdb(start, end, true, std::cout);
    }
    else if (nType == 1)
    {
        std::string fileName = "print_block_" + std::to_string(start) + "_" + std::to_string(end) + ".txt";
        std::ofstream filestream;
        filestream.open(fileName);
        if (!filestream)
        {
            std::cout << "Open file failed!" << std::endl;
            return;
        }
        printRocksdb(start, end, true, filestream);
    }
}

void gen_mnemonic()
{
    char out[1024 * 10] = {0};

    // g_AccountInfo.GetMnemonic(nullptr, out, sizeof(out));
    // printf("Mnemonic_data: [%s]\n", out);

    std::string mnemonic;

    Account defaultEd;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultEd);
    MagicSingleton<AccountManager>::GetInstance()->GetMnemonic(defaultEd.GetBase58(), mnemonic);
    std::cout << "mnemonic : " << mnemonic << std::endl;
    std::cout << "priStr : " << Str2Hex(defaultEd.GetPriStr()) << std::endl;
    std::cout << "pubStr : " << Str2Hex(defaultEd.GetPubStr()) << std::endl;

    std::cout << "input mnemonic:" << std::endl;
    std::string str;
    std::cin.ignore(std::numeric_limits<streamsize>::max(), '\n');
    std::getline(std::cin, str);

    int len = 0;
    if (mnemonic.size() > str.size())
    {
        len = mnemonic.size();
    }
    else
    {
        len = str.size();
    }

    for (int i = 0; i < len; i++)
    {
        if (mnemonic[i] != str[i])
        {
            std::cout << "not equal!" << std::endl;
        }
    }

    if (str != mnemonic)
    {
        std::cout << "mnemonic error !!! " << std::endl;
    }
    std::cout << "final mnemonic : " << str << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->ImportMnemonic(mnemonic);
}

void printTxdata()
{
    std::string hash;
    std::cout << "TX hash: ";
    std::cin >> hash;

    DBReader data_reader;

    CTransaction tx;
    std::string TxRaw;
    auto ret = data_reader.GetTransactionByHash(hash, TxRaw);
    if (ret != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("GetTransactionByHash failed!");
        return;
    }
    if (!tx.ParseFromString(TxRaw))
    {
        ERRORLOG("Transaction Parse failed!");
        return;
    }

    nlohmann::json data_json = nlohmann::json::parse(tx.data());
    std::string data = data_json.dump(4);
    std::cout << data << std::endl;
}


void multiTx()
{
    std::ifstream fin;
	fin.open("toaddr.txt", ifstream::binary);
    if (!fin.is_open())
	{
		cout << "open file error" << endl;
		return;
	}

    std::vector<std::string> fromAddr;

    std::string addr;
    std::cout << "input fromaddr >:";
    std::cin >> addr;
    fromAddr.push_back(addr);

    std::vector<std::string> to_addrs;
    std::map<std::string, int64_t> toAddr;
    std::string Addr;
    double amt = 0;
    std::cout << "input amount>:";
    std::cin >> amt;


     
    while (getline(fin, Addr))
    {
        if(Addr[Addr.length()-1]=='\r')
        {
            Addr=Addr.substr(0,Addr.length()-1);
        }
        to_addrs.push_back(Addr);
    }


    uint32_t start_count = 0;
    uint32_t end_count = 0;
    std::cout << "please input start index>:" ;
    std::cin >> start_count;

    std::cout << "please input end index>:" ;
    std::cin >> end_count;

    for(uint32_t i = start_count ; i <= end_count ; i++)
    {
        toAddr.insert(std::make_pair(to_addrs[i],amt * global::ca::kDecimalNum));
    }


    fin.close();

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxMsgReq txMsg;
    TxHelper::vrfAgentType isNeedAgent_flag;
    CTransaction outTx;
    Vrf info_;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddr, top + 1, outTx,isNeedAgent_flag,info_);
    if (ret != 0)
	{
		ERRORLOG("CreateTxTransaction error!!");
		return;
	}


	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf){
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);
    }

    auto msg = make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}", ret, outTx.hash());
}

void evmAddrConversion()
{
    std::string strInput;
    std::cout << "Please enter non-checksummed version addr:" << std::endl;
    std::cin >> strInput;
    bool need0x = false;
    if (strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
        need0x = true;
    }

    std::string checksum_addr = evm_utils::EvmAddrToChecksum(strInput);
    if (need0x)
    {
        checksum_addr = "0x" + checksum_addr;
    }

    std::cout << checksum_addr << std::endl;
}



static bool benchmark_automic_write_switch = false;
void printBenchmarkToFile()
{
    if(benchmark_automic_write_switch)
    {
        benchmark_automic_write_switch = false;
        std::cout << "benchmark automic write has stoped" << std::endl;
        return;
    }
    std::cout << "enter write time interval (unit second) :";
    int interval = 0;
    std::cin >> interval;
    if(interval <= 0)
    {
         std::cout << "time interval less or equal to 0" << std::endl;
         return;
    }
    benchmark_automic_write_switch = true;
    auto benchmark_automic_write_thread = std::thread(
            [interval]()
            {
                while (benchmark_automic_write_switch)
                {
                    MagicSingleton<TFSbenchmark>::GetInstance()->PrintBenchmarkSummary(true);
                    MagicSingleton<TFSbenchmark>::GetInstance()->PrintBenchmarkSummary_DoHandleTx(true);
                    sleep(interval);
                }
            }
    );
    benchmark_automic_write_thread.detach();


    return;
}


void get_balance_by_utxo()
{
    std::cout << "Inquiry address:";
    std::string addr;
    std::cin >> addr;

    DBReader reader;
    std::vector<std::string> utxoHashs;
    reader.GetUtxoHashsByAddress(addr, utxoHashs);

    auto utxoOutput = [addr, utxoHashs, &reader](ostream &stream)
    {
        stream << "account:" << addr << " utxo list " << std::endl;

        uint64_t total = 0;
        for (auto i : utxoHashs)
        {
            std::string txRaw;
            reader.GetTransactionByHash(i, txRaw);

            CTransaction tx;
            tx.ParseFromString(txRaw);

            uint64_t value = 0;
            for (int j = 0; j < tx.utxo().vout_size(); j++)
            {
                CTxOutput txout = tx.utxo().vout(j);
                if (txout.addr() != addr)
                {
                    continue;
                }
                value += txout.value();
            }
            stream << i << " : " << value << std::endl;
            total += value;
        }

        stream << "address: " << addr << " UTXO total: " << utxoHashs.size() << " UTXO gross value:" << total << std::endl;
    };

    if (utxoHashs.size() < 10)
    {
        utxoOutput(std::cout);
    }
    else
    {
        std::string fileName = "utxo_" + addr + ".txt";
        ofstream file(fileName);
        if (!file.is_open())
        {
            ERRORLOG("Open file failed!");
            return;
        }
        utxoOutput(file);
        file.close();
    }
}

int imitate_create_tx_struct()
{
    Account acc;
    EVP_PKEY_free(acc.GetKey());
    if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
    {
        return -1;
    }

    const std::string addr = acc.GetBase58();
    uint64_t time = global::ca::kGenesisTime;

    CTransaction tx;
    tx.set_version(0);
    tx.set_time(time);
    tx.set_n(0);
    tx.set_identity(addr);
    tx.set_type(global::ca::kGenesisSign);

    CTxUtxo *utxo = tx.mutable_utxo();
    utxo->add_owner(addr);
    {
        CTxInput *txin = utxo->add_vin();
        CTxPrevOutput *prevOut = txin->add_prevout();
        prevOut->set_hash(std::string(64, '0'));
        prevOut->set_n(0);
        txin->set_sequence(0);

        std::string serVinHash = getsha256hash(txin->SerializeAsString());
        std::string signature;
        std::string pub;

        if (acc.Sign(serVinHash, signature) == false)
        {
            return -2;
        }

        CSign *sign = txin->mutable_vinsign();
        sign->set_sign(signature);
        sign->set_pub(acc.GetPubStr());
    }

    {
        CTxOutput *txout = utxo->add_vout();
        txout->set_value(global::ca::kM2 * global::ca::kDecimalNum);
        txout->set_addr(addr);
    }

    {
        std::string serUtxo = getsha256hash(utxo->SerializeAsString());
        std::string signature;
        if (acc.Sign(serUtxo, signature) == false)
        {
            return -3;
        }

        CSign *multiSign = utxo->add_multisign();
        multiSign->set_sign(signature);
        multiSign->set_pub(acc.GetPubStr());
    }
    
    tx.set_txtype((uint32)global::ca::TxType::kTxTypeGenesis);

    tx.set_hash(getsha256hash(tx.SerializeAsString()));

    CBlock block;
    block.set_time(time);
    block.set_version(0);
    block.set_prevhash(std::string(64, '0'));
    block.set_height(0);

    CTransaction *tx0 = block.add_txs();
    *tx0 = tx;

    nlohmann::json blockData;
    blockData["Name"] = "Transformers";
    blockData["Type"] = "Genesis";
    block.set_data(blockData.dump());

    block.set_merkleroot(ca_algorithm::CalcBlockMerkle(block));
    block.set_hash(getsha256hash(block.SerializeAsString()));

    std::string hex = Str2Hex(block.SerializeAsString());
    std::cout << std::endl
              << hex << std::endl;

    return 0;
}


void multi_tx()
{
    int addrCount = 0;
    std::cout << "Number of initiator accounts:";
    std::cin >> addrCount;

    std::vector<std::string> fromAddr;
    for (int i = 0; i < addrCount; ++i)
    {
        std::string addr;
        std::cout << "Initiating account" << i + 1 << ": ";
        std::cin >> addr;
        fromAddr.push_back(addr);
    }

    std::cout << "Number of receiver accounts:";
    std::cin >> addrCount;

    std::map<std::string, int64_t> toAddr;
    for (int i = 0; i < addrCount; ++i)
    {
        std::string addr;
        double amt = 0;
        std::cout << "Receiving account" << i + 1 << ": ";
        std::cin >> addr;
        std::cout << "amount : ";
        std::cin >> amt;
        toAddr.insert(make_pair(addr, amt * global::ca::kDecimalNum));
    }

    DBReader db_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxMsgReq txMsg;
    TxHelper::vrfAgentType isNeedAgent_flag;
    CTransaction outTx;
    Vrf info_;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddr, top + 1, outTx,isNeedAgent_flag,info_);
    if (ret != 0)
	{
		ERRORLOG("CreateTxTransaction error!!");
		return;
	}


	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf){
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);
    }

    auto msg = make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}", ret, outTx.hash());
}

void get_all_pledge_addr()
{
    DBReader reader;
    std::vector<std::string> addressVec;
    reader.GetStakeAddress(addressVec);

    auto allPledgeOutput = [addressVec](ostream &stream)
    {
        stream << std::endl
               << "---- Pledged address start ----" << std::endl;
        for (auto &addr : addressVec)
        {
            uint64_t pledgeamount = 0;
            SearchStake(addr, pledgeamount, global::ca::StakeType::kStakeType_Node);
            stream << addr << " : " << pledgeamount << std::endl;
        }
        stream << "---- Number of pledged addresses:" << addressVec.size() << " ----" << std::endl
               << std::endl;
        stream << "---- Pledged address end  ----" << std::endl
               << std::endl;
    };

    if (addressVec.size() <= 10)
    {
        allPledgeOutput(std::cout);
    }
    else
    {
        std::string fileName = "all_pledge.txt";

        std::cout << "output to a file" << fileName << std::endl;

        std::ofstream fileStream;
        fileStream.open(fileName);
        if (!fileStream)
        {
            std::cout << "Open file failed!" << std::endl;
            return;
        }

        allPledgeOutput(fileStream);

        fileStream.close();
    }
}

void auto_tx()
{
    if (bIsCreateTx)
    {
        int i = 0;
        std::cout << "1. Close the transaction" << std::endl;
        std::cout << "0. Continue trading" << std::endl;
        std::cout << ">>>" << std::endl;
        std::cin >> i;
        if (i == 1)
        {
            bStopTx = true;
        }
        else if (i == 0)
        {
            return;
        }
        else
        {
            std::cout << "Error!" << std::endl;
            return;
        }
    }
    else
    {
        bStopTx = false;
        std::vector<std::string> addrs;

        MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
        MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

        double sleepTime = 0;
        std::cout << "Interval time (seconds):";
        std::cin >> sleepTime;
        sleepTime *= 1000000;
        std::thread th(TestCreateTx, addrs, (int)sleepTime);
        th.detach();
        return;
    }
}

void get_blockinfo_by_txhash()
{
    DBReader reader;

    std::cout << "Tx Hash : ";
    std::string txHash;
    std::cin >> txHash;

    std::string blockHash;
    reader.GetBlockHashByTransactionHash(txHash, blockHash);

    if (blockHash.empty())
    {
        std::cout << RED << "Error : GetBlockHashByTransactionHash failed !" << RESET << std::endl;
        return;
    }

    std::string blockStr;
    reader.GetBlockByBlockHash(blockHash, blockStr);
    CBlock block;
    block.ParseFromString(blockStr);

    std::cout << GREEN << "Block Hash : " << blockHash << RESET << std::endl;
    std::cout << GREEN << "Block height : " << block.height() << RESET << std::endl;
}

void get_tx_hash_by_height()
{
    int64_t start = 0;
    int64_t end = 0;

    std::cout << "Please input start height:";
    std::cin >> start;

    std::cout << "Please input end height:";
    std::cin >> end;

    if (end < start)
    {
        std::cout << "input invalid" << std::endl;
        return;
    }

    std::string fileName = "TPS_INFO_" + std::to_string(start) + "_" + std::to_string(end) + ".txt";
    std::ofstream filestream;
    filestream.open(fileName);
    if (!filestream)
    {
        std::cout << "Open file failed!" << std::endl;
        return;
    }
    filestream << "TPS_INFO:" << std::endl;
    DBReader db_reader;
    uint64_t tx_total = 0;
    uint64_t block_total = 0;
    for (int64_t i = end; i >= start; --i)
    {

        std::vector<std::string> tmp_block_hashs;
        if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(i, tmp_block_hashs))
        {
            ERRORLOG("(get_tx_hash_by_height) GetBlockHashsByBlockHeight  Failed!!");
            return;
        }

        int tx_hash_count = 0;
        for (auto &blockhash : tmp_block_hashs)
        {
            std::string blockstr;
            db_reader.GetBlockByBlockHash(blockhash, blockstr);
            CBlock block;
            block.ParseFromString(blockstr);

            tx_hash_count += block.txs_size();
        }
        tx_total += tx_hash_count;
        block_total += tmp_block_hashs.size();
        filestream << GREEN << "height: " << i << " block: " << tmp_block_hashs.size() << " tx: " << tx_hash_count << RESET << std::endl;
    }

    filestream << GREEN << "block sum " << block_total << RESET << std::endl;
    filestream << GREEN << "tx sum " << tx_total  << RESET << std::endl;

    std::vector<std::string> start_hashes;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(start, start_hashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail  top = {} ", start);
        return;
    }

    //Take out the blocks at the starting height and sort them from the smallest to the largest in time
    std::vector<CBlock> start_blocks;
    for (auto &hash : start_hashes)
    {
        std::string blockStr;
        db_reader.GetBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        start_blocks.push_back(block);
    }
    std::sort(start_blocks.begin(), start_blocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    std::vector<std::string> end_hashes;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(end, end_hashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail  top = {} ", end);
        return;
    }

    //Take out the blocks at the end height and sort them from small to large in time
    std::vector<CBlock> end_blocks;
    for (auto &hash : end_hashes)
    {
        std::string blockStr;
        db_reader.GetBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        end_blocks.push_back(block);
    }
    std::sort(end_blocks.begin(), end_blocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    uint64_t time_diff = 0;
    if (end_blocks[end_blocks.size() - 1].time() - start_blocks[0].time() != 0)
    {
        time_diff = (end_blocks[end_blocks.size() - 1].time() - start_blocks[0].time()) / 1000000;
    }
    else
    {
        time_diff = 1;
    }
    uint64_t tx_conut = tx_total ;
    uint64_t tps = tx_conut / time_diff;
    filestream << "TPS : " << tps << std::endl;
}

void get_investedNodeBlance()
{
    std::string addr;
    std::cout << "Please enter the address you need to inquire: " << std::endl;
    std::cin >> addr;

    std::shared_ptr<GetAllInvestAddressReq> req = std::make_shared<GetAllInvestAddressReq>();
    req->set_version(global::kVersion);
    req->set_addr(addr);

    GetAllInvestAddressAck ack;
    GetAllInvestAddressReqImpl(req, ack);
    if (ack.code() != 0)
    {
        std::cout << "code: " << ack.code() << std::endl;
        ERRORLOG("get_investedNodeBlance failed!");
        return;
    }

    std::cout << "------------" << ack.addr() << "------------" << std::endl;

    for (int i = 0; i < ack.list_size(); i++)
    {
        const InvestAddressItem info = ack.list(i);
        std::cout << "addr:" << info.addr() << "\tamount:" << info.value() << std::endl;
    }
}
void print_database_block()
{
    DBReader dbReader;
    std::string str = printBlocks(100, false);
    std::cout << str << std::endl;
}

void ThreadTest::TestCreateTx_2(const std::string &from, const std::string &to)
{
    std::cout << "from:" << from << std::endl;
    std::cout << "to:" << to << std::endl;

    uint64_t start_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    bool Initiate = false;
    ON_SCOPE_EXIT{
        if(!Initiate)
        {
            MagicSingleton<TFSbenchmark>::GetInstance()->ClearTransactionInitiateMap();
        }
    };

    int intPart = 0;
    double decPart = (double)(rand() % 10) / 10000;
    std::string amountStr = std::to_string(intPart + decPart);

    std::vector<std::string> fromAddr;
    fromAddr.emplace_back(from);
    std::map<std::string, int64_t> toAddrAmount;
    uint64_t amount = (stod(amountStr) + global::ca::kFixDoubleMinPrecision) * global::ca::kDecimalNum;

    if (amount == 0)
    {
        std::cout << "amount = 0" << std::endl;
        DEBUGLOG("amount = 0");
        return;
    }

    toAddrAmount[to] = amount;

    DBReader data_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }
    
    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, top + 1, outTx,isNeedAgent_flag,info_);
    if (ret != 0)
    {
        ERRORLOG("CreateTxTransaction error!!");
        return;
    }
    

    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);


    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf){
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

    }

    auto msg = make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }
    global::ca::TxNumber++;
    DEBUGLOG("Transaction result,ret:{}  txHash:{}, TxNumber:{}", ret, outTx.hash(), global::ca::TxNumber);
    Initiate = true;
    MagicSingleton<TFSbenchmark>::GetInstance()->AddTransactionInitiateMap(start_time, MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
    
    std::cout << "=====Transaction initiator:" << from << std::endl;
    std::cout << "=====Transaction recipient:" << to << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl;
}

bool bStopTx_2 = true;
bool bIsCreateTx_2 = false;
static int i = -1;
static int i_count = 1;
static int count_wheel = 0;
int GetIndex(uint32_t &tranNum, std::vector<std::string> &addrs, bool flag = false)
{
    if ((i + 1) > ((tranNum * 2) - 1))
    {
        i = 0;
    }
    else
    {
        i += 1;
    }
    if (flag)
    {
        std::vector<CTransaction> vectTxs;
    }
    return i;
}
void ThreadTest::set_StopTx_flag(const bool &flag)
{
    bStopTx_2 = flag;
}



void ThreadTest::get_StopTx_flag(bool &flag)
{
   flag =  bStopTx_2 ;
}



void ThreadTest::test_createTx(uint32_t tranNum, std::vector<std::string> addrs_,int timeout)
{
    DEBUGLOG("test_createTx start at {}", MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
    Cycliclist<std::string> addrs;

    for (auto &U : addrs_)
    {
        addrs.push_back(U);
    }

    if(addrs.isEmpty())
    {
        std::cout << "account list is empty" << std::endl;
        return;
    }
    auto iter=addrs.begin();
    while (bStopTx_2==false)
    {
        MagicSingleton<TFSbenchmark>::GetInstance()->SetTransactionInitiateBatchSize(tranNum);
        for (int i = 0; i < tranNum; i++)
        {
           
            std::string from = iter->data;
            iter++;
            std::string to = iter->data;
            std::thread th(ThreadTest::TestCreateTx_2, from, to);
            th.detach();
            
        }
        sleep(timeout);
    }
}

void Create_multi_thread_automatic_transaction()
{
    std::cout << "1. tx " << std::endl;
    std::cout << "2. close" << std::endl;

    int check=0;
     std::cout << "chose:" ;
     std::cin >> check;

     if(check==1){
       if(bStopTx_2==true){

            bStopTx_2=false;
       }else {

            std::cout << "has run" << std::endl;
            return;
       }
     }else if(check ==2){
        bStopTx_2=true;
        return;
     }else{
        std::cout<< " invalui" << std::endl;
        return;
     }
     if(bStopTx_2)
     {
        return;
     }

    int TxNum = 0;
    int timeout = 0;

    std::cout << "Interval time (seconds):";
    std::cin >> timeout;

    std::cout << "Interval frequency :" ;

    std:: cin >> TxNum;
    std::vector<std::string> addrs_;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs_);

    std::thread th(ThreadTest::test_createTx,TxNum, addrs_, timeout);
    th.detach();
}

void TestCreateStake_2(const std::string &from)
{
    TxHelper::PledgeType pledgeType = TxHelper::PledgeType::kPledgeType_Node;
    uint64_t stake_amount = 10  * global::ca::kDecimalNum ;

    DBReader data_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }


    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    std::vector<TxHelper::Utxo> outVin;
    if(TxHelper::CreateStakeTransaction(from, stake_amount, top + 1, pledgeType, outTx, outVin,isNeedAgent_flag,info_) != 0)
    {
        return;
    }
    std::cout << " from: " << from << " amout: " << stake_amount << std::endl;
    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if(isNeedAgent_flag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);
    }
    auto msg = std::make_shared<TxMsgReq>(txMsg);
    int ret = 0;
    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }

    if (ret != 0)
    {
        ret -= 100;
    }
    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
    //MagicSingleton<TranMonitor>::GetInstance()->SetDoHandleTxStatus(outTx, ret);
}


void Create_multi_thread_automatic_stake_transaction()
{
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(addrs.begin(), addrs.end(), MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    if (it != addrs.end())
    {
        addrs.erase(it);
    }

    for (int i = 0; i <= addrs.size(); ++i)
    {
        std::thread th(TestCreateStake_2, addrs[i]);
        th.detach();
    }
}

void TestCreateInvestment(const std::string &strFromAddr, const std::string &strToAddr, const std::string &amountStr)
{

    TxHelper::InvestType investType = TxHelper::InvestType::kInvestType_NetLicence;

    uint64_t invest_amount = std::stod(amountStr) * global::ca::kDecimalNum;

    // DBReader db_reader;
    DBReader data_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }


    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType isNeedAgent_flag;
    Vrf info_;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, invest_amount, top + 1, investType,outTx, outVin,isNeedAgent_flag,info_);
	if(ret != 0)
	{
        ERRORLOG("Failed to create investment transaction! The error code is:{}", ret);
        return;
    }
    //MagicSingleton<TranMonitor>::GetInstance()->AddTranMonitor(outTx);
    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_height(top);

    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultBase58Addr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();

    if (isNeedAgent_flag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultBase58Addr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }

    //MagicSingleton<TranMonitor>::GetInstance()->SetDoHandleTxStatus(outTx, ret);
    std::cout << "=====Transaction initiator:" << strFromAddr << std::endl;
    std::cout << "=====Transaction recipient:" << strToAddr << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl
              << std::endl;
}

void Auto_investment()
{

    std::cout << "input aummot: ";
    std::string aummot;
    std::cin >> aummot;

    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(addrs.begin(), addrs.end(), MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    if (it != addrs.end())
    {
        addrs.erase(it);
    }

    int i = 0;
    while (i < addrs.size())
    {
        std::string from;
        std::string to;
        from = addrs[i];
        if ((i + 1) >= addrs.size())
        {
            i = 0;
        }
        else
        {
            i += 1;
        }

        to = addrs[i];

        if (from != "")
        {
            if (!MagicSingleton<AccountManager>::GetInstance()->IsExist(from))
            {
                DEBUGLOG("Illegal account.");
                return;
            }
        }
        else
        {
            DEBUGLOG("Illegal account. from base58addr is null !");
            return;
        }
        std::thread th(TestCreateInvestment, from, to, aummot);
        th.detach();
        if (i == 0)
        {
            return;
        }
        sleep(1);
    }
}

void print_verify_node()
{
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();

    vector<Node> result_node;
    for (const auto &node : nodelist)
    {
        int ret = VerifyBonusAddr(node.base58address);

        int64_t stake_time = ca_algorithm::GetPledgeTimeByAddr(node.base58address, global::ca::StakeType::kStakeType_Node);
        if (stake_time > 0 && ret == 0)
        {
            result_node.push_back(node);
        }
    }

    std::string fileName = "verify_node.txt";
    std::ofstream filestream;
    filestream.open(fileName);
    if (!filestream)
    {
        std::cout << "Open file failed!" << std::endl;
        return;
    }

    filestream << "------------------------------------------------------------------------------------------------------------" << std::endl;
    for (auto &i : result_node)
    {
        filestream
            << "  base58(" << i.base58address << ")"
            << std::endl;
    }
    filestream << "------------------------------------------------------------------------------------------------------------" << std::endl;
    filestream << "PeerNode size is: " << result_node.size() << std::endl;
}

void print_block_cache()
{
    std::cout << "input height :";
    int height;
    std::cin >> height;
    std::map<uint64_t, std::set<CBlock, CBlockCompare>> _cache;
    MagicSingleton<CBlockCache>::GetInstance()->GetCache(_cache);
    auto iter = _cache.begin();
    for (; iter != _cache.end(); ++iter)
    {
        if (iter->first == height)
        {
            for (auto block : iter->second)
            {
                std::cout << block.hash() << std::endl;
            }
        }
    }
}

void GetRewardAmount()
{
    int64_t start_height = 0;
    int64_t end_height = 0;
    std::string addr;
    std::cout << "Please input start height:";
    std::cin >> start_height;
    std::cout << "Please input end height:";
    std::cin >> end_height;
    if(end_height < start_height)
    {
        std::cout<< "input invalid" << std::endl;
        return ;
    } 
    std::cout << "Please input the base58address:";
    std::cin >> addr;
    
    if(!CheckBase58Addr(addr))
    {
        std::cout<< "Input addr error!" <<std::endl;
        return ; 
    }
    DBReader db_reader;
 
    uint64_t tx_totall = 0;
    uint64_t claim_amount=0;
    for(int64_t i = start_height; i <= end_height; ++i)
    {
        std::vector<std::string> block_hashs;
        if(DBStatus::DB_SUCCESS != db_reader.GetBlockHashsByBlockHeight(i,block_hashs)) 
        {
            ERRORLOG("(get_tx_hash_by_height) GETBlockHashsByBlockHeight  Failed!!");
            return;
        }
        std::vector<CBlock> blocks;
        for(auto &blockhash : block_hashs)
        {
            std::string blockstr;
            if(DBStatus::DB_SUCCESS !=   db_reader.GetBlockByBlockHash(blockhash,blockstr)) 
            {
            ERRORLOG("(GetBlockByBlockHash) GetBlockByBlockHash Failed!!");
            return;
            }
            CBlock block;
            block.ParseFromString(blockstr);
            blocks.push_back(block);
        }
            std::sort(blocks.begin(),blocks.end(),[](CBlock &a,CBlock &b){
                return a.time()<b.time();
            });
             
                for(auto & block :blocks)
                {
                   time_t s =(time_t)(block.time()/1000000);
                   struct tm * gm_date;
                   gm_date = localtime(&s);
                   cout<< gm_date->tm_year + 1900 << "-" << gm_date->tm_mon + 1 << "-" << gm_date->tm_mday << " "  << gm_date->tm_hour << ":" << gm_date->tm_min << ":" << gm_date->tm_sec << "(" << time << ")"<< std::endl;
                for(auto tx : block.txs())
                {
                    if((global::ca::TxType)tx.txtype() == global::ca::TxType::kTxTypeBonus )
                    {
                    std::map< std::string,uint64_t> kmap;
                    try {
                        nlohmann::json data_json = nlohmann::json::parse(tx.data());
                        nlohmann::json tx_info = data_json["TxInfo"].get<nlohmann::json>();
                        claim_amount = tx_info["BonusAmount"].get<uint64_t>();
                        }
                        catch(...)
                        {
                            ERRORLOG(RED "JSON failed to parse data field!" RESET);
                            
                        }                    
                            for(auto & owner : tx.utxo().owner())   
                            {
                                
                            if(owner != addr)
                            {
                            for(auto &vout : tx.utxo().vout())
                            { 
                                if(vout.addr() != owner && vout.addr() != "VirtualBurnGas")
                                {
                                {
                                 kmap[vout.addr()]=vout.value();
                                 tx_totall += vout.value();
                                }
                                }
                            }
                            }

                            }
                            
                            
                    for(auto it = kmap.begin(); it != kmap.end();++it)
                    {
                            std::cout << "reward addr:" << it->first << "reward amount" << it->second <<endl;   
                    }
                    if(claim_amount!=0)
                    {
                        std::cout << "self node reward addr:" << addr <<"self node reward amount:" << claim_amount-tx_totall; 
                        std::cout << "total reward amount"<< claim_amount;
                    }
                    }
                }
                }   
        }

    

}


 