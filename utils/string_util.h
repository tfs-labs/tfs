#ifndef _STRINGUTIL_H_
#define _STRINGUTIL_H_

#include <string>
#include <vector>

class StringUtil
{   
public:
    StringUtil() = default;
    ~StringUtil() = default;
    /**
    * @brief       
    * @param       str: 
    * @param       bLeft:
    * @param       bRight:
    */
    void Trim(std::string& str, bool bLeft, bool bRight);
    /**
    * @brief       
    * @param       v: 
    * @param       sign:
    * @return      string
    */
    static std::string concat(const std::vector<std::string>& v, std::string sign)
    {
        std::string str;
        for(size_t i = 0; i < v.size(); i++)
        {  
            str += (v[i] + sign);
        }
        if(str.size() == 0)
        {
            return str;
        }
        str.pop_back();
        return str;
    }
    /**
    * @brief       
    * @param       s: 
    * @param       c:
    * @param       v:
    */
    static void SplitString(const std::string& s,  const std::string& c, std::vector<std::string>& v );

    static int64_t StringToNumber(const std::string& s);
	
};


#endif