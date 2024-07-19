#ifndef __CRYPTO_H
#define __CRYPTO_H

#include <cstdint>
#include <cstring>
#include <string>

namespace crypto{

#define CRYPTO_CURVE_NID            NID_X9_62_prime256v1


#define CRYPTO_EC_PUB_KEY_LEN        65
#define CRYPTO_EC_PRIV_KEY_LEN       32
#define CRYPTO_SALT_LEN              32
#define CRYPTO_ECDH_SHARED_KEY_LEN   32
#define CRYPTO_HMAC_SHA256           32
#define CRYPTO_AES_KEY_LEN           32
#define CRYPTO_AES_IV_LEN            12
#define CRYPTO_AES_TAG_LEN           16

#define CRYPTO_ECDSA_SIG_s_LEN       32
#define CRYPTO_ECDSA_SIG_r_LEN       32
#define CRYPTO_ECDSA_SIG_LEN         (CRYPTO_ECDSA_SIG_s_LEN+CRYPTO_ECDSA_SIG_r_LEN)

#define CRYPTO_VERSION               1
#define CRYPTO_KEY_INFO              "ENCRYPTION"
/**
 * @brief
 * 
 * @param       key:
 * @param       bytes:
 * @return      true
 * @return      false
*/
bool rand_salt(uint8_t salt[], int32_t bytes);
/**
 * @brief
 * 
 * @param       ecdh_public_key:
 * @param       ecdh_private_key:
 * @return      true
 * @return      false
*/
bool generate_ecdh_keys(uint8_t ecdh_public_key[CRYPTO_EC_PUB_KEY_LEN],
                       uint8_t ecdh_private_key[CRYPTO_EC_PRIV_KEY_LEN]);
/**
 * @brief
 * 
 * @param       ecdh1_public_key:
 * @param       ecdh1_private_key:
 * @param       ecdh2_public_key:
 * @param       ecdh_shared_key:
 * @return      true
 * @return      false
*/
bool calc_ecdh_shared_key(const uint8_t ecdh1_public_key[CRYPTO_EC_PUB_KEY_LEN],
                        const uint8_t ecdh1_private_key[CRYPTO_EC_PRIV_KEY_LEN],
                        const uint8_t ecdh2_public_key[CRYPTO_EC_PUB_KEY_LEN],
                        uint8_t ecdh_shared_key[CRYPTO_ECDH_SHARED_KEY_LEN]);
/**
 * @brief       calculate the given hash data to a signature
 * 
 * @param       ec_private_key: pointer to a ec private key
 * @param       hash: pointer to a hash data
 * @param       hash_len: length of the hash data
 * @param       sign: output calculated signature
 * @return      true
 * @return      false
*/
bool ecdsa_sign(const uint8_t ec_private_key[CRYPTO_EC_PRIV_KEY_LEN], uint8_t *hash, uint8_t hash_len, uint8_t sign[CRYPTO_ECDSA_SIG_LEN]);

/**
 * @brief
 * 
 * @param       ec_public_key:
 * @param       hash:
 * @param       hash_len:
 * @param       sign:
 * @return      bool
*/
bool ecdsa_verify(const uint8_t ec_public_key[CRYPTO_EC_PUB_KEY_LEN], const uint8_t *hash, int hash_len, const uint8_t sign[CRYPTO_ECDSA_SIG_LEN]);
/**
 * @brief
 * 
 * @param       hmac:
 * @param       key:
 * @param       key_len:
 * @param       data:
 * @param       data_len:
 * @return      true
 * @return      false
*/
bool hmac_sha256(uint8_t hmac[CRYPTO_HMAC_SHA256],
                const uint8_t key[], uint8_t key_len,
                const uint8_t data[], uint8_t data_len);
/**
 * @brief
 * 
 * @param       data1:
 * @param       data1_len:
 * @param       data2:
 * @param       data2_len:
 * @param       out:
 * @return      true
 * @return      false
*/
bool bytes_xor(const uint8_t data1[], int data1_len,
               const uint8_t data2[], int data2_len,
               uint8_t out[]);
/**
 * @brief
 * 
 * @param       ecdh_shared_key:
 * @param       salt:
 * @param       info:
 * @param       info_len:
 * @param       out:
 * @return      true
 * @return      false
*/
bool generate_hkdf_bytes(const uint8_t ecdh_shared_key[CRYPTO_ECDH_SHARED_KEY_LEN],
                        const uint8_t salt[CRYPTO_SALT_LEN],
                        const uint8_t info[], int info_len,
                        uint8_t out[]);
/**
 * @brief
 * 
 * @param       plaintext:
 * @param       plaintext_len:
 * @param       key:
 * @param       ciphertext:
 * @param       tag:
 * @return      true
 * @return      false
*/
bool aes_encrypt(const unsigned char *plaintext, int plaintext_len,
                const unsigned char *key, const unsigned char *iv,
                unsigned char *ciphertext, unsigned char *tag);
/**
 * @brief
 * 
 * @param       ciphertext:
 * @param       ciphertext_len:
 * @param       tag:
 * @param       key:
 * @param       iv:
 * @param       plaintext:
 * @return      true
 * @return      false
*/
bool aes_decrypt(const unsigned char *ciphertext, int ciphertext_len,
                const unsigned char *tag, const unsigned char *key, const unsigned char *iv,
                unsigned char *plaintext);
};

#endif
