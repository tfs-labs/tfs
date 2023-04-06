#ifndef __BASE_64_H
#define __BASE_64_H
#include <string>
class Base64
{
    std::string _base64_table;
    static const char base64_pad = '='; public:
    Base64()
    {
        _base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; /*����Base64����ʹ�õı�׼�ֵ�*/
    }
    
public:
    std::string Encode(const unsigned char* str, int bytes);
    std::string Decode(const char* str, int bytes);

};

#endif

