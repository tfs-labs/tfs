/**
 * *****************************************************************************
 * @file        console.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
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

/**
 * @brief       
 * 
 */
class CaConsole
{
public:
    /**
     * @brief       Construct a new Ca Console object
     * 
     * @param       foregroundColor: 
     * @param       backgroundColor: 
     * @param       highlight: 
     */
    CaConsole(const ConsoleColor foregroundColor = kConsoleColor_White, 
                const ConsoleColor backgroundColor = kConsoleColor_Black, 
                bool highlight = false);
    
    /**
     * @brief       Destroy the Ca Console object
     * 
     */
    ~CaConsole();
    
    /**
     * @brief       Returns the Color
     * 
     * @return      const std::string 
     */
    const std::string Color(); 

    /**
     * @brief       Set the Color object
     * 
     * @param       foregroundColor: 
     * @param       backgroundColor: 
     * @param       highlight: 
     */
    void SetColor(const ConsoleColor foregroundColor = kConsoleColor_White, 
                const ConsoleColor backgroundColor = kConsoleColor_Black, 
                bool highlight = false);
    
    /**
     * @brief       
     * 
     * @return      const std::string 
     */
    const std::string Reset(); //Reset

    /**
     * @brief       
     * 
     */
    void Clear(); //Clear

    operator const char * ();
    operator char * ();

private:
    std::string _bColorString;
    std::string _fColorString;
    bool _isHightLight;
};

#endif // !_CA_CONSOLE_H_