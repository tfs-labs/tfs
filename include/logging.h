#ifndef TFS_COMMON_LOGGING_H_
#define TFS_COMMON_LOGGING_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <mutex>
#include <utils/magic_singleton.h>
#include <map>

typedef enum {
    LOGMAIN = 0,
    LOGEND  = 1
} LOGSINK;


static std::map<std::string,spdlog::level::level_enum> levelMap
{
    {"trace",spdlog::level::level_enum::trace},
    {"debug",spdlog::level::level_enum::debug},
    {"info",spdlog::level::level_enum::info},
    {"warn",spdlog::level::level_enum::warn},
    {"err",spdlog::level::level_enum::err},
    {"critical",spdlog::level::level_enum::critical},
    {"off",spdlog::level::level_enum::off},
};

class Log
{
public:
    Log(){}
    ~Log()
    {  
    for(auto& logger : _logger) 
    {  
        logger.reset(); 
    }
    _logMutex.lock();
    spdlog::shutdown();
    _logMutex.unlock(); 
    }   

/**
 * @brief       Init the log string
 * 
 * @param       path 
 * @param       console_out 
 * @param       level 
 * @return      true 
 * @return      false 
 */
bool LogInit(const std::string &path, bool console_out = false, const std::string &level = "OFF");

/**
 * @brief       Init the log level
 * 
 * @param       path 
 * @param       console_out 
 * @param       level 
 * @return      true 
 * @return      false 
 */
bool LogInit(const std::string &path, bool console_out = false, spdlog::level::level_enum level = spdlog::level::off);


/**
 * @brief  Close the log
 */
void LogDeinit();

/**
 * @brief       Get the Log Level object
 * 
 * @param       level 
 * @return      spdlog::level::level_enum 
 */
spdlog::level::level_enum GetLogLevel(const std::string & level);

/**
 * @brief       Get the Sink object
 * 
 * @param       sink 
 * @return      std::shared_ptr<spdlog::logger> 
 */
std::shared_ptr<spdlog::logger> GetSink(LOGSINK sink);

private:
    std::mutex _logMutex;
    std::shared_ptr<spdlog::logger> _logger[LOGEND] = {nullptr};//_logger
    const std::vector<std::string>  _loggerType = {"tfs","net","ca","contract"};//_loggerType
    const std::vector<std::string>  _loggerLevel = {"trace","debug","info","warn","error","critical","off"};//_loggerLevel
    std::vector<spdlog::sink_ptr>   _sinks;
};


/**
 * @brief       
 * 
 * @param       signum 
 */
void SystemErrorHandler(int signum);

std::string format_color_log(spdlog::level::level_enum level);
#define LOGSINK(sink, level, format, ...)                                                                       \
    do {                                                                                                        \
       auto sink_ptr = MagicSingleton<Log>::GetInstance()->GetSink(sink);                                   \
        if(nullptr != sink_ptr){                                                                                \
            sink_ptr->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, level, format,##__VA_ARGS__);\
        }\
   } while (0);                                                                                                 \

#define TRACELOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::trace, format, ##__VA_ARGS__)
#define DEBUGLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::debug, format, ##__VA_ARGS__)
#define INFOLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::info, format, ##__VA_ARGS__)
#define WARNLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::warn, format, ##__VA_ARGS__)
#define ERRORLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::err, format, ##__VA_ARGS__)
#define CRITICALLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::critical, format, ##__VA_ARGS__)

#define TRACERAWDATALOGSINK(sink, prefix, container) TRACELOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define DEBUGRAWDATALOGSINK(sink, prefix, container) DEBUGLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define INFORAWDATALOGSINK(sink, prefix, container) INFOLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define WARNRAWDATALOGSINK(sink, prefix, container) WARNLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define ERRORRAWDATALOGSINK(sink, prefix, container) ERRORLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define CRITICALRAWDATALOGSINK(sink, prefix, container) CRITICALLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))

#define TRACELOG(format, ...) TRACELOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define DEBUGLOG(format, ...) DEBUGLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define INFOLOG(format, ...) INFOLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define WARNLOG(format, ...) WARNLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define ERRORLOG(format, ...) ERRORLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define CRITICALLOG(format, ...) CRITICALLOGSINK(LOGMAIN, format, ##__VA_ARGS__)

#define TRACERAWDATALOG(prefix, container) TRACERAWDATALOGSINK(LOGMAIN, prefix, container)
#define DEBUGRAWDATALOG(prefix, container) DEBUGRAWDATALOGSINK(LOGMAIN, prefix, container)
#define INFORAWDATALOG(prefix, container) INFORAWDATALOGSINK(LOGMAIN, prefix, container)
#define WARNRAWDATALOG(prefix, container) WARNRAWDATALOGSINK(LOGMAIN, prefix, container)
#define ERRORRAWDATALOG(prefix, container) ERRORRAWDATALOGSINK(LOGMAIN, prefix, container)
#define CRITICALRAWDATALOG(prefix, container) CRITICALRAWDATALOGSINK(LOGMAIN, prefix, container)

#endif
