#include <sys/time.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "./time_util.h"
#include "../include/logging.h"
#include "../ca/global.h"

void Trim(std::string& str, bool bLeft, bool bRight)
{
    static const std::string delims = " \t\r\n";
    if(bRight)
        str.erase(str.find_last_not_of(delims)+1); // trim right
    if(bLeft)
        str.erase(0, str.find_first_not_of(delims)); // trim left
}

TimeUtil::TimeUtil()
{
    
}

TimeUtil::~TimeUtil()
{
    
}

x_uint64_t TimeUtil::GetUTCTimestamp()
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

x_uint64_t TimeUtil::GetTheTimestampPerUnitOfTime(const x_uint64_t& utcTime)
{
    x_uint64_t minutes = utcTime / 3000000;  // Convert timestamp
    return minutes *  3000000;
}


x_uint64_t TimeUtil::GetTimestamp()
{
    x_uint64_t time = GetNtpTimestamp();
    if(time == 0)
    {
        time = GetUTCTimestamp();
    }
    return time;
}

x_uint64_t TimeUtil::GetNtpTimestampConf()
{
    std::vector< std::string > ntpHost;

	std::ifstream infile;
	infile.open("./ntp_server.conf");  
	if (!infile) {
		std::cout << "unable to open ntp_server.conf" << std::endl;
		return 0;
	}
	std::string temp;
	while (getline(infile, temp)) {
		if (temp.size() < 0) break;
		ntpHost.push_back(temp);
	}

    int err = -1;
    x_uint64_t timev = 0ULL;

    for(int i = 0; i < 3; i++)
    {
        std::string host = ntpHost[rand()%(ntpHost.size())];
        
        Trim(host,true,true);      
        err = ntp_get_time(host.c_str(), NTP_PORT, 1000, &timev);
        if (0 == err)
        {
            break;
        }
    }
    infile.close();
    return timev/10;
}

x_uint64_t TimeUtil::GetNtpTimestamp(bool is_sync)
{
    std::vector< std::string > ntpHost;
    ntpHost.push_back(std::string("ntp1.aliyun.com"));
    ntpHost.push_back(std::string("ntp2.aliyun.com"));
    ntpHost.push_back(std::string("ntp3.aliyun.com"));
    ntpHost.push_back(std::string("ntp4.aliyun.com"));
    ntpHost.push_back(std::string("ntp5.aliyun.com"));
    ntpHost.push_back(std::string("ntp6.aliyun.com"));
    ntpHost.push_back(std::string("ntp7.aliyun.com"));
    ntpHost.push_back(std::string("time1.cloud.tencent.com"));
    ntpHost.push_back(std::string("time2.cloud.tencent.com"));
    ntpHost.push_back(std::string("time3.cloud.tencent.com"));
    ntpHost.push_back(std::string("time4.cloud.tencent.com"));
    ntpHost.push_back(std::string("time5.cloud.tencent.com"));

    int err = -1;
    x_uint64_t timev = 0ULL;

    for(int i = 0; i < 3; i++)
    {
        std::string host = ntpHost[rand()%(ntpHost.size())];
                
        err = ntp_get_time(host.c_str(), NTP_PORT, 1000, &timev);
        if (0 == err)
        {
            break;
        }
    }
    if(err == 0 && is_sync)
    {
        SetLocalTime(timev/10);
    }
    return timev/10;
}

bool TimeUtil::SetLocalTime(x_uint64_t timestamp)
{
    struct timeval tv;
    tv.tv_sec = timestamp/1000000;
	tv.tv_usec = timestamp - timestamp/1000000*1000000;
    std::cout << "sec:" << tv.tv_sec << std::endl;
    std::cout << "usec:" << tv.tv_usec << std::endl;
	if (settimeofday(&tv, NULL) < 0)
	{
		return false;
	}
	return true;
}


void TimeUtil::TestNtpDelay()
{
    int err = -1;

    x_uint64_t xut_timev = 0ULL;

    std::vector< std::string > ntpHost;
    ntpHost.push_back(std::string("ntp1.aliyun.com"));
    ntpHost.push_back(std::string("ntp2.aliyun.com"));
    ntpHost.push_back(std::string("ntp3.aliyun.com"));
    ntpHost.push_back(std::string("ntp4.aliyun.com"));
    ntpHost.push_back(std::string("ntp5.aliyun.com"));
    ntpHost.push_back(std::string("ntp6.aliyun.com"));
    ntpHost.push_back(std::string("ntp7.aliyun.com"));
    ntpHost.push_back(std::string("time1.cloud.tencent.com"));
    ntpHost.push_back(std::string("time2.cloud.tencent.com"));
    ntpHost.push_back(std::string("time3.cloud.tencent.com"));
    ntpHost.push_back(std::string("time4.cloud.tencent.com"));
    ntpHost.push_back(std::string("time5.cloud.tencent.com"));
    
    for (std::vector< std::string >::iterator itvec = ntpHost.begin(); itvec != ntpHost.end(); ++itvec)
    {
        xut_timev = 0ULL;
        err = ntp_get_time_test(itvec->c_str(), NTP_PORT, 1000, &xut_timev);
        if (0 != err)
        {
            ERRORLOG("{} return error code :{}", itvec->c_str(), err);
        }
    }    
}


std::string TimeUtil::FormatTimestamp(x_uint64_t timestamp)
{
    time_t s = (time_t)(timestamp / 1000000);
    std::stringstream ss;
    ss << std::put_time(localtime(&s), "%F %T");
    return ss.str();
}
std::string TimeUtil::FormatUTCTimestamp(x_uint64_t timestamp)
{
    char buffer [128];
    time_t s = (time_t)(timestamp / 1000000);
    struct tm *p;
    p=gmtime(&s);
    strftime (buffer,sizeof(buffer),"%Y/%m/%d %H:%M:%S",p);
    return buffer;
}
uint64_t TimeUtil::GetMorningTime(time_t t) 
{  

	t = t/1000000;
    struct tm * tm= gmtime(&t);
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    uint64_t zero_time=mktime(tm);

    return  zero_time;
}  
uint64_t TimeUtil::GetPeriod(uint64_t TxTime)
{
    return ((TxTime/1000000) - (global::ca::kGenesisTime/1000000)) / (24  *60 * 60);
}
std::string TimeUtil::GetDate(int d)
{
    uint64_t cur_time=GetNtpTimestamp();
		
	struct timeval tv;
	tv.tv_sec = cur_time/1000000;
	time_t now = tv.tv_sec-24*60*60*d;
	tm* p = gmtime(&now);
    std::string date=std::to_string(1900+p->tm_year)+"."+std::to_string(1+p->tm_mon)+"."+std::to_string(p->tm_mday);
    return date;
}
