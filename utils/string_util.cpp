
#include "./string_util.h"


void StringUtil::Trim(std::string& str, bool bLeft, bool bRight)
{
    static const std::string delims = " \t\r\n";
    if(bRight)
        str.erase(str.find_last_not_of(delims)+1); // trim right
    if(bLeft)
        str.erase(0, str.find_first_not_of(delims)); // trim left
}


void StringUtil::SplitString(const std::string& s, const std::string& c ,std::vector<std::string>& v)
{
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2)
    {
        std::string str = s.substr(pos1, pos2-pos1);

        if (!str.empty())
        {
            v.push_back(str);
        }
        
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }

    if(pos1 != s.length())
    {
        std::string str = s.substr(pos1);
        if (!str.empty())
        {
            v.push_back(str);
        }
    }
}
