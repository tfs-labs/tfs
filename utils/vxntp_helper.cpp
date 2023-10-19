#include "vxntp_helper.h"

#ifdef _MSC_VER
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <time.h>
#else // !_MSC_VER
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif // _MSC_VER

#include <string>
#include <vector>

  

#if NTP_OUTPUT

#include <stdio.h>

static inline x_void_t ts_output(x_cstring_t xszt_name, const x_ntp_time_context_t * const xtm_ctxt)
{
    printf("\t%s : %04d-%02d-%02d_%02d-%02d-%02d.%03d\n",
        xszt_name           ,
        xtm_ctxt->xut_year  ,
        xtm_ctxt->xut_month ,
        xtm_ctxt->xut_day   ,
        xtm_ctxt->xut_hour  ,
        xtm_ctxt->xut_minute,
        xtm_ctxt->xut_second,
        xtm_ctxt->xut_msec  );
}

static inline x_void_t tn_output(x_cstring_t xszt_name, const x_ntp_timestamp_t * const xtm_stamp)
{
    x_ntp_time_context_t xtm_ctxt;
    ntp_tmctxt_ts(xtm_stamp, &xtm_ctxt);
    ts_output(xszt_name, &xtm_ctxt);
}

static inline x_void_t tv_output(x_cstring_t xszt_name, const x_ntp_timeval_t * const xtm_value)
{
    x_ntp_time_context_t xtm_ctxt;
    ntp_tmctxt_tv(xtm_value, &xtm_ctxt);
    ts_output(xszt_name, &xtm_ctxt);
}

static inline x_void_t bv_output(x_cstring_t xszt_name, x_uint64_t xut_time)
{
    x_ntp_time_context_t xtm_ctxt;
    ntp_tmctxt_bv(xut_time, &xtm_ctxt);
    ts_output(xszt_name, &xtm_ctxt);
}

#define XOUTLINE(szformat, ...)           do { printf((szformat), ##__VA_ARGS__); printf("\n"); } while (0)
#define TS_OUTPUT(xszt_name, xtm_ctxt )   ts_output((xszt_name), (xtm_ctxt ))
#define TN_OUTPUT(xszt_name, xtm_stamp)   tn_output((xszt_name), (xtm_stamp))
#define TV_OUTPUT(xszt_name, xtm_value)   tv_output((xszt_name), (xtm_value))
#define BV_OUTPUT(xszt_name, xut_time )   bv_output((xszt_name), (xut_time ))

#else // !NTP_OUTPUT

#define XOUTLINE(szformat, ...)
#define TS_OUTPUT(xszt_name, xtm_ctxt )
#define TN_OUTPUT(xszt_name, xtm_stamp)
#define TV_OUTPUT(xszt_name, xtm_value)
#define BV_OUTPUT(xszt_name, xut_time )

#endif // NTP_OUTPUT

  

#define JAN_1970     0x83AA7E80             //< 1900 ~ 1970 Time between 1900 ~ 1970 seconds
#define NS100_1970   116444736000000000LL   //< 1601 ~ 1970 Time between 1601 ~ 1970 100 100 nanoseconds

/**
 * @enum  em_ntp_mode_t
 * @brief NTP The relevant enumeration value for the NTP operating mode
 */
typedef enum em_ntp_mode_t
{
    ntp_mode_unknow     = 0,  //<Undefined
    ntp_mode_initiative = 1,  //<Active peer mode
    ntp_mode_passive    = 2,  //<Passive peer mode
    ntp_mode_client     = 3,  //<Client mode
    ntp_mode_server     = 4,  //<Server mode
    ntp_mode_broadcast  = 5,  //<Broadcast mode or multicast mode
    ntp_mode_control    = 6,  //<The message is an NTP control packet
    ntp_mode_reserved   = 7,  //<Reserved for internal use
} em_ntp_mode_t;

/**
 * @struct x_ntp_packet_t
 * @brief  Message format.
 */
typedef struct x_ntp_packet_t
{
    x_uchar_t         xct_li_ver_mode;      //<2 bits, leap indicator; 3 bits, version number; 3 bits, NTP operating mode (see em_ntp_mode_t Related enumeration values)
    x_uchar_t         xct_stratum    ;      
    x_uchar_t         xct_poll       ;      //<Polling time, which is the time interval between two consecutive NTP packets
    x_uchar_t         xct_percision  ;      //<The accuracy of the system clock

    x_uint32_t        xut_root_delay     ;  //<The round-trip time locally to the primary reference source
    x_uint32_t        xut_root_dispersion;  //<The maximum error of the system clock relative to the primary reference clock
    x_uint32_t        xut_ref_indentifier;  //<The identification of the reference clock source

    x_ntp_timestamp_t xtmst_reference;      //<The last time the system clock was set or updated (used to store T1 after the answer is completed)
    x_ntp_timestamp_t xtmst_originate;      //<The local time of the sender when the NTP request packet leaves the sender (after the reply is completed, it is used to store T4)
    x_ntp_timestamp_t xtmst_receive  ;      //<The local time of the receiver when the NTP request packet arrives at the receiver (after the reply is completed, it is used to store T2)
    x_ntp_timestamp_t xtmst_transmit ;      //<The local time of the responder when the NTP reply message leaves the responder (used to store T3 after the reply is complete.)
} x_ntp_packet_t;

  

