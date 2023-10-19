#include "compress.h"
#include <zlib.h>
#include <string.h>
#include "include/logging.h"	
using namespace std;

void Compress::compressFunc()
{   
    uint64_t datalen = (_rawData.size() + 12) * 1.001 + 2;
    char * pressdata = new char[datalen]{0};
    int err = compress((Bytef *)pressdata, &datalen, (const Bytef *)_rawData.c_str(), _rawData.size());
    if (err != Z_OK) {
        //cerr << "compress error:" << err << endl;
        ERRORLOG("compress error: ", err);
        return;
    }
    string tmp(pressdata, datalen);
    _compressData = tmp;

    delete [] pressdata;
}
void Compress::uncompressFunc()
{   
    char * uncompressData= new char[_uncompressLen]{0};
    int err = uncompress((Bytef *)uncompressData, &_uncompressLen,(const Bytef *)_compressData.c_str(), _compressData.size());
    if (err != Z_OK) {
        //cerr << "uncompress error:" << err << endl;
        ERRORLOG("compress error: ", err);
        return;
    }
    string tmp(uncompressData, _uncompressLen);
    _rawData = tmp;

    delete [] uncompressData;
}