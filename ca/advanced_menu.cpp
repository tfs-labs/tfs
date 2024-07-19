#include "advanced_menu.h"

#include <map>
#include <regex>
#include <thread>
#include <ostream>
#include <fstream>

#include "ca/ca.h"
#include "ca/test.h"
#include "ca/global.h"
#include "ca/contract.h"
#include "ca/txhelper.h"
#include "ca/interface.h"
#include "ca/algorithm.h"

#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/block_monitor.h"

#include "net/api.h"
#include "net/peer_node.h"

#include "include/scope_guard.h"
#include "include/logging.h"

#include "utils/tmp_log.h"
#include "utils/console.h"
#include "utils/time_util.h"
#include "utils/contract_utils.h"
#include "utils/tfs_bench_mark.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/base64.h"

#include "db/db_api.h"
#include "ca/evm/evm_manager.h"
#include "openssl/rand.h"

struct contractJob{
    std::string fromAddr;
    std::string deployer;
    std::string contractAddresses;
    std::string arg;
    std::string tip;
    std::string money;
};

std::vector<contractJob> jobs;
std::atomic<int> jobs_index=0;
std::atomic<int> perrnode_index=0;
boost::threadpool::pool test_pool;

void ReadContract_json(const std::string & file_name){
    std::ifstream file(file_name);
    std::stringstream buffer;  
    buffer << file.rdbuf();  
    std::string contents(buffer.str());
    if(contents.empty()){
        errorL("no data");
        return;
    }
    nlohmann::json jsonboj;
    try {
       jsonboj=nlohmann::json::parse(contents);
    } catch (std::exception & e) {
       errorL(e.what());
       return;
    }

    if(!jsonboj.is_array()){
        errorL("not a array");
       return;
    }
    try{
       for(auto &aitem:jsonboj){
            contractJob job;
            job.deployer=aitem["deployer"];
            job.contractAddresses=aitem["contractAddresses"];
            job.arg=aitem["arg"];
            job.money=aitem["money"];
            jobs.push_back(job);
       }
    }catch(std::exception & e){
        errorL("wath:%s",e.what());
       return;
    }
}

void ContrackInvke(contractJob job){
    

    infoL("fromAddr:%s",job.fromAddr);
    infoL("deployer:%s",job.deployer);
    infoL("deployutxo:%s",job.contractAddresses);
    infoL("money:%s",job.money);
    infoL("arg:%s",job.arg);

    std::string strFromAddr=job.fromAddr;

    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }
    DBReader dataReader;

    std::string strToAddr=job.deployer;
    if(!isValidAddress(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;        
    }

    std::string _contractAddresses=job.contractAddresses;
    std::string strInput=job.arg;
    if(strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

    std::string contractTipStr="0";
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(contractTipStr, pattern))
    {
        std::cout << "input contract tip error ! " << std::endl;
        return;
    }

    std::string contractTransferStr=job.money;
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
//    CTransaction tx;
//    std::string txRaw;
//    if (DBStatus::DB_SUCCESS != dataReader.GetTransactionByHash(strTxHash, txRaw))
//    {
//        ERRORLOG("get contract transaction failed!!");
//        return ;
//    }
//    if(!tx.ParseFromString(txRaw))
//    {
//        ERRORLOG("contract transaction parse failed!!");
//        return ;
//    }
//
//
//    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
//    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
//    int vmType = txInfo[Evmone::contractVirtualMachineKeyName].get<int>();
 
    int ret = 0;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<std::string> dirtyContract;
    std::string encodedInfo = "";
    ret = TxHelper::CreateEvmCallContractTransaction(strFromAddr, strToAddr, strInput,encodedInfo,top + 1,
                                                     outTx, isNeedAgentFlag, info, contractTip, contractTransfer,
                                                     dirtyContract, _contractAddresses);
//    if (vmType == global::ca::VmType::EVM)
//    {
        //const std::string& contractAddress = evm_utils::GenerateContractAddr(strToAddr + strTxHash);
        //ret = TxHelper::CreateEvmCallContractTransaction(strFromAddr, strToAddr, strTxHash, strInput, top + 1,
         //                                                outTx, isNeedAgentFlag, info, contractTip, contractTransfer,
        //                                                 dirtyContract, contractAddress);
        //if(ret != 0)
        //{
        //    ERRORLOG("Create call contract transaction failed! ret:{}", ret);
       //     return;
       // }
//    }
//    else
//    {
//        return;
//    }

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

    if(isNeedAgentFlag== TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg->mutable_vrfinfo();
        newInfo -> CopyFrom(info);

    }
 
    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        ret = DropCallShippingTx(msg,outTx);
        MagicSingleton<BlockMonitor>::GetInstance()->addDropshippingTxVec(outTx.hash());
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}


void test_contact_thread(){
    ReadContract_json("contract.json");
    std::vector<std::string> acccountlist;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(acccountlist);
   
    jobs_index=0;
    perrnode_index=0;
    int time_s;
    std::cout << "time_s:" ;
    std::cin >>  time_s;
    std::cout << "second:";
    long long _second;
    std::cin >> _second;
    std::cout << "hom much:";
    long long _much;
    std::cin >> _much;
    int oneSecond=0;
    while(time_s){
        oneSecond++;
        std::cout <<"hhh h" << std::endl;
        jobs[jobs_index].fromAddr=acccountlist[perrnode_index];
        test_pool.schedule(boost::bind(ContrackInvke, jobs[jobs_index]));
        std::thread th=std::thread(ContrackInvke,jobs[jobs_index]);
        th.detach();
        jobs_index=++jobs_index%jobs.size();
        perrnode_index=++perrnode_index%acccountlist.size();
        ::usleep(_second *1000 *1000 / _much);
        if(oneSecond == _much){
            time_s--;
            oneSecond=0;
        }
    }
}


