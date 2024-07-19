#ifndef __CA_HEXCODE_H__
#define __CA_HEXCODE_H__
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief
 * 
 * @param       hexstr:
 * @param       p_:
 * @param       len:
*/
void encode_hex(char *hexstr, const void *p_, size_t len);
/**
 * @brief
 * 
 * @param       p:
 * @param       max_len:
 * @param       hexstr:
 * @param       out_len_:
 * @return      true
 * @return      false
*/
bool decode_hex(void *p, size_t max_len, const char *hexstr, size_t *out_len_);
/**
 * @brief
 * 
 * @param       hexstr:
 * @param       len:
*/
void hex_print(const unsigned char *hexstr, const int len);
/**
 * @brief
 * 
 * @param       rawStr:
 * @return      string
*/
std::string Str2Hex(const std::string & rawStr);
/**
 * @brief
 * 
 * @param       hexstr:
 * @return      string
*/
std::string Hex2Str(const std::string & hexStr);


#ifdef __cplusplus
}
#endif
namespace Test
{
	template<class Elem, class Traits>
	inline void hex_dump(const void* aData, std::size_t aLength, std::basic_ostream<Elem, Traits>& aStream, std::size_t aWidth = 16)
	{
		const char* const start = static_cast<const char*>(aData);
		const char* const end = start + aLength;
		const char* line = start;
		while (line != end)
		{
			aStream << "    ";
			aStream.width(4);
			aStream.fill('0');
			aStream << std::hex << line - start << " : ";
			std::size_t lineLength = std::min(aWidth, static_cast<std::size_t>(end - line));
			for (std::size_t pass = 1; pass <= 2; ++pass)
			{
				for (const char* next = line; next != end && next != line + aWidth; ++next)
				{
					char ch = *next;
					switch(pass)
					{
					case 1:
						aStream << (ch < 32 ? '.' : ch);
						break;
					case 2:
						if (next != line)
							aStream << " ";
						aStream.width(2);
						aStream.fill('0');
						aStream << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(ch));
						break;
					}
				}
				if (pass == 1 && lineLength != aWidth)
					aStream << std::string(aWidth - lineLength, ' ');
				aStream << " ";
			}
			aStream << std::endl;
			line = line + lineLength;
		}
	}

    inline void hex_dump(const std::string &data)
    {
        hex_dump(data.data(), data.size(), std::cout);
    }
}
void encode_hex_uint8_t(uint8_t *hexstr, const uint8_t *p_, size_t len);

void string_to_hex_array(const std::string& str, uint8_t* hex_array, size_t array_size);
#endif