/**********************************************************/
/**
 * // Convert x_ntp_timeval_t to x_ntp_timestamp_t
 */
static inline x_void_t ntp_timeval_to_timestamp(x_ntp_timestamp_t * xtm_timestamp, const x_ntp_timeval_t * const xtm_timeval)
{
    const x_lfloat_t xlft_frac_per_ms = 4.294967296E6;  // 2^32 / 1000

    xtm_timestamp->xut_seconds  = (x_uint32_t)(xtm_timeval->tv_sec  + JAN_1970);
    xtm_timestamp->xut_fraction = (x_uint32_t)(xtm_timeval->tv_usec / 1000.0 * xlft_frac_per_ms);
}

/**********************************************************/
/**
 * //Convert x_ntp_timeval_t to x_ntp_timestamp_t
 */
static inline x_void_t ntp_timestamp_to_timeval(x_ntp_timeval_t * xtm_timeval, const x_ntp_timestamp_t * const xtm_timestamp)
{
    const x_lfloat_t xlft_frac_per_ms = 4.294967296E6;  // 2^32 / 1000

    if (xtm_timestamp->xut_seconds >= JAN_1970)
    {
        xtm_timeval->tv_sec  = (x_long_t)(xtm_timestamp->xut_seconds - JAN_1970);
        xtm_timeval->tv_usec = (x_long_t)(xtm_timestamp->xut_fraction / xlft_frac_per_ms * 1000.0);
    }
    else
    {
        xtm_timeval->tv_sec  = 0;
        xtm_timeval->tv_usec = 0;
    }
}

/**********************************************************/
/**
 * //Converts x_ntp_timeval_t to a value in units of 100 nanoseconds.
 */
static inline x_uint64_t ntp_timeval_ns100(const x_ntp_timeval_t * const xtm_timeval)
{
    return (10000000ULL * xtm_timeval->tv_sec + 10ULL * xtm_timeval->tv_usec);
}

/**********************************************************/
/**
 *  Converts x_ntp_timeval_t to millisecond values.
 */
static inline x_uint64_t ntp_timeval_ms(const x_ntp_timeval_t * const xtm_timeval)
{
    return (1000ULL * xtm_timeval->tv_sec + (x_uint64_t)(xtm_timeval->tv_usec / 1000.0 + 0.5));
}

/**********************************************************/
/**
 * Convert x_ntp_timestamp_t to a value in units of 100 nanoseconds
 */
static inline x_uint64_t ntp_timestamp_ns100(const x_ntp_timestamp_t * const xtm_timestamp)
{
    x_ntp_timeval_t xmt_timeval;
    ntp_timestamp_to_timeval(&xmt_timeval, xtm_timestamp);
    return ntp_timeval_ns100(&xmt_timeval);
}

/**********************************************************/
/**
 * Convert x_ntp_timestamp_t to millisecond values.
 */
static inline x_uint64_t ntp_timestamp_ms(const x_ntp_timestamp_t * const xtm_timestamp)
{
    x_ntp_timeval_t xmt_timeval;
    ntp_timestamp_to_timeval(&xmt_timeval, xtm_timestamp);
    return ntp_timeval_ms(&xmt_timeval);
}

  

/**********************************************************/
/**
 * Gets the time value of the current system in 100 nanoseconds from January 1, 1970 to the present
 */
x_uint64_t ntp_gettimevalue(void)
{
#ifdef _MSC_VER
    FILETIME       xtime_file;
    ULARGE_INTEGER xtime_value;

    GetSystemTimeAsFileTime(&xtime_file);
    xtime_value.LowPart  = xtime_file.dwLowDateTime;
    xtime_value.HighPart = xtime_file.dwHighDateTime;

    return (x_uint64_t)(xtime_value.QuadPart - NS100_1970);
#else // !_MSC_VER
    struct timeval tmval;
    gettimeofday(&tmval, X_NULL);

    return (10000000ULL * tmval.tv_sec + 10ULL * tmval.tv_usec);
#endif // _MSC_VER
}

/**********************************************************/
/**
 * Get the timeval value of the current system (time from January 1, 1970 to the present)
 */
