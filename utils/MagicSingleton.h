#ifndef _MAGICSINGLETON_H_
#define _MAGICSINGLETON_H_

#include <mutex>
#include <memory>

template<typename T>
class MagicSingleton 
{
public:

	// Gets the global singleton object
	template<typename ...Args>
	static std::shared_ptr<T> GetInstance(Args&&... args) 
    {
		if (!m_pSington) 
        {
			std::lock_guard<std::mutex> gLock(m_Mutex);
			if (nullptr == m_pSington) 
            {
				m_pSington = std::make_shared<T>(std::forward<Args>(args)...);
			}
		}
		return m_pSington;
	}

	//Active destruction of singleton objects (generally not required unless specifically required)
	static void DesInstance() 
    {
		if (m_pSington) 
        {
			m_pSington.reset();
			m_pSington = nullptr;
		}
	}

private:
	explicit MagicSingleton();
	MagicSingleton(const MagicSingleton&) = delete;
	MagicSingleton& operator=(const MagicSingleton&) = delete;
	~MagicSingleton();

private:
	static std::shared_ptr<T> m_pSington;
	static std::mutex m_Mutex;
};

template<typename T>
std::shared_ptr<T> MagicSingleton<T>::m_pSington = nullptr;

template<typename T>
std::mutex MagicSingleton<T>::m_Mutex;

#endif // _MAGICSINGLETON_H_
