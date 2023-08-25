#include "ca_AdvancedMenu.h"

#include <cstdint>
#include <ostream>
#include <string>
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
#include "utils/tmplog.h"


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
    if (net_com::input_send_one_message() == 0){
        DEBUGLOG("send one msg Succ.");
    }else{
        DEBUGLOG("send one msg Fail.");
    }
        
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
    std::stringstream ss;
    ss << message << "_" << std::to_string(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());

    EchoReq echoReq;
    echoReq.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
    echoReq.set_message(ss.str());
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

void evmAddrToBase58()
{
    std::string strInput;
    std::cout << "Please md160 addr:" << std::endl;
    std::cin >> strInput;
    if (strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

    std::string base58_addr = evm_utils::EvmAddrToBase58(strInput);


    std::cout << base58_addr << std::endl;
}

void generateEvmAddr()
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


    std::string addr = evm_utils::EvmAddrToChecksum(evm_utils::generateEvmAddr(strToAddr+strTxHash));


    std::cout << addr << std::endl;
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

    //Check whether the Genesis account is in the address list
    std::vector<std::string> base58_list;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(base58_list);
    if(std::find(base58_list.begin(), base58_list.end(), global::ca::kInitAccountBase58Addr) == base58_list.end())
    {
        std::cout << "The Genesis account is not in the node list !" << std::endl;
        return -2;
    }

    std::map<std::string,uint64_t> addr_value =
    {
        {"12u85eP4Kd6NPwA1btZY2rZk8BQZvPZ7LJ",11000},
{"16hCNpdxF94Mm3QDZ1F9AXS8UPjaE9AvXB",11000},
{"18oM7XVJuHQCKwrQq8vs3habQ5dJoQmSva",11000},
{"16Z5ALbkeYMh5gCES8HFoSC14kzHpvJqpq",11000},
{"1w6jWxBjgGiw4h9x6G9BsCGkEgR5vZo8W",11000},
{"1GyPLVwWDtKnYN4eP3UfKQhMUZyzSAUom3",11000},
{"156ctQvkTqS23BwrCZ7pXpZELk7u7JmJBF",11000},
{"122q7Erg1bcrexAjNU6SyB3ZiCyE4G79yt",11000},
{"1DBZbzUHYbdZZ7csZ5DRtLpherYo3JBK3E",11000},
{"1AhKA4UZEvZPj8XDC5NFi13cS91pKyuw3P",11000},
{"1E5nfVSZGEecAbfiyhDjPLHxnkpMtzMuRu",11000},
{"1LiDzQ8tKjBbBMdYWHEn37czVQD65yXQy3",11000},
{"1NjgWZCzaz22kKj6NLerYhotv5BUsS3mkZ",11000},
{"1K531keawuuECY8g5E12wGxzs9LWjrQWZc",11000},
{"16tuYNcascTvKfUR3ENNLE9EuZDX7DKocd",11000},
{"1AhuZC8QEF1YpkJKEdSJt34JcxLvXocwDm",11000},
{"1CyUoFGRMWoHQuw4ufRLXqKEeB8aUM2MkF",11000},
{"1P1MmDxzUnFHffCZvgyowYbfFN84wAPhuZ",11000},
{"178gmqQdoYtx2dY6myaqz8KFCn9EP6VuoV",11000},
{"1B6qci2h5717p72n2PEDm2yELjDjPthihK",11000},
{"1GVco1eXRwtjjZbT4qDuyDAsSiJGMtkUiY",11000},
{"1CwUfVAxT4EHLuTUaTS7A2jErjhPRfrtqN",11000},
{"16pF1KiSX61Dgt5JtkCzZg6ioqTpqXRbgz",11000},
{"127UrEKXvrPCiCFhyoxDBpWBAQCY83uN4b",11000},
{"13EgBSQKFYZGB8DcejxgcY7QSkFgcrZDSf",11000},
{"13cyQwgzdo2HhAqP1adRVgZh4sd125ehHt",11000},
{"12Hiko2c1UKtP6vRxNWJ2USpLiy1YSDuxo",11000},
{"13woAE1ALKRcEGQ5U11eudCkqCsBHLgc5M",11000},
{"14dVkX6d1dMbLRJNjCGXXitjZZ5h5T726z",11000},
{"18hXG5nyWnMPPnzEhusFQcGxzEnSfTGahn",11000},
{"14FLEXVvPZ5aZSVZcawr9wj2QfzDaNWAme",11000},
{"19A8eq4oWxjdoL7xtreuJoMvwSWrGDED4b",11000},
{"14KXJsZibiPYSc2XQXGQscWd49aur9YkAG",11000},
{"19WFBXw6JuwnfjjUgPWbgupYYYMs2S7Lqy",11000},
{"14PQVCepWpQ1dcfMLoyNYfTBs67vAtoZTb",11000},
{"1ANwkbyrXnqSNiwZd9UFxLyAmqxMWhU9m9",11000},
{"1539VtN5V2ipGRCNCVVsQjvAAvbip4UapD",11000},
{"1AUkq7cXFtCqjoX9NvbAJ4GS3bhWyzZuwk",11000},
{"164ai1J9vSbLFGytx1gZmTn3qiC3aDATMk",11000},
{"16aFNhQf6hCvUFocXPKH8GnDDp4b6F5ukq",11000},
{"16q7ajCKtLG6FyFN5ciPUvK6YmEDkKvxLL",11000},
{"177uvL6GYNs7gsws2o13HGfeftpQfsUwit",11000},
{"17ejqTV3PjSXR3KVmZamfaox2FB5jg4Sc2",11000},
{"17fYyyHk6mFy92WieXMDVXzvk1x1ksGmRf",11000},
{"1E82Jaz89gVABKGqs3Pn7rKBcCCyjwxyvF",11000},
{"17n3Q6DyC8dXTFojKQ8PYxPzKAEAmCXZpo",11000},
{"1ETLYLT66TNb54UwxH3hRfyxeDf7u6iQHi",11000},
{"186RrLPg7DkwrPbVLeL2j3MpvqNVn1bAzH",11000},
{"1FcTKypjsSyiCrxBiBocKsQsy1ucJANqKy",11000},
{"18uyNbbQAJDGHnStNfMTnzMeg7TMpVdcso",11000},
{"197w59A23K1ZChTWnHowSLzUfz9FUo164R",11000},
{"1KPdcT36MqJnjrjjUTSA6WhzqCMzEERcAs",11000},
{"19vwRb565qe8x5kuavxxkUqUZc3zGZdzL8",11000},
{"1MV2T6p9AFcrAb8jnothJwcDT9PighWMX1",11000},
{"1MsWV5VWEXEuXKa9Ma3zXYMbwR4WH1AAmS",11000},
{"1Aie8HmxsXQxVXes5bJ6phu1zMRQKkjhKS",11000},
{"1AmTVsnFfi4GHNKWKpniaGWP9jH1hxUAwF",11000},
{"1NSyG35kgR6ZbxHnqPBcoXN2toAoXgZ4LW",11000},
{"1AQiyZXZrYsLe5MszQMiQaFZxGzpjBKfCg",11000},
{"1jvpxsXgQpDkEJifbiYw2J3M1xeNTV3By",11000},
{"1AZXE1XaCxDXcCSCsaBD35DLAykXwEXGtU",11000},
{"1B2pHegoTXTNu8nMFKYUroFaJBn2K5TEiW",11000},
{"1B9oXEqMeX5dTvyyiEC29a4W97g1azQnAr",11000},
{"1BHAmbTgCApUWyxVPn4CQa3Xnz7sfquvk1",11000},
{"1Bv52v6Fw7bRcbqgqiXspU9zthwcrL9UcW",11000},
{"1BxiGEV4fa1NhMcFhqKDTWzVBnpPaFaR3H",11000},
{"1BxZZhA93BL6EWBoEA9T9XX2m9YgMLiirC",11000},
{"1Ce3ZjRW9ktEo9Q9BEL9WEvNECbWgzBAQn",11000},
{"15QannrnHMqR6UZsjGx4EsrC743A8rBbza",11000},
{"1DS9nxYVCMD7kj3mamwys1Kr8d94A8Yjde",11000},
{"1E2xrgUkxp25dPuryFA3BHezFNNuBG6hX3",11000},
{"1EEXbLcAXrPBWP85RnLwfpe6j8MNfncp6b",11000},
{"17zTS8iK64HzkKy9qUHXgYSxT27FNQhABB",11000},
{"1EfrxA82K81L3u4rYWRezVudF68AwwJRWW",11000},
{"1MR2uVW9ZmLoXqUXJkn6Ugf6modFHpVrS3",11000},
{"15gTa5j7CKZVSVLAfsudeDY6oWb7AfunGr",11000},
{"1NGJp7cmMj33Udk4dYDfXPCwzfQfKmKiRy",11000},
{"1EHPSo6LGmSeSxmqx5v82o8EYSpk6yC2Kw",11000},
{"1M6iEr78rcBR8JjxMGbZ5q5arNHhtyVxW1",11000},
{"134b5AHqGpT6DmrHssiuBWySkBEt8uQ74h",11000},
{"1JDYnkr8x8kQgxyKnuwWk7o8yZwQ6eUtt4",11000},
{"1GLseeESXE8CJqNP5ydGsquXN7Qq2YxGwF",11000},
{"1Gvyq2qFgkVfiA5jEb9fmssg9Rnb8GdkWX",11000},
{"1DW4y1iX16Mok5usp1gco8ZZ9SxS8o5Prr",11000},
{"1G36gQ291aBqTcufeRUrBTznP7Fz6ijAdi",11000},
{"1HKoSEy9aogGk14PJL2PPmB94dy3Pz5CtR",11000},
{"1Fb7aeSxAuyzJW1WoGZCo3LWp59eikadav",11000},
{"1NDxNkscnHDFamtYeFNNSYhYsEguDdxLWk",11000},
{"1QBrN93p7JXCPpdtEpU66mcfiWEJghLB5v",11000},
{"1DJ86UGm4jRozPNXrpKXQDtUSmKWnhwW2U",11000},
{"1KC1pyW8YSGF1pTUmfFmHAgy1rKEe1XThD",11000},
{"1KUHxfyfc7sHxSE6ZnJB1cEqTWqvpB8b3c",11000},
{"1KvW2gS34MTSUJ2DAeEBxJNVr61ZrD6Bmy",11000},
{"1MZ3W4UaR8nQqhMckf3xveUJAGunvr1qs5",11000},
{"1N7MhG61y3eLANLY9VSQ3CtaKV9NUC9RB7",11000},
{"1HYn9c3FefchcDDqki6WTFSaFM8qEhKnhB",11000},
{"1NEzai1ot46MRztiw1RKfnrsPJ8bwhgEps",11000},
{"1NJKxboLeFVEGgxDZyZcSAJ2ettLfRurue",11000},
{"1PLTj3RpxhfUFk5rGC1hRqTafrk9tvf6sa",11000},
{"12DqHdKJnVsoA7pSfwWaz8EMguqP2AX5tc",11000},
{"141s7evrcnBULuQbSPFYycsFcrMf2RZys1",11000},
{"145hthEbZWv56qpdxLCXvd51aUG1bUQSUB",11000},
{"167FkujkDru1UstE5eT7aNk2vfUEwN3ZVh",11000},
{"19F2MbfCT5DFoHNeGyN1shuXrcU9wvou5k",11000},
{"1B4oeWMQAk4ty2t2FCifedb21N4WG6Cn4o",11000},
{"1PjE9WazBd8f9rCXvkjyGDQ8bznzujBvJQ",11000},
{"14P9s65WLCSag8Bgw77T3UnAeEox3YncWR",11000},
{"134YJkgQrnTivb8Qn1ekAYWXnnmLNSwuJL",11000},
{"1AfwPgJWdfMivbBtcckVtjLeP52PaqgrF4",11000},
{"1AnSMArzacpLFCwriy2ZY3bAu8Ysi4a1z2",11000},
{"1CEcRrPV6FEK7ngDqfyvvZKxVonKMZtv82",11000},
{"1F36o1nyjzvuVk4EFNGLaYXbaA96Uk5EvL",11000},
{"1LnEc9bWLSDEAnw59sxHiyRRpsbjuvgd1z",11000},
{"1MLg9XFs8Gu4ZQaoEe4zRUWTyXibLgL5Hi",11000},
{"16ARNbmtqgyt7N6UgcbLqfqGN8FoDeTkYt",11000},
{"16iAd4spDyMMoS1tfoPgnt52bxhYdH5Wae",11000},
{"17enPcq4ef7Bqu1eKZpDiD66daddNnvJe6",11000},
{"1EjpybxPL5T2dvE7JnRcgvimT9FLQ3VCDH",11000},
{"12GuJ9FtcpXdX5EkUCsQA4FHwgiH3s1X5H",11000},
{"15q5TwcopH5FV4rBKybKsNULN3fpsxgUPa",11000},
{"1GQnEbEnbkrgSg2mMqzv2DGPMEgJHtcihu",11000},
{"1N4akFjmo4DvA9uKEGzbou94ZWsyunZi78",11000},
{"17hZU4AGKRUfodVj9ZMo5Kr3oxDojtTs7f",11000},
{"1818hFhhRED76sf6H1bqLANZRrcz7BZzJH",11000},
{"19qAy8D5XJJ9ag92Qsozxfm9aD9nu7XtZY",11000},
{"1PUT79Dq9k2i4zrnDuUWFPGdNzmnLanmFs",11000},
{"16F58JQXfjwG4ADDm78a5WaB8AmbB11L5V",11000},
{"18yNpiJq6Jag8EB9FmEF15ae4XEcSktVTS",11000},
{"19cbwSQXJ1jU58WsaHZ5YNahxf9r48aUuB",11000},
{"1B1wbY9ozppm5o3ZqdQb91TEj17ADHkZvJ",11000},
{"1CXEDDHz4C87oUp9muZQeahLJnBB8BkG9J",11000},
{"1Jtmo85Lbdd8rtfpfEHDNecRFF4i6DAzsU",11000},
{"157FwHkeJkMreCPJtphVAVua9fpr1zG3iM",11000},
{"15staEsUjVSx4nR8XUvdkbgrddJgAYtBTe",11000},
{"167SxvzwWJBpufGsd7JbTrqMHGW5QBsn3v",11000},
{"169PEZd4Scw5RRB9cysnJHzGCjncrCESnN",11000},
{"17XGQK1SnXiYzr9Xmp2CLRCY9YZWNRkhNb",11000},
{"18ijeniFNafhkBFf69J5gLLJN62yP7yuVu",11000},
{"1ACT3a177B9gw7FQMWHLizSYdMM8ZN9asy",11000},
{"1AHFpKEJXQhQd8PYxfNL24LLNGgoGufeH6",11000},
{"1Ba3zhzPc4kobAikKFUvXSBAFLAXoXqrzN",11000},
{"1Co3XYAffT29bJqhZ4izDuYGhwHz4i5fhj",11000},
{"1KgxmFvEcsg1fzoM3YvTfceqRfJZEytgAz",11000},
{"1LGDt4iezd4fhHWtALUZhfKPnCYdUiWiSi",11000},
{"1PzhJe5zbi7X7MCPaHi9aFqbFE498eKDGy",11000},
{"1Pxc1hvoDguffu9rJU6e76ZgceSrgJfKrL",11000},
{"1QEqhMiqmAtvu2kiGnUC4jz9NEBFYmMtfA",11000},
{"1AMJaWwcVp7tqCwgTL4Bt9ed37nqNqSe3f",11000},
{"1GoshyMJ9GwPjcz5MREfs5w1yWVwS8dV33",11000},
{"15cNv5FRhyC2qMGHWri7kSe27tY7jxG8PN",11000},
{"16J1xFYdwZMnhZV4b5D8vqVK9Zg4w9ApMd",11000},
{"17st4yEMBEyPFXoq9fhDyQjCVSvX5Vic8D",11000},
{"19UEm1oSz2MBuzZiMhUHbCPx55ruNuopKv",11000},
{"1LiVZ9yzuC8sDUidRgWdFNyUj3D9XN2ean",11000},
{"12AqaisLxPiwVEnLGmUzhtU9pmY8REUy8z",11000},
{"13VfDcqskzh79FctagngjRYgxdVkAHDAkL",11000},
{"15meAw8oY331vX3aR6J42Gu2Q6wpRLFurN",11000},
{"18GgjWPPyc13xN47f2jXqaiZF8pxaTWRbF",11000},
{"18funjm4jc5cMJLnrBdYwJTSW9J9tgFMyd",11000},
{"1EP9szCHHZV9EqiqWesQM6CRWFR9a5HSYT",11000},
{"1KGkWaHvKUEeWd9kvyw9NQJP1iW8K6ypqV",11000},
{"1KUMpSSsZdXvc6hn8ZbWpmwCjbuo97Y266",11000},
{"1Kd89vyEasL9K85Y8Nu917WXqMWwJnsadt",11000},
{"1LA9y2NMUrCwiq2BSbvaCYFFpbiUTEA5a9",11000},
{"1kwronToBoXvvU3R4GmeVVx1bkvAF6Qjm",11000},
{"15z8CAU5gY2C87ia1pdh1fH4QBPsjot7E2",11000},
{"123ouHPfkx1kfRpqDVquwKbzGeUMuuqsHG",11000},
{"12P6Qxae46kFzAS8cEo5vLLjFo7xztXuRJ",11000},
{"12fYj9z8krHxVz8Ligga2GH13sQihVCdm7",11000},
{"13JocapVGDXHSK1zayzLAu1mY67kfAgXLv",11000},
{"13MQ7gmA55gYSos6teguwJkraGEamQwAZh",11000},
{"14GKR4tTc9PorMkmmWQ5NCmMU9TENF8E4N",11000},
{"159TNWoJQGPQrNaTF2yXr2u7iJFSFKZPWo",11000},
{"16Vw2yrqHY7DMLD6f9xRkMGspQYG7PPu7j",11000},
{"16YdMiHdF7qY3EQXc1zyqMcmXeEhpmZugH",11000},
{"17k1BnxYQUVPRhkpWZCSBorkQe9rgDgbij",11000},
{"17rmdqxEmAgLFcmPSR5Ag1wofa1aTQWFpx",11000},
{"1977piKCvvBFJxR9nrQmRD5zWvDAbYJts6",11000},
{"19VUVEYHYhSyQaBoATuUKKYXZkL9ShiCMX",11000},
{"19jtGaPiM1i7MWLJEwjDsVhks6AVto8X2M",11000},
{"1Dwk8hnovPGYzTxhLe13g7HW2YhFiKC8sD",11000},
{"1EgH9ysxmMUPUiC3aa1D1sguBNosA7idqZ",11000},
{"1FCrQnGfaymu8DtgdVLCTiA6uvKjFMDDpS",11000},
{"1FL5iXBYiB7rrzbZTE9miJTjmkMa1unB6y",11000},
{"1HYGe9LwNAcQ1uBzWSUhnLwAg3tRq8QBKs",11000},
{"1Hvf7SoMn24h8z6RyA8h6HgPAHHvD74KSj",11000},
{"1J5LgtkEsfyKFvqL7YkdvgabEx73M8atQ4",11000},
{"1Jc8GKxMQbZESejB3RVDa9i3SAaKNgztVi",11000},
{"1JyVo4FaVBoqAKhK6amJv1QqswzvoUtoy4",11000},
{"1Lr2x7L6hhHmPdeueinLQt8ZsKbUNMnqXQ",11000},
{"1MXJHzEnei1TFMwWFQGzv5oTVnuDNCkGYM",11000},
{"1PHYcQUYq5mxxipxHeiCVA19HCMXb1PVCj",11000},
{"1SKpzyMJ6SKaMqKrdFa1iLk85iY6x4BM7",11000},
{"1PhfueW4F4Ywn6MYctbUH1ScpanbDQC9Zm",11000},
{"1AMH7LY6fky5TBRfiYjG2b3hpyV6VFQeUX",11000},
{"1Gw3C7yvLtXTQjorpBULLuPESUawPzLcJv",11000},
{"18eiKdJ8XpvfZfhzJzpDuNvb82BtnDvLME",11000},
{"15JSZvsbyPPBFyzRsrsG4uRJWgD4nKpxuv",11000},
{"1GrY4H1im2QVbyTZPdABTTxbekHJA9YMzS",11000},
{"1MSqsQ7fKqVgPVYSDGcg9iHbs9zjjqe3Uf",11000},
{"1DHuTpYQ3uj8VcoFJizVriAWqwioqCUmvL",11000},
{"1HrCWNuvKN2CFHJc3cYatzEzwqbWGZdjGo",11000},
{"1FX8jQcLjZ5QT6Grs6pQ7e2dDiLVKhhdoD",11000},
{"18gDVThNoTHE85JJ9jHcxT5ff83X6PZAgJ",11000},
{"1KkvAJV96iMXNczaF7VSLEZqz9m3yGxuad",11000},
{"14z6ZhoXA65JV6GSSKUDihompqqLGL9XfB",11000},
{"1NoTT4BD78H6zPjCHru9usJu1u5GgmwwFq",11000},
{"145mgdgJvMhUDQu5axgZ9yh5PkE9C435S9",11000},
{"19pcC9beqYLvVci2eWHR1bXsZzP83gsZ5G",11000},
{"1BWjxb6sKPkmoXu48qA5TZLeZv8ieqnHk8",11000},
{"1HtF6AbMEFte5x8LwufexSRD9NNAKnkaga",11000},
{"18KubKseuWBfSGrzbgbp9Mn6tiAsENiTnW",11000},
{"1J1VfmwM53quMQqLPbLWMA26QzQm9zPTBS",11000},
{"18cNdf1kUDSGxsGF8jcuwabUyQFBvtkTin",11000},
{"17nCtX5MCBYVwfgucPTeSeyNcq2nsXUwmR",11000},
{"15MsLuunKRuWt9VHcJdLMzV66bJv3F1dbe",11000},
{"1KbpoiLpmHQcy7juNmjkQFwRdywHp7rFrA",11000},
{"1AC7LhsB8kt84FRvpsn8kU3fd7UYJ44Yku",11000},
{"1EjdKxcjeBiYe842R8CtPkYVn7D2xEj3V8",11000},
{"1GEFLd7NxEYAFpHBcXntJiuUU5MK963L2g",11000},
{"1PKaASFmeZuPQWBNcqrGyNKrNj15XvgECq",11000},
{"13FAh7T58jhiSayFH4pErqq3QMF5iCK8uM",11000},
{"14dfVKuwTm5sNLssG8nnqadzHFox9DcXM5",11000},
{"1CwsqnwhDV3j5cR6d8xsxZ5xNfND3ip3PW",11000},
{"1BF2zjJBUc3xgZ9q1Ed93gspp9tdvis8fX",11000},
{"1BkMKqjiufgXkrFbNEYFUpbKoSiLbvUDZL",11000},
{"13bZSE36m1CYQAE49X7M14HWx3ygaGPo2C",11000},
{"14gVFeSPUZYUhseb6EHW7pLFeKyUKDwJe6",11000},
{"12oD2zNMzmW9F9i2x1TDRBjKiYWfLvxQNs",11000},
{"1GXazUPXDwS3merbdMDGc7oj92C58RvAVz",11000},
{"1NRYv3LjnKif8B59oQTGg1trSFAxXqJTmq",11000},
{"1Lb5BRnFoadifqRyrwe7fZrdZyMeGPvbG5",11000},
{"18CbFRTYLRPpU8MJqDRGPBjv8dk8xpbnuH",11000},
{"1BJEHjppW76FHiTwr4uKVYEpp1m6FH5eK7",11000},
{"1PFwEPx6fjRmdihUpb6pPeY8fzcBD58Cuh",11000},
{"12x9jgAgs7FAiFGX1UoUwNNLGrWJEKJ9yR",11000},
{"195Me9w2oAGjfzwqAdo8f7tvCvVhQhkrnv",11000},
    };

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
        txout->set_value(67392763 * global::ca::kDecimalNum);
        txout->set_addr(addr);

        for(auto & obj : addr_value)
        {
            CTxOutput *new_txout = utxo->add_vout();
            new_txout->set_value((obj.second + 1)* global::ca::kDecimalNum);
            new_txout->set_addr(obj.first);
        }
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

void get_tx_hash_by_height(int64_t start_,int64_t end_,std::ofstream& filestream)
{
    int64_t start = start_;
    int64_t end = end_;

    // std::cout << "Please input start height:";
    // std::cin >> start;

    // std::cout << "Please input end height:";
    // std::cin >> end;

    if (end < start)
    {
        std::cout << "input invalid" << std::endl;
        return;
    }
    

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
        filestream  << "height: " << i << " block: " << tmp_block_hashs.size() << " tx: " << tx_hash_count  << std::endl;
    }

    filestream  << "block sum " << block_total  << std::endl;
    filestream  << "tx sum " << tx_total   << std::endl;
    //debugL("..............");
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


void tps_count(){
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
    std::string StartW = std::to_string(start);
    std::string EndW = std::to_string(end);
    std::string fileName =  "TPS_INFO_" +StartW +"_"+ EndW+".txt";
    std::ofstream filestream;
    filestream.open(fileName,std::ios::app);
    for(int i = start;i < end;){
        get_tx_hash_by_height(i,i+100,filestream);
        i+=100;
    }
     
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

    for (int i = 0; i < addrs.size(); ++i)
    {
        std::thread th(TestCreateStake_2, addrs[i]);
        th.detach();
    }
}

void TestCreateInvestment(const std::string &strFromAddr, const std::string &strToAddr, const std::string &amountStr)
{
    TxHelper::InvestType investType = TxHelper::InvestType::kInvestType_NetLicence;
    uint64_t invest_amount = std::stod(amountStr) * global::ca::kDecimalNum;

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
//    std::cout << "input height :";
//    int height;
//    std::cin >> height;
//    std::map<uint64_t, std::set<CBlock, CBlockCompare>> _cache;
//    MagicSingleton<CBlockCache>::GetInstance()->GetCache(_cache);
//    auto iter = _cache.begin();
//    for (; iter != _cache.end(); ++iter)
//    {
//        if (iter->first == height)
//        {
//            for (auto block : iter->second)
//            {
//                std::cout << block.hash() << std::endl;
//            }
//        }
//    }
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
    std::cout << "Please input the base58address：";
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
                            try 
                            {
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
                                            kmap[vout.addr()]=vout.value();
                                            tx_totall += vout.value();
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

void tests_handle_invest()
{
    std::cout << std::endl
              << std::endl;
    std::cout << "AddrList:" << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.GetBase58();

    std::cout << "Please enter your addr:" << std::endl;
    std::cout << strFromAddr << std::endl;
    if (!CheckBase58Addr(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    // std::string strToAddr;
    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cout << strFromAddr << std::endl;
    if (!CheckBase58Addr(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strInvestFee = "10000";
    std::cout << "Please enter the amount to delegate:" << std::endl;
    std::cout << strInvestFee << std::endl;
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
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strFromAddr, invest_amount, top + 1,  investType, outTx, outVin,isNeedAgent_flag,info_);
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
