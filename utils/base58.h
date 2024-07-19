#ifndef __CA_BASE58__H
#define __CA_BASE58__H

#include <stdbool.h>
#include <stddef.h>
#include <string>
#include "utils/util2.h"
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif

#define  B58VER 0x00

extern bool (*b58_sha256_impl)(void *, const void *, size_t);

extern bool b58tobin(void *bin, size_t *binsz, const char *b58, size_t b58sz);

extern bool b58enc(char *b58, size_t *b58sz, const void *bin, size_t binsz);
int base58_encode(const char *in, size_t in_len, char *out, size_t *out_len);
int base58_decode(const char *in, size_t in_len, char *out, size_t *out_len);

extern bool b58check_enc(char *b58c, size_t *b58c_sz, uint8_t ver, const void *data, size_t datasz);

bool GetBase58Addr(char *b58c, size_t *b58c_sz, uint8_t ver, const void *data, size_t datasz);


#ifdef __cplusplus
}
#endif


enum class Base58Ver
{
    kBase58Ver_Normal = 0x00,
    kBase58Ver_MultiSign = 0x05,
    kBase58Ver_All = 99,
};
/**
 * @brief
 * 
 * @param       key:
 * @return      string
*/
std::string GetMd160(std::string key);
/**
 * @brief
 * 
 * @param       key:
 * @param       ver:
 * @return      string
*/
std::string GetBase58Addr(std::string key, Base58Ver ver = Base58Ver::kBase58Ver_Normal);

/**
 * @brief
 * 
 * @param       base58Addr:
 * @param       ver:
 * @return      bool
*/
bool CheckBase58Addr(const std::string & base58Addr, Base58Ver ver = Base58Ver::kBase58Ver_All);
#endif
