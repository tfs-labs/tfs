#include "bind_thread.h"
#include "../net/global.h"

static std::mutex mutexForCpuIndex;
static int cpuIndex = 0;

/// @brieftid Gets the thread tid
int SystemGetId()
{
	int tid = 0;
	tid = syscall(__NR_gettid);
	return tid;
}

/// @brief Put the thread into the specified CPU to run
/// @param [in] cpuIndex CPU sequence number, starting from 0, 0 representing the first CPU
void SysThreadSetCpu(unsigned int cpuIndex)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpuIndex, &mask);
	int tid = SystemGetId();
	if (-1 == sched_setaffinity(tid, sizeof(mask), &mask))
	{
		printf("%s:%s:%d, WARNING: Could not set thread to CPU %d\n", __FUNCTION__, __FILE__, __LINE__, cpuIndex);
	}
}

/// @briefPut the thread into the specified CPU to run
/// @param [in] cpuIndex CPU CPU sequence number, starting from 0, 0 representing the first CPU
void SysThreadSetCpu(unsigned int cpuIndex, pthread_t tid)
{

	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpuIndex, &mask);
	if (-1 == pthread_setaffinity_np(tid, sizeof(mask), &mask))
	{
		printf("%s:%s:%d, WARNING: Could not set thread to CPU %d\n", __FUNCTION__, __FILE__, __LINE__, cpuIndex);
	}
}

int GetCpuIndex()
{
	std::lock_guard<std::mutex> lck(mutexForCpuIndex);
	int res = cpuIndex;
	if (cpuIndex + 1 < global::g_cpuNums)
	{
		cpuIndex += 1;
	}
	else
	{
		cpuIndex = 0;
	}

	return res;
}