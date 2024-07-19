/**
 * *****************************************************************************
 * @file        msg_queue.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <condition_variable>

#include "./ip_port.h"
#include "./peer_node.h"

#include "../include/logging.h"

enum DataType
{
    E_READ,
    E_WORK,
    E_WRITE
};

typedef struct MsgData
{
    DataType type;
    int fd;
    uint16_t port;
    uint32_t ip;
    std::string data;
    NetPack pack;
    std::string id;
}MsgData;


struct MsgDataCompare
{
	bool operator () (MsgData & a, MsgData & b)
	{
		return (a.pack.flag & 0xF) < (b.pack.flag & 0xF);
	}
};


class MsgQueue
{
public:
    std::string strInfo;
    std::priority_queue<MsgData, std::vector<MsgData>, MsgDataCompare> msgQueue;
	std::mutex mutexForList;

	std::condition_variable_any notEmpty;	
	std::condition_variable_any notFull;	
	size_t maxSize;						

public:
	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool IsFull() const
	{
		return msgQueue.size() == maxSize;
	}

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool IsEmpty() const
	{
		return msgQueue.empty();
	}
	
	/**
	 * @brief       
	 * 
	 * @param       data 
	 * @return      true 
	 * @return      false 
	 */
    bool Push(MsgData& data)
    {
		std::lock_guard<std::mutex> lck(mutexForList);
		while (IsFull())
		{
			DEBUGLOG(" {} the blocking queue is full,waiting...",strInfo.c_str());
			notFull.wait(mutexForList);
		}
		msgQueue.push(std::move(data));
		notEmpty.notify_one();

		return true;
    };

	/**
	 * @brief       
	 * 
	 * @param       out 
	 * @return      true 
	 * @return      false 
	 */
    bool TryWaitTop(MsgData & out)
    {
		std::lock_guard<std::mutex> lck(mutexForList);
		while (IsEmpty())
		{
			notEmpty.wait(mutexForList);
		}

		out = std::move(msgQueue.top());
		msgQueue.pop();
		notFull.notify_one();
		return true;
    };
    
	/**
	 * @brief       
	 * 
	 * @param       ret 
	 * @return      true 
	 * @return      false 
	 */
    bool Pop(MsgData *ret)
    {
	    return ret;
    };

    MsgQueue() : strInfo(""), maxSize(10000 * 5) {}
	MsgQueue(std::string strInfo) : maxSize(10000 * 5)
	{
         strInfo = strInfo;
    }
    ~MsgQueue() = default;
};


#endif
