/**
 * *****************************************************************************
 * @file        base64.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __BASE_64_H
#define __BASE_64_H
#include <string>
class Base64
{
    std::string base64Table;
    static const char base64_pad = '='; 
public:
    Base64()
    {
        base64Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; /*����Base64����ʹ�õı�׼�ֵ�*/
    }
    
public:
    /**
     * @brief       
     * 
     * @param       str 
     * @param       bytes 
     * @return      std::string 
     */
    std::string Encode(const unsigned char* str, int bytes);

    /**
     * @brief       
     * 
     * @param       str 
     * @param       bytes 
     * @return      std::string 
     */
    std::string Decode(const char* str, int bytes);

};

#endif

