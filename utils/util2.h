#ifndef __CA_UTIL_H__
#define __CA_UTIL_H__
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "utils/ripemd160.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
extern "C" {
#endif

void bu_Hash4(unsigned char *md32, const void *data, size_t data_len);
void bu_Hash(unsigned char *md256, const void *data, size_t data_len);
void bu_Hash160(unsigned char *md160, const void *data, size_t data_len);

#ifdef __cplusplus
}
#endif


#endif
