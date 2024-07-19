#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <stdio.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>

#define  OPENSSL_1_1_0 0  // Support HKDF after openssl 1.1.0
#if OPENSSL_1_1_0
#include <openssl/kdf.h>
#endif

#include "crypto.h"
#include "../include/logging.h"

bool crypto::rand_salt(uint8_t salt[], int32_t bytes)
{
    return (RAND_bytes(salt, bytes)==1);
}

bool crypto::generate_ecdh_keys(uint8_t ecdh_public_key[CRYPTO_EC_PUB_KEY_LEN],
                              uint8_t ecdh_private_key[CRYPTO_EC_PRIV_KEY_LEN])
{
    int len = 0;
    bool ret = false;

    EC_KEY *ecdh = EC_KEY_new();
    const EC_POINT *point = NULL;
    const EC_GROUP *group = NULL;

    //Generate Public
    ecdh = EC_KEY_new_by_curve_name(CRYPTO_CURVE_NID);

    group = EC_KEY_get0_group(ecdh);

    /* get x y */
    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field)
    {
        if (!EC_KEY_generate_key(ecdh))
        {
            ERRORLOG("Ecdh NIST P-256 generate error.");
            goto err;
        }

        point = EC_KEY_get0_public_key(ecdh);

        len = EC_POINT_point2oct(group,
                                 point,
                                 POINT_CONVERSION_UNCOMPRESSED,
                                 ecdh_public_key,
                                 CRYPTO_EC_PUB_KEY_LEN, NULL);
        if (len != CRYPTO_EC_PUB_KEY_LEN)
        {
            ERRORLOG("Ecdh NIST P-256 public key get error.");
            goto err;
        }

        len = BN_bn2bin(EC_KEY_get0_private_key(ecdh), ecdh_private_key);
        if (len != CRYPTO_EC_PRIV_KEY_LEN)
        {
            ERRORLOG("Ecdh NIST P-256 private key get error.");
            goto err;
        }

        ret = true;
    }

err:
    EC_KEY_free(ecdh);
    return ret;
}

bool crypto::calc_ecdh_shared_key(const uint8_t ecdh1_public_key[CRYPTO_EC_PUB_KEY_LEN],
                                 const uint8_t ecdh1_private_key[CRYPTO_EC_PRIV_KEY_LEN],
                                 const uint8_t ecdh2_public_key[CRYPTO_EC_PUB_KEY_LEN],
                                 uint8_t ecdh_shared_key[CRYPTO_ECDH_SHARED_KEY_LEN])
{
    int len = 0;
    int ret = false;
    EC_KEY *ecdh = EC_KEY_new();
    const EC_GROUP *group = NULL;
    BIGNUM   *priv = NULL;
    EC_POINT *p_ecdh1_public = NULL;
    EC_POINT *p_ecdh2_public = NULL;

    ecdh = EC_KEY_new_by_curve_name(CRYPTO_CURVE_NID);
    if (ecdh == NULL)
    {
        ERRORLOG("Ecdh key by curve name error.");
        goto err;
    }

    group = EC_KEY_get0_group(ecdh);

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field)
    {
        /* 1==> Set ecdh1's public and privat key. */
        p_ecdh1_public = EC_POINT_new(group);
        if (p_ecdh1_public == NULL)
        {
            ERRORLOG("EC_POINT new error.");
            goto err;
        }

        ret = EC_POINT_oct2point(group,
                                 p_ecdh1_public,
                                 ecdh1_public_key,
                                 CRYPTO_EC_PUB_KEY_LEN, NULL);
        if (!ret)
        {
            ERRORLOG("EC_POINT oct2point error.");
            goto err;
        }

        if (!EC_KEY_set_public_key(ecdh, p_ecdh1_public))
        {
            ERRORLOG("Ecdh set public key error.");
        }

        priv = BN_bin2bn(ecdh1_private_key,
                            CRYPTO_EC_PRIV_KEY_LEN,
                            NULL);
        if (!EC_KEY_set_private_key(ecdh, priv))
        {
            printf("set private error \n");
        }
        /*-------------*/

        /* 2==> Set ecdh2's public key */
        p_ecdh2_public = EC_POINT_new(group);
        if (p_ecdh2_public == NULL)
        {
            ERRORLOG("EC_POINT new error.");
            goto err;
        }

        ret = EC_POINT_oct2point(group,
                                 p_ecdh2_public,
                                 ecdh2_public_key,
                                 CRYPTO_EC_PUB_KEY_LEN,
                                 NULL);
        if (!ret)
        {
            ERRORLOG("EC_POINT oct2point error.");
            goto err;
        }

        if (!EC_KEY_set_public_key(ecdh, p_ecdh2_public))
        {
            ERRORLOG("Ecdh set public key error.");
            goto err;
        }
        /*------------*/

        /* 3==> Calculate the shared key of ecdh1 and ecdh2 */
        len = ECDH_compute_key(ecdh_shared_key,
                               CRYPTO_ECDH_SHARED_KEY_LEN,
                               p_ecdh2_public,
                               ecdh,
                               NULL);
        if (len != CRYPTO_ECDH_SHARED_KEY_LEN)
        {
            ERRORLOG("Ecdh compute key error.");
            goto err;
        }

        ret = 0;
    }

