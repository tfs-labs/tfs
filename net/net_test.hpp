#ifndef _NET_TEST_H_
#define _NET_TEST_H_

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "../common/task_pool.h"

class netTest
{
    
public:
    netTest():netTestTime(-1),signal(true),flag(false) {}

    void setTime(double t)
    {
        std::unique_lock<std::mutex> lock(mutex_time);
        flag = true;
        netTestTime = t;
    }

    void isValue()
    {
        int i = 0;
        for(; i < 20; i++)
        {
            if(netTestTime == -1)
            {
                sleep(1);
                continue;
            }
            break;
        }

        std::unique_lock<std::mutex> lock(mutex_time);
        signal = true;
    }

    double getTime()
    {
        std::unique_lock<std::mutex> lock(mutex_time);
        double t = netTestTime;
        netTestTime = -1;
        flag = false;
        return t;
    }

    bool getSignal()
    {
        return signal;
    }
    bool getflag()
    {
        return flag;
    }

    void netTestRegister()
    {
        mutex_time.lock();
        signal = false;
        mutex_time.unlock();
        MagicSingleton<taskPool>::GetInstance()->commit_ca_task(std::bind(&netTest::isValue, this));
    }

private:
    double netTestTime;
    bool signal;
    bool flag;
    std::mutex mutex_time;
};

class echoTest
{
public:
    void add_echo_catch(std::string time, std::string IP)
    {
        auto find = echo_catch.find(time);
        if(find == echo_catch.end())
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            echo_catch.insert(std::make_pair(time, std::vector<std::string>()));
            echo_catch[time].emplace_back(IP);
        }
        else
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            echo_catch[time].emplace_back(IP);
        }
    }

    bool delete_echo_catch(std::string time)
    {
        auto find = echo_catch.find(time);
        if(find == echo_catch.end())
        {
            return false;
        }
        std::unique_lock<std::mutex> lock(m_mutex);
        echo_catch.erase(time);
        return true;
    }

    void all_clear()
    {
        echo_catch.clear();
    }

    std::map<std::string, std::vector<std::string>> get_echo_catch()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return echo_catch;
    }
private:
std::mutex m_mutex;
std::map<std::string, std::vector<std::string>>  echo_catch;

};



#endif 