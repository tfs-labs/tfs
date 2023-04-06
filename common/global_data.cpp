#include "common/global_data.h"
#include "utils/MagicSingleton.h"
#include "utils/time_util.h"
#include "utils/AccountManager.h"

struct GlobalData
{
    std::string msg_id;
    std::uint32_t ret_num;
    std::uint32_t time_out_sec;
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::string> data;
};

bool GlobalDataManager::CreateWait(uint32_t time_out_sec, uint32_t ret_num, std::string &out_msg_id)
{
    out_msg_id = getsha256hash(std::to_string(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp()));
    std::shared_ptr<GlobalData> data_ptr = std::make_shared<GlobalData>();
    data_ptr->msg_id = out_msg_id;
    data_ptr->time_out_sec = time_out_sec;
    data_ptr->ret_num = ret_num;
    {
        std::lock_guard lock(global_data_mutex_);
        global_data_.insert(std::make_pair(data_ptr->msg_id, data_ptr));
    }

    return true;
}

bool GlobalDataManager::AddWaitData(const std::string &msg_id, const std::string &data)
{
    std::shared_ptr<GlobalData> data_ptr;
    {
        std::lock_guard lock(global_data_mutex_);
        auto it = global_data_.find(msg_id);
        if (global_data_.end() == it)
        {
            return false;
        }
        data_ptr = it->second;
    }
    {
        std::lock_guard lock(data_ptr->mutex);
        data_ptr->data.push_back(data);
        if (data_ptr->data.size() >= data_ptr->ret_num)
        {
            data_ptr->condition.notify_all();
        }
    }
    return true;
}

bool GlobalDataManager::WaitData(const std::string &msg_id, std::vector<std::string> &ret_data)
{
    std::shared_ptr<GlobalData> data_ptr;
    {
        std::lock_guard lock(global_data_mutex_);
        auto it = global_data_.find(msg_id);
        if (global_data_.end() == it)
        {
            return false;
        }
        data_ptr = it->second;
    }
    std::unique_lock<std::mutex> lock(data_ptr->mutex);
    bool flag = true;
    if (data_ptr->data.size() < data_ptr->ret_num)
    {
        data_ptr->condition.wait_for(lock, std::chrono::seconds(data_ptr->time_out_sec));
    }
    data_ptr->mutex.unlock();
    {
        std::lock_guard lock(global_data_mutex_);
        auto it = global_data_.find(msg_id);
        if (global_data_.end() != it)
        {
            global_data_.erase(it);
        }
    }
    {
        std::lock_guard lock(data_ptr->mutex);
        ret_data = data_ptr->data;
    }
    if (ret_data.size() < data_ptr->ret_num)
    {
        flag = false;
    }
    return flag;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager()
{
    static GlobalDataManager g_global_data_mgr;
    return g_global_data_mgr;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager2()
{
    static GlobalDataManager g_global_data_mgr;
    return g_global_data_mgr;
}

GlobalDataManager &GlobalDataManager::GetGlobalDataManager3()
{
    static GlobalDataManager g_global_data_mgr;
    return g_global_data_mgr;
}
