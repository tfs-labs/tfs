#ifndef bind_thread_h
#define bind_thread_h

#include <thread>

/**
 * @brief       
 * 
 * @param       cpuIndex 
 * @param       tid 
 */
void SysThreadSetCpu(unsigned int cpuIndex);
void SysThreadSetCpu(unsigned int cpuIndex, pthread_t tid);

/**
 * @brief       Get the Cpu Index object
 * 
 * @return      int 
 */
int GetCpuIndex();


#endif bind_thread_h