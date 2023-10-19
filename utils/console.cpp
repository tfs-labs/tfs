#include "utils/console.h"
#include <stdio.h>
#include <sstream>
#include <iostream>

CaConsole::CaConsole(const ConsoleColor foregroundColor, const ConsoleColor backgroundColor, bool highlight)
{
    SetColor(foregroundColor, backgroundColor, highlight);
}

CaConsole::~CaConsole()
{
    Reset();
}

const std::string CaConsole::Color()
{
    std::string output = "\033[" + _bColorString + ";" + _fColorString + "m";

    if (_isHightLight)
    {
        output = "\033[1m" + output;
    }
    
    return output;
}

CaConsole::operator const char *()
{
    return Color().c_str();
}

CaConsole::operator char *()
{
    return const_cast<char*>(Color().c_str());
}

void CaConsole::SetColor(const ConsoleColor foregroundColor, const ConsoleColor backgroundColor,  bool highlight)
{
    std::stringstream  s;
    s << (foregroundColor + 30); 
    s >> _fColorString;

    std::stringstream ss;
    ss << (backgroundColor + 40);
    ss >> _bColorString;

    _isHightLight = highlight;
}

const std::string CaConsole::Reset()
{
    return std::string("\033[0m");
}

void CaConsole::Clear()
{
    std::cout << "\033[2J";
}