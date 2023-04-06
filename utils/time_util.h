#ifndef _TIMEUTIL_H_
#define _TIMEUTIL_H_

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include "VxNtpHelper.h"


class TimeUtil
{
    
public:
    TimeUtil();
    ~TimeUtil();
    
    //Obtain the NTP server timestamp in microseconds 1s = 1,000,000 microseconds

    //is_sync Whether to synchronize the time obtained from NTP to the local computer
    x_uint64_t getNtpTimestamp(bool is_sync = false);

    //Get the NTP server timestamp, read the NTP server from the configuration file, and request the unit: microseconds
    x_uint64_t getNtpTimestampConf();

    //Gets the local timestamp in microseconds 
    x_uint64_t getUTCTimestamp();

    //Obtain the timestamp, first obtain it from NTP, and then go to the local level if it is unsuccessful
    x_uint64_t getTimestamp();

    //Set the local time
    //param:
    //  timestamp,timestamp, in microseconds
    bool setLocalTime(x_uint64_t timestamp);

    //Test the delay of the NTP server acquisition time
    void testNtpDelay();

    //YYYY-MM-DD HH-MM-SS The formatted timestamp is YYYY-MM-DD HH-MM-SS
    std::string formatTimestamp(x_uint64_t timestamp);

    //YYYY-MM-DD HH-MM-SS Format the UTC timestamp as YYYY-MM-DD HH-MM-SS
    std::string formatUTCTimestamp(x_uint64_t timestamp);

	uint64_t getMorningTime(time_t t);

    uint64_t getPeriod(uint64_t TxTime);

    std::string GetDate(int d=0);
};


#endif