void GenKey()
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
        Account acc(true);
        MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
        MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.GetAddr());
    }

    std::cout << "Successfully generated account " << std::endl;
}

void RollBack()
{
    MagicSingleton<BlockHelper>::GetInstance()->RollbackTest();
}

void GetStakeList()
{
    DBReader dbReader;
    std::vector<std::string> addResses;
    dbReader.GetStakeAddress(addResses);
    std::cout << "StakeList :" << std::endl;
    for (auto &it : addResses)
    {
        double timp = 0.0;
        ca_algorithm::GetCommissionPercentage(it, timp);
        std::cout << "addr: " << "0x"+it << "\tbonus pumping: " << timp << std::endl;
    }
}
int GetBonusAddrInfo()
{
    DBReader dbReader;
    std::vector<std::string> addResses;
    std::vector<std::string> bonusAddrs;
    dbReader.GetBonusaddr(bonusAddrs);
    for (auto &bonusAddr : bonusAddrs)
    {
        std::cout << YELLOW << "BonusAddr: " << addHexPrefix(bonusAddr) << RESET << std::endl;
        auto ret = dbReader.GetInvestAddrsByBonusAddr(bonusAddr, addResses);
        if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
        {
            return -1;
        }

        uint64_t sumInvestAmount = 0;
        std::cout << "InvestAddr:" << std::endl;
        for (auto &address : addResses)
        {
            std::cout << addHexPrefix(address) << std::endl;
            std::vector<std::string> utxos;
            ret = dbReader.GetBonusAddrInvestUtxosByBonusAddr(bonusAddr, address, utxos);
            if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
            {
                return -2;
            }

            uint64_t investAmount = 0;
            for (const auto &hash : utxos)
            {
                std::string txRaw;
                if (dbReader.GetTransactionByHash(hash, txRaw) != DBStatus::DB_SUCCESS)
                {
                    return -3;
                }
                CTransaction tx;
                if (!tx.ParseFromString(txRaw))
                {
                    return -4;
                }
                for (int i = 0; i < tx.utxo().vout_size(); i++)
                {
                    if (tx.utxo().vout(i).addr() == global::ca::kVirtualInvestAddr)
                    {
                        investAmount += tx.utxo().vout(i).value();
                        break;
                    }
                }
            }
            sumInvestAmount += investAmount;
        }
        std::cout << "total invest amount :" << sumInvestAmount << std::endl;
    }
    return 0;
}

#pragma region netMenu
void SendMessageToUser()
{
    if (net_com::SendOneMessageByInput() == 0){
        DEBUGLOG("send one msg Succ.");
    }else{
        DEBUGLOG("send one msg Fail.");
    }
        
}

void ShowMyKBucket()
{
    std::cout << "The K bucket is being displayed..." << std::endl;
    auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    MagicSingleton<PeerNode>::GetInstance()->Print(nodeList);
}

void KickOutNode()
{
    std::string id;
    std::cout << "input id:" << std::endl;
    std::cin >> id;
    MagicSingleton<PeerNode>::GetInstance()->DeleteNode(id);
    std::cout << "Kick out node succeed!" << std::endl;
}

void TestEcho()
{
    
    std::string message;
    std::cout << "please input message:" << std::endl;
    std::cin >> message;
    std::stringstream ss;
    ss << message << "_" << std::to_string(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());

    EchoReq echoReq;
    echoReq.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    echoReq.set_message(ss.str());
    bool isSucceed = net_com::BroadCastMessage(echoReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
    if (isSucceed == false)
    {
        ERRORLOG(":broadcast EchoReq failed!");
        return;
    }
}

void PrintReqAndAck()
{
    double total = .0f;
    std::cout << "------------------------------------------" << std::endl;
    for (auto &item : global::g_reqCntMap)
    {
        total += (double)item.second.second;
        std::cout.precision(3);
        std::cout << item.first << ": " << item.second.first << " size: " << (double)item.second.second / 1024 / 1024 << " MB" << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total: " << total / 1024 / 1024 << " MB" << std::endl;
}

void MenuBlockInfo()
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
            getTxBlockInfo(top);
            break;
       

        default:
            std::cout << "Invalid input." << std::endl;
            continue;
        }

        sleep(1);
    }
}

void getTxBlockInfo(uint64_t &top)
{
    auto amount = std::to_string(top);
    std::string inputStart, inputEnd;
    uint64_t start, end;

    std::cout << "amount: " << amount << std::endl;
    std::cout << "pleace input start: ";
    std::cin >> inputStart;
    if (inputStart == "a" || inputStart == "pa")
    {
        inputStart = "0";
        inputEnd = amount;
    }
    else
    {
        if (std::stoul(inputStart) > std::stoul(amount))
        {
            std::cout << "input > amount" << std::endl;
            return;
        }
        std::cout << "pleace input end: ";
        std::cin >> inputEnd;
        if (std::stoul(inputStart) < 0 || std::stoul(inputEnd) < 0)
        {
            std::cout << "params < 0!!" << std::endl;
            return;
        }
        if (std::stoul(inputStart) > std::stoul(inputEnd))
        {
            inputStart = inputEnd;
        }
        if (std::stoul(inputEnd) > std::stoul(amount))
        {
            inputEnd = std::to_string(top);
        }
    }
    start = std::stoul(inputStart);
    end = std::stoul(inputEnd);

    std::cout << "Print to screen[0] or file[1] ";
    uint64_t nType = 0;
    std::cin >> nType;
    if (nType == 0)
    {
        PrintRocksdb(start, end, true, std::cout);
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
        PrintRocksdb(start, end, true, filestream);
    }
}

