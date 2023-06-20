#ifndef __TIME_REPORT_H__
#define __TIME_REPORT_H__

#include <string>

using namespace std;

class TimeReport
{
public:
    TimeReport();
    ~TimeReport();
    TimeReport(TimeReport&&) = delete;
    TimeReport& operator=(const TimeReport&) = delete;
    TimeReport& operator=(TimeReport&&) = delete;

public:
    void Init();
    void End();
    void Report();
    void Report(const string& title);
    uint64_t GetStart() { return start_; }
    uint64_t GetEnd() { return end_; }

private:
    string title_;
    uint64_t start_;
    uint64_t end_;
};

#endif // __TIME_REPORT_H__