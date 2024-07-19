#include "time_report.h"
#include "../utils/time_util.h"
#include "../utils/magic_singleton.h"
#include "../include/logging.h"
#include "../utils/console.h"
#include "../utils/tmp_log.h"
#include <iostream>

TimeReport::TimeReport() : _title("Elapsed time:"), _start(0), _end(0)
{
    Init();
}


TimeReport::~TimeReport()
{
    
}

void TimeReport::Init()
{
    _start = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
}

void TimeReport::End()
{
    _end = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
}

void TimeReport::Report()
{
    Report(_title);
}

void TimeReport::Report(const std::string& title)
{
    End();
    int64_t usedTime = _end - _start;
    double dUsendTime = ((double)usedTime) / 1000000.0;
    write_tmplog(title + " " + std::to_string(dUsendTime) + " second", OUTTYPE::file, "time_test");
}
