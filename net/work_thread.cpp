#include "./work_thread.h"
#include "../include/logging.h"
#include "./pack.h"
#include "./ip_port.h"
#include "peer_node.h"
#include "../include/net_interface.h"
#include "global.h"
#include "net.pb.h"
#include "dispatcher.h"
#include "./socket_buf.h"
#include "./epoll_mode.h"
#include "../utils/console.h"

static std::mutex g_mutex_for_vec_;
static deque<Node> g_deque_node; //list_nodelist_node is used to store nodes with an empty neighbor node list

static std::mutex g_mutex_for_map_;
static std::mutex mutex_for_cpu_index;
static int cpu_index = 0;


/// @brieftid Gets the thread tid
int sys_get_tid()
{
	int tid = 0;
	tid = syscall(__NR_gettid);
	return tid;
}

/// @brief Put the thread into the specified CPU to run
/// @param [in] cpu_index CPU sequence number, starting from 0, 0 representing the first CPU
void sys_thread_set_cpu(unsigned int cpu_index)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_index, &mask);
	int tid = sys_get_tid();
	if (-1 == sched_setaffinity(tid, sizeof(mask), &mask))
	{
		printf("%s:%s:%d, WARNING: Could not set thread to CPU %d\n", __FUNCTION__, __FILE__, __LINE__, cpu_index);
	}
}

/// @briefPut the thread into the specified CPU to run
/// @param [in] cpu_index CPU CPU sequence number, starting from 0, 0 representing the first CPU
void sys_thread_set_cpu(unsigned int cpu_index, pthread_t tid)
{

	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_index, &mask);
	if (-1 == pthread_setaffinity_np(tid, sizeof(mask), &mask))
	{
		printf("%s:%s:%d, WARNING: Could not set thread to CPU %d\n", __FUNCTION__, __FILE__, __LINE__, cpu_index);
	}
}

int get_cpu_index()
{
	std::lock_guard<std::mutex> lck(mutex_for_cpu_index);
	int res = cpu_index;
	if (cpu_index + 1 < global::cpu_nums)
	{
		cpu_index += 1;
	}
	else
	{
		cpu_index = 0;
	}

	return res;
}
//cpu Bind the CPU
void bind_cpu()
{
	if (global::cpu_nums >= 4)
	{
		int index = get_cpu_index();
		sys_thread_set_cpu(index);
	}
}

void WorkThreads::start()
{
	int k = 0;

	int WorkNum = 128;
	INFOLOG("will create {} threads", WorkNum);
	if(0 == WorkNum)
	{
		WorkNum = global::cpu_nums * 2;
	}
    if(0 == WorkNum)
    {
        WorkNum = 8;
    }

	for (auto i = 0; i < 8; i++)
	{
		this->m_threads_read_list.push_back(std::thread(WorkThreads::work_read, i));
		this->m_threads_read_list[i].detach();
	}

	for (auto i = 0; i < 8; i++)
	{
		this->m_threads_trans_list.push_back(std::thread(WorkThreads::work_write, i));
		this->m_threads_trans_list[i].detach();
	}

	for (auto i = 0; i < WorkNum; i++)
	{
		this->m_threads_work_list.push_back(std::thread(WorkThreads::work, i));
		this->m_threads_work_list[i].detach();
	}	
}

void WorkThreads::work_write(int id)
{

	bind_cpu();

	while (1)
	{
		MsgData data;
		if (false == global::queue_write.try_wait_pop(data))
			continue;

		switch (data.type)
		{
		case E_WRITE: 
			WorkThreads::handle_net_write(data);
			break;
		default:
			INFOLOG(YELLOW "work_write drop data: data.fd :{}" RESET, data.fd);
			break;
		}
	}
}
void WorkThreads::work_read(int id) //Read socket dedicated thread
{
	bind_cpu();

	while (1)
	{
		MsgData data;
		if (false == global::queue_read.try_wait_pop(data))
			continue;

		switch (data.type)
		{
		case E_READ:
			WorkThreads::handle_net_read(data);
			break;
		default:
			INFOLOG(YELLOW "work_read drop data: data.fd {}" RESET, data.fd);
			break;
		}
	}
}

void WorkThreads::work(int id)
{

	bind_cpu();

	while (1)
	{
		MsgData data;
		if (false == global::queue_work.try_wait_pop(data))
			continue;

		switch (data.type)
		{
		case E_WORK:

			WorkThreads::handle_network_event(data);
			break;
		default:
			INFOLOG("drop data: data.fd :{}", data.fd);
			break;
		}
	}
}

int WorkThreads::handle_network_event(MsgData &data)
{
	MagicSingleton<ProtobufDispatcher>::GetInstance()->handle(data);
	return 0;
}


int WorkThreads::handle_net_read(const MsgData &data)
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
			MagicSingleton<BufferCrol>::GetInstance()->add_read_buffer_queue(data.ip, data.port, buf, nread);
		}		
		if (nread == 0 && errno != EAGAIN)
		{
			DEBUGLOG("++++handle_net_read++++ ip:({}) port:({}) fd:({})",IpPort::ipsz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(data.fd);
			return -1;
		}
		if (nread < 0)
		{
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
			{
				MagicSingleton<EpollMode>::GetInstance()->add_epoll_event(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
				return 0;
			}

			DEBUGLOG("++++handle_net_read++++ ip:({}) port:({}) fd:({})",IpPort::ipsz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(data.fd);

			return -1;
		}
	} while (nread >= MAXLINE);

	MagicSingleton<EpollMode>::GetInstance()->add_epoll_event(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return 0;
}

bool WorkThreads::handle_net_write(const MsgData &data)
{
	auto port_and_ip = net_com::pack_port_and_ip(data.port, data.ip);

	if (!MagicSingleton<BufferCrol>::GetInstance()->is_exists(port_and_ip))
	{
		DEBUGLOG("!MagicSingleton<BufferCrol>::GetInstance()->is_exists({})", port_and_ip);
		return false;
	}
	if(data.fd < 0)
	{
		ERRORLOG("handle_net_write fd < 0");
		return false;
	}
	std::mutex& buff_mutex = get_fd_mutex(data.fd);
	std::lock_guard<std::mutex> lck(buff_mutex);
	std::string buff = MagicSingleton<BufferCrol>::GetInstance()->get_write_buffer_queue(port_and_ip);
	
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
		
		MagicSingleton<BufferCrol>::GetInstance()->pop_n_write_buffer_queue(port_and_ip, ret);
		return true;
	}
	if (ret == (int)buff.size())
	{
		
		MagicSingleton<BufferCrol>::GetInstance()->pop_n_write_buffer_queue(port_and_ip, ret);
		global::mutex_for_phone_list.lock();
		for(auto it = global::phone_list.begin(); it != global::phone_list.end(); ++it)
		{
			if(data.fd == *it)
			{
				close(data.fd);
				if(!MagicSingleton<BufferCrol>::GetInstance()->delete_buffer(data.ip, data.port))
				{
					ERRORLOG(RED "delete_buffer ERROR ip:({}), port:({})" RESET, IpPort::ipsz(data.ip), data.port);
				}
				MagicSingleton<EpollMode>::GetInstance()->delete_epoll_event(data.fd);
				global::phone_list.erase(it);
				break;
			}
		}
		global::mutex_for_phone_list.unlock();
		return true;
	}

	

	return false;
}

