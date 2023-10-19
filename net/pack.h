/**
 * *****************************************************************************
 * @file        pack.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _PACK_H_
#define _PACK_H_

#include <string>

#include "./peer_node.h"

#include "../proto/net.pb.h"
#include "../proto/common.pb.h"
#include "../utils/compress.h"
#include "../common/global.h"

class Pack
{
public:
	/**
	 * @brief       
	 * 
	 * @param       pack 
	 * @param       buff 
	 * @param       buffLen 
	 */
	static void PackagToBuff(const NetPack & pack, char* buff, int buffLen);
	
	/**
	 * @brief       
	 * 
	 * @param       pack 
	 * @return      std::string 
	 */
	static std::string PackagToStr(const NetPack& pack);

	/**
	 * @brief       
	 * 
	 * @param       pk 
	 * @param       pack 
	 * @param       len 
	 * @return      true 
	 * @return      false 
	 */
	static bool ApartPack(NetPack& pk, const char* pack, int len);

	/**
	 * @brief       
	 * 
	 * @tparam T 
	 * @param       msg 
	 * @param       submsg 
	 * @param       encrypt 
	 * @param       compress 
	 * @return      true 
	 * @return      false 
	 */
	template <typename T>
	static bool InitCommonMsg(CommonMsg & msg, T& submsg, int32_t encrypt = 0, int32_t compress = 0);

	/**
	 * @brief       
	 * 
	 * @param       msg 
	 * @param       priority 
	 * @param       pack 
	 * @return      true 
	 * @return      false 
	 */
	static bool PackCommonMsg(const CommonMsg& msg, const int8_t priority, NetPack& pack);

};

/**
 * @brief       
 * 
 */
template <typename T>
bool Pack::InitCommonMsg(CommonMsg& msg, T& submsg, int32_t encrypt, int32_t compress)
{
	msg.set_type(submsg.descriptor()->name());
	msg.set_version(global::kNetVersion);
	msg.set_encrypt(encrypt);
	
	const string & tmp = submsg.SerializeAsString();
	if (compress) 
	{
		Compress cpr(tmp);
		//Try compression, if the compression ratio is poor, do not use compression
		if (cpr._compressData.size() > tmp.size())
		{
			msg.set_compress(0);
			msg.set_data(tmp);
		}
		else
		{
			msg.set_compress(compress);
			msg.set_data(cpr._compressData);
		}
	}
	else 
	{
		msg.set_compress(0);
		msg.set_data(tmp);
	}
	return true;
}

#endif//_PACK_H_