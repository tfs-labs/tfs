#include <sys/time.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "./util.h"
#include "common/global.h"
#include "logging.h"
uint32_t Util::adler32(const unsigned char *data, size_t len) 
{
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1, b = 0;
    size_t index;
    
    // Process each byte of the data in order
    for (index = 0; index < len; ++index)
    {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

int Util::IsLinuxVersionCompatible(const std::vector<std::string> & vRecvVersion)
{
	if (vRecvVersion.size() != 3)
	{
		ERRORLOG("(linux) version error: -1");
		return -1;
	}

	if (global::kBuildType == global::BuildType::kBuildType_Primary)
	{
		if (vRecvVersion[2] != "p")
		{
			ERRORLOG("(linux) version error: -2");
			return -2;
		}
	}
	else if (global::kBuildType == global::BuildType::kBuildType_Test)
	{
		if (vRecvVersion[2] != "t")
		{
			ERRORLOG("(linux) version error: -3");
			return -3;
		}
	}
	else
	{
		if (vRecvVersion[2] != "d")
		{
			ERRORLOG("(linux) version error: -4");
			return -4;
		}
	}

	std::string ownerVersion = global::kVersion;
	std::vector<std::string> vOwnerVersion;
	StringUtil::SplitString(ownerVersion, "_", vOwnerVersion);

	if (vOwnerVersion.size() != 3)
	{
		ERRORLOG("(linux) version error: -5");
		return -5;
	}

	std::vector<std::string> vOwnerVersionNum;
	StringUtil::SplitString(vOwnerVersion[1], ".", vOwnerVersionNum);
	if (vOwnerVersionNum.size() == 0)
	{
		ERRORLOG("(linux) version error: -6");
		return -6;
	}

	std::vector<std::string> vRecvVersionNum;
	StringUtil::SplitString(vRecvVersion[1], ".", vRecvVersionNum);
	if (vRecvVersionNum.size() != vOwnerVersionNum.size() || vRecvVersionNum.size() != 3)
	{
		ERRORLOG("(linux) version error: -7");
		return -7;
	}

	if (vRecvVersionNum[0] != vOwnerVersionNum[0])
	{
		ERRORLOG("(linux) version error: -8");
		return -8;
	}

	if (vRecvVersionNum[1] != vOwnerVersionNum[1])
	{
		ERRORLOG("(linux) version error: -9");
		return -9;
	}

	return 0;
}

int Util::IsOtherVersionCompatible(const std::string & vRecvVersion, bool bIsAndroid)
{
	if (vRecvVersion.size() == 0)
	{
		ERRORLOG("(other)  version error: -1");
		return -1;
	}

	std::vector<std::string> vRecvVersionNum;
	StringUtil::SplitString(vRecvVersion, ".", vRecvVersionNum);
	if (vRecvVersionNum.size() != 3)
	{
		ERRORLOG("(other)  version error: -2");
		return -2;
	}

	std::string ownerVersion;
	if (bIsAndroid)
	{
		ownerVersion = global::kAndroidCompatible;
	}
	else
	{
		ownerVersion = global::kIOSCompatible;
	}
	
	std::vector<std::string> vOwnerVersionNum;
	StringUtil::SplitString(ownerVersion, ".", vOwnerVersionNum);

	for (size_t i = 0; i < vOwnerVersionNum.size(); ++i)
	{
		if (vRecvVersionNum[i] < vOwnerVersionNum[i])
		{
			ERRORLOG("(other)  version error: -3");
			ERRORLOG("(other) receive version: {}", vRecvVersion); // receive version
			ERRORLOG("(other) local version: {}", ownerVersion); // local version
			return -3;
		}
		else if (vRecvVersionNum[i] > vOwnerVersionNum[i])
		{
			return 0;
		}
	}

	return 0;
}

int Util::IsVersionCompatible( std::string recvVersion )
{
	if (recvVersion.size() == 0)
	{
		ERRORLOG(" version error: -1");
		return -1;
	}

	std::vector<std::string> vRecvVersion;
	StringUtil::SplitString(recvVersion, "_", vRecvVersion);
	if (vRecvVersion.size() < 1 || vRecvVersion.size() > 3 )
	{
		ERRORLOG(" version error: -2");
		return -2;
	}

	if (vRecvVersion.size() == 1)
	{
		if (vRecvVersion[0] == global::kNetVersion)
		{
			return 0;
		}
		else
		{
			ERRORLOG(" version error: -3");
			return -3;
		}
	}

	int versionPrefix = std::stoi(vRecvVersion[0]);
	if (versionPrefix > 4 || versionPrefix < 1)
	{
		ERRORLOG(" version error: -3");
		return -4;
	}
	
	switch(versionPrefix)
	{
		case 1:
		{
			if ( 0 != IsLinuxVersionCompatible(vRecvVersion) )
			{
				return -5;
			}
			break;
		}
		case 2:
		{
			return -6;
		}
		case 3:
		{
			if ( 0 != IsOtherVersionCompatible(vRecvVersion[1], false) )
			{
				return -7;
			}
			break;
		}
		case 4:
		{
			if ( 0 != IsOtherVersionCompatible(vRecvVersion[1], true) )
			{
				return -8;
			}
			break;
		}
		default:
		{
			return -9;
		}
	}
	return 0;
}

int64_t Util::integerRound(int64_t number)
{
    int64_t floor = number / 10 * 10;
    int64_t remainder = number % 10;
    if (remainder < 5 )
    {
        return floor;
    }
    else
    {
        return floor + 10;
    }
}

int64_t Util::Unsign64toSign64(uint64_t u)
{
    if (u > static_cast<size_t>((std::numeric_limits<int64_t>::max)()))
    {
        throw std::overflow_error(
                "uint64_t value " +  std::to_string(u) + " cannot be stored in a variable of type int.");
    }

    return static_cast<int64_t>(u);
}