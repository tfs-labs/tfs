#ifndef __CA_BLOCK_HTTP_CALLBACK_H__
#define __CA_BLOCK_HTTP_CALLBACK_H__

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iomanip>
#include "block.pb.h"



class CBlockHttpCallback
{
public:
    CBlockHttpCallback();

    bool AddBlock(const std::string& block);
    bool AddBlock(const CBlock& block);

    bool RollbackBlockStr(const std::string& block);
    bool RollbackBlock(const CBlock& block);

    
    bool Start(const std::string& ip, int port, const std::string& path);
    void Stop();
    int AddBlockWork(const std::string &method);
    int RollbackBlockWork(const std::string &method);
    bool IsRunning();
    void Test();

    static int Process(CBlockHttpCallback* self);
     
private:
    std::string ToJson(const CBlock& block);
    bool SendBlockHttp(const std::string& block, const std::string& method);

private:
    std::vector<std::string>    addblocks_;
    std::vector<std::string>    rollbackblocks_;
    std::thread                 work_addblock_thread_;
    std::thread                 work_rollback_thread_;
    std::mutex                  add_mutex_;
    std::mutex                  rollback_mutex_;
    std::condition_variable     cvadd_;
    std::condition_variable     cvrollback_;
    volatile std::atomic_bool   running_;

    std::string ip_;
    uint32_t port_;
    std::string path_;
};

#endif