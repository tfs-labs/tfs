#include "tmp_log.h"
#include <execinfo.h>

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

#define  MAX_SIZE 102400

std::string print_trace(void)
{
    std::string ret;
    size_t i, size;
    void *array[MAX_SIZE];
    size = backtrace(array, MAX_SIZE);
   	char **strings = backtrace_symbols(array, size);
    for (i = 0; i < size; i++){
         printf("%ld# %s\n",i, strings[i]);
    }
    free(strings);
    return 0;
}

