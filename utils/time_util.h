/**
 * *****************************************************************************
 * @file        time_util.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _TIMEUTIL_H_
#define _TIMEUTIL_H_

#include <string>
#include <fstream>
#include <map>
#include <vector>

#include "vxntp_helper.h"

/**
 * @brief       
 * 
 */
class TimeUtil
{
    
public:
    TimeUtil();
    ~TimeUtil();
    
    /**             
     * @brief       Obtain the NTP server timestamp in microseconds 1s = 1,000,000 microseconds
                    is_sync Whether to synchronize the time obtained from NTP to the local computer
     *                              
     * @param       is_sync: 
     * @return      x_uint64_t 
     */
    x_uint64_t GetNtpTimestamp(bool is_sync = false);

    /**
     * @brief       Get the NTP server timestamp, read the NTP server from the configuration file, and request the unit: microseconds
     * 
     * @return      x_uint64_t 
     */
    x_uint64_t GetNtpTimestampConf();

    /**
     * @brief       Gets the local timestamp in microseconds 
     * 
     * @return      x_uint64_t 
     */
    x_uint64_t GetUTCTimestamp();
    
    /**
    * @brief
    *
    * @return       x_uint64_t
    */
    x_uint64_t GetTheTimestampPerUnitOfTime(const x_uint64_t& utcTime);

    /**
     * @brief       Obtain the timestamp, first obtain it from NTP, and then go to the local level if it is unsuccessful
     * 
     * @return      x_uint64_t 
     */
    x_uint64_t GetTimestamp();

    /**
     * @brief       Set the Local Time object
     * 
     * @param       timestamp:  timestamp,timestamp, in microseconds
     * @return      true 
     * @return      false 
     */
    bool SetLocalTime(x_uint64_t timestamp);

    /**
     * @brief      Test the delay of the NTP server acquisition time 
     * 
     */
    void TestNtpDelay();

    /**
     * @brief       YYYY-MM-DD HH-MM-SS The formatted timestamp is YYYY-MM-DD HH-MM-SS
     * 
     * @param       timestamp: 
     * @return      std::string 
     */
    std::string FormatTimestamp(x_uint64_t timestamp);

    /**
     * @brief       YYYY-MM-DD HH-MM-SS Format the UTC timestamp as YYYY-MM-DD HH-MM-SS
     * 
     * @param       timestamp: 
     * @return      std::string 
     */
    std::string FormatUTCTimestamp(x_uint64_t timestamp);

    /**
     * @brief       Get the Morning Time object
     * 
     * @param       t: 
     * @return      uint64_t 
     */
	uint64_t GetMorningTime(time_t t);

    /**
     * @brief       Get the Period object
     * 
     * @param       TxTime: 
     * @return      uint64_t 
     */
    uint64_t GetPeriod(uint64_t TxTime);

    /**
     * @brief       Get the Date object
     * 
     * @param       d: 
     * @return      std::string 
     */
    std::string GetDate(int d=0);
};


#endif