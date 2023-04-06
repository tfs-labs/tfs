#ifndef COMPRESS_H_
#define COMPRESS_H_
#include <iostream>
#include <string>

class Compress
{
public:
    Compress(std::string raw_data) : m_raw_data(raw_data){ compressFunc(); }
    Compress(std::string compress_data, uint64_t uncompress_len) 
        : m_compress_data(compress_data), m_uncompress_len(uncompress_len){ uncompressFunc(); }
    ~Compress(){}

    void compressFunc();
    void uncompressFunc();

    std::string m_raw_data;
    std::string m_compress_data;
    uint64_t m_uncompress_len;
};


#endif