x_void_t ntp_gettimeofday(x_ntp_timeval_t * xtm_value)
{
#ifdef _MSC_VER
    FILETIME       xtime_file;
    ULARGE_INTEGER xtime_value;

    GetSystemTimeAsFileTime(&xtime_file);
    xtime_value.LowPart  = xtime_file.dwLowDateTime;
    xtime_value.HighPart = xtime_file.dwHighDateTime;

    xtm_value->tv_sec  = (x_long_t)((xtime_value.QuadPart - NS100_1970) / 10000000LL); 
    xtm_value->tv_usec = (x_long_t)((xtime_value.QuadPart / 10LL      ) % 1000000LL ); 
#else // !_MSC_VER
    struct timeval tmval;
    gettimeofday(&tmval, X_NULL);

    xtm_value->tv_sec  = tmval.tv_sec ;
    xtm_value->tv_usec = tmval.tv_usec;
#endif // _MSC_VER
}

/**********************************************************/
/**
 * Convert x_ntp_time_context_t to 100 nanoseconds
 * Time value in units (time from January 1, 1970 to the present).
 */
x_uint64_t ntp_time_value(x_ntp_time_context_t * xtm_context)
{
    x_uint64_t xut_time = 0ULL;

#if 0
    if ((xtm_context->xut_year   < 1970) ||
        (xtm_context->xut_month  <    1) || (xtm_context->xut_month > 12) ||
        (xtm_context->xut_day    <    1) || (xtm_context->xut_day   > 31) ||
        (xtm_context->xut_hour   >   23) ||
        (xtm_context->xut_minute >   59) ||
        (xtm_context->xut_second >   59) ||
        (xtm_context->xut_msec   >  999))
    {
        return xut_time;
    }
#endif

#ifdef _MSC_VER
    ULARGE_INTEGER xtime_value;
    FILETIME       xtime_sysfile;
    FILETIME       xtime_locfile;
    SYSTEMTIME     xtime_system;

    xtime_system.wYear         = xtm_context->xut_year  ;
    xtime_system.wMonth        = xtm_context->xut_month ;
    xtime_system.wDay          = xtm_context->xut_day   ;
    xtime_system.wDayOfWeek    = xtm_context->xut_week  ;
    xtime_system.wHour         = xtm_context->xut_hour  ;
    xtime_system.wMinute       = xtm_context->xut_minute;
    xtime_system.wSecond       = xtm_context->xut_second;
    xtime_system.wMilliseconds = xtm_context->xut_msec  ;

    if (SystemTimeToFileTime(&xtime_system, &xtime_locfile))
    {
        if (LocalFileTimeToFileTime(&xtime_locfile, &xtime_sysfile))
        {
            xtime_value.LowPart  = xtime_sysfile.dwLowDateTime ;
            xtime_value.HighPart = xtime_sysfile.dwHighDateTime;
            xut_time = xtime_value.QuadPart - NS100_1970;
        }
    }
#else // !_MSC_VER
    struct tm       xtm_system;
    x_ntp_timeval_t xtm_value;

    xtm_system.tm_sec   = xtm_context->xut_second;
    xtm_system.tm_min   = xtm_context->xut_minute;
    xtm_system.tm_hour  = xtm_context->xut_hour  ;
    xtm_system.tm_mday  = xtm_context->xut_day   ;
    xtm_system.tm_mon   = xtm_context->xut_month - 1   ;
    xtm_system.tm_year  = xtm_context->xut_year  - 1900;
    xtm_system.tm_wday  = 0;
    xtm_system.tm_yday  = 0;
    xtm_system.tm_isdst = 0;

    xtm_value.tv_sec  = mktime(&xtm_system);
    xtm_value.tv_usec = xtm_context->xut_msec * 1000;
    if (-1 != xtm_value.tv_sec)
    {
        xut_time = ntp_timeval_ns100(&xtm_value);
    }
#endif // _MSC_VER

    return xut_time;
}

/**********************************************************/
/**
 *      Converted (in 100 nanoseconds) time value (time from January 1, 1970 to the present)
 *      Describe the information for a specific time (i.e. x_ntp_time_context_t)
 *
 * @param [in ] xut_time    : Time value (time from January 1, 1970 to the present)
 * @param [out] xtm_context : The time description returned for the success of the operation.
 *
 * @return x_bool_t
 *      Success, returning X_TRUE;
 * -     Failed, returned X_FALSE
 */
