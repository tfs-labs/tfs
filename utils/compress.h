/**
 * *****************************************************************************
 * @file        compress.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef COMPRESS_H_
#define COMPRESS_H_
#include <iostream>
#include <string>

class Compress
{
public:
    Compress(std::string rawData) : _rawData(rawData){ compressFunc(); }
    Compress(std::string compress_data, uint64_t uncompressLen) 
        : _compressData(compress_data), _uncompressLen(uncompressLen){ uncompressFunc(); }
    ~Compress(){}
    /**
     * @brief
    */
    void compressFunc();
    /**
     * @brief
    */
    void uncompressFunc();

    std::string _rawData;
    std::string _compressData;
    uint64_t _uncompressLen;
};


#endif