err:
    if (priv)
        BN_free(priv);
    if (ecdh)
        EC_KEY_free(ecdh);
    if (p_ecdh1_public)
        EC_POINT_free(p_ecdh1_public);
    if (p_ecdh2_public)
        EC_POINT_free(p_ecdh2_public);

    return (ret==0);
}

bool crypto::ecdsa_sign(const uint8_t ec_private_key[CRYPTO_EC_PRIV_KEY_LEN], uint8_t *hash, uint8_t hash_len, uint8_t sign[CRYPTO_ECDSA_SIG_LEN])
{
    int len = 0;
    int ret = false;
    EC_KEY *eckey = EC_KEY_new();
    BIGNUM   *priv = NULL;
    const EC_GROUP *group = NULL;
    EC_POINT *p_ec_point = NULL;

    eckey = EC_KEY_new_by_curve_name(CRYPTO_CURVE_NID);
    if (eckey == NULL)
    {
        ERRORLOG("Ecdh key by curve name error.");
        goto err;
    }

    group = EC_KEY_get0_group(eckey);

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field)
    {
        priv = BN_bin2bn(ec_private_key,
                            CRYPTO_EC_PRIV_KEY_LEN,
                            NULL);
        if (!EC_KEY_set_private_key(eckey, priv))
        {
            printf("set private error \n");
        }

        ECDSA_SIG *signature = ECDSA_do_sign(hash, hash_len, eckey);
        if (NULL == signature)
        {
            ERRORLOG("ECDSA_do_sign error.");
            goto err;
        }

        ret = 0;

        const BIGNUM *r;
        const BIGNUM *s;
        ECDSA_SIG_get0(signature, &r, &s);
        BN_bn2binpad(r, sign, CRYPTO_ECDSA_SIG_LEN / 2);
        BN_bn2binpad(s, sign + CRYPTO_ECDSA_SIG_LEN / 2, CRYPTO_ECDSA_SIG_LEN / 2);

        ECDSA_SIG_free(signature);
    }
err:
    if (priv)
        BN_free(priv);
    if (eckey)
        EC_KEY_free(eckey);

    return (ret == 0);
}

bool crypto::ecdsa_verify(const uint8_t ec_public_key[CRYPTO_EC_PUB_KEY_LEN], const uint8_t *hash, int hash_len, const uint8_t sign[CRYPTO_ECDSA_SIG_LEN])
{
    int len = 0;
    int ret = false;
    EC_KEY *eckey = EC_KEY_new();
    const EC_GROUP *group = NULL;
    EC_POINT *p_ec_point = NULL;

    eckey = EC_KEY_new_by_curve_name(CRYPTO_CURVE_NID);
    if (eckey == NULL)
    {
        ERRORLOG("Ecdh key by curve name error.");
        goto err;
    }

    group = EC_KEY_get0_group(eckey);

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field)
    {
        /* 1==> Set ecdh1's public and privat key. */
        p_ec_point = EC_POINT_new(group);
        if (p_ec_point == NULL)
        {
            ERRORLOG("EC_POINT new error.");
            goto err;
        }

        ret = EC_POINT_oct2point(group,
                                 p_ec_point,
                                 ec_public_key,
                                 CRYPTO_EC_PUB_KEY_LEN, NULL);
        if (!ret)
        {
            ERRORLOG("EC_POINT oct2point error.");
            goto err;
        }

        if (!EC_KEY_set_public_key(eckey, p_ec_point))
        {
            ERRORLOG("Ecdh set public key error.");
        }

        ECDSA_SIG * signature = ECDSA_SIG_new();
        if (signature == NULL)
        {
            ERRORLOG("ECDSA_SIG_new error.");
            goto err;
        }


        BIGNUM *r = BN_bin2bn(sign, CRYPTO_ECDSA_SIG_LEN / 2, NULL);
        BIGNUM *s = BN_bin2bn(sign + CRYPTO_ECDSA_SIG_LEN / 2, CRYPTO_ECDSA_SIG_LEN / 2, NULL);

        if (r == NULL || s == NULL) 
        {
            ERRORLOG("r s new error.");
            goto err;
        }
        ECDSA_SIG_set0(signature, r, s);
        
        int verifyret = ECDSA_do_verify(hash, hash_len, signature, eckey);

        if (verifyret == 1) ret = 0;

        if (signature) ECDSA_SIG_free(signature);
    }

err:
    if (eckey)
        EC_KEY_free(eckey);
    if (p_ec_point)
        EC_POINT_free(p_ec_point);

    return (ret == 0);
}

bool crypto::hmac_sha256(uint8_t hmac[CRYPTO_HMAC_SHA256],
                       const uint8_t key[], uint8_t key_len,
                       const uint8_t data[], uint8_t data_len)
{
    unsigned int resultlen = 0;
    HMAC(EVP_sha256(), key, key_len, data, data_len, hmac, &resultlen);

    if (resultlen != CRYPTO_HMAC_SHA256)
    {
        ERRORLOG("HMAC SHA-256 error.");
        return false;
    }

    return true;
}

