#ifndef _Util_H_
#define _Util_H_

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <functional>
#include "../utils/string_util.h"

class Util
{
    
public:
    Util();
    ~Util();
    /**
     * @brief       
     * 
     * @param       data:
     * @param       len:
     * @return      uint32_t 
    */
    static uint32_t adler32(const unsigned char *data, size_t len); 
    /**
     * @brief       
     * 
     * @param       recvVersion:
     * @return      int 
    */
    static int IsVersionCompatible( std::string recvVersion );
    /**
     * @brief       
     * 
     * @param       vRecvVersion:
     * @return      int 
    */
    static int IsLinuxVersionCompatible(const std::vector<std::string> & vRecvVersion);
    /**
     * @brief       
     * 
     * @param       vRecvVersion:
     * @param       bIsAndroid:
     * @return      int 
    */
    static int IsOtherVersionCompatible(const std::string & vRecvVersion, bool bIsAndroid);

    static int64_t integerRound(int64_t number);

    static int64_t Unsign64toSign64(uint64_t u);
};


#endif