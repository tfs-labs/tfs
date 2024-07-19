#include "./work_thread.h"

#include "./socket_buf.h"
#include "./epoll_mode.h"
#include "./pack.h"
#include "./ip_port.h"
#include "./peer_node.h"
#include "./global.h"
#include "./net/dispatcher.h"

#include "../proto/net.pb.h"
#include "../net/interface.h"
#include "../utils/console.h"
#include "../include/logging.h"
#include "../common/bind_thread.h"
#include "key_exchange.h"

//cpu Bind the CPU
void BindCpu()
{
	if (global::g_cpuNums >= 4)
	{
		int index = GetCpuIndex();
		SysThreadSetCpu(index);
	}
}

void WorkThreads::Start()
{
	int k = 0;

	int workNum = 128;
	INFOLOG("will create {} threads", workNum);
	if(0 == workNum)
	{
		workNum = global::g_cpuNums * 2;
	}
    if(0 == workNum)
    {
        workNum = 8;
    }

	for (auto i = 0; i < 8; i++)
	{
		this->_threadsReadList.push_back(std::thread(WorkThreads::WorkRead, i));
		this->_threadsReadList[i].detach();
	}

	for (auto i = 0; i < 8; i++)
	{
		this->_threadsTransList.push_back(std::thread(WorkThreads::WorkWrite, i));
		this->_threadsTransList[i].detach();
	}

	for (auto i = 0; i < workNum; i++)
	{
		this->_threadsWorkList.push_back(std::thread(WorkThreads::Work, i));
		this->_threadsWorkList[i].detach();
	}	
}

void WorkThreads::WorkWrite(int id)
{

	BindCpu();
	while (1)
	{
		MsgData data;
		if (false == global::g_queueWrite.TryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_WRITE: 
			WorkThreads::HandleNetWrite(data);
			break;
		default:
			INFOLOG(YELLOW "WorkWrite drop data: data.fd :{}" RESET, data.fd);
			break;
		}
	}
}
void WorkThreads::WorkRead(int id) //Read socket dedicated thread
{
	BindCpu();

	while (1)
	{
		MsgData data;
		if (false == global::g_queueRead.TryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_READ:
			WorkThreads::HandleNetRead(data);
			break;
		default:
			INFOLOG(YELLOW "WorkRead drop data: data.fd {}" RESET, data.fd);
			break;
		}
	}
}

void WorkThreads::Work(int id)
{

	BindCpu();

	while (1)
	{
		MsgData data;
		if (false == global::g_queueWork.TryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_WORK:

			WorkThreads::HandleNetworkRead(data);
			break;
		default:
			INFOLOG("drop data: data.fd :{}", data.fd);
			break;
		}
	}
}

int WorkThreads::HandleNetworkRead(MsgData &data)
{
	MagicSingleton<ProtobufDispatcher>::GetInstance()->Handle(data);
	return 0;
}


int WorkThreads::HandleNetRead(const MsgData &data)
{
	int nread = 0;
	char buf[MAXLINE] = {0};
	do
	{
		nread = 0;
		memset(buf, 0, sizeof(buf));
		nread = read(data.fd, buf, MAXLINE); 
		if (nread > 0)
		{
			MagicSingleton<BufferCrol>::GetInstance()->AddReadBufferQueue(data.ip, data.port, buf, nread);
		}		
		if (nread == 0 && errno != EAGAIN)
		{
			DEBUGLOG("++++HandleNetRead++++ ip:({}) port:({}) fd:({})",IpPort::IpSz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->DeleteByFd(data.fd);
			MagicSingleton<KeyExchangeManager>::GetInstance()->removeKey(data.fd);
			return -1;
		}
		if (nread < 0)
		{
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
			{
				MagicSingleton<EpollMode>::GetInstance()->EpollLoop(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
				return 0;
			}

			DEBUGLOG("++++HandleNetRead++++ ip:({}) port:({}) fd:({})",IpPort::IpSz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->DeleteByFd(data.fd);
			MagicSingleton<KeyExchangeManager>::GetInstance()->removeKey(data.fd);
			return -1;
		}
	} while (nread >= MAXLINE);

	MagicSingleton<EpollMode>::GetInstance()->EpollLoop(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return 0;
}

bool WorkThreads::HandleNetWrite(const MsgData &data)
{
	auto port_and_ip = net_com::DataPackPortAndIp(data.port, data.ip);

	if (!MagicSingleton<BufferCrol>::GetInstance()->IsExists(port_and_ip))
	{
		DEBUGLOG("!MagicSingleton<BufferCrol>::GetInstance()->IsExists({})", port_and_ip);
		return false;
	}
	if(data.fd < 0)
	{
		ERRORLOG("HandleNetWrite fd < 0");
		return false;
	}
	std::mutex& buff_mutex = GetFdMutex(data.fd);
	std::lock_guard<std::mutex> lck(buff_mutex);
	std::string buff = MagicSingleton<BufferCrol>::GetInstance()->GetWriteBufferQueue(port_and_ip);
	
	if (0 == buff.size())
	{
		return true;
	}

	auto ret = net_tcp::Send(data.fd, buff.c_str(), buff.size(), 0);

	if (ret == -1)
	{
		ERRORLOG("net_tcp::Send error");
		return false;
	}
	if (ret > 0 && ret < (int)buff.size())
	{
		
		MagicSingleton<BufferCrol>::GetInstance()->PopNWriteBufferQueue(port_and_ip, ret);
		return true;
	}
	if (ret == (int)buff.size())
	{
		
		MagicSingleton<BufferCrol>::GetInstance()->PopNWriteBufferQueue(port_and_ip, ret);
		global::g_mutexForPhoneList.lock();
		for(auto it = global::g_phoneList.begin(); it != global::g_phoneList.end(); ++it)
		{
			if(data.fd == *it)
			{
				close(data.fd);
				if(!MagicSingleton<BufferCrol>::GetInstance()->DeleteBuffer(data.ip, data.port))
				{
					ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(data.ip), data.port);
				}
				MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(data.fd);
				global::g_phoneList.erase(it);
				break;
			}
		}
		global::g_mutexForPhoneList.unlock();
		return true;
	}

	return false;
}

