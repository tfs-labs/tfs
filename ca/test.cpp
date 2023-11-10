#include <cstdint>
#include <iostream>
#include <sstream>
#include <stack>
#include <sys/types.h>
#include <time.h>
#include <fstream>
#include <string>

#include "failed_transaction_cache.h"
#include "block_cache.h"
#include "block_helper.h"
#include "block_monitor.h"
#include "block_stroage.h"
#include "test.h"
#include "cache.h"
#include "net/socket_buf.h"
#include "net/unregister_node.h"
#include "utils/console.h"
#include "utils/hex_code.h"
#include "../include/logging.h"
#include "utils/base58.h"
#include "ca/global.h"
#include "utils/json.hpp"
#include "utils/time_util.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "db/db_api.h"
#include "utils/tmp_log.h"
#include "double_spend_cache.h"
#include "net/http_server.h"
#include "net/test.hpp"
#include "utils/envelop.h"
#include "block_http_callback.h"
#include "net/epoll_mode.h"
int PrintFormatTime(uint64_t time, bool isConsoleOutput, std::ostream & stream)
{
    time_t s = (time_t)(time / 1000000);
    struct tm * gmDate;
    gmDate = localtime(&s);

	CaConsole tmColor(kConsoleColor_White, kConsoleColor_Black, true);
    if(isConsoleOutput)
	{
        stream << tmColor.Color() << gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")" << tmColor.Reset() << std::endl;
	}
	else
	{
        stream << gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")" << std::endl;
    }

    return 0;
}

int PrintRocksdb(uint64_t start, uint64_t end, bool isConsoleOutput, std::ostream & stream)
{
    if(start > end)
    {
        ERRORLOG("start > end");
        return -1;
    }

	DBReader dbReader;
    CaConsole bkColor(kConsoleColor_Blue, kConsoleColor_Black, true);
    uint64_t count = 0 ;
    uint64_t height = end;
    for (; height >= start; --height) 
    {
	    std::vector<std::string> vBlockHashs;
	    dbReader.GetBlockHashsByBlockHeight(height, vBlockHashs);
        std::vector<CBlock> blocks;
        for (auto hash : vBlockHashs)
        {
            std::string strHeader;
            dbReader.GetBlockByBlockHash(hash, strHeader);
            CBlock block;
            block.ParseFromString(strHeader);
            blocks.push_back(block);
        }
        std::sort(blocks.begin(), blocks.end(), [](CBlock & a, CBlock & b){
            return a.time() < b.time();
        });

        count ++;
        std::cout << "rate of progress------>" << count << "/" << end << std::endl;
        for (auto & block : blocks)
        {

            PrintBlock(block, isConsoleOutput, stream);
        }
        if(height == start)break;
    }
    
    return 0;
}

int PrintBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream)
{
    CaConsole bkColor(kConsoleColor_Blue, kConsoleColor_Black, true);
    CaConsole greenColor(kConsoleColor_Green, kConsoleColor_Black, true);
    stream << std::endl << "BlockInfo ---------------------- > height [" << block.height() << "]" << std::endl;
    stream << "HashMerkleRoot -> " << block.merkleroot() << std::endl;
    stream << "HashPrevBlock -> " << block.prevhash() << std::endl;
    if (isConsoleOutput)
    {
        stream << "BlockHash -> " << bkColor.Color() << block.hash() << bkColor.Reset() << std::endl;
    }
    else
    {
        stream << "BlockHash -> " << block.hash() << std::endl;
    }

    stream << "blockverifySign[" << block.sign_size() << "]" << std::endl;
    for (auto & verifySign : block.sign())
    {
        stream << "block Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GetBase58Addr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    for (auto & verifySign : block.sign())
    {
        stream << "block signer -> [" << greenColor.Color() << GetBase58Addr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    stream << "Time-> ";
    PrintFormatTime(block.time(), isConsoleOutput, stream);
    
    for (int i = 0; i < block.txs_size(); i++) 
    {
        CTransaction tx = block.txs(i);
        stream << "TX_INFO -----------> index[" << i << "]" << std::endl;
        PrintTx(tx, isConsoleOutput, stream);
    }

    return 0;
}

std::string PrintBlocks(int num, bool pre_hash_flag)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.GetBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--){
        str += (std::to_string(i) + "\t");
        std::vector<std::string> vBlockHashs;
        dbRead.GetBlockHashsByBlockHeight(i, vBlockHashs);
        std::sort(vBlockHashs.begin(), vBlockHashs.end());
        for (auto hash : vBlockHashs) {
            std::string strHeader;
            dbRead.GetBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(pre_hash_flag)
            {
                str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
            }else{
                str = str + hash.substr(0,6) + " ";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }
    str += "--------------\n";
    return str;
}

std::string PrintBlocksHash(int num, bool pre_hash_flag)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.GetBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--){
        str += (std::to_string(i) + "\n");
        std::vector<std::string> vBlockHashs;
        dbRead.GetBlockHashsByBlockHeight(i, vBlockHashs);
        std::sort(vBlockHashs.begin(), vBlockHashs.end());
        for (auto hash : vBlockHashs) {
            std::string strHeader;
            dbRead.GetBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(pre_hash_flag)
            {
                str = str + hash + "(" + header.prevhash().substr(0,6) + ")" + " \n";
            }else{
                str = str + hash + " \n";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }
    str += "--------------\n";
    return str;
}

std::string PrintRangeBlocks(int startNum,int num, bool pre_hash_flag)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.GetBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";

    if(startNum > top || startNum < 0)
    {
        std::string strTop = std::to_string(top);
        str += "height error,Current height ";
        str += strTop;
        return str;
    }
    if(num > startNum)
    {
        num = startNum;
    }

    int j = 0;
    for(int i = startNum; i >= 0; i--){
        str += (std::to_string(i) + "\t");
        std::vector<std::string> vBlockHashs;
        dbRead.GetBlockHashsByBlockHeight(i, vBlockHashs);
        std::sort(vBlockHashs.begin(), vBlockHashs.end());
        for (auto hash : vBlockHashs) {
            std::string strHeader;
            dbRead.GetBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(pre_hash_flag)
            {
                str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
            }else{
                str = str + hash.substr(0,6) + " ";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }

    str += "--------------\n";
    return str;
}

int PrintTx(const CTransaction & tx, bool isConsoleOutput, std::ostream & stream)
{
    if (isConsoleOutput)
    {
        CaConsole txColor(kConsoleColor_Red, kConsoleColor_Black, true);
        CaConsole greenColor(kConsoleColor_Green, kConsoleColor_Black, true);
        stream << "TxHash -> " << txColor.Color() << tx.hash() << txColor.Reset() << std::endl;
        stream << "n -> " << tx.n() << std::endl;
        stream << "identity -> " << "[" << greenColor.Color() << tx.identity() << greenColor.Reset() << "] " << std::endl;
        stream << "type -> " << tx.type() << std::endl;

        stream << "verifySign[" << tx.verifysign_size() << "]" << std::endl;

        for (auto & verifySign : tx.verifysign())
        {
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GetBase58Addr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << greenColor.Color() << GetBase58Addr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
        }

        stream << "Owner -> ";
        for (auto & addr : tx.utxo().owner())
        {
            stream << "[" << greenColor.Color() << addr << greenColor.Reset() << "]";
        }
        stream << std::endl;

        for (int j = 0; j < tx.utxo().vin_size(); j++)
        {
            const CTxInput & vin = tx.utxo().vin(j);
            stream << "vin[" << j << "] sequence -> " << vin.sequence() << std::endl;
            stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << greenColor.Color() << GetBase58Addr(vin.vinsign().pub()) << greenColor.Reset() << "]" << std::endl;

            for (auto & prevout : vin.prevout())
            {
                stream << "vin[" << j << "] Prev Output Hash -> " << prevout.n() << " : " << prevout.hash() << std::endl;
            }
        }

        for (int j = 0; j < tx.utxo().vout_size(); j++)
        {
            const CTxOutput & vout = tx.utxo().vout(j);
            CaConsole amount(kConsoleColor_Yellow, kConsoleColor_Black, true);
            stream << "vout[" << j << "] public key -> [" << greenColor.Color() <<  vout.addr() << greenColor.Reset() << "]" << std::endl;
            stream << "vout[" << j << "] value -> [" << amount.Color() <<  vout.value() << amount.Reset() << "]" << std::endl;
        }

        for (int j = 0; j < tx.utxo().multisign_size(); j++)
        {
            const CSign & multiSign = tx.utxo().multisign(j);
            stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << "[" << greenColor.Color() << GetBase58Addr(multiSign.pub()) << greenColor.Reset() << "]" << std::endl;
        }
    }
    else
    {
        stream << "TxHash -> " << tx.hash() << std::endl;
        stream << "n -> " << tx.n() << std::endl;
        stream << "identity -> " << tx.identity() << std::endl;
        stream << "type -> " << tx.type() << std::endl;

        stream << "verifySign[" << tx.verifysign_size() << "]" << std::endl;

        for (auto & verifySign : tx.verifysign())
        {
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << GetBase58Addr(verifySign.pub()) << "]" << std::endl;  
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << GetBase58Addr(verifySign.pub()) << "]" << std::endl;
        }

        stream << "Owner -> ";
        for (auto & addr : tx.utxo().owner())
        {
            stream << "[" << addr << "]";
        }
        stream << std::endl;

        for (int j = 0; j < tx.utxo().vin_size(); j++)
        {
            const CTxInput & vin = tx.utxo().vin(j);
            stream << "vin[" << j << "] sequence -> " << vin.sequence() << std::endl;
            stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << GetBase58Addr(vin.vinsign().pub()) << "]" << std::endl;

            for (auto & prevout : vin.prevout())
            {
                stream << "vin[" << j << "] Prev Output Hash -> " << prevout.n() << " : " << prevout.hash() << std::endl;
            }
        }

        for (int j = 0; j < tx.utxo().vout_size(); j++)
        {
            const CTxOutput & vout = tx.utxo().vout(j);
            stream << "vout[" << j << "] public key -> [" << vout.addr() << "]" << std::endl;
            stream << "vout[" << j << "] value -> [" << vout.value() << "]" << std::endl;
        }

        for (int j = 0; j < tx.utxo().multisign_size(); j++)
        {
            const CSign & multiSign = tx.utxo().multisign(j);
            stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << GetBase58Addr(multiSign.pub()) << std::endl;
        }
    }

    stream << "Time -> " << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(tx.time()) << std::endl;
    stream << "(" << tx.time() <<")" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> dataMap;
    std::string strData;
    if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
    {
        try
        {
            nlohmann::json dataJson = nlohmann::json::parse(tx.data());
            global::ca::TxType txType = (global::ca::TxType)tx.txtype();
            dataMap.push_back(std::make_pair("TxType", std::to_string((int32_t)txType)));
            dataMap.push_back(std::make_pair("Consensus", std::to_string(tx.consensus())));
            dataMap.push_back(std::make_pair("Gas", std::to_string(0)));
            dataMap.push_back(std::make_pair("Cost", std::to_string(0)));
            if (txType == global::ca::TxType::kTxTypeTx)
            {
               
            }
            else if (txType == global::ca::TxType::kTxTypeStake)
            {
                dataMap.push_back(std::make_pair("StakeType", dataJson["TxInfo"]["StakeType"].get<std::string>()));
                dataMap.push_back(std::make_pair("StakeAmount", std::to_string(dataJson["TxInfo"]["StakeAmount"].get<uint64_t>())));
                dataMap.push_back(std::make_pair("BonusPumping", std::to_string(dataJson["TxInfo"]["BonusPumping"].get<double>())));
            }
            else if (txType == global::ca::TxType::kTxTypeUnstake)
            {
                dataMap.push_back(std::make_pair("UnstakeUtxo", dataJson["TxInfo"]["UnstakeUtxo"].get<std::string>()));
            }
            
            for (auto & item : dataMap)
            {
                strData += "  " + item.first + " : " + item.second + "\n";
            }
        }
        catch (...)
        {
        }
    }

    stream << "data -> " << std::endl;
    stream << strData;
    stream << "version -> " << tx.version() << std::endl;
    
    stream << "----------------------------" << std::endl;
    stream << tx.data() << std::endl;
    return 0;
}

void BlockInvert(const std::string & strHeader, nlohmann::json &blocks)
{
    CBlock block;
    if(!block.ParseFromString(strHeader))
    {
        ERRORLOG("block_raw parse fail!");
        return ;
        
    }

    nlohmann::json allTx;
    nlohmann::json jsonBlock;
    jsonBlock["merkleroot"] = block.merkleroot();
    jsonBlock["prevhash"] = block.prevhash();
    jsonBlock["hash"] = block.hash();
    jsonBlock["height"] = block.height();
    jsonBlock["time"] = block.time();
    jsonBlock["bytes"] = block.ByteSizeLong();

    for(auto & blocksign : block.sign())
    {
        nlohmann::json block_verifySign;
        block_verifySign["sign"] = Base64Encode(blocksign.sign());
        block_verifySign["pub"] = Base64Encode(blocksign.pub());
        std::string sign_addr = GetBase58Addr(blocksign.pub(), Base58Ver::kBase58Ver_Normal);
        block_verifySign["signaddr"] = sign_addr;

        jsonBlock["blocksign"].push_back(block_verifySign);
    }


    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
            Tx["time"] = tx.time();
            Tx["txHash"] = tx.hash();
            Tx["identity"] = tx.identity();

            for(auto & owner: tx.utxo().owner())
            {
                Tx["utxo"]["owner"].push_back(owner);
            }

            for(auto & vin : tx.utxo().vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    Tx["utxo"]["vin"]["prevout"]["hash"].push_back(prevout.hash());
                }

                nlohmann::json utxoVinsign;
                utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

                Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                utxoVout["addr"] = vout.addr();
                utxoVout["value"] = vout.value();

                Tx["utxo"]["vout"].push_back(utxoVout); 
            }

            for(auto & multiSign : tx.utxo().multisign())
            {
                nlohmann::json utxoMultisign;
                utxoMultisign["sign"] = Base64Encode(multiSign.sign());
                utxoMultisign["pub"] = Base64Encode(multiSign.sign());

                Tx["utxo"]["multisign"].push_back(utxoMultisign);
            }

            Tx["Type"] = tx.type();
            Tx["Consensus"] = tx.consensus();
            Tx["Gas"] = 0;
            Tx["Cost"] = 0;
            Tx["txType"] = tx.txtype();

            if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
            {
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                Tx["data"] = dataJson;
            }

            for(auto & verifySign : tx.verifysign())
            {
                nlohmann::json utxoVerifySign;
                utxoVerifySign["sign"] = Base64Encode(verifySign.sign());
                utxoVerifySign["pub"] = Base64Encode(verifySign.pub());
                std::string signAddr = GetBase58Addr(verifySign.pub(), Base58Ver::kBase58Ver_Normal);
                utxoVerifySign["signaddr"] = signAddr;

                Tx["verifySign"].push_back(utxoVerifySign);
            }
            
            allTx[k++] = Tx;
        }
        else if(tx.type() == global::ca::kGenesisSign)
        {
            Tx["time"] = tx.time();
            Tx["txHash"] = tx.hash();
            Tx["identity"] = tx.identity();

            for(auto & owner: tx.utxo().owner())
            {
                Tx["utxo"]["owner"].push_back(owner);
            }

            for(auto & vin : tx.utxo().vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    Tx["utxo"]["vin"]["prevout"]["hash"].push_back(prevout.hash());
                }

                nlohmann::json utxoVinsign;
                utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

                Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                utxoVout["addr"] = vout.addr();
                utxoVout["value"] = vout.value();

                Tx["utxo"]["vout"].push_back(utxoVout); 
            }
            Tx["Type"] = tx.type();
            allTx[k++] = Tx;
        }
    }

    blocks["block"] = jsonBlock;
    blocks["tx"] = allTx;

}


