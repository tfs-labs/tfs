#include "tmplog.h"

void write_tmplog(const std::string& content, OUTTYPE out, const std::string& log_name)
{
    if (out == file)
    {
        std::string fileName = log_name;
        std::ofstream file(fileName, std::ios::app);
        if (!file.is_open() )
        {
            ERRORLOG("Open file failed!");
            return;
        }
        file << content << std::endl;
        file.close();
    }
    else if (out == screen)
    {
        std::cout << content << std::endl;
    }

}

