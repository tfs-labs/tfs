#ifndef _MAGICSINGLETON_H_
#define _MAGICSINGLETON_H_

#include <mutex>
#include <memory>

template<typename T>
class MagicSingleton 
{
public:

	/**
 	 * @brief		Gets the global singleton object
	*/
	template<typename ...Args>
	static std::shared_ptr<T> GetInstance(Args&&... args) 
    {
		if (!_pSington) 
        {
			std::lock_guard<std::mutex> gLock(_mutex);
			if (nullptr == _pSington) 
            {
				_pSington = std::make_shared<T>(std::forward<Args>(args)...);
			}
		}
		return _pSington;
	}

	/**
 	 * @brief		Active destruction of singleton objects (generally not required unless specifically required)
	*/
	static void DesInstance() 
    {
		if (_pSington) 
        {
			_pSington.reset();
			_pSington = nullptr;
		}
	}

private:
	explicit MagicSingleton();
	MagicSingleton(const MagicSingleton&) = delete;
	MagicSingleton& operator=(const MagicSingleton&) = delete;
	~MagicSingleton();

private:
	static std::shared_ptr<T> _pSington;
	static std::mutex _mutex;
};

template<typename T>
std::shared_ptr<T> MagicSingleton<T>::_pSington = nullptr;

template<typename T>
std::mutex MagicSingleton<T>::_mutex;

#endif // _MAGICSINGLETON_H_
