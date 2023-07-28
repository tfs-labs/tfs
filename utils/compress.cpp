#include "compress.h"
#include <zlib.h>
#include <string.h>
using namespace std;

void Compress::compressFunc()
{   
    uint64_t datalen = (m_raw_data.size() + 12) * 1.001 + 2;
    char * pressdata = new char[datalen]{0};
    int err = compress((Bytef *)pressdata, &datalen, (const Bytef *)m_raw_data.c_str(), m_raw_data.size());
    if (err != Z_OK) {
        cerr << "compress error:" << err << endl;
        return;
    }
    string tmp(pressdata, datalen);
    m_compress_data = tmp;

    delete [] pressdata;
}
void Compress::uncompressFunc()
{   
    char * uncompressData= new char[m_uncompress_len]{0};
    int err = uncompress((Bytef *)uncompressData, &m_uncompress_len,(const Bytef *)m_compress_data.c_str(), m_compress_data.size());
    if (err != Z_OK) {
        cerr << "uncompress error:" << err << endl;
        return;
    }
    string tmp(uncompressData, m_uncompress_len);
    m_raw_data = tmp;

    delete [] uncompressData;
}