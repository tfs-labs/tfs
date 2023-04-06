#include "utils/console.h"
#include <stdio.h>
#include <sstream>
#include <iostream>

ca_console::ca_console(const ConsoleColor foregroundColor, const ConsoleColor backgroundColor, bool highlight)
{
    setColor(foregroundColor, backgroundColor, highlight);
}

ca_console::~ca_console()
{
    reset();
}

const std::string ca_console::color()
{
    std::string output = "\033[" + bColorString + ";" + fColorString + "m";

    if (isHightLight)
    {
        output = "\033[1m" + output;
    }
    
    return output;
}

ca_console::operator const char *()
{
    return color().c_str();
}

ca_console::operator char *()
{
    return const_cast<char*>(color().c_str());
}

void ca_console::setColor(const ConsoleColor foregroundColor, const ConsoleColor backgroundColor,  bool highlight)
{
    std::stringstream  s;
    s << (foregroundColor + 30); 
    s >> fColorString;

    std::stringstream ss;
    ss << (backgroundColor + 40);
    ss >> bColorString;

    isHightLight = highlight;
}

const std::string ca_console::reset()
{
    return std::string("\033[0m");
}

void ca_console::clear()
{
    std::cout << "\033[2J";
}