#include <string>
#include <random>
#include <chrono>

#include "./pack.h"

#include "../../include/logging.h"
#include "../proto/net.pb.h"
#include "../../utils/util.h"


void Pack::PackagToBuff(const NetPack & pack, char* buff, int buffLen)
{
	memcpy(buff, &pack.len, 4);
	memcpy(buff + 4, pack.data.data(), pack.data.size());
	memcpy(buff + 4 + pack.data.size(), &pack.checkSum, 4);
	memcpy(buff + 4 + pack.data.size() + 4, &pack.flag, 4);
	memcpy(buff + 4 + pack.data.size() + 4 + 4, &pack.endFlag, 4);
}

std::string Pack::PackagToStr(const NetPack& pack)
{
	int buffSize = pack.len + sizeof(int);
	char* buff = new char[buffSize]{0};
	Pack::PackagToBuff(pack, buff, buffSize);
	std::string msg(buff, buffSize);
	delete [] buff;
	return msg;
}

bool Pack::ApartPack( NetPack& pk, const char* pack, int packLen)
{
	if (NULL == pack)
	{
		ERRORLOG("ApartPack is NULL");
		return false;
	}
	if(packLen < 12)
	{	
		ERRORLOG("ApartPack len < 12");
		return false;
	}

	pk.len = packLen;
	pk.data = std::string(pack , packLen - sizeof(uint32_t) * 3); //checkSum, flagend_flag Subtract checkSum, flag and endFlag

	memcpy(&pk.checkSum, pack + pk.data.size(),   	  4);
	memcpy(&pk.flag, pack + pk.data.size() + 4,     	4);
	memcpy(&pk.endFlag, pack + pk.data.size() + 4 + 4, 4);

	return true;
}


bool Pack::PackCommonMsg(const CommonMsg& msg, const int8_t priority, NetPack& pack)
{
	pack.data = msg.SerializeAsString();
	pack.len = pack.data.size() + sizeof(int) + sizeof(int) + sizeof(int);
	pack.checkSum = Util::adler32((unsigned char *)pack.data.data(), pack.data.size());
	pack.flag = priority & 0xF;

	return true;
}
