#include "../include/logging.h"
#include <string.h>
#include <stdbool.h>
#include <string>
#include "utils/hexcode.h"
#include "ca_test.h"

static void init_hexdigit_val(unsigned char *hexdigit_val)
{
    hexdigit_val['0'] = 0;
    hexdigit_val['1'] = 1;
    hexdigit_val['2'] = 2;
    hexdigit_val['3'] = 3;
    hexdigit_val['4'] = 4;
    hexdigit_val['5'] = 5;
    hexdigit_val['6'] = 6;
    hexdigit_val['7'] = 7;
    hexdigit_val['8'] = 8;
    hexdigit_val['9'] = 9;
    hexdigit_val['a'] = 0xa;
    hexdigit_val['b'] = 0xb;
    hexdigit_val['c'] = 0xc;
    hexdigit_val['d'] = 0xd;
    hexdigit_val['e'] = 0xe;
    hexdigit_val['f'] = 0xf;
    hexdigit_val['A'] = 0xa;
    hexdigit_val['B'] = 0xb;
    hexdigit_val['C'] = 0xc;
    hexdigit_val['D'] = 0xd;
    hexdigit_val['E'] = 0xe;
    hexdigit_val['F'] = 0xf;
}

bool decode_hex(void *p, size_t max_len, const char *hexstr, size_t *out_len_)
{
    if (!p || !hexstr)
        return false;
    if (!strncmp(hexstr, "0x", 2))
        hexstr += 2;
    if (strlen(hexstr) > (max_len * 2))
    {
        return false;
    }
	unsigned char hexdigit_val[256] = {0};
    init_hexdigit_val(hexdigit_val);

    unsigned char *buf = (unsigned char *)p;
    size_t out_len = 0;

    while (*hexstr) {
        unsigned char c1 = (unsigned char) hexstr[0];
        unsigned char c2 = (unsigned char) hexstr[1];

        unsigned char v1 = hexdigit_val[c1];
        unsigned char v2 = hexdigit_val[c2];

        if (!v1 && (c1 != '0'))
            return false;
        if (!v2 && (c2 != '0'))
            return false;

        *buf = (v1 << 4) | v2;

        out_len++;
        buf++;
        hexstr += 2;
    }

    if (out_len_)
        *out_len_ = out_len;
    return true;
}

static const char hexdigit[] = "0123456789abcdef";

void encode_hex(char *hexstr, const void *p_, size_t len)
{
    const unsigned char *p = (const unsigned char *)p_;
    unsigned int i;

    for (i = 0; i < len; i++) {
        unsigned char v, n1, n2;

        v = p[i];
        n1 = v >> 4;
        n2 = v & 0xf;

        *hexstr++ = hexdigit[n1];
        *hexstr++ = hexdigit[n2];
    }

    *hexstr = 0;
}

void hex_print(const unsigned char *hexstr, const int len)
{
    unsigned int size = len * 2 + 2;
    char * ps = (char *) malloc(sizeof(char) * size);
    memset(ps, 0, size);
    encode_hex(ps, hexstr, len);
    printf("%s\n", ps);
    free(ps);
}


std::string Str2Hex(const std::string & rawStr)
{
    int size = rawStr.size();
    if(size == 0)
    {
        return std::string();
    }    
    char * p = new char[size * 2 + 2]{0};
    encode_hex(p, rawStr.c_str(), size);
    std::string retStr(p, size * 2);
    delete [] p;

    return retStr;
}

std::string Hex2Str(const std::string & hexStr)
{
    size_t size  = hexStr.size();
    if(size == 0)
    {
        return std::string();
    }
    char * p = new char[size / 2 + 2]{0};
    decode_hex(p, size / 2 + 2, hexStr.c_str(), &size);
    std::string retStr(p, size);
    delete [] p;
    return retStr;
}