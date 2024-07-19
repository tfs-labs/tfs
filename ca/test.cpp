#include <cstdint>
#include <iostream>
#include <sstream>
#include <stack>
#include <sys/types.h>
#include <time.h>
#include <fstream>
#include <string>

#include "ca/test.h"
#include "ca/global.h"
#include "ca/block_helper.h"
#include "ca/block_monitor.h"
#include "ca/block_stroage.h"
#include "ca/double_spend_cache.h"
#include "ca/block_http_callback.h"
#include "ca/failed_transaction_cache.h"
#include "ca/ca.h"
#include "ca/sync_block.h"

#include "net/test.hpp"
#include "net/epoll_mode.h"
#include "net/socket_buf.h"
#include "net/http_server.h"
#include "net/unregister_node.h"
#include "net/work_thread.h"


#include "utils/json.hpp"
#include "utils/envelop.h"
#include "utils/tmp_log.h"
#include "utils/console.h"
#include "utils/hex_code.h"
#include "utils/time_util.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/contract_utils.h"

#include "db/db_api.h"
#include "db/cache.h"
#include "include/logging.h"

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
            DEBUGLOG("blockHash:{}, blockSize:{}", hash.substr(0,6), strHeader.size());
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
        stream << "block Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    for (auto & verifySign : block.sign())
    {
        stream << "block signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    stream << "Time-> ";
    PrintFormatTime(block.time(), isConsoleOutput, stream);
    
    for (int i = 0; i < block.txs_size(); i++) 
    {
        CTransaction tx = block.txs(i);
        stream << "TX_INFO -----------> index[" << i << "]" << std::endl;
        PrintTx(tx, isConsoleOutput, stream);
    }

    stream << "Block data ------->"<<  block.data() << std::endl;
    return 0;
}

std::string PrintBlocks(int num, bool preHashFlag)
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
            if(preHashFlag)
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

std::string PrintBlocksHash(int num, bool preHashFlag)
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
            if(preHashFlag)
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

