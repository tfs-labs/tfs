
#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <condition_variable>
#include "./ip_port.h"
#include "../include/logging.h"
#include "./peer_node.h"


using namespace std;
enum DataType
{
    E_READ,
    E_WORK,
    E_WRITE
};

typedef struct msg_data
{
    DataType type;
    int fd;
    uint16_t port;
    uint32_t ip;
    std::string data;
    net_pack pack;
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
    std::string str_info_;
    std::priority_queue<MsgData, std::vector<MsgData>, MsgDataCompare> msg_queue_;
	std::mutex mutex_for_list_;

	std::condition_variable_any m_notEmpty;	
	std::condition_variable_any m_notFull;	
	size_t max_size_;						

public:
	bool IsFull() const
	{
		return msg_queue_.size() == max_size_;
	}
	bool IsEmpty() const
	{
		return msg_queue_.empty();
	}

public:
    bool push(MsgData& data)
    {
		std::lock_guard<std::mutex> lck(mutex_for_list_);
		while (IsFull())
		{
			DEBUGLOG(" {} the blocking queue is full,waiting...",str_info_.c_str());
			m_notFull.wait(mutex_for_list_);
		}
		msg_queue_.push(std::move(data));
		m_notEmpty.notify_one();

		return true;
    };

    bool try_wait_pop(MsgData & out)
    {
		std::lock_guard<std::mutex> lck(mutex_for_list_);
		while (IsEmpty())
		{
			m_notEmpty.wait(mutex_for_list_);
		}

		out = std::move(msg_queue_.top());
		msg_queue_.pop();
		m_notFull.notify_one();
		return true;
    };
    
    bool pop(MsgData *ret)
    {
	    return ret;
    };

    MsgQueue() : str_info_(""), max_size_(10000 * 5)
	{

	}
	
	MsgQueue(std::string strInfo) : max_size_(10000 * 5)
	{
         str_info_ = strInfo;
    }
    
	~MsgQueue() = default;
};


#endif
