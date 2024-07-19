#include "logging.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <iostream>
#include <signal.h>
#include <execinfo.h>


bool Log::LogInit(const std::string &path, bool console_out, const std::string &level)
{  
    spdlog::level::level_enum convertLevel = GetLogLevel(level);
    return LogInit(path, console_out, convertLevel);
}
bool Log::LogInit(const std::string &path, bool console_out, spdlog::level::level_enum level)
{
    if(level >= spdlog::level::level_enum::off)
    {
        return true;
    }
    std::lock_guard lock(_logMutex);
    try {
        spdlog::sink_ptr _consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        if(console_out)
        {
            _sinks.push_back(_consoleSink);
        }
        for (int i = LOGMAIN; i < LOGEND; i++)
        {
            _sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>(path + "/" + _loggerType[i] + ".log", 0, 0, false, 7));
            _logger[i] = std::make_shared<spdlog::logger>(_loggerType[i], begin(_sinks), end(_sinks));
            //Set the minimum log level
            _logger[i]->set_level(level);
            //Writes cached data to a file as soon as an err-level log occurs
            _logger[i]->flush_on(spdlog::level::trace);
            
            //Set the log output format
            _logger[i]->set_pattern("[%Y-%m-%d %H:%M:%S.%e][-%o][%t][%@:%!]%^[%l]:%v%$");       
      
            //Set up error handling
            _logger[i]->set_error_handler([=](const std::string &msg) {
            std::cout << " An error occurred in the " << _loggerType[i] << " log system: " << msg << std::endl;
            });
        }
        //Prints the function call stack when the program crashes
	    signal(SIGSEGV, SystemErrorHandler);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
        LogDeinit();
        return false;
    }
    catch (...)
    {
        std::cout << "Log initialization failed" << std::endl;
        LogDeinit();
        return false;
    }
    return true;
}

spdlog::level::level_enum Log::GetLogLevel(const std::string &level)
{
    auto it = std::find(_loggerLevel.begin(),_loggerLevel.end(),level);
    if(it != _loggerLevel.end())
    {
        std::lock_guard lock(_logMutex);
        auto levelEnum = levelMap[level];
        return levelEnum;
    }
    return spdlog::level::warn;
}

void SystemErrorHandler(int signum)
{
    const int len = 1024;
    void *func[len];
    signal(signum,SIG_DFL);
    size_t size = backtrace(func,len);
    char ** funs = backtrace_symbols(func,size);
	ERRORLOG("System error, Stack trace:");
    for(size_t i = 0; i < size; ++i)
		ERRORLOG("{}: {}", i, funs[i]);
    free(funs);
}

std::shared_ptr<spdlog::logger> Log::GetSink(LOGSINK sink)
{
    return _logger[LOGMAIN];
}

void Log::LogDeinit()
{
  for(auto& logger : _logger) 
  {  
    logger.reset(); 
  }

  _logMutex.lock();

  spdlog::shutdown();

  _logMutex.unlock();
}
