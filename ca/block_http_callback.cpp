#include <sstream>
#include <random>

#include "ca/global.h"
#include "ca/block_http_callback.h"

#include "include/logging.h"
#include "include/scope_guard.h"

#include "db/db_api.h"
#include "net/httplib.h"
#include "common/config.h"
#include "utils/magic_singleton.h"
#include "test.h"
#include "ca.h"


CBlockHttpCallback::CBlockHttpCallback() : _running(false),_ip("localhost"),_port(11190),_path("/tfsBrowser/block")
{
    Config::HttpCallback httpCallback = {};
    MagicSingleton<Config>::GetInstance()->GetHttpCallback(httpCallback);
    if (!httpCallback.ip.empty() && httpCallback.port > 0)
    {
        this->Start(httpCallback.ip, httpCallback.port, httpCallback.path);
    }
    else
    {
        ERRORLOG("Http callback is not config!");
    }
}

bool CBlockHttpCallback::AddBlock(const std::string& block)
{
    if (block.empty())
        return false;
    std::unique_lock<std::mutex> lck(_addMutex);
    _addblocks.push_back( block );
    _cvadd.notify_all();

    return true;
}

bool CBlockHttpCallback::AddBlock(const CBlock& block)
{
    std::string json = ToJson(block);
    return AddBlock(json);
}

bool CBlockHttpCallback::RollbackBlockStr(const std::string& block)
{
      if (block.empty())
        return false;
        
    std::unique_lock<std::mutex> lck(_rollbackMutex);
    _rollbackblocks.push_back( block );
    _cvrollback.notify_all();

    return true;
}


bool CBlockHttpCallback::RollbackBlock(const CBlock& block)
{
    std::string json = ToJson(block);
    return RollbackBlockStr(json);
}


void CBlockHttpCallback::AddBlockWork(const std::string &method)
{
    while (_running)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(_addMutex);
            while (_addblocks.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                _cvadd.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = _addblocks.front();
            _addblocks.erase(_addblocks.begin());
        }

        SendBlockHttp(currentBlock,method);
    }
}


void CBlockHttpCallback::RollbackBlockWork(const std::string &method)
{
    while (_running)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(_rollbackMutex);
            while (_rollbackblocks.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                _cvrollback.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = _rollbackblocks.front();
            _rollbackblocks.erase(_rollbackblocks.begin());
        }
        SendBlockHttp(currentBlock,method);
    }
}

void CBlockHttpCallback::Start(const std::string& ip, int port,const std::string& path)
{
    _ip = ip;
    _port = port;
    _path = path;
    _running = true;
    const std::string method1 = "/addblock";
    const std::string method2 = "/rollbackblock";
    _workAddblockThread = std::thread(std::bind(&CBlockHttpCallback::AddBlockWork, this, method1));
    _workRollbackThread = std::thread(std::bind(&CBlockHttpCallback::RollbackBlockWork, this, method2));
    _workAddblockThread.detach();
    _workRollbackThread.detach();
}

void CBlockHttpCallback::Stop()
{
    _running = false;
}

bool CBlockHttpCallback::IsRunning()
{
    return _running;
}

bool CBlockHttpCallback::SendBlockHttp(const std::string& block,const std::string &method)
{
    httplib::Client client(_ip, _port);
    std::string path = _path + method;
    auto res = client.Post(path.data(), block, "application/json");
    if (res)
    {
        DEBUGLOG("status:{}, Content-Type:{}, body:{}", res->status, res->get_header_value("Content-Type"), res->body);
    }
    else
    {
        DEBUGLOG("Client post failed");
    }

    return (bool)res;
}

std::string CBlockHttpCallback::ToJson(const CBlock& block)
{
    nlohmann::json allTx;
    nlohmann::json jsonBlock;
    jsonBlock["hash"] = addHexPrefix(block.hash());
    jsonBlock["height"] = block.height();
    jsonBlock["time"] = block.time();
    if(!block.data().empty())
    {
        nlohmann::json blockdataJson = nlohmann::json::parse(block.data());
        nlohmann::json modifiedJsonData;


        for (auto it = blockdataJson.begin(); it != blockdataJson.end(); ++it) {

            std::string originalKey = it.key();
            auto value = it.value();
            std::string modifiedKey = addHexPrefix(originalKey);
            modifiedJsonData[modifiedKey] = value;
        }
        blockdataJson = modifiedJsonData;

        if(blockdataJson.contains("dependentCTx") && !blockdataJson["dependentCTx"].get<std::string>().empty()){
            blockdataJson["dependentCTx"] = addHexPrefix(blockdataJson["dependentCTx"].get<std::string>());
        }
        else
        {
            blockdataJson["dependentCTx"] = "";
        }
        jsonBlock["blockdata"] = blockdataJson;
        
    }
    else
    {
        jsonBlock["blockdata"] = "";
    }
    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
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
            
            Tx["time"] = tx.time();
            Tx["hash"] = addHexPrefix(tx.hash());

            for(auto & owner: tx.utxo().owner())
            {
                Tx["from"].push_back(addHexPrefix(owner));
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                utxoVout["pub"] = addHexPrefix(vout.addr());
                utxoVout["value"] = vout.value();

                Tx["to"].push_back(utxoVout); 
            }

            Tx["type"] = tx.txtype();

            allTx[k++] = FixTxField(Tx);
        }
        else if(tx.type() == global::ca::kGenesisSign)
        {
            Tx["time"] = tx.time();
            Tx["hash"] = addHexPrefix(tx.hash());

            for(auto & owner: tx.utxo().owner())
            {
                Tx["from"].push_back(addHexPrefix(owner));
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxoVout;
                utxoVout["addr"] = addHexPrefix(vout.addr());
                utxoVout["value"] = vout.value();

                Tx["to"].push_back(utxoVout); 
            }
            Tx["type"] = tx.txtype();
            allTx[k++] = FixTxField(Tx);
        }
    }
    
    jsonBlock["tx"] = allTx;
    
    std::string json = jsonBlock.dump(4);
    return json;
}

void CBlockHttpCallback::Test()
{
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, 32767);

    std::stringstream stream;
    stream << "Test http callback, ID: " << dist(mt);
    std::string testStr = stream.str();
    AddBlock(testStr);
}