void GenMnemonic()
{
    char out[1024 * 10] = {0};

    std::string mnemonic;
    Account defaultEd;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultEd);
    MagicSingleton<AccountManager>::GetInstance()->GetMnemonic(defaultEd.GetAddr(), mnemonic);
    std::cout << "mnemonic : " << mnemonic << std::endl;
    std::cout << "priStr : " << Str2Hex(defaultEd.GetPriStr()) << std::endl;
    std::cout << "pubStr : " << Str2Hex(defaultEd.GetPubStr()) << std::endl;

    std::cout << "input mnemonic:" << std::endl;
    std::string str;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
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

void PrintTxData()
{
    std::string hash;
    std::cout << "TX hash: ";
    std::cin >> hash;

    DBReader dbReader;

    CTransaction tx;
    std::string TxRaw;
    auto ret = dbReader.GetTransactionByHash(hash, TxRaw);
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

    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
    std::string data = dataJson.dump(4);
    std::cout << data << std::endl;
}


void MultiTx()
{
    std::ifstream fin;
	fin.open("toaddr.txt", std::ifstream::binary);
    if (!fin.is_open())
	{
		std::cout << "open file error" << std::endl;
		return;
	}

    std::vector<std::string> fromAddr;

    std::string addr;
    std::cout << "input fromaddr >:";
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    fromAddr.push_back(addr);

    std::vector<std::string> toAddrs;
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
        toAddrs.push_back(Addr);
    }


    uint32_t startCount = 0;
    uint32_t endCount = 0;
    std::cout << "please input start index>:" ;
    std::cin >> startCount;

    std::cout << "please input end index>:" ;
    std::cin >> endCount;

    for(uint32_t i = startCount ; i <= endCount ; i++)
    {
        toAddr.insert(std::make_pair(toAddrs[i],amt * global::ca::kDecimalNum));
    }


    fin.close();

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxMsgReq txMsg;
    TxHelper::vrfAgentType isNeedAgentFlag;
    CTransaction outTx;
    Vrf info;
    std::string encodedInfo;
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddr, encodedInfo, top + 1, outTx,isNeedAgentFlag,info, false);
    if (ret != 0)
	{
		ERRORLOG("CreateTxTransaction error!!");
		return;
	}


	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
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
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}", ret, outTx.hash());
}

void testaddr1()
{
    Account acc(true);
    MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
    MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.GetAddr());

    std::cout<<"addr:"<< addHexPrefix(acc.GetAddr()) << std::endl;
    if (!isValidAddress(acc.GetAddr()))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }
}

void testaddr2()
{    
    while(true)
    {
        std::cout << "isValidAddress: " << std::endl;
        std::string addr;
        std::cin>>addr;
        if (addr.substr(0, 2) == "0x") 
        {
            addr = addr.substr(2);
        }
        if (!isValidAddress(addr))
        {
            std::cout << "Input addr error!" << std::endl;
            return;
        }
    }
}

void testaddr3()
{
    std::map<std::string, Account> accs;
    while(true)
    {
        Account acc(true);
        MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
        if(accs.find(acc.GetAddr()) != accs.end())
        {
            std::cout<<"errrrrrrrre addr:{}"<< addHexPrefix(acc.GetAddr()) << std::endl;
            return ;
        }
        accs[acc.GetAddr()] = acc;
        if(acc.GetAddr().substr(0,3) == "666")
        {
            MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.GetAddr());
        }
        std::cout<<"addr:"<< addHexPrefix(acc.GetAddr()) << std::endl;
        if (!isValidAddress(acc.GetAddr()))
        {
            std::cout << "Input addr error!" << std::endl;
            return;
        }
    }
}
void testNewAddr()
{
    testaddr3();
}

void getContractAddr()
{
    // std::cout << std::endl
    //           << std::endl;

    // std::cout << "AddrList : " << std::endl;
    // MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    // std::string strFromAddr;
    // std::cout << "Please enter your addr:" << std::endl;
    // std::cin >> strFromAddr;
    // if (!isValidAddress(strFromAddr))
    // {
    //     std::cout << "Input addr error!" << std::endl;
    //     return;
    // }

    // DBReader dbReader;
    // std::vector<std::string> vecDeployers;
    // dbReader.GetAllEvmDeployerAddr(vecDeployers);
    // std::cout << "=====================deployers=====================" << std::endl;
    // for(auto& deployer : vecDeployers)
    // {
    //     std::cout << "deployer: " << deployer << std::endl;
    // }
    // std::cout << "=====================deployers=====================" << std::endl;
    // std::string strToAddr;
    // std::cout << "Please enter to addr:" << std::endl;
    // std::cin >> strToAddr;
    // if(!isValidAddress(strToAddr))
    // {
    //     std::cout << "Input addr error!" << std::endl;
    //     return;
    // }

    // std::vector<std::string> vecDeployUtxos;
    // dbReader.GetDeployUtxoByDeployerAddr(strToAddr, vecDeployUtxos);
    // std::cout << "=====================deployed utxos=====================" << std::endl;
    // for(auto& deployUtxo : vecDeployUtxos)
    // {
    //     std::cout << "deployed utxo: " << deployUtxo << std::endl;
    // }
    // std::cout << "=====================deployed utxos=====================" << std::endl;
    // std::string strTxHash;
    // std::cout << "Please enter tx hash:" << std::endl;
    // std::cin >> strTxHash;


    // std::string addr = evm_utils::GenerateContractAddr(strToAddr+strTxHash);


    // std::cout << addr << std::endl;
}