std::string PrintRangeBlocks(int startNum,int num, bool preHashFlag)
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
            if(preHashFlag)
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
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
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
            stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << greenColor.Color() << GenerateAddr(vin.vinsign().pub()) << greenColor.Reset() << "]" << std::endl;

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
            stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << "[" << greenColor.Color() << GenerateAddr(multiSign.pub()) << greenColor.Reset() << "]" << std::endl;
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
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << GenerateAddr(verifySign.pub()) << "]" << std::endl;  
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << GenerateAddr(verifySign.pub()) << "]" << std::endl;
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
            stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << GenerateAddr(vin.vinsign().pub()) << "]" << std::endl;

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
            stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << GenerateAddr(multiSign.pub()) << std::endl;
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
                dataMap.push_back(std::make_pair("CommissionRate", std::to_string(dataJson["TxInfo"]["CommissionRate"].get<double>())));
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
    jsonBlock["merkleroot"] = addHexPrefix(block.merkleroot());
    jsonBlock["prevhash"] = addHexPrefix(block.prevhash());
    jsonBlock["hash"] = addHexPrefix(block.hash());
    jsonBlock["height"] = block.height();
    jsonBlock["time"] = block.time();
    jsonBlock["bytes"] = block.ByteSizeLong();
    if(!block.data().empty())
    {
        nlohmann::json blockdataJson = nlohmann::json::parse(block.data());
        nlohmann::json modifiedJsonData;

        for (auto it = blockdataJson.begin(); it != blockdataJson.end(); ++it) {

            std::string originalKey = it.key();
            auto value = it.value();
            if(value.contains("dependentCTx"))
            {
                std::set<std::string> tempVec;
                for(auto & dep : value["dependentCTx"])
                {
                    tempVec.insert(addHexPrefix(dep));
                }
                value["dependentCTx"] = tempVec;
            }

            std::string modifiedKey = addHexPrefix(originalKey);
            modifiedJsonData[modifiedKey] = value;
        }
        blockdataJson = modifiedJsonData;
        jsonBlock["data"] = blockdataJson;
    }
    else
    {
        jsonBlock["data"] = "";
    }

    for(auto & blocksign : block.sign())
    {
        nlohmann::json block_verifySign;
        block_verifySign["sign"] = Base64Encode(blocksign.sign());
        block_verifySign["pub"] = Base64Encode(blocksign.pub());
        std::string sign_addr = GenerateAddr(blocksign.pub());
        block_verifySign["signaddr"] = addHexPrefix(sign_addr);

        jsonBlock["blocksign"].push_back(block_verifySign);
    }


    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
            Tx["time"] = tx.time();
            Tx["txHash"] = addHexPrefix(tx.hash());
            Tx["identity"] = addHexPrefix(tx.identity());

            for(auto & owner: tx.utxo().owner())
            {
                Tx["utxo"]["owner"].push_back(addHexPrefix(owner));
            }

            for(auto & vin : tx.utxo().vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    Tx["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                }

                nlohmann::json utxoVinsign;
                utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

                Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
            }
            //int count = 0;
            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                 if(vout.addr().substr(0, 6) == "Virtua")
                {
                   utxoVout["addr"] = vout.addr();
                    
                }
                else
                {
                    utxoVout["addr"] =  addHexPrefix(vout.addr());
                }
                
                utxoVout["value"] = vout.value();

                Tx["utxo"]["vout"].push_back(utxoVout); 
                //count += 1;
            }

            for(auto & multiSign : tx.utxo().multisign())
            {
                nlohmann::json utxoMultisign;
                utxoMultisign["sign"] = Base64Encode(multiSign.sign());
                utxoMultisign["pub"] = Base64Encode(multiSign.pub());

                Tx["utxo"]["multisign"].push_back(utxoMultisign);
            }

            Tx["Type"] = tx.type();
            Tx["info"] = tx.info();
            Tx["Consensus"] = tx.consensus();
            Tx["txType"] = tx.txtype();
 
            if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
            {
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("BonusAddr")) 
                {
                    std::string bonusAddr = dataJson["TxInfo"]["BonusAddr"].get<std::string>();
                    dataJson["TxInfo"]["BonusAddr"] = addHexPrefix(bonusAddr);
                }
                
                if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("DisinvestUtxo")) 
                {
                    std::string disinvestUtxo = dataJson["TxInfo"]["DisinvestUtxo"].get<std::string>();
                    dataJson["TxInfo"]["DisinvestUtxo"] = addHexPrefix(disinvestUtxo);
                }
                Tx["data"] = dataJson;
            }

            for(auto & verifySign : tx.verifysign())
            {
                nlohmann::json utxoVerifySign;
                utxoVerifySign["sign"] = Base64Encode(verifySign.sign());
                utxoVerifySign["pub"] = Base64Encode(verifySign.pub());
                std::string signAddr = GenerateAddr(verifySign.pub());
                utxoVerifySign["signaddr"] = addHexPrefix(signAddr);

                Tx["verifySign"].push_back(utxoVerifySign);
            }
            
            allTx[k++] = FixTxField(Tx);
        }
        else if(tx.type() == global::ca::kGenesisSign)
        {
            Tx["time"] = tx.time();
            Tx["txHash"] = addHexPrefix(tx.hash());
            Tx["identity"] = addHexPrefix(tx.identity());

            for(auto & owner: tx.utxo().owner())
            {
                Tx["utxo"]["owner"].push_back(addHexPrefix(owner));
            }

            for(auto & vin : tx.utxo().vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    Tx["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                }

                nlohmann::json utxoVinsign;
                utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

                Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                if(vout.addr() == global::ca::kVirtualStakeAddr || vout.addr() == global::ca::kVirtualInvestAddr 
                    || vout.addr() == global::ca::kVirtualBurnGasAddr ||vout.addr() == global::ca::kVirtualDeployContractAddr || 
                    vout.addr() == global::ca::kVirtualCallContractAddr)
                {
                   utxoVout["addr"] = vout.addr();
                }else
                {
                   utxoVout["addr"] = addHexPrefix(vout.addr());
                }
                utxoVout["value"] = vout.value();

                Tx["utxo"]["vout"].push_back(utxoVout); 
            }
            Tx["Type"] = tx.type();
            allTx[k++] = FixTxField(Tx);
        }
    }

    blocks["block"] = jsonBlock;
    blocks["tx"] = allTx;

}


