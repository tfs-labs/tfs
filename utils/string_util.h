#ifndef _STRINGUTIL_H_
#define _STRINGUTIL_H_

#include <string>
#include <vector>

class StringUtil
{   
public:
    StringUtil() = default;
    ~StringUtil() = default;
    
    void Trim(std::string& str, bool bLeft, bool bRight);
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

    static void SplitString(const std::string& s,  const std::string& c, std::vector<std::string>& v );
	
};


#endif