static bool benchmarkAutomicWriteSwitch = false;
void PrintBenchmarkToFile()
{
    if(benchmarkAutomicWriteSwitch)
    {
        benchmarkAutomicWriteSwitch = false;
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
    benchmarkAutomicWriteSwitch = true;
    auto benchmarkAutomicWriteThread = std::thread(
            [interval]()
            {
                while (benchmarkAutomicWriteSwitch)
                {
                    MagicSingleton<TFSbenchmark>::GetInstance()->PrintBenchmarkSummary(true);
                    MagicSingleton<TFSbenchmark>::GetInstance()->PrintBenchmarkSummary_DoHandleTx(true);
                    sleep(interval);
                }
            }
    );
    benchmarkAutomicWriteThread.detach();
    return;
}

void GetBalanceByUtxo()
{
    std::cout << "Inquiry address:";
    std::string addr;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    DBReader reader;
    std::vector<std::string> utxoHashs;
    reader.GetUtxoHashsByAddress(addr, utxoHashs);

    auto utxoOutput = [addr, utxoHashs, &reader](std::ostream &stream)
    {
        stream << "account:" << addHexPrefix(addr) << " utxo list " << std::endl;

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

        stream << "address: " << addHexPrefix(addr) << " UTXO total: " << utxoHashs.size() << " UTXO gross value:" << total << std::endl;
    };

    if (utxoHashs.size() < 10)
    {
        utxoOutput(std::cout);
    }
    else
    {
        std::string fileName = "utxo_" + addr + ".txt";
        std::ofstream file(fileName);
        if (!file.is_open())
        {
            ERRORLOG("Open file failed!");
            return;
        }
        utxoOutput(file);
        file.close();
    }
}

int ImitateCreateTxStruct()
{
    // Account acc;
    // if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
    // {
    //     return -1;
    // }
    Account acc;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc);

    const std::string addr = acc.GetAddr();
    uint64_t time = global::ca::kGenesisTime;

    CTransaction tx;
    tx.set_version(global::ca::kCurrentTransactionVersion);
    tx.set_time(time);
    tx.set_n(0);
    tx.set_identity(addr);
    tx.set_type(global::ca::kGenesisSign);

    //Check whether the Genesis account is in the address list
    // std::vector<std::string> List;
    // MagicSingleton<AccountManager>::GetInstance()->GetAccountList(List);
    // if(std::find(List.begin(), List.end(), global::ca::kInitAccountAddr) == List.end())
    // {
    //     std::cout << "The Genesis account is not in the node list !" << std::endl;
    //     return -2;
    // }

    std::map<std::string,uint64_t> addrValue =
    {
        {"049E40ce759614c3f58fA5BCD8CB07EeB419952b", 11000},
        {"13D10d6B51A05cD22D4a553E7928B9786613484c", 11000},
        {"14Ab3819BF1498e58fe15Ae8c3E67ecd1B224F3a", 11000},
        {"15bB9BE3ef36A10ce2E8B3eBaaFA4317Ca644375", 11000},
        {"15Cf841043F4f55FB55E28745dEA02AAa6100896", 11000},
        {"17402F969725a58BaB442443098aAACFaBc2c29e", 11000},
        {"1835DfF0B0bD5673A456a88AfC94eC61edcD0d46", 11000},
        {"1B94Ff8F093b27202Fb9b93f04044eb24f6a8de2", 11000},
        {"1DeF2D7B559Cc1b4E34D9Fd9cB7B59173AfE56D5", 11000},
        {"1f8B3ff7a3fE1aa8c9dFc1f43B2f3226A2275469", 11000},
        {"253B5fE677C3A8D8c9D7B288A6995844b431cC86", 11000},
        {"25E3d41F50a7d0f017Dc5bf91db4717D9A8166c2", 11000},
        {"2920360301f7939f3C4e121Ef121bdfA2E7BD795", 11000},
        {"35525d472B5D1783748E652759B914f7c12B7d75", 11000},
        {"3D6031fcCe79e43C56cE2235e02CD6ba2cAC89cF", 11000},
        {"3e70938F3DBe721443a4e3607bCFE9981134e53f", 11000},
        {"3F5C04EFf4935186620a692C7a6eBC0cBBe6Ca1D", 11000},
        {"45db82A95C809e9B2249FB80cBb4C2a21d47d60E", 11000},
        {"46068f5D2b68E886B5E50585D1950849Aa1A126f", 11000},
        {"4d9ccD16c3EFE22F4aca9CDAc3d5dAe97c62BE15", 11000},
        {"4edAa66B3aB1144EefdB4f9F86397eFe766be564", 11000},
        {"5206c3f982bC7a45a4fb75929AC370A4dF946b20", 11000},
        {"5416650D19EF61978EFAe496137437d275e70dA4", 11000},
        {"549A546ac5dAFA54936b1bfA36743797399d664E", 11000},
        {"54bFb9120488c8E9EdfB9066A9c4Aa1c4d0c0EF0", 11000},
        {"5E92f48D8D740eA5cC83886e88Cb69B44fD3Aeb9", 11000},
        {"5fd0aE3aF4911788BC54188D0642eBD9c1a487be", 11000},
        {"619C26BF88Df5E1AFa4B6f2A8199D89c672DdB2c", 11000},
        {"6434370D451a652A05Cae16B73A83557a2Cb058c", 11000},
        {"69B81541d4d4bEeeBDA494164e7Ea8279F71a38a", 11000},
        {"7e36d01800A4696eEA6ee65448eC3548B5de71aE", 11000},
        {"7F8c17331Db5A2f75Da32CdfB5a3F88cEf1Eaf36", 11000},
        {"80B1C733712F308C3E5c327852168575d174Ea16", 11000},
        {"8Ab2F209c201dbF27EC7506789f9Eeb935B40c9C", 11000},
        {"90717ba9d9B83d6314a69A8df2B2ad43A2077987", 11000},
        {"948Af3Dd7f64EF4092cD0448dC72a5B26bE279aE", 11000},
        {"98BD1A6D782c8439261d772f287D99aa70505c2E", 11000},
        {"98eed48771bD22Bb8282CCDd1B5FC43873356d80", 11000},
        {"9f2C4A0ecA709E6009792947277FACbb93FDaCd4", 11000},
        {"9f2D8b4554AcD13b167A86189c4579c0f4Ec7c17", 11000},
        {"a2B79601D164b1bA9B195f75523f55eAc2e5B830", 11000},
        {"A4DFdF4fe34ed84AFEB453a5D2C5e7ABac170Ca9", 11000},
        {"aEdbAF12953c9Ed3feDbB52c299721fa792f40D5", 11000},
        {"B2C69892269cA4E78E78C48aa5d6AF05Ff7F5cAF", 11000},
        {"B8d2d5Ab0615b3f18411AE17AB9e029963057c96", 11000},
        {"BdeCf692B3178194C9f7d97E725176b87ca77d28", 11000},
        {"C3852a255B82B04b43526c66fB30Ed88071AfA85", 11000},
        {"C81431cFf4608fA8aA580B983f9F84adfFBDbaBa", 11000},
        {"C9ce0350a02Bee6e6C74F3BB595ddeA2BfBaFf87", 11000},
        {"CEd4F31a7EB8E30ee69A5Ef73d790A9c1e5A7c27", 11000},
        {"CF85E332Abd98710824fe4FB5c0b5Ff93319FA98", 11000},
        {"D32Eb30B3A1943c9dfC752a54C56122b0836a693", 11000},
        {"d398DBc49A372D710FC3d80bDcADF6ACE6006f03", 11000},
        {"D837858060940DaBE6C1D7dFb59120b55b157963", 11000},
        {"DdB3140F192236e726Aa7ce4DB7E52c1C5D831a1", 11000},
        {"E08ad553b5756777752C22793A8FFd726E441575", 11000},
        {"E86Ee29b8Ad91E675ec2aEAC2312AdB599a5B71A", 11000},
        {"Ec8bDe5cf5446cf19Be41DfD6294569d51fD5E8B", 11000},
        {"F0635C3d7a883457C152f8640EB6b83eC1177ac6", 11000},
        {"F137891f3B1e8210BeD4cAa80edC97fF96f1513D", 11000},
        {"F26A24D15c1FD4f1e27EFb0598aDedEc9DeaA9A2", 11000},
        {"f5281e9AD7217C9E8e7FC5640e7C9AfDd1173E7f", 11000},
        {"F5931e60D5658a149245478118fE235795389051", 11000},
        {"F660E42d4Ac7600e0dA39e71fb8BDC9DACc86888", 11000},
        {"f87fA575ee575F92577d4054282BDd93461B9755", 11000},
        {"ffff89cAF30a3Dd8E69654fC8BA40Ff62a163f29", 110000000}
    };
    CTxUtxo *utxo = tx.mutable_utxo();
    utxo->add_owner(addr);
    {
        CTxInput *txin = utxo->add_vin();
        CTxPrevOutput *prevOut = txin->add_prevout();
        prevOut->set_hash(std::string(64, '0'));
        prevOut->set_n(0);
        txin->set_sequence(0);

        std::string serVinHash = Getsha256hash(txin->SerializeAsString());
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

        for(auto & obj : addrValue)
        {
            CTxOutput *newTxout = utxo->add_vout();
            newTxout->set_value((obj.second + 1)* global::ca::kDecimalNum);
            newTxout->set_addr(obj.first);
        }
    }

    {
        std::string serUtxo = Getsha256hash(utxo->SerializeAsString());
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

    tx.set_hash(Getsha256hash(tx.SerializeAsString()));

    CBlock block;
    block.set_time(time);
    block.set_version(global::ca::kCurrentBlockVersion);
    block.set_prevhash(std::string(64, '0'));
    block.set_height(0);

    CTransaction *tx0 = block.add_txs();
    *tx0 = tx;

    nlohmann::json blockData;
    blockData["Name"] = "Transformers";
    blockData["Type"] = "Genesis";
    block.set_data(blockData.dump());

    block.set_merkleroot(ca_algorithm::CalcBlockMerkle(block));
    block.set_hash(Getsha256hash(block.SerializeAsString()));
    
    std::string hex = Str2Hex(block.SerializeAsString());
    std::cout << std::endl
              << hex << std::endl;

    return 0;
}


void MultiTransaction()
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
        if (addr.substr(0, 2) == "0x") 
        {
            addr = addr.substr(2);
        }
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
        if (addr.substr(0, 2) == "0x") 
        {
            addr = addr.substr(2);
        }
        std::cout << "amount : ";
        std::cin >> amt;
        toAddr.insert(make_pair(addr, amt * global::ca::kDecimalNum));
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxMsgReq txMsg;
    TxHelper::vrfAgentType isNeedAgentFlag;
    CTransaction outTx;
    Vrf info;
    std::string encodedInfo = "";
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddr, encodedInfo, top + 1, outTx,isNeedAgentFlag,info, false);
    if (ret != 0)
	{
		ERRORLOG("CreateTxTransaction error!!");
		return;
	}
    uint64_t txUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, txUtxoHeight);

    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

	txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
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
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}", ret, outTx.hash());
}

