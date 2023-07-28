#include "ca_block_http_callback.h"
#include "net/httplib.h"
#include "common/config.h"
#include "include/ScopeGuard.h"
#include "include/logging.h"
#include "ca_txhelper.h"
#include "ca_global.h"
#include "utils/MagicSingleton.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <random>
#include "db/db_api.h"

CBlockHttpCallback::CBlockHttpCallback() : running_(false),ip_("localhost"),port_(11190),path_("/donetBrowser/block")
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
    std::unique_lock<std::mutex> lck(add_mutex_);
    addblocks_.push_back( block );
    cvadd_.notify_all();

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
        
    std::unique_lock<std::mutex> lck(rollback_mutex_);
    rollbackblocks_.push_back( block );
    cvrollback_.notify_all();

    return true;
}


bool CBlockHttpCallback::RollbackBlock(const CBlock& block)
{
    std::string json = ToJson(block);
    return RollbackBlockStr(json);
}


int CBlockHttpCallback::AddBlockWork(const std::string &method)
{
    while (running_)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(add_mutex_);
            while (addblocks_.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                cvadd_.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = addblocks_.front();
            addblocks_.erase(addblocks_.begin());
        }

        SendBlockHttp(currentBlock,method);
    }

    return true;
}


int CBlockHttpCallback::RollbackBlockWork(const std::string &method)
{
    while (running_)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(rollback_mutex_);
            while (rollbackblocks_.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                cvrollback_.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = rollbackblocks_.front();
            rollbackblocks_.erase(rollbackblocks_.begin());
        }
        SendBlockHttp(currentBlock,method);
    }

    return true;
}

bool CBlockHttpCallback::Start(const std::string& ip, int port,const std::string& path)
{
    ip_ = ip;
    port_ = port;
    path_ = path;
    running_ = true;
    const std::string method1 = "/addblock";
    const std::string method2 = "/rollbackblock";
    work_addblock_thread_ = std::thread(std::bind(&CBlockHttpCallback::AddBlockWork, this, method1));
    work_rollback_thread_ = std::thread(std::bind(&CBlockHttpCallback::RollbackBlockWork, this, method2));
    work_addblock_thread_.detach();
    work_rollback_thread_.detach();
    return true;
}

void CBlockHttpCallback::Stop()
{
    running_ = false;
}

bool CBlockHttpCallback::IsRunning()
{
    return running_;
}

bool CBlockHttpCallback::SendBlockHttp(const std::string& block,const std::string &method)
{
    httplib::Client client(ip_, port_);
    std::string path = path_ + method;
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
    nlohmann::json Block;
    Block["hash"] = block.hash();
    Block["height"] = block.height();
    Block["time"] = block.time();

    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
            if((global::ca::TxType)tx.txtype() == global::ca::TxType::kTxTypeBonus)
            {
                nlohmann::json data_json = nlohmann::json::parse(tx.data());
                nlohmann::json tx_info = data_json["TxInfo"].get<nlohmann::json>();
                Tx["data"] = tx_info;
            }
            
            Tx["time"] = tx.time();
            Tx["hash"] = tx.hash();

            for(auto & owner: tx.utxo().owner())
            {
                Tx["from"].push_back(owner);
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxo_vout;
                utxo_vout["pub"] = vout.addr();
                utxo_vout["value"] = vout.value();

                Tx["to"].push_back(utxo_vout); 
            }

            Tx["type"] = tx.txtype();

            allTx[k++] = Tx;
        }
        else if(tx.type() == global::ca::kGenesisSign)
        {
            Tx["time"] = tx.time();
            Tx["hash"] = tx.hash();

            for(auto & owner: tx.utxo().owner())
            {
                Tx["from"].push_back(owner);
            }

            for(auto & vout : tx.utxo().vout())
            {
                nlohmann::json utxo_vout;
                utxo_vout["addr"] = vout.addr();
                utxo_vout["value"] = vout.value();

                Tx["to"].push_back(utxo_vout); 
            }
            Tx["type"] = tx.txtype();
            allTx[k++] = Tx;
        }
    }
    
    Block["tx"] = allTx;
    
    std::string json = Block.dump(4);
    return json;
}

void CBlockHttpCallback::Test()
{
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, 32767);

    std::stringstream stream;
    stream << "Test http callback, ID: " << dist(mt);
    std::string test_str = stream.str();
    AddBlock(test_str);
}

