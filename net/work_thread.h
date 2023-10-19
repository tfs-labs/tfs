/**
 * *****************************************************************************
 * @file        work_thread.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _WORK_THREAD_H_
#define _WORK_THREAD_H_

#include <thread>
#include <vector>
#include <list>
#include <deque>

#include "./api.h"

#include "./msg_queue.h"
#include "../common/config.h"

/**
 * @brief       
 * 
 * @param       cpuIndex 
 * @param       tid 
 */
void SysThreadSetCpu(unsigned int cpuIndex, pthread_t tid);

/**
 * @brief       Get the Cpu Index object
 * 
 * @return      int 
 */
int GetCpuIndex();

class WorkThreads
{
public:
	WorkThreads() = default;
	~WorkThreads() = default;

	/**
	 * @brief       Processing network messages
	 * 
	 * @param       data 
	 * @return      int 
	 */
	static int HandleNetRead(const MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       data 
	 * @return      int 
	 */
	static int HandleNetworkRead(MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       data 
	 * @return      true 
	 * @return      false 
	 */
	static bool HandleNetWrite(const MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       num 
	 */
	static void Work(int num);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 */
	static void WorkRead(int id);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 */
	static void WorkWrite(int id);

	/**
	 * @brief       
	 * 
	 */
	void Start();
private:
    friend std::string PrintCache(int where);
	std::vector<std::thread> _threadsWorkList;
	std::vector<std::thread> _threadsReadList;
	std::vector<std::thread> _threadsTransList;
};

#endif