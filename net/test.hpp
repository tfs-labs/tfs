/**
 * *****************************************************************************
 * @file        test.hpp
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
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
    netTest()
    :_netTestTime(-1)
    ,_signal(true)
    ,_flag(false) 
    {}

    /**
     * @brief       Set the Time object
     * 
     * @param       t 
     */
    void SetTime(double t)
    {
        std::unique_lock<std::mutex> lock(_mutexTime);
        _flag = true;
        _netTestTime = t;
    }

    void IsValue()
    {
        int i = 0;
        for(; i < 20; i++)
        {
            if(_netTestTime == -1)
            {
                sleep(1);
                continue;
            }
            break;
        }

        std::unique_lock<std::mutex> lock(_mutexTime);
        _signal = true;
    }

    double GetTime()
    {
        std::unique_lock<std::mutex> lock(_mutexTime);
        double t = _netTestTime;
        _netTestTime = -1;
        _flag = false;
        return t;
    }

    bool GetSignal()
    {
        return _signal;
    }
    bool GetFlag()
    {
        return _flag;
    }

    void NetTestRegister()
    {
        _mutexTime.lock();
        _signal = false;
        _mutexTime.unlock();
        MagicSingleton<TaskPool>::GetInstance()->CommitCaTask(std::bind(&netTest::IsValue, this));
    }

private:
    double _netTestTime;
    bool _signal;
    bool _flag;
    std::mutex _mutexTime;
};

class echoTest
{
public:
    void AddEchoCatch(std::string time, std::string IP)
    {
        auto find = _echoCatch.find(time);
        if(find == _echoCatch.end())
        {
            std::unique_lock<std::mutex> lock(_echoMutex);
            _echoCatch.insert(std::make_pair(time, std::vector<std::string>()));
            _echoCatch[time].emplace_back(IP);
        }
        else
        {
            std::unique_lock<std::mutex> lock(_echoMutex);
            _echoCatch[time].emplace_back(IP);
        }
    }

    bool DeleteEchoCatch(std::string time)
    {
        auto find = _echoCatch.find(time);
        if(find == _echoCatch.end())
        {
            return false;
        }
        std::unique_lock<std::mutex> lock(_echoMutex);
        _echoCatch.erase(time);
        return true;
    }

    void AllClear()
    {
        _echoCatch.clear();
    }

    std::map<std::string, std::vector<std::string>> GetEchoCatch()
    {
        std::unique_lock<std::mutex> lock(_echoMutex);
        return _echoCatch;
    }
private:
friend std::string PrintCache(int where);
std::mutex _echoMutex;
std::map<std::string, std::vector<std::string>>  _echoCatch;

};



#endif 