x_bool_t ntp_tmctxt_bv(x_uint64_t xut_time, x_ntp_time_context_t * xtm_context)
{
#ifdef _MSC_VER
    ULARGE_INTEGER xtime_value;
    FILETIME       xtime_sysfile;
    FILETIME       xtime_locfile;
    SYSTEMTIME     xtime_system;

    if (X_NULL == xtm_context)
    {
        return X_FALSE;
    }

    xtime_value.QuadPart = xut_time + NS100_1970;
    xtime_sysfile.dwLowDateTime  = xtime_value.LowPart;
    xtime_sysfile.dwHighDateTime = xtime_value.HighPart;
    if (!FileTimeToLocalFileTime(&xtime_sysfile, &xtime_locfile))
    {
        return X_FALSE;
    }

    if (!FileTimeToSystemTime(&xtime_locfile, &xtime_system))
    {
        return X_FALSE;
    }

    xtm_context->xut_year   = xtime_system.wYear        ;
    xtm_context->xut_month  = xtime_system.wMonth       ;
    xtm_context->xut_day    = xtime_system.wDay         ;
    xtm_context->xut_week   = xtime_system.wDayOfWeek   ;
    xtm_context->xut_hour   = xtime_system.wHour        ;
    xtm_context->xut_minute = xtime_system.wMinute      ;
    xtm_context->xut_second = xtime_system.wSecond      ;
    xtm_context->xut_msec   = xtime_system.wMilliseconds;
#else // !_MSC_VER
    struct tm xtm_system;
    time_t xtm_time = (time_t)(xut_time / 10000000ULL);
    localtime_r(&xtm_time, &xtm_system);

    xtm_context->xut_year   = xtm_system.tm_year + 1900;
    xtm_context->xut_month  = xtm_system.tm_mon  + 1   ;
    xtm_context->xut_day    = xtm_system.tm_mday       ;
    xtm_context->xut_week   = xtm_system.tm_wday       ;
    xtm_context->xut_hour   = xtm_system.tm_hour       ;
    xtm_context->xut_minute = xtm_system.tm_min        ;
    xtm_context->xut_second = xtm_system.tm_sec        ;
    xtm_context->xut_msec   = (x_uint32_t)((xut_time % 10000000ULL) / 10000L);
#endif // _MSC_VER

    return X_TRUE;
}

/**********************************************************/
/**
 * Convert (type x_ntp_timeval_t) time values
 * Describe the information for a specific time (i.e. x_ntp_time_context_t)
 *
 * @param [in ] xtm_value   : Time value
 * @param [out] xtm_context : The time description returned for the success of the operation.
 *
 * @return x_bool_t
 *   Success, returning X_TRUE;
 * - Failed, returned X_FALSE
 */
x_bool_t ntp_tmctxt_tv(const x_ntp_timeval_t * const xtm_value, x_ntp_time_context_t * xtm_context)
{
    return ntp_tmctxt_bv(ntp_timeval_ns100(xtm_value), xtm_context);
}

/**********************************************************/
/**
 * @brief 
 *  Convert (type x_ntp_timeval_t) time values
 * is a specific time description information (i.e. x_ntp_time_context_t).
 *
 * @param [in ] xtm_timestamp :Time value.
 * @param [out] xtm_context   :The time description returned for the success of the operation
 *
 * @return x_bool_t
 *  Success, returning X_TRUE;
 * - Failed, returned X_FALSE.
 */
x_bool_t ntp_tmctxt_ts(const x_ntp_timestamp_t * const xtm_timestamp, x_ntp_time_context_t * xtm_context)
{
    return ntp_tmctxt_bv(ntp_timestamp_ns100(xtm_timestamp), xtm_context);
}

  

/**********************************************************/
/**
 * Determine whether the string is a valid 4-segment IP address format.
 *
 * @param [in ] xszt_vptr : The string of judgments
 * @param [out] xut_value : If the input parameter is not X_NULL, the corresponding IP address value is returned when the operation is successful.
 *
 * @return x_bool_t
 *  Success, returning X_TRUE;
 * - Failed, returned X_FALSE.
 */
