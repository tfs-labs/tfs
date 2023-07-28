#ifndef _CA_CONSOLE_H_
#define _CA_CONSOLE_H_

#include <string>

#define RESET "\033[0m"
#define BLACK "\033[30m"	/* Black */
#define RED "\033[31m"		/* Red */
#define GREEN "\033[32m"	/* Green */
#define YELLOW "\033[33m"	/* Yellow */
#define BLUE "\033[34m"		/* Blue */

typedef enum ConsoleColor
{
    kConsoleColor_Black,
    kConsoleColor_Red,
    kConsoleColor_Green,
    kConsoleColor_Yellow,
    kConsoleColor_Blue,
    kConsoleColor_Purple,
    kConsoleColor_DarkGreen,
    kConsoleColor_White,

} ConsoleColor;

class ca_console
{
public:
    ca_console(const ConsoleColor foregroundColor = kConsoleColor_White, 
                const ConsoleColor backgroundColor = kConsoleColor_Black, 
                bool highlight = false);
    ~ca_console();
    
    const std::string color(); //Returns the color
    void setColor(const ConsoleColor foregroundColor = kConsoleColor_White, 
                const ConsoleColor backgroundColor = kConsoleColor_Black, 
                bool highlight = false);
    
    const std::string reset(); //reset
    void clear(); //clear

    operator const char * ();
    operator char * ();

private:
    std::string bColorString;
    std::string fColorString;
    bool isHightLight;
};

#endif // !_CA_CONSOLE_H_