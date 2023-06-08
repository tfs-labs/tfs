#include "logging.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <iostream>
#include <signal.h>
#include <execinfo.h>

static std::shared_ptr<spdlog::logger> g_sink[LOGEND] = { nullptr };
static const char * g_sink_names[] = {"tfs", "net", "ca", "contract"};
static const char * g_level_strs[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL", "OFF" };
static void SystemErrorHandler(int signum);
static spdlog::level::level_enum GetLogLevel(const std::string &level);

bool LogInit(const std::string &path, bool console_out, const std::string &level)
{  
    spdlog::level::level_enum l = GetLogLevel(level);
    return LogInit(path, console_out, l);
}
bool LogInit(const std::string &path, bool console_out, spdlog::level::level_enum level)
{
    if(level >= spdlog::level::level_enum::off)
    {
        return true;
    }
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks;
        if(console_out)
            sinks.push_back(console_sink);
        for (int i = LOGMAIN; i < LOGEND; i++)
        {
            sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>(path + "/" + g_sink_names[i] + ".log", 0, 0, false, 7));
            g_sink[i] = std::make_shared<spdlog::logger>(g_sink_names[i], begin(sinks), end(sinks));

            //Set the minimum log level
            g_sink[i]->set_level(level);

            //Writes cached data to a file as soon as an err-level log occurs
            g_sink[i]->flush_on(spdlog::level::trace);

            //Set the log output format
            g_sink[i]->set_pattern("[%Y-%m-%d %H:%M:%S.%e][-%o][%t][%@:%!]%^[%l]:%v%$");

            //Set up error handling
            g_sink[i]->set_error_handler([=](const std::string &msg) {
                std::cout << " An error occurred in the " << g_sink_names[i] << " log system: " << msg << std::endl;
            });
            sinks.pop_back();
        }

        //Prints the function call stack when the program crashes
	    signal(SIGSEGV, SystemErrorHandler);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
        LogFini();
        return false;
    }
    catch (...)
    {
        std::cout << "Log initialization failed" << std::endl;
        LogFini();
        return false;
    }
    return true;
}

spdlog::level::level_enum GetLogLevel(const std::string &level)
{
	for (size_t i = 0; i < sizeof(g_level_strs) / sizeof(const char *); i++)
	{
		if (strcasecmp(g_level_strs[i], level.c_str()) == 0)
		{
			return (spdlog::level::level_enum)i;
		}
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
std::shared_ptr<spdlog::logger> GetSink(LOGSINK sink)
{
    if(sink > LOGMAIN && sink < LOGEND)
        return g_sink[sink];
    return g_sink[LOGMAIN];
}

void LogFini()
{
    spdlog::shutdown();
}