static x_bool_t ntp_ipv4_valid(x_cstring_t xszt_vptr, x_uint32_t * xut_value)
{
    x_uchar_t xct_ipv[4] = { 0, 0, 0, 0 };

    x_int32_t xit_itv = 0;
    x_int32_t xit_sum = 0;
    x_bool_t  xbt_okv = X_FALSE;

    x_char_t    xct_iter = '\0';
    x_cchar_t * xct_iptr = xszt_vptr;

    if (X_NULL == xszt_vptr)
    {
        return X_FALSE;
    }

    for (xct_iter = *xszt_vptr; X_TRUE; xct_iter = *(++xct_iptr))
    {
        if ((xct_iter != '\0') && (xct_iter >= '0') && (xct_iter <= '9'))
        {
            xit_sum = 10 * xit_sum + (xct_iter - '0');
            xbt_okv = X_TRUE;
        }
        else if (xbt_okv && (('\0' == xct_iter) || ('.' == xct_iter)) && (xit_itv < (x_int32_t)sizeof(xct_ipv)) && (xit_sum <= 0xFF))
        {
            xct_ipv[xit_itv++] = xit_sum;
            xit_sum = 0;
            xbt_okv = X_FALSE;
        }
        else
            break;

        if ('\0' == xct_iter)
        {
            break;
        }
    }

#define MAKE_IPV4_VALUE(b1,b2,b3,b4)  ((x_uint32_t)(((x_uint32_t)(b1)<<24)+((x_uint32_t)(b2)<<16)+((x_uint32_t)(b3)<<8)+((x_uint32_t)(b4))))

    xbt_okv = (xit_itv == sizeof(xct_ipv)) ? X_TRUE : X_FALSE;
    if (X_NULL != xut_value)
    {
        *xut_value = xbt_okv ? MAKE_IPV4_VALUE(xct_ipv[0], xct_ipv[1], xct_ipv[2], xct_ipv[3]) : 0xFFFFFFFF;
    }

#undef MAKE_IPV4_VALUE

    return xbt_okv;
}

/**********************************************************/
/**
 * Get a table of IP addresses under the domain name (instead of the system's gethostbyname() API call)
 *
 * @param [in ] xszt_dname :The specified domain name (in the format: www.163.com).
 * @param [in ] xit_family :The type of socket address structure expected to be returned
 * @param [out] xvec_host  :The list of addresses returned for a successful operation.
 *
 * @return x_int32_t
 *  Success, returning 0;
 * - Failed, returned error code.
 */
static x_int32_t ntp_gethostbyname(x_cstring_t xszt_dname, x_int32_t xit_family, std::vector< std::string > & xvec_host)
{
    x_int32_t xit_err = -1;

    struct addrinfo   xai_hint;
    struct addrinfo * xai_rptr = X_NULL;
    struct addrinfo * xai_iptr = X_NULL;

    x_char_t xszt_iphost[TEXT_LEN_256] = { 0 };

    do
    {
          

        if (X_NULL == xszt_dname)
        {
            break;
        }

        memset(&xai_hint, 0, sizeof(xai_hint));
        xai_hint.ai_family   = xit_family;
        xai_hint.ai_socktype = SOCK_DGRAM;

        xit_err = getaddrinfo(xszt_dname, X_NULL, &xai_hint, &xai_rptr);
        if (0 != xit_err)
        {
            break;
        }

          

        for (xai_iptr = xai_rptr; X_NULL != xai_iptr; xai_iptr = xai_iptr->ai_next)
        {
            if (xit_family != xai_iptr->ai_family)
            {
                continue;
            }

            memset(xszt_iphost, 0, TEXT_LEN_256);
            if (X_NULL == inet_ntop(xit_family, &(((struct sockaddr_in *)(xai_iptr->ai_addr))->sin_addr), xszt_iphost, TEXT_LEN_256))
            {
                continue;
            }

            xvec_host.push_back(std::string(xszt_iphost));
        }

          

        xit_err = (xvec_host.size() > 0) ? 0 : -3;
    } while (0);

    if (X_NULL != xai_rptr)
    {
        freeaddrinfo(xai_rptr);
        xai_rptr = X_NULL;
    }

    return xit_err;
}

/**********************************************************/
/**
 * Returns the error code of the socket current operation failure.
 */
static x_int32_t ntp_sockfd_lasterror()
{
#ifdef _MSC_VER
    return (x_int32_t)WSAGetLastError();
#else // !_MSC_VER
    return errno;
#endif // _MSC_VER
}

/**********************************************************/
/**
 * Close the socket.
 */
static x_int32_t ntp_sockfd_close(x_sockfd_t xfdt_sockfd)
{
#ifdef _MSC_VER
    return closesocket(xfdt_sockfd);
#else // !_MSC_VER
    return close(xfdt_sockfd);
#endif // _MSC_VER
}

/**********************************************************/
/**
 * Initialize the NTP request packet.
 */
static x_void_t ntp_init_request_packet(x_ntp_packet_t * xnpt_dptr)
{
    const x_uchar_t xct_leap_indicator = 0;
    const x_uchar_t xct_ntp_version    = 3;
    const x_uchar_t xct_ntp_mode       = ntp_mode_client;

    xnpt_dptr->xct_li_ver_mode = (xct_leap_indicator << 6) | (xct_ntp_version << 3) | (xct_ntp_mode << 0);
    xnpt_dptr->xct_stratum     = 0;
    xnpt_dptr->xct_poll        = 4;
    xnpt_dptr->xct_percision   = ((-6) & 0xFF);

    xnpt_dptr->xut_root_delay      = (1 << 16);
    xnpt_dptr->xut_root_dispersion = (1 << 16);
    xnpt_dptr->xut_ref_indentifier = 0;

    xnpt_dptr->xtmst_reference.xut_seconds  = 0;
    xnpt_dptr->xtmst_reference.xut_fraction = 0;
    xnpt_dptr->xtmst_originate.xut_seconds  = 0;
    xnpt_dptr->xtmst_originate.xut_fraction = 0;
    xnpt_dptr->xtmst_receive  .xut_seconds  = 0;
    xnpt_dptr->xtmst_receive  .xut_fraction = 0;
    xnpt_dptr->xtmst_transmit .xut_seconds  = 0;
    xnpt_dptr->xtmst_transmit .xut_fraction = 0;
}

