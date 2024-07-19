#include "timer.hpp"
#include <future>

CTimer::CTimer(const std::string sTimerName):m_bExpired(true), m_bTryExpired(false), m_bLoop(false)
{
    m_sName = sTimerName;
}

CTimer::~CTimer()
{
    CTimer::Cancel();   //Try to expire the task
}

bool CTimer::Start(unsigned int msTime, std::function<void()> task, bool bLoop, bool async)
{
    if (!m_bExpired || m_bTryExpired) return false;  //The task is not expired (that is, the task still exists internally or is running)
    m_bExpired = false;
    m_bLoop = bLoop;
    m_nCount = 0;

    if (async) {
        DeleteThread();
        m_Thread.reset(new std::thread([this, msTime, task]() {
            if (!m_sName.empty()) {
#if (defined(__ANDROID__) || defined(ANDROID))      //Android Compatible with Android
                pthread_setname_np(pthread_self(), m_sName.c_str());
#elif defined(__APPLE__)                            //Compatible with Apple systems
                pthread_setname_np(m_sName.c_str());    // Sets the thread (timer) name
#endif
            }
            
            while (!m_bTryExpired) {
                m_ThreadCon.wait_for(m_ThreadLock, std::chrono::milliseconds(msTime));  //dormancy
                if (!m_bTryExpired) {
                    task();     //Perform tasks

                    m_nCount ++;
                    if (!m_bLoop) {
                        break;
                    }
                }
            }
            
            m_bExpired = true;      // Task execution completed (indicates that an existing task has expired)
            m_bTryExpired = false;  // In order to load the task again next time
        }));
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(msTime));
        if (!m_bTryExpired) {
            task();
        }
        m_bExpired = true;
        m_bTryExpired = false;
    }
    
    return true;
}

void CTimer::Cancel()
{
    if (m_bExpired || m_bTryExpired || !m_Thread) {
        return;
    }
    
    m_bTryExpired = true;
    m_ThreadCon.notify_all();     
    if (m_Thread->joinable()) {
        m_Thread->join();
    }
    else
    {
        m_Thread->detach();
    }

}

void CTimer::DeleteThread()
{
    if (m_Thread) {
        m_ThreadCon.notify_all();   //Wake from sleep
        m_Thread->join();           //Wait for the thread to exit
        m_Thread.reset();        
    }
}

