#ifndef TFS_COMMON_LOGGING_H_
#define TFS_COMMON_LOGGING_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <mutex>

typedef enum {
    LOGMAIN = 0,
    LOGEND
} LOGSINK;


bool LogInit(const std::string &path, bool console_out = false, const std::string &level = "OFF");
bool LogInit(const std::string &path, bool console_out = false, spdlog::level::level_enum level = spdlog::level::off);
std::shared_ptr<spdlog::logger> GetSink(LOGSINK sink);
void LogFini();

#define LOGSINK(sink, level, format, ...)                                                                       \
    do {                                                                                                        \
       auto sink_ptr = GetSink(sink);                                                                           \
        if(nullptr != sink_ptr){                                                                                \
            sink_ptr->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, level, format, ##__VA_ARGS__);\
        }\
   } while (0);

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