/**********************************************************/
/**
 * Convert the Network endianness field in x_ntp_packet_t to Host endianness
 */
static x_void_t ntp_ntoh_packet(x_ntp_packet_t * xnpt_nptr)
{
#if 0
    xnpt_nptr->xct_li_ver_mode = xnpt_nptr->xct_li_ver_mode;
    xnpt_nptr->xct_stratum     = xnpt_nptr->xct_stratum    ;
    xnpt_nptr->xct_poll        = xnpt_nptr->xct_poll       ;
    xnpt_nptr->xct_percision   = xnpt_nptr->xct_percision  ;
#endif
    xnpt_nptr->xut_root_delay               = ntohl(xnpt_nptr->xut_root_delay              );
    xnpt_nptr->xut_root_dispersion          = ntohl(xnpt_nptr->xut_root_dispersion         );
    xnpt_nptr->xut_ref_indentifier          = ntohl(xnpt_nptr->xut_ref_indentifier         );
    xnpt_nptr->xtmst_reference.xut_seconds  = ntohl(xnpt_nptr->xtmst_reference.xut_seconds );
    xnpt_nptr->xtmst_reference.xut_fraction = ntohl(xnpt_nptr->xtmst_reference.xut_fraction);
    xnpt_nptr->xtmst_originate.xut_seconds  = ntohl(xnpt_nptr->xtmst_originate.xut_seconds );
    xnpt_nptr->xtmst_originate.xut_fraction = ntohl(xnpt_nptr->xtmst_originate.xut_fraction);
    xnpt_nptr->xtmst_receive  .xut_seconds  = ntohl(xnpt_nptr->xtmst_receive  .xut_seconds );
    xnpt_nptr->xtmst_receive  .xut_fraction = ntohl(xnpt_nptr->xtmst_receive  .xut_fraction);
    xnpt_nptr->xtmst_transmit .xut_seconds  = ntohl(xnpt_nptr->xtmst_transmit .xut_seconds );
    xnpt_nptr->xtmst_transmit .xut_fraction = ntohl(xnpt_nptr->xtmst_transmit .xut_fraction);
}

/**********************************************************/
/**
 * Convert the Host endianness field in x_ntp_packet_t to Network endianness.
 */
static x_void_t ntp_hton_packet(x_ntp_packet_t * xnpt_nptr)
{
#if 0
    xnpt_nptr->xct_li_ver_mode = xnpt_nptr->xct_li_ver_mode;
    xnpt_nptr->xct_stratum     = xnpt_nptr->xct_stratum    ;
    xnpt_nptr->xct_poll        = xnpt_nptr->xct_poll       ;
    xnpt_nptr->xct_percision   = xnpt_nptr->xct_percision  ;
#endif
    xnpt_nptr->xut_root_delay               = htonl(xnpt_nptr->xut_root_delay              );
    xnpt_nptr->xut_root_dispersion          = htonl(xnpt_nptr->xut_root_dispersion         );
    xnpt_nptr->xut_ref_indentifier          = htonl(xnpt_nptr->xut_ref_indentifier         );
    xnpt_nptr->xtmst_reference.xut_seconds  = htonl(xnpt_nptr->xtmst_reference.xut_seconds );
    xnpt_nptr->xtmst_reference.xut_fraction = htonl(xnpt_nptr->xtmst_reference.xut_fraction);
    xnpt_nptr->xtmst_originate.xut_seconds  = htonl(xnpt_nptr->xtmst_originate.xut_seconds );
    xnpt_nptr->xtmst_originate.xut_fraction = htonl(xnpt_nptr->xtmst_originate.xut_fraction);
    xnpt_nptr->xtmst_receive  .xut_seconds  = htonl(xnpt_nptr->xtmst_receive  .xut_seconds );
    xnpt_nptr->xtmst_receive  .xut_fraction = htonl(xnpt_nptr->xtmst_receive  .xut_fraction);
    xnpt_nptr->xtmst_transmit .xut_seconds  = htonl(xnpt_nptr->xtmst_transmit .xut_seconds );
    xnpt_nptr->xtmst_transmit .xut_fraction = htonl(xnpt_nptr->xtmst_transmit .xut_fraction);
}