std::string PrintCache(int where){
    std::string rocksdbUsage;
    MagicSingleton<RocksDB>::GetInstance()->GetDBMemoryUsage(rocksdbUsage);
    std::cout << rocksdbUsage << std::endl;
    std::stringstream ss;

    auto CaheString=[&](const std::string & whatCahe,uint64_t cacheSize,bool isEnd=false){
        std::time_t t = std::time(NULL);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&t))) {
        }

       ss << cacheSize <<( (isEnd) ? ",\n" :",");
       
    };

    auto blcohelper= MagicSingleton<BlockHelper>::GetInstance();
    auto blockMonitor_= MagicSingleton<BlockMonitor>::GetInstance();
    auto blockStroage_=MagicSingleton<BlockStroage>::GetInstance(); 
    auto DoubleSpendCache_cd=MagicSingleton<DoubleSpendCache>::GetInstance(); 
    auto  FailedTransactionCache_cd=MagicSingleton<FailedTransactionCache>::GetInstance(); 
    auto syncBlock_t= MagicSingleton<SyncBlock>::GetInstance();
    auto CtransactionCache_=MagicSingleton<TransactionCache>::GetInstance();
    auto vrfo= MagicSingleton<VRF>::GetInstance();
    auto TFPBenchMark_C=  MagicSingleton<TFSbenchmark>::GetInstance();

    GlobalDataManager & manager=GlobalDataManager::GetGlobalDataManager();
    
    
    auto UnregisterNode__= MagicSingleton<UnregisterNode>::GetInstance();

    auto bufcontrol =MagicSingleton<BufferCrol>::GetInstance();

    auto pernode = MagicSingleton<PeerNode>::GetInstance();

    auto dispach=MagicSingleton<ProtobufDispatcher>::GetInstance();

    auto _echoCatch = MagicSingleton<echoTest>::GetInstance();

    auto workThread =MagicSingleton<WorkThreads>::GetInstance();

    auto phone_list = global::g_phoneList;
    auto cBlockHttpCallback_ = MagicSingleton<CBlockHttpCallback>::GetInstance();


    std::stack<std::string> emyp;

    {
        CaheString("",blockStroage_->_preHashMap.size());
        CaheString("",blockStroage_->_blockStatusMap.size());

        CaheString("",vrfo->vrfCache.size());
        CaheString("",vrfo->txvrfCache.size());
        CaheString("", vrfo->vrfVerifyNode.size());

        CaheString("",manager._globalData.size());
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
            blockStroage_->_preHashMap.clear();
            blockStroage_->_blockStatusMap.clear();
    

            vrfo->vrfCache.clear();
            vrfo->txvrfCache.clear();
            vrfo->vrfVerifyNode.clear();
            manager._globalData.clear();

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
            MagicSingleton<BlockHelper>::DesInstance();
            MagicSingleton<TaskPool>::DesInstance();
            MagicSingleton<CBlockHttpCallback>::DesInstance();
            MagicSingleton<VRF>::DesInstance();
            MagicSingleton<BlockMonitor>::DesInstance();
            MagicSingleton<BlockStroage>::DesInstance();
            MagicSingleton<BonusAddrCache>::DesInstance();
            MagicSingleton<DoubleSpendCache>::DesInstance();
            MagicSingleton<FailedTransactionCache>::DesInstance();
            MagicSingleton<SyncBlock>::DesInstance();
            MagicSingleton<TransactionCache>::DesInstance();
            MagicSingleton<EpollMode>::DesInstance();
            MagicSingleton<WorkThreads>::DesInstance();

            MagicSingleton<RocksDB>::GetInstance()->DestoryDB();
            MagicSingleton<RocksDB>::DesInstance();

            std::map<std::string, std::shared_ptr<GlobalData>> global_data_temp;
            GlobalDataManager & manager=GlobalDataManager::GetGlobalDataManager();

            manager._globalData.swap(global_data_temp);

            
            std::cout<<"end DesInstance"<<std::endl;

        }break;
    
    }
    return std::string("ddd");

}

int PrintContractBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream)
{
    CaConsole bkColor(kConsoleColor_Blue, kConsoleColor_Black, true);
    CaConsole greenColor(kConsoleColor_Green, kConsoleColor_Black, true);
    auto tempTransactions = block.txs();
    if((global::ca::TxType)tempTransactions[0].txtype()!=global::ca::TxType::kTxTypeCallContract && (global::ca::TxType)tempTransactions[0].txtype()!=global::ca::TxType::kTxTypeDeployContract)
    {
        return -1;
    }
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
        stream << "block Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    for (auto & verifySign : block.sign())
    {
        stream << "block signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
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

std::string PrintContractBlocks(int num, bool preHashFlag)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.GetBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--)
    {
        str += (std::to_string(i) + "\t");
        std::vector<std::string> vBlockHashs;
        dbRead.GetBlockHashsByBlockHeight(i, vBlockHashs);
        std::sort(vBlockHashs.begin(), vBlockHashs.end());
        for (auto hash : vBlockHashs) 
        {
            std::string strHeader;
            dbRead.GetBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            auto tempTransactions = header.txs();
            if(preHashFlag)
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeCallContract || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeDeployContract)
                {
                    str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
                else
                {
                    str = str + hash.substr(0,6) +"(c)"+ "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
            }
            else
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeCallContract || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeDeployContract)
                {
                    str = str + hash.substr(0,6) +"(c)"+ " " ;
                }
                else
                {
                    str = str + hash.substr(0,6) + " ";
                }
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

std::string PrintRangeContractBlocks(int startNum,int num, bool preHashFlag)
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
            auto tempTransactions = header.txs();
            if(preHashFlag)
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeCallContract || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeDeployContract)
                {
                    str = str + hash.substr(0,6) +"(c)"+ "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
                else
                {
                    str = str + hash.substr(0,6)  +"(" + header.prevhash().substr(0,6) + ")" + " ";
                }
            }
            else
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeCallContract || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTxTypeDeployContract)
                {
                    str = str + hash.substr(0,6) +"(c)"+ " " ;
                }
                else
                {
                    str = str + hash.substr(0,6) + " ";
                }
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