std::string PrintCache(int where){
    string rocksdbUsage;
    MagicSingleton<RocksDB>::GetInstance()->GetDBMemoryUsage(rocksdbUsage);
    std::cout << rocksdbUsage << std::endl;
    std::stringstream ss;

    auto CaheString=[&](const std::string & whatCahe,uint64_t cacheSize,bool isEnd=false){
        std::time_t t = std::time(NULL);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&t))) {
            //std::cout << mbstr << '\n';
        }
        // if(where==2){
        //    // ss << "[" << mbstr << "]:" << "["   << whatCahe  << "]:"  << cacheSize << "\n";
        //     return;
        // }
       // ss << "[" << mbstr << "]:" << "[" << GREEN_t  << whatCahe << WHITE_t << "]:" << RED_t << cacheSize << WHITE_t<< "\n";

       ss << cacheSize <<( (isEnd) ? ",\n" :",");
       
    };

    auto cblockcahe= MagicSingleton<CBlockCache>::GetInstance();
    auto blcohelper= MagicSingleton<BlockHelper>::GetInstance();
    auto blockMonitor_= MagicSingleton<BlockMonitor>::GetInstance();
    auto blockStroage_=MagicSingleton<BlockStroage>::GetInstance(); 
    auto DoubleSpendCache_cd=MagicSingleton<DoubleSpendCache>::GetInstance(); 
    auto  FailedTransactionCache_cd=MagicSingleton<FailedTransactionCache>::GetInstance(); 
    auto syncBlock_t= MagicSingleton<SyncBlock>::GetInstance();
    auto CtransactionCache_=MagicSingleton<TransactionCache>::GetInstance();
    auto dbCache_=MagicSingleton<DBCache>::GetInstance();
    auto vrfo= MagicSingleton<VRF>::GetInstance();
    auto TFPBenchMark_C=  MagicSingleton<TFSbenchmark>::GetInstance();

     GlobalDataManager & manager=GlobalDataManager::GetGlobalDataManager();
     GlobalDataManager & manager2=GlobalDataManager::GetGlobalDataManager2();
     GlobalDataManager & manager3=GlobalDataManager::GetGlobalDataManager3();
    auto UnregisterNode__= MagicSingleton<UnregisterNode>::GetInstance();

    auto bufcontrol =MagicSingleton<BufferCrol>::GetInstance();

    auto pernode = MagicSingleton<PeerNode>::GetInstance();

    auto dispach=MagicSingleton<ProtobufDispatcher>::GetInstance();

    auto _echoCatch = MagicSingleton<echoTest>::GetInstance();

    auto workThread =MagicSingleton<WorkThreads>::GetInstance();

    // auto AccountManager_accountList = MagicSingleton<AccountManager>::GetInstance()->_accountList;
    auto phone_list = global::g_phoneList;
    auto cBlockHttpCallback_ = MagicSingleton<CBlockHttpCallback>::GetInstance();


    std::stack<std::string> emyp;

    {
        CaheString("", cblockcahe->_cache.size());
            // CaheString("",blcohelper->missing_utxos.size());
            // CaheString("",blcohelper->_broadcast_blocks.size());
            // CaheString("",blcohelper->sync_blocks.size());
            // CaheString("",blcohelper->fast_sync_blocks.size());
            // CaheString("",blcohelper->rollback_blocks.size());
            // CaheString("",blcohelper->pending_blocks.size());
            // CaheString("",blcohelper->hash_pending_blocks.size());
            // CaheString("",blcohelper->utxo_missing_blocks.size());
            // CaheString("",blcohelper->missing_blocks.size());
            // CaheString("",blcohelper->DoubleSpend_blocks.size());
            // CaheString("",blcohelper->DuplicateChecker.size());
            CaheString("",blockStroage_->_blockMap.size());
	        CaheString("",blockStroage_->_preHashMap.size());
	        CaheString("",blockStroage_->_blockStatusMap.size());
            // CaheString("",DoubleSpendCache_cd->_pendingAddrs.size());
            // CaheString("",FailedTransactionCache_cd->txPending.size());
            // CaheString("",syncBlock_t->sync_from_zero_cache.size());
            // CaheString("",syncBlock_t->sync_from_zero_reserve_heights.size());
            // CaheString("",CtransactionCache_->_cache.size());
            CaheString("",dbCache_->data_.size());
            CaheString("", dbCache_->time_data_.size());
            CaheString("",vrfo->vrfCache.size());
            CaheString("",vrfo->txvrfCache.size());
            CaheString("", vrfo->vrfVerifyNode.size());
            // CaheString("",TFPBenchMark_C->transactionVerifyMap.size());
            // CaheString("",TFPBenchMark_C->agentTransactionReceiveMap.size());
            // CaheString("",TFPBenchMark_C->transactionSignReceiveMap.size());
            // CaheString("",TFPBenchMark_C->transactionSignReceiveCache.size());
            // CaheString("",TFPBenchMark_C->blockContainsTransactionAmountMap.size());
            // CaheString("",TFPBenchMark_C->blockVerifyMap.size());
            // CaheString("",TFPBenchMark_C->blockPoolSaveMap.size());
            CaheString("",manager._globalData.size());
            CaheString("",manager2._globalData.size());
            CaheString("",manager3._globalData.size());
            CaheString("",UnregisterNode__->_nodes.size());
            CaheString("",UnregisterNode__->_consensusNodeList.size());
            CaheString("", bufcontrol->_BufferMap.size());
            CaheString("",pernode->_nodeMap.size());
            CaheString("",dispach->_caProtocbs.size());
            CaheString("",dispach->_netProtocbs.size());
            CaheString("",dispach->_broadcastProtocbs.size());
            CaheString("",dispach->_txProtocbs.size());
            CaheString("",dispach->_syncBlockProtocbs.size());
            CaheString("",dispach->_saveBlockProtocbs.size());
            CaheString("",dispach->_blockProtocbs.size());
            CaheString("",global::g_reqCntMap.size());
            CaheString("",HttpServer::rpcCbs.size());
	        CaheString("",HttpServer::_cbs.size());
            CaheString("",_echoCatch->_echoCatch.size());
            CaheString("",workThread->_threadsWorkList.size());
	        CaheString("",workThread->_threadsReadList.size());
	        CaheString("",workThread->_threadsTransList.size());
            CaheString("",phone_list.size());
            // CaheString("",AccountManager_accountList.size());
            CaheString("",cBlockHttpCallback_->_addblocks.size());
            CaheString("",GetMutexSize());
	        CaheString("",cBlockHttpCallback_->_rollbackblocks.size(),true);
    }

    switch (where) {

        case 0:
        {
            std::cout << ss.str();
            return std::string("hh");
        }break;
        case 1:{
            cblockcahe->_cache.clear();
            // blcohelper->missing_utxos.swap(emyp);
            // blcohelper->_broadcast_blocks.clear();
            // blcohelper->fast_sync_blocks.clear();
            // blcohelper->rollback_blocks.clear();
            // blcohelper->pending_blocks.clear();
            // blcohelper->hash_pending_blocks.clear();
            // blcohelper->utxo_missing_blocks.clear();
            // blcohelper->DoubleSpend_blocks.clear();
            // blcohelper->DuplicateChecker.clear();
            blockStroage_->_blockMap.clear();
            blockStroage_->_preHashMap.clear();
            blockStroage_->_blockStatusMap.clear();
            // DoubleSpendCache_cd->_pendingAddrs.clear();
            // FailedTransactionCache_cd->txPending.clear();
            // syncBlock_t->sync_from_zero_cache.clear();
            // syncBlock_t->sync_from_zero_reserve_heights.clear();
            // CtransactionCache_->_cache.clear();

            dbCache_->data_.clear();
            dbCache_->time_data_.clear();
            vrfo->vrfCache.clear();
            vrfo->txvrfCache.clear();
            vrfo->vrfVerifyNode.clear();
            manager._globalData.clear();
            manager2._globalData.clear();
            manager3._globalData.clear();
            // AccountManager_accountList.clear();
            phone_list.clear();
        }break;
        case 2:
        {
            std::ofstream file("cache.txt",std::ios::app);
            file << ss.str();
            file.close();
            return std::string("k");
        }break;
        case 3:
        {
            std::cout<<"start DesInstance"<<std::endl;
            MagicSingleton<Config>::DesInstance();
            MagicSingleton<TFSbenchmark>::DesInstance();
            MagicSingleton<ProtobufDispatcher>::DesInstance();
            MagicSingleton<AccountManager>::DesInstance();
            MagicSingleton<PeerNode>::DesInstance();
            MagicSingleton<UnregisterNode>::DesInstance();
            MagicSingleton<TimeUtil>::DesInstance();
            MagicSingleton<netTest>::DesInstance();
            MagicSingleton<Envelop>::DesInstance();
            MagicSingleton<echoTest>::DesInstance();
            MagicSingleton<BufferCrol>::DesInstance();
            MagicSingleton<CBlockCache>::DesInstance();
            MagicSingleton<BlockHelper>::DesInstance();
            MagicSingleton<TaskPool>::DesInstance();
            MagicSingleton<CBlockHttpCallback>::DesInstance();
            MagicSingleton<VRF>::DesInstance();
            MagicSingleton<BlockMonitor>::DesInstance();
            MagicSingleton<BlockStroage>::DesInstance();
            MagicSingleton<BounsAddrCache>::DesInstance();
            MagicSingleton<DoubleSpendCache>::DesInstance();
            MagicSingleton<FailedTransactionCache>::DesInstance();
            MagicSingleton<SyncBlock>::DesInstance();
            MagicSingleton<TransactionCache>::DesInstance();
            MagicSingleton<DBCache>::DesInstance();
            MagicSingleton<EpollMode>::DesInstance();
            MagicSingleton<WorkThreads>::DesInstance();

            //DestoryDB
            MagicSingleton<RocksDB>::GetInstance()->DestoryDB();
            MagicSingleton<RocksDB>::DesInstance();

            std::map<std::string, std::shared_ptr<GlobalData>> global_data_temp;
            GlobalDataManager & manager=GlobalDataManager::GetGlobalDataManager();
            GlobalDataManager & manager2=GlobalDataManager::GetGlobalDataManager2();
            GlobalDataManager & manager3=GlobalDataManager::GetGlobalDataManager3();
            manager._globalData.swap(global_data_temp);
            manager2._globalData.swap(global_data_temp);
            manager3._globalData.swap(global_data_temp);
            
            std::cout<<"end DesInstance"<<std::endl;

        }break;
    
    }
    return std::string("ddd");

}
    