/**********************************************************/
/**
 *  Send an NTP request to the NTP server to obtain the timestamp required for the relevant calculation (T1, T2, T3, T4 as described below).
 * <pre>
 * The client sends an NTP packet to the server with the timestamp when it leaves the client, which is T1.
 * When this NTP packet arrives at the server, the server adds its own timestamp, which is T2.
 * When this NTP packet leaves the server, the server adds its own timestamp, which is T3.
 * When the client receives the reply packet, the local timestamp of the client is T4
 * </pre>
 *
 *                          The IP of the NTP server (four-segment IP address).
 * @param [in ] xut_port  :The port number of the NTP server (whichever is the default port number NTP_PORT : 123).
 * @param [in ] xut_tmout :Timeout (in milliseconds)
 * @param [out] xit_tmlst :The timestamp required for the computation returned by the operation successfully (T1, T2, T3, T4).
 *
 * @return x_int32_t
 * - Success, returns 0;
 * - Failed, returned error code.
 */
static x_int32_t ntp_get_time_values(x_cstring_t xszt_host, x_uint16_t xut_port, x_uint32_t xut_tmout, x_int64_t xit_tmlst[4])
{
    x_int32_t xit_err = -1;

    x_sockfd_t      xfdt_sockfd = X_INVALID_SOCKFD;
    x_ntp_packet_t  xnpt_buffer;
    x_ntp_timeval_t xtm_value;

    x_int32_t xit_addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in skaddr_host;

    do 
    {
          

        if ((X_NULL == xszt_host) || (xut_tmout <= 0) || (X_NULL == xit_tmlst))
        {
            break;
        }

          

        xfdt_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (X_INVALID_SOCKFD == xfdt_sockfd)
        {
            break;
        }

        //Set the Send/Receive timeout
#ifdef _MSC_VER
        setsockopt(xfdt_sockfd, SOL_SOCKET, SO_SNDTIMEO, (x_char_t *)&xut_tmout, sizeof(x_uint32_t));
        setsockopt(xfdt_sockfd, SOL_SOCKET, SO_RCVTIMEO, (x_char_t *)&xut_tmout, sizeof(x_uint32_t));
#else // !_MSC_VER
        xtm_value.tv_sec  = (x_long_t)((xut_tmout / 1000));
        xtm_value.tv_usec = (x_long_t)((xut_tmout % 1000) * 1000);
        setsockopt(xfdt_sockfd, SOL_SOCKET, SO_SNDTIMEO, (x_char_t *)&xtm_value, sizeof(x_ntp_timeval_t));
        setsockopt(xfdt_sockfd, SOL_SOCKET, SO_RCVTIMEO, (x_char_t *)&xtm_value, sizeof(x_ntp_timeval_t));
#endif // _MSC_VER

        //The server host address
        memset(&skaddr_host, 0, sizeof(struct sockaddr_in));
        skaddr_host.sin_family = AF_INET;
        skaddr_host.sin_port   = htons(xut_port);
        inet_pton(AF_INET, xszt_host, &skaddr_host.sin_addr.s_addr);

          

        //Initialize the request packet
        ntp_init_request_packet(&xnpt_buffer);

        // The local time of the NTP request packet when it leaves the sender
        ntp_gettimeofday(&xtm_value);
        ntp_timeval_to_timestamp(&xnpt_buffer.xtmst_originate, &xtm_value);

        
        xit_tmlst[0] = (x_int64_t)ntp_timeval_ns100(&xtm_value);

       // Converted to network endianness
        ntp_hton_packet(&xnpt_buffer);

        xit_err = sendto(xfdt_sockfd,
                         (x_char_t *)&xnpt_buffer,
                         sizeof(x_ntp_packet_t),
                         0,
                         (sockaddr *)&skaddr_host,
                         sizeof(struct sockaddr_in));
        if (xit_err < 0)
        {
            xit_err = ntp_sockfd_lasterror();
            continue;
        }

          

        memset(&xnpt_buffer, 0, sizeof(x_ntp_packet_t));

        //Receive an answer
        xit_err = recvfrom(xfdt_sockfd,
                           (x_char_t *)&xnpt_buffer,
                           sizeof(x_ntp_packet_t),
                           0,
                           (sockaddr *)&skaddr_host,
                           (socklen_t *)&xit_addrlen);
        if (xit_err < 0)
        {
            xit_err = ntp_sockfd_lasterror();
            break;
        }

        if (sizeof(x_ntp_packet_t) != xit_err)
        {
            xit_err = -1;
            break;
        }

        
        xit_tmlst[3] = (x_int64_t)ntp_gettimevalue();

        //Converted to host endianness
        ntp_ntoh_packet(&xnpt_buffer);

        xit_tmlst[1] = (x_int64_t)ntp_timestamp_ns100(&xnpt_buffer.xtmst_receive ); 
        xit_tmlst[2] = (x_int64_t)ntp_timestamp_ns100(&xnpt_buffer.xtmst_transmit); 

          
        xit_err = 0;
    } while (0);

    if (X_INVALID_SOCKFD != xfdt_sockfd)
    {
        ntp_sockfd_close(xfdt_sockfd);
        xfdt_sockfd = X_INVALID_SOCKFD;
    }

    return xit_err;
}

