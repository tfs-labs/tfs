#ifndef _DEFINE_H_
#define _DEFINE_H_

#include <cstdint>
#include <string>

constexpr int END_FLAG              = 7777777;             
constexpr int IP_LEN				= 16;	//IPlenght
			  
constexpr int MAXEPOLLSIZE			= 100000;
constexpr int MAXLINE				= 10240l;
			   
constexpr int HEART_INTVL     =  60;   // detect how many times to start sending heartbeat packets,
constexpr int HEART_PROBES    =  3;   //If the other party does not respond after sending several heartbeat packets, the connection is close


typedef int int32;
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef char int8;

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;

using i64 = std::int64_t;

using nll = long long;
using ull = unsigned long long;


//Network packet body
typedef struct NetPack
{
	uint32_t	len				= 0;
	std::string	data			= "";
	uint32_t   	checkSum		= 0;
	uint32_t	flag			= 0;
	uint32_t    endFlag        = END_FLAG;
}NetPack;

#endif