nlohmann::json FixTxField(const nlohmann::json& inTx)
{
    nlohmann::json outTx = inTx;
    if (outTx.contains("data") && outTx["data"].contains("TxInfo") && outTx["data"]["TxInfo"].contains("recipient") && outTx["data"]["TxInfo"].contains("sender")) 
    {
        std::string recipient = outTx["data"]["TxInfo"]["recipient"].get<std::string>();
        outTx["data"]["TxInfo"]["recipient"] = addHexPrefix(recipient);

        std::string sender = outTx["data"]["TxInfo"]["sender"].get<std::string>();
        outTx["data"]["TxInfo"]["sender"] = addHexPrefix(sender);
    }

    if (outTx.contains("data") && outTx["data"].contains("TxInfo") && outTx["data"]["TxInfo"].contains("contractDeployer"))     
    {
        std::string contractDeployer = outTx["data"]["TxInfo"]["contractDeployer"].get<std::string>();
        outTx["data"]["TxInfo"]["contractDeployer"] = addHexPrefix(contractDeployer);
    }

    return outTx;
}

    
std::string TxInvet(const CTransaction& tx)
{
    nlohmann::json outTxJs;
    if(tx.type() == global::ca::kTxSign)
    {   
        outTxJs["time"] = tx.time();
        outTxJs["txHash"] = addHexPrefix(tx.hash());
        outTxJs["identity"] = addHexPrefix(tx.identity());

        for(auto & owner: tx.utxo().owner())
        {
            outTxJs["utxo"]["owner"].push_back(addHexPrefix(owner));
        }

        for(auto & vin : tx.utxo().vin())
        {
            for(auto &prevout : vin.prevout())
            {
                outTxJs["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
            }

            nlohmann::json utxoVinsign;
            utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
            utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

            outTxJs["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
        }
        //int count = 0;
        for(auto & vout : tx.utxo().vout())
        {
            nlohmann::json utxoVout;
            if(vout.addr().substr(0, 6) == "Virtua")
            {
            utxoVout["addr"] = vout.addr();
                
            }
            else
            {
                utxoVout["addr"] =  addHexPrefix(vout.addr());
            }
            
            utxoVout["value"] = vout.value();

            outTxJs["utxo"]["vout"].push_back(utxoVout); 
            //count += 1;
        }

        for(auto & multiSign : tx.utxo().multisign())
        {
            nlohmann::json utxoMultisign;
            utxoMultisign["sign"] = Base64Encode(multiSign.sign());
            utxoMultisign["pub"] = Base64Encode(multiSign.sign());

            outTxJs["utxo"]["multisign"].push_back(utxoMultisign);
        }

        outTxJs["Type"] = tx.type();
        outTxJs["info"] = tx.info();
        outTxJs["Consensus"] = tx.consensus();
        outTxJs["txType"] = tx.txtype();

        if((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx)
        {
            nlohmann::json dataJson = nlohmann::json::parse(tx.data());
            // if()
    
            if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("BonusAddr")) {

            std::string bonusAddr = dataJson["TxInfo"]["BonusAddr"].get<std::string>();
    
                
                dataJson["TxInfo"]["BonusAddr"] = addHexPrefix(bonusAddr);;
            }

            if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("DisinvestUtxo")) 
            {
                std::string disinvestUtxo = dataJson["TxInfo"]["DisinvestUtxo"].get<std::string>();
                dataJson["TxInfo"]["DisinvestUtxo"] = addHexPrefix(disinvestUtxo);
            }

            outTxJs["data"] = dataJson;
        }

        for(auto & verifySign : tx.verifysign())
        {
            nlohmann::json utxoVerifySign;
            utxoVerifySign["sign"] = Base64Encode(verifySign.sign());
            utxoVerifySign["pub"] = Base64Encode(verifySign.pub());
            std::string signAddr = GenerateAddr(verifySign.pub());
            utxoVerifySign["signaddr"] = addHexPrefix(signAddr);

            outTxJs["verifySign"].push_back(utxoVerifySign);
        }
    }
    else if(tx.type() == global::ca::kGenesisSign)
    {
        outTxJs["time"] = tx.time();
        outTxJs["txHash"] = addHexPrefix(tx.hash());
        outTxJs["identity"] = addHexPrefix(tx.identity());

        for(auto & owner: tx.utxo().owner())
        {
            outTxJs["utxo"]["owner"].push_back(addHexPrefix(owner));
        }

        for(auto & vin : tx.utxo().vin())
        {
            for(auto &prevout : vin.prevout())
            {
                outTxJs["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
            }

            nlohmann::json utxoVinsign;
            utxoVinsign["sign"] = Base64Encode(vin.vinsign().sign());
            utxoVinsign["pub"] = Base64Encode(vin.vinsign().pub());

            outTxJs["utxo"]["vin"]["vinsign"].push_back(utxoVinsign);
        }

        for(auto & vout : tx.utxo().vout())
        {
            nlohmann::json utxoVout;
            if(vout.addr() == global::ca::kVirtualStakeAddr || vout.addr() == global::ca::kVirtualInvestAddr 
                || vout.addr() == global::ca::kVirtualBurnGasAddr ||vout.addr() == global::ca::kVirtualDeployContractAddr || 
                vout.addr() == global::ca::kVirtualCallContractAddr)
            {
            utxoVout["addr"] = vout.addr();
            }else
            {
            utxoVout["addr"] = addHexPrefix(vout.addr());
            }
            utxoVout["value"] = vout.value();

            outTxJs["utxo"]["vout"].push_back(utxoVout); 
        }
        outTxJs["Type"] = tx.type();
    }

    nlohmann::json txJs = FixTxField(outTxJs);
    return txJs.dump();
}


