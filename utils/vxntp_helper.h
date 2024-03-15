#ifndef __VXNTPHELPER_H__
#define __VXNTPHELPER_H__

#include "vxd_type.h"

 

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

 

#ifndef NTP_OUTPUT
#define NTP_OUTPUT 1    //<The interface calls the macro switch whether the procedure outputs debugging information
#endif // NTP_OUTPUT
 
#define NTP_PORT   123  //< NTPNTP private port number

/**
 * @struct x_ntp_timestamp_t
 * @brief  NTP
 */
typedef struct x_ntp_timestamp_t
{ 
    x_uint32_t  xut_seconds;    //<The number of seconds elapsed from 1900 to the present
    x_uint32_t  xut_fraction;   //<The decimal part is a factor of 4294.967296 ( = 2^32 / 10^6 ) of the number of microseconds
} x_ntp_timestamp_t;

/**
 * @struct x_ntp_timeval_t
 * @brief  Redefine the timeval struct of the system.
 */
typedef struct x_ntp_timeval_t
{
    x_long_t    tv_sec;    //<
    x_long_t    tv_usec;   //< 
} x_ntp_timeval_t;

/**
 * @struct x_ntp_time_context_t
 * @brief  Time describes the information structure.
 */
typedef struct x_ntp_time_context_t
{
    x_uint32_t   xut_year   : 16;  //<  year
    x_uint32_t   xut_month  :  6;  //<  month
    x_uint32_t   xut_day    :  6;  //<  day
    x_uint32_t   xut_week   :  4;  //<  Day of the week
    x_uint32_t   xut_hour   :  6;  //<  time
    x_uint32_t   xut_minute :  6;  //<  divide
    x_uint32_t   xut_second :  6;  //<  second
    x_uint32_t   xut_msec   : 14;  //<  millisecond
} x_ntp_time_context_t;

 

/**********************************************************/
/**
 *  Gets the time value of the current system in 100 nanoseconds from January 1, 1970 to the present.
 */
x_uint64_t ntp_gettimevalue(void);

/**********************************************************/
/**
 * Get the timeval value of the current system (time from January 1, 1970 to the present)
 */
x_void_t ntp_gettimeofday(x_ntp_timeval_t * xtm_value);

/**********************************************************/
/**
 * Convert x_ntp_time_context_t to 100 nanoseconds 
 * Time value in units (time from January 1, 1970 to the present)
 */
x_uint64_t ntp_time_value(x_ntp_time_context_t * xtm_context);

/**********************************************************/
/**
 * Converted (in 100 nanoseconds) time value (time from January 1, 1970 to the present)
 * is a specific time description information (i.e. x_ntp_time_context_t).
 *
 * @param [in ] xut_time    : Time value (time from January 1, 1970 to the present).
 * @param [out] xtm_context : The time description returned for the success of the operation.
 *
 * @return x_bool_t
 *  Success, returning X_TRUE;
 * - Failed, returned X_FALSE
 */
x_bool_t ntp_tmctxt_bv(x_uint64_t xut_time, x_ntp_time_context_t * xtm_context);

/**********************************************************/
/**
 * @brief 
 * Convert (type x_ntp_timeval_t) time values
 * Describe the information for a specific time (i.e. x_ntp_time_context_t)
 *
 * @param [in ] xtm_value   : Time value.
 * @param [out] xtm_context : The time description returned for the success of the operation.
 *
 * @return x_bool_t
 * - Success, return X_TRUE;
 * - Failed, returned X_FALSE
 */
x_bool_t ntp_tmctxt_tv(const x_ntp_timeval_t * const xtm_value, x_ntp_time_context_t * xtm_context);

/**********************************************************/
/**
 * @brief
 * Convert (type x_ntp_timeval_t) time values
 * Describe the information for a specific time (i.e. x_ntp_time_context_t)
 *
 * @param [in ] xtm_timestamp : Time value.
 * @param [out] xtm_context   : The time description returned for the success of the operation.
 *
 * @return x_bool_t
 * - Success, return X_TRUE;
 * - Failed, returned X_FALSE
 */
x_bool_t ntp_tmctxt_ts(const x_ntp_timestamp_t * const xtm_timestamp, x_ntp_time_context_t * xtm_context);

 

/**********************************************************/
/**
 * @brief 
 * Send an NTP request to the NTP server to obtain the server timestamp for testing.
 *
 * @param [in ] xszt_host : The IP (four-segment IP address) or domain name (such as 3.cn.pool.ntp.org) of the NTP server.
 * @param [in ] xut_port  : The port number of the NTP server (whichever is the default port number NTP_PORT : 123).
 * @param [in ] xut_tmout : The timeout period for network requests in milliseconds.
 * @param [out] xut_timev : The answer time value returned by the successful operation in 100 nanoseconds from January 1, 1970 to the present.
 *
 * @return x_int32_t
 * Success, returning 0;
 * - Failed, returned error code.
 */
x_int32_t ntp_get_time_test(x_cstring_t xszt_host, x_uint16_t xut_port, x_uint32_t xut_tmout, x_uint64_t * xut_timev);

/**********************************************************/
/**
 * @brief Send an NTP request to the NTP server to get the server timestamp.
 *
 * @param [in ] xszt_host : The IP (four-segment IP address) or domain name (such as 3.cn.pool.ntp.org) of the NTP server.
 * @param [in ] xut_port  : The port number of the NTP server (whichever is the default port number NTP_PORT : 123).
 * @param [in ] xut_tmout : The timeout period for network requests in milliseconds.
 * @param [out] xut_timev : The answer time value returned by the successful operation in 100 nanoseconds from January 1, 1970 to the present.
 *
 * @return x_int32_t
 */
x_int32_t ntp_get_time(x_cstring_t xszt_host, x_uint16_t xut_port, x_uint32_t xut_tmout, x_uint64_t * xut_timev);


 

#ifdef __cplusplus
}; // extern "C"
#endif // __cplusplus

 

#endif // __VXNTPHELPER_H__