bool crypto::bytes_xor(const uint8_t data1[], int data1_len,
               const uint8_t data2[], int data2_len,
               uint8_t out[])
{
    int i = 0;

    if ((data1_len != data2_len) || (out == NULL))
        return false;

    for (i = 0; i < data1_len; i ++)
    {
        out[i] = data1[i] ^ data2[i];
    }

    return true;
}

bool crypto::generate_hkdf_bytes(const uint8_t ecdh_shared_key[CRYPTO_ECDH_SHARED_KEY_LEN],
                               const uint8_t salt[CRYPTO_SALT_LEN],
                               const uint8_t info[], int info_len,
                               uint8_t out[])
{
#if OPENSSL_1_1_0
    int ret = -1;
    size_t out_len = CRYPTO_HMAC_SHA256;
    EVP_PKEY_CTX *pctx;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (pctx == NULL)
        goto err;

    if (EVP_PKEY_derive_init(pctx) <= 0)
        goto err;

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0)
        goto err;

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, CRYPTO_SALT_LEN) <= 0)
        goto err;

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, ecdh_shared_key, CRYPTO_ECDH_SHARED_KEY_LEN) <= 0)
        goto err;

    if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) <= 0)
        goto err;

    if (EVP_PKEY_derive(pctx, out, &out_len) <= 0)
        goto err;
    else
        ret = out_len;
err:
    if (pctx)
        EVP_PKEY_CTX_free(pctx);

    return (ret==0);
#else
    const EVP_MD *md = EVP_sha256();
    unsigned char prk[CRYPTO_ECDH_SHARED_KEY_LEN], T[CRYPTO_ECDH_SHARED_KEY_LEN]={0}, tmp[CRYPTO_AES_KEY_LEN];
    uint32_t outlen = CRYPTO_ECDH_SHARED_KEY_LEN;
    int i, ret, tmplen;
    unsigned char *p;
    /*extract is a simple HMAC...
      Note:salt should be treated as hmac key and ikm should be treated as data
     */
    if (!HMAC(md, salt, CRYPTO_SALT_LEN, ecdh_shared_key, CRYPTO_ECDH_SHARED_KEY_LEN, prk, &outlen))
        return false;

    /*
    * printf("HKDF extract prk:%d\n",outlen);
    * for(i=0;i<outlen;i++)
    *     printf("%02X", prk[i]);
    * printf("\n");
    */

    /*do expand*/

    /*calc the round times*/
    ret = CRYPTO_AES_KEY_LEN/CRYPTO_ECDH_SHARED_KEY_LEN + !!(CRYPTO_AES_KEY_LEN%CRYPTO_ECDH_SHARED_KEY_LEN);

    tmplen = outlen;
    for (i = 0; i < ret; i++)
    {
        p = tmp;

        /*T(0) = empty string (zero length)*/
        if (i != 0)
        {
            memcpy(p, T, CRYPTO_ECDH_SHARED_KEY_LEN);
            p += CRYPTO_ECDH_SHARED_KEY_LEN;
        }

        memcpy(p, info, info_len);
        p += info_len;
        *p++ = i + 1;

        HMAC(md, prk, CRYPTO_ECDH_SHARED_KEY_LEN, tmp, (int)(p - tmp), T, &outlen);
        memcpy(out + i*CRYPTO_ECDH_SHARED_KEY_LEN, T, tmplen < CRYPTO_ECDH_SHARED_KEY_LEN ? tmplen:CRYPTO_ECDH_SHARED_KEY_LEN);
        tmplen -= CRYPTO_ECDH_SHARED_KEY_LEN;
    }

    /*
    *printf("HKDF expand okm:%d\n",CRYPTO_AES_KEY_LEN);
    *for(i=0;i<CRYPTO_AES_KEY_LEN;i++)
    *    printf("%02X", okm[i]);
    *printf("\n");
    */

    return true;
#endif
}

bool crypto::aes_encrypt(const unsigned char *plaintext, int plaintext_len,
            const unsigned char *key, const unsigned char *iv,
            unsigned char *ciphertext, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    int ret = -1;


    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) goto err;

    /* Initialise the encryption operation. */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        goto err;

    /* Initialise key and IV */
    if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv))  goto err;

    /* Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        goto err;
    ciphertext_len = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext, &len))  goto err;

    /* Get the tag */
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag))
         goto err;

err:
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return (ciphertext_len==plaintext_len);
}

bool crypto::aes_decrypt(const unsigned char *ciphertext, int ciphertext_len,
            const unsigned char *tag, const unsigned char *key, const unsigned char *iv,
            unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret = -1;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) goto err;

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        goto err;

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) goto err;

    /* Provide any AAD data. This can be called zero or more times as
     * required
     */

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        goto err;
    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag))
        goto err;

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx, plaintext, &len);
err:
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    if(ret > 0)
    {
        /* Success */
        return (plaintext_len==ciphertext_len);
    }
    else
    {
        /* Verify failed */
        return false;
    }
}