void GetAllPledgeAddr()
{
    DBReader reader;
    std::vector<std::string> addressVec;
    reader.GetStakeAddress(addressVec);

    auto allPledgeOutput = [addressVec](std::ostream &stream)
    {
        stream << std::endl
               << "---- Pledged address start ----" << std::endl;
        for (auto &addr : addressVec)
        {
            uint64_t pledgeamount = 0;
            SearchStake(addr, pledgeamount, global::ca::StakeType::kStakeType_Node);
            stream << addHexPrefix(addr) << " : " << pledgeamount << std::endl;
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

void AutoTx()
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

void GetBlockinfoByTxhash()
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

void GetTxHashByHeight(int64_t start,int64_t end,std::ofstream& filestream)
{
    int64_t localStart = start;
    int64_t localEnd = end;

    if (localEnd < localStart)
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
    DBReader dbReader;
    uint64_t txTotal = 0;
    uint64_t blockTotal = 0;
    for (int64_t i = localEnd; i >= localStart; --i)
    {

        std::vector<std::string> tmpBlockHashs;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(i, tmpBlockHashs))
        {
            ERRORLOG("(GetTxHashByHeight) GetBlockHashsByBlockHeight  Failed!!");
            return;
        }

        int txHashCount = 0;
        for (auto &blockhash : tmpBlockHashs)
        {
            std::string blockstr;
            dbReader.GetBlockByBlockHash(blockhash, blockstr);
            CBlock block;
            block.ParseFromString(blockstr);
            txHashCount += block.txs_size();
        }
        txTotal += txHashCount;
        blockTotal += tmpBlockHashs.size();
        filestream  << "Height >: " << i << " Blocks >: " << tmpBlockHashs.size() << " Txs >: " << txHashCount  << std::endl;
        for(auto &blockhash : tmpBlockHashs)
        {
            std::string blockstr;
            dbReader.GetBlockByBlockHash(blockhash, blockstr);
            CBlock block;
            block.ParseFromString(blockstr);
            std::string tmpBlockHash = block.hash();
            tmpBlockHash = tmpBlockHash.substr(0,6);
            int tmpHashSize = block.txs_size();
            filestream << " BlockHash: " << tmpBlockHash << " TxHashSize: " << tmpHashSize << std::endl;
        }
    }

    filestream  << "Total block sum >:" << blockTotal  << std::endl;
    filestream  << "Total tx sum >:" << txTotal   << std::endl;
    std::vector<std::string> startHashes;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(localStart, startHashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail  top = {} ", localStart);
        return;
    }

    //Take out the blocks at the starting height and sort them from the smallest to the largest in time
    std::vector<CBlock> startBlocks;
    for (auto &hash : startHashes)
    {
        std::string blockStr;
        dbReader.GetBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        startBlocks.push_back(block);
    }
    std::sort(startBlocks.begin(), startBlocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    std::vector<std::string> endHashes;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(localEnd, endHashes))
    {
        ERRORLOG("GetBlockHashsByBlockHeight fail  top = {} ", localEnd);
        return;
    }

    //Take out the blocks at the end height and sort them from small to large in time
    std::vector<CBlock> endBlocks;
    for (auto &hash : endHashes)
    {
        std::string blockStr;
        dbReader.GetBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        endBlocks.push_back(block);
    }
    std::sort(endBlocks.begin(), endBlocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    float timeDiff = 0;
    if (endBlocks[endBlocks.size() - 1].time() - startBlocks[0].time() != 0)
    {
        timeDiff = float(endBlocks[endBlocks.size() - 1].time() - startBlocks[0].time()) / float(1000000);
    }
    else
    {
        timeDiff = 1;
    }
    uint64_t tx_conut = txTotal ;
    float tps = float(tx_conut) / float(timeDiff);
    filestream << "TPS : " << tps << std::endl;
}




void TpsCount(){
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
    filestream.open(fileName);
    GetTxHashByHeight(start,end,filestream);

     
}

void Get_InvestedNodeBlance()
{
    std::string addr;
    std::cout << "Please enter the address you need to inquire: " << std::endl;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    std::shared_ptr<GetAllInvestAddressReq> req = std::make_shared<GetAllInvestAddressReq>();
    req->set_version(global::kVersion);
    req->set_addr(addr);

    GetAllInvestAddressAck ack;
    GetAllInvestAddressReqImpl(req, ack);
    if (ack.code() != 0)
    {
        std::cout << "code: " << ack.code() << std::endl;
        ERRORLOG("Get_InvestedNodeBlance failed!");
        return;
    }

    std::cout << "------------" << ack.addr() << "------------" << std::endl;
    std::cout << "size: " << ack.list_size() << std::endl;
    for (int i = 0; i < ack.list_size(); i++)
    {
        const InvestAddressItem info = ack.list(i);
        std::cout << "addr:" << "0x"+info.addr() << "\tamount:" << info.value() << std::endl;
    }
}
void PrintDatabaseBlock()
{
    DBReader dbReader;
    std::string str = PrintBlocks(100, false);
    std::cout << str << std::endl;
}

void ThreadTest::TestCreateTx_2(const std::string &from, const std::string &to)
{
    std::cout << "from:" << addHexPrefix(from) << std::endl;
    std::cout << "to:" << addHexPrefix(to) << std::endl;

    uint64_t startTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
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
    std::string encodedInfo = "";
    int ret = TxHelper::CreateTxTransaction(fromAddr, toAddrAmount, encodedInfo, top + 1, outTx,isNeedAgentFlag,info, false);
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
        Vrf * new_info = txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info);

    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {
        MagicSingleton<BlockMonitor>::GetInstance()->addDropshippingTxVec(outTx.hash());
        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        MagicSingleton<BlockMonitor>::GetInstance()->addDoHandleTxTxVec(outTx.hash());
        ret = DoHandleTx(msg, outTx);
    }
    global::ca::TxNumber++;
    DEBUGLOG("Transaction result,ret:{}  txHash:{}, TxNumber:{}", ret, outTx.hash(), global::ca::TxNumber);
    Initiate = true;
    MagicSingleton<TFSbenchmark>::GetInstance()->AddTransactionInitiateMap(startTime, MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    
    std::cout << "=====Transaction initiator:" << addHexPrefix(from) << std::endl;
    std::cout << "=====Transaction recipient:" << addHexPrefix(to) << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl;
}

bool bStopTx_2 = true;
bool bIsCreateTx_2 = false;
static int i = -1;
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
void ThreadTest::SetStopTxFlag(const bool &flag)
{
    bStopTx_2 = flag;
}

void ThreadTest::GetStopTxFlag(bool &flag)
{
   flag =  bStopTx_2 ;
}

void ThreadTest::TestCreateTx(uint32_t tranNum, std::vector<std::string> addrs_,int timeout)
{
    DEBUGLOG("TestCreateTx start at {}", MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
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

void CreateMultiThreadAutomaticTransaction()
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
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::thread th(ThreadTest::TestCreateTx,TxNum, addrs, timeout);
    th.detach();
}

void TestCreateStake_2(const std::string &from)
{
    TxHelper::pledgeType pledgeType = TxHelper::pledgeType::kPledgeType_Node;
    uint64_t stakeAmount = 10  * global::ca::kDecimalNum ;

    DBReader data_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != data_reader.GetBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType isNeedAgentFlag;
    Vrf info;
    std::vector<TxHelper::Utxo> outVin;  
    std::string encodedInfo = "";
    if(TxHelper::CreateStakeTransaction(from, stakeAmount, encodedInfo, top + 1, pledgeType, outTx, outVin,isNeedAgentFlag,info,global::ca::KMaxCommissionRate) != 0)
    {
        return;
    }
    std::cout << " from: " << addHexPrefix(from) << " amout: " << stakeAmount << std::endl;
    TxMsgReq txMsg;
    txMsg.set_version(global::kVersion);
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
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
        Vrf * newInfo = txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);
    }
    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
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


void CreateMultiThreadAutomaticStakeTransaction()
{
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(addrs.begin(), addrs.end(), MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
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
    std::string encodedInfo;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, invest_amount, encodedInfo, top + 1, investType,outTx, outVin,isNeedAgentFlag,info);
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
    txMsgInfo->set_nodeheight(top);

    uint64_t localTxUtxoHeight;
    ret = TxHelper::GetTxUtxoHeight(outTx, localTxUtxoHeight);
    if(ret != 0)
    {
        ERRORLOG("GetTxUtxoHeight fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(localTxUtxoHeight);

    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        Vrf * newInfo=txMsg.mutable_vrfinfo();
        newInfo->CopyFrom(info);
    }

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();

    if (isNeedAgentFlag == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
    {

        ret = DropshippingTx(msg, outTx);
    }
    else
    {
        ret = DoHandleTx(msg, outTx);
    }

    std::cout << "=====Transaction initiator:" << addHexPrefix(strFromAddr) << std::endl;
    std::cout << "=====Transaction recipient:" << addHexPrefix(strToAddr) << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl
              << std::endl;
}

void AutoInvestment()
{

    std::cout << "input aummot: ";
    std::string aummot;
    std::cin >> aummot;

    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(addrs.begin(), addrs.end(), MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
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
            DEBUGLOG("Illegal account. from addr is null !");
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

void PrintVerifyNode()
{
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

    std::vector<Node> resultNode;
    for (const auto &node : nodelist)
    {
        int ret = VerifyBonusAddr(node.address);

        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::kStakeType_Node);
        if (stakeTime > 0 && ret == 0)
        {
            resultNode.push_back(node);
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
    for (auto &i : resultNode)
    {
        filestream
            << "  addr(" << addHexPrefix(i.address) << ")"
            << std::endl;
    }
    filestream << "------------------------------------------------------------------------------------------------------------" << std::endl;
    filestream << "PeerNode size is: " << resultNode.size() << std::endl;
}

void GetRewardAmount()
{
    int64_t startHeight = 0;
    int64_t endHeight = 0;
    std::string addr;
    std::cout << "Please input start height:";
    std::cin >> startHeight;
    std::cout << "Please input end height:";
    std::cin >> endHeight;
    if(endHeight < startHeight)
    {
        std::cout<< "input invalid" << std::endl;
        return ;
    } 
    std::cout << "Please input the address:";
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    
    if(!isValidAddress(addr))
    {
        std::cout<< "Input addr error!" <<std::endl;
        return ; 
    }
    DBReader dbReader;
 
    uint64_t txTotall = 0;
    uint64_t claimAmount=0;
    for(int64_t i = startHeight; i <= endHeight; ++i)
    {
        std::vector<std::string> block_hashs;
        if(DBStatus::DB_SUCCESS != dbReader.GetBlockHashsByBlockHeight(i,block_hashs)) 
        {
            ERRORLOG("(GetTxHashByHeight) GETBlockHashsByBlockHeight  Failed!!");
            return;
        }
        std::vector<CBlock> blocks;
        for(auto &blockhash : block_hashs)
        {
            std::string blockstr;
            if(DBStatus::DB_SUCCESS !=   dbReader.GetBlockByBlockHash(blockhash,blockstr)) 
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
                   struct tm * gmDate;
                   gmDate = localtime(&s);
                   std::cout<< gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")"<< std::endl;
                    for(auto tx : block.txs())
                    {
                        if((global::ca::TxType)tx.txtype() == global::ca::TxType::kTxTypeBonus )
                        {
                            std::map< std::string,uint64_t> kmap;
                            try 
                            {
                                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                                nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
                                claimAmount = txInfo["BonusAmount"].get<uint64_t>();
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
                                            txTotall += vout.value();
                                        }
                                    }
                                }
                            }
                            for(auto it = kmap.begin(); it != kmap.end();++it)
                            {
                                    std::cout << "reward addr:" << addHexPrefix(it->first) << "reward amount" << it->second <<std::endl;   
                            }
                            if(claimAmount!=0)
                            {
                                std::cout << "self node reward addr:" << addHexPrefix(addr) <<"self node reward amount:" << claimAmount-txTotall; 
                                std::cout << "total reward amount"<< claimAmount;
                            }
                        }
                    }
                }   
        }
}

void TestsHandleInvest()
{
    std::cout << std::endl
              << std::endl;
    std::cout << "AddrList:" << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.GetAddr();

    std::cout << "Please enter your addr:" << std::endl;
    std::cout << addHexPrefix(strFromAddr) << std::endl;
    if (!isValidAddress(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cout << addHexPrefix(strFromAddr) << std::endl;
    if (!isValidAddress(strFromAddr))
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
    Vrf info_;
    std::string encodedInfo;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strFromAddr, investAmount, encodedInfo, top + 1, investType, outTx, outVin,isNeedAgentFlag,info_);
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
        Vrf * new_info=txMsg.mutable_vrfinfo();
        new_info->CopyFrom(info_);

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



void TestHandleInvestMoreToOne(std::string strFromAddr, std::string strToAddr, std::string strInvestFee)
{
    
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
    std::string encodedInfo;
    int ret = TxHelper::CreateInvestTransaction(strFromAddr, strToAddr, investAmount, encodedInfo, top + 1,  investType, outTx, outVin,isNeedAgentFlag,info);
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
        ret=DoHandleTx(msg,outTx);
    }
    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void TestManToOneDelegate()
{
    uint32_t num = 0;
    std::cout << "plase inter delegate num: " << std::endl;
    std::cin >> num; 

    std::vector<std::string> _list;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(_list);

    if(num > _list.size())
    {
        std::cout << "error: Account num < " << num << std::endl;
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

    DBReadWriter dbReader;
    std::set<std::string> pledgeAddr;

    std::vector<std::string> stakeAddr;
    auto status = dbReader.GetStakeAddress(stakeAddr);
    if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
    {
        std::cout << "GetStakeAddress error" << std::endl;
        return;
    }

    for(const auto& addr : stakeAddr)
    {
        if(VerifyBonusAddr(addr) != 0)
        {
            continue;
        }
        pledgeAddr.insert(addr);
    }

    int successNum = 0;
    int testNum = 0;
    for(int i = 0; successNum != num; ++i)
    {
        std::string fromAddr;
        try
        {
            fromAddr = _list.at(i);
        }
        catch (const std::exception&)
        {
            break;
        }

        if (!isValidAddress(fromAddr))
        {
            ERRORLOG("fromAddr addr error!");
            std::cout << "fromAddr addr error! : " << addHexPrefix(fromAddr) << std::endl;
            continue;
        }

        auto it = pledgeAddr.find(fromAddr);
        if(it != pledgeAddr.end())
        {
            ++testNum;
            continue;
        }

        TestHandleInvestMoreToOne(fromAddr, strToAddr, strInvestFee);  
        ++successNum;
    }

    std::cout << "testNum: " << testNum << std::endl;
}
void OpenLog()
{
    Config::Log log = {};
    MagicSingleton<Config>::GetInstance()->GetLog(log);
    MagicSingleton<Log>::GetInstance()->LogInit(log.path,log.console,"debug");
}

void CloseLog()
{

    Config::Log log = {};
    MagicSingleton<Config>::GetInstance()->GetLog(log);
    MagicSingleton<Log>::GetInstance()->LogDeinit();
    std::string tmpString = "logs";
    if(std::filesystem::remove_all(tmpString))
    {
        std::cout<< "File deleted successfully" <<std::endl;
    }
    else
    {
        std::cout << "Failed to delete the file"<<std::endl;    
    }
}

void TestSign()
{
    Account account(true);
    std::string serVinHash = Getsha256hash("1231231asdfasdf");
	std::string signature;
	std::string pub;

    if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account) == -1)
    {
        std::cout <<"get account error";
    }
	if (account.Sign(serVinHash, signature) == true)
	{
		std::cout << "tx sign true !" << std::endl;
	}
    if(account.Verify(serVinHash,signature) == true)
    {
        std::cout << "tx verify true" << std::endl;
    }
}

