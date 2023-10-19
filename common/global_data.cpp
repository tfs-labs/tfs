#include "common/global_data.h"
#include "utils/magic_singleton.h"
#include "utils/time_util.h"
#include "utils/account_manager.h"

struct GlobalData
{
    std::string msgId;
    std::uint32_t retNum;
    std::uint32_t timeOutSec;
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::string> data;
};

bool GlobalDataManager::CreateWait(uint32_t timeOutSec, uint32_t retNum, std::string &outMsgId)
{
    outMsgId = Getsha256hash(std::to_string(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp()));
    std::shared_ptr<GlobalData> dataPtr = std::make_shared<GlobalData>();
    dataPtr->msgId = outMsgId;
    dataPtr->timeOutSec = timeOutSec;
    dataPtr->retNum = retNum;
    {
        std::lock_guard lock(_globalDataMutex);
        _globalData.insert(std::make_pair(dataPtr->msgId, dataPtr));
    }

    return true;
}

bool GlobalDataManager::AddWaitData(const std::string &msgId, const std::string &data)
{
    std::shared_ptr<GlobalData> dataPtr;
    {
        std::lock_guard lock(_globalDataMutex);
        auto it = _globalData.find(msgId);
        if (_globalData.end() == it)
        {
            return false;
        }
        dataPtr = it->second;
    }
    {
        std::lock_guard lock(dataPtr->mutex);
        dataPtr->data.push_back(data);
        if (dataPtr->data.size() >= dataPtr->retNum)
        {
            dataPtr->condition.notify_all();
        }
    }
    return true;
}

bool GlobalDataManager::WaitData(const std::string &msgId, std::vector<std::string> &retData)
{
    std::shared_ptr<GlobalData> dataPtr;
    {
        std::lock_guard lock(_globalDataMutex);
        auto it = _globalData.find(msgId);
        if (_globalData.end() == it)
        {
            return false;
        }
        dataPtr = it->second;
    }
    std::unique_lock<std::mutex> lock(dataPtr->mutex);
    bool flag = true;
    if (dataPtr->data.size() < dataPtr->retNum)
    {
        dataPtr->condition.wait_for(lock, std::chrono::seconds(dataPtr->timeOutSec));
    }
    dataPtr->mutex.unlock();
    {
        std::lock_guard lock(_globalDataMutex);
        auto it = _globalData.find(msgId);
        if (_globalData.end() != it)
        {
            _globalData.erase(it);
        }
    }
    {
        std::lock_guard lock(dataPtr->mutex);
        retData = dataPtr->data;
    }
    if (retData.size() < dataPtr->retNum)
    {
        flag = false;
    }
    return flag;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager()
{
    static GlobalDataManager g_globalDataMgr;
    return g_globalDataMgr;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager2()
{
    static GlobalDataManager g_globalDataMgr;
    return g_globalDataMgr;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager3()
{
    static GlobalDataManager g_globalDataMgr;
    return g_globalDataMgr;
}