/**********************************************************/
/**
 * @brief Send an NTP request to the NTP server to get the server timestamp.
 * 
 * @param [in ] xszt_host : The IP (four-segment IP address) or domain name (such as 3.cn.pool.ntp.org) of the NTP server.
 * @param [in ] xut_port  : Port number of the NTP server (default port number NTP_PORT : 123)
 * @param [in ] xut_tmout : Timeout for network requests in milliseconds
 * @param [out] xut_timev : The answer time value returned by the successful operation in 100 nanoseconds from January 1, 1970 to the present.
 * 
 * @return x_int32_t
 * Success, returning 0;
 * - Failed, returned error code.
 */
x_int32_t ntp_get_time(x_cstring_t xszt_host, x_uint16_t xut_port, x_uint32_t xut_tmout, x_uint64_t * xut_timev)
{
    x_int32_t xit_err = -1;
    std::vector< std::string > xvec_host;

    x_int64_t xit_tmlst[4] = { 0 };

      
    //Parameter validation

    if ((X_NULL == xszt_host) || (xut_tmout <= 0) || (X_NULL == xut_timev))
    {
        return -1;
    }

      
    // Get a list of IP addresses

    if (ntp_ipv4_valid(xszt_host, X_NULL))
    {
        xvec_host.push_back(std::string(xszt_host));
    }
    else
    {
        xit_err = ntp_gethostbyname(xszt_host, AF_INET, xvec_host);
        if (0 != xit_err)
        {
            return xit_err;
        }
    }

    if (xvec_host.empty())
    {
        return -1;
    }

    for (std::vector< std::string >::iterator itvec = xvec_host.begin(); itvec != xvec_host.end(); ++itvec)
    {

        xit_err = ntp_get_time_values(itvec->c_str(), xut_port, xut_tmout, xit_tmlst);
        if (0 == xit_err)
        {
            
            *xut_timev = xit_tmlst[3] + ((xit_tmlst[1] - xit_tmlst[0]) + (xit_tmlst[2] - xit_tmlst[3])) / 2;
            break;
        }
    }
    return xit_err;
}

x_int32_t ntp_get_time_test(x_cstring_t xszt_host, x_uint16_t xut_port, x_uint32_t xut_tmout, x_uint64_t * xut_timev)
{
    x_int32_t xit_err = -1;
    std::vector< std::string > xvec_host;

    x_int64_t xit_tmlst[4] = { 0 };
      
    if ((X_NULL == xszt_host) || (xut_tmout <= 0) || (X_NULL == xut_timev))
    {
        return -1;
    }
      
    if (ntp_ipv4_valid(xszt_host, X_NULL))
    {
        xvec_host.push_back(std::string(xszt_host));
    }
    else
    {
        xit_err = ntp_gethostbyname(xszt_host, AF_INET, xvec_host);
        if (0 != xit_err)
        {
            return xit_err;
        }
    }

    if (xvec_host.empty())
    {
        return -1;
    }

      

    for (std::vector< std::string >::iterator itvec = xvec_host.begin(); itvec != xvec_host.end(); ++itvec)
    {

        xit_err = ntp_get_time_values(itvec->c_str(), xut_port, xut_tmout, xit_tmlst);
        if (0 == xit_err)
        {
            XOUTLINE("========================================");
            XOUTLINE("  %s -> %s\n", xszt_host, itvec->c_str());
            
            *xut_timev = xit_tmlst[3] + ((xit_tmlst[1] - xit_tmlst[0]) + (xit_tmlst[2] - xit_tmlst[3])) / 2;

            BV_OUTPUT("time1", xit_tmlst[0]);
            BV_OUTPUT("time2", xit_tmlst[1]);
            BV_OUTPUT("time3", xit_tmlst[2]);
            BV_OUTPUT("time4", xit_tmlst[3]);
            BV_OUTPUT("timev", *xut_timev);
            BV_OUTPUT("timec", ntp_gettimevalue());

            printf("\tdelay : %lldms\n", (xit_tmlst[3] - xit_tmlst[0] - (xit_tmlst[2] - xit_tmlst[1]))/10000 );

            break;
        }
    }

      

    return xit_err;
}

