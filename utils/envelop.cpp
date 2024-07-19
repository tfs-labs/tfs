#include "envelop.h"
#include <openssl/rsa.h>
#include "utils/util2.h"
#include <vector>
#include <openssl/types.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include "utils/hex_code.h"
#include <openssl/rand.h>
#include "utils/tmp_log.h"
#include "./string_util.h"



Envelop::Envelop(/* args */)
{
    const int kBits = 2048;
    rsaPkey = EVP_PKEY_new();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if(EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, kBits) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return;
    }

    if(EVP_PKEY_keygen(ctx, &rsaPkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        return;
    }
    EVP_PKEY_CTX_free(ctx);
}

Envelop::~Envelop()
{
    EVP_PKEY_free(rsaPkey);
}

std::string Envelop::GetPubstr()
{
    std::string pubstr;
    unsigned char* pkeyDer = NULL;
    int publen = i2d_PUBKEY(rsaPkey, &pkeyDer);
    pubstr.append(reinterpret_cast<const char*>(pkeyDer), publen);
    return pubstr;
}



bool Envelop::Encrypt(const std::string &in, std::string &out, std::string &key)
{       
    EVP_CIPHER_CTX *ctx;
    
    ctx = EVP_CIPHER_CTX_new();

    unsigned char aesKey[EVP_MAX_KEY_LENGTH];
    unsigned char aesIv[EVP_MAX_IV_LENGTH];
    RAND_bytes(aesKey, EVP_MAX_KEY_LENGTH);
    RAND_bytes(aesIv, EVP_MAX_IV_LENGTH);

    int ret = EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, aesKey, aesIv);
    if(ret != 1)
    {
        return false;
    }

    int mlen = 0;
    out.resize(in.size() + EVP_MAX_BLOCK_LENGTH);
    ret = EVP_EncryptUpdate(ctx, (unsigned char*)out.data(), &mlen, (const unsigned char*)in.data(), in.size());
    if(ret != 1)
    {
        return false;
    }

    int flen = 0;
    ret = EVP_EncryptFinal_ex(ctx, (unsigned char *)out.data() + mlen, &flen);
    if(ret != 1)
    {
        return false;
    }
    out.resize(mlen + flen);
    
    key.append(reinterpret_cast<const char*>(aesKey), sizeof(aesKey));
    std::string keyHex =  Str2Hex(key); 
    std::string ivec(reinterpret_cast<const char*>(aesIv), sizeof(aesIv));
    std::string ivecHex =  Str2Hex(ivec);

    key =   keyHex + "_" + ivecHex;

    EVP_CIPHER_CTX_free(ctx);


    return true;
}


bool Envelop::Decrypt(const std::string &in, std::string &out, const std::string &key)
{
    EVP_CIPHER_CTX *ctx;
    ctx = EVP_CIPHER_CTX_new();

    std::vector<std::string> keyIv;

	StringUtil::SplitString(key, "_", keyIv);
    if(keyIv.size() != 2)
    {
        return false;
    }

    std::string iv = Hex2Str(keyIv[1]);
    std::string aes_key = Hex2Str(keyIv[0]);

    int ret = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, (const unsigned char*)aes_key.data(), (const unsigned char*)iv.data());
    if(ret != 1)
    {
        return false;
    }

    int mlen = 0;
    out.resize(in.size());
    ret = EVP_DecryptUpdate(ctx, (unsigned char*)out.data(), &mlen, (const unsigned char*)in.data(), in.size());
    if(ret != 1)
    {
        return false;
    }

    int flen = 0;
    ret = EVP_DecryptFinal_ex(ctx, (unsigned char *)out.data() + mlen, &flen);
    if(ret != 1)
    {
        return false;
    }
    out.resize(mlen + flen);

    EVP_CIPHER_CTX_free(ctx);

    return true;
}


int Envelop::EncryptByRSA(const std::string &data, EVP_PKEY * &pkey, std::string & ciphertext)
{
    unsigned char* bufPtr = (unsigned char *)data.data();
    const unsigned char *data_str = bufPtr;
    size_t dataLen = data.size();
    size_t outLen;
    unsigned char* out = nullptr;
    char szErr[1024];

    size_t len = EVP_PKEY_get_size(pkey);
    out = (unsigned char *)OPENSSL_malloc(len);
  
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);

    if(!ctx)
    {
        return -4;
    }

    int ret = EVP_PKEY_encrypt_init(ctx);
    if(ret <= 0)
    {
        return -2;
    }

    int rv = EVP_PKEY_encrypt(ctx ,out, &outLen, data_str, dataLen);
    if (rv <= 0) {
        return -1;
    }

    if(out == nullptr)
    {
        return -3;
    }
    ciphertext.append(reinterpret_cast<const char *>(out), outLen);

    OPENSSL_free(out);
    EVP_PKEY_CTX_free(ctx);
    return 0;
}

int Envelop::DecryptByRSA(const std::string &enc, EVP_PKEY *pkey, std::string & ciphertext)
{

    unsigned char * bufPtr =(unsigned char *)enc.data();
    const unsigned char *data_str = bufPtr;
    size_t inLen = enc.size();
    size_t outLen;
    unsigned char* out = nullptr;

    size_t len = EVP_PKEY_get_size(pkey);
    out = (unsigned char *)OPENSSL_malloc(len);
  
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    EVP_PKEY_decrypt_init(ctx);

    if(EVP_PKEY_decrypt(ctx, out, &outLen, data_str, inLen) <= 0)
    {
        return -1;
    }
    ciphertext.append(reinterpret_cast<const char *>(out), outLen);
    OPENSSL_free(out);
    return 0;
}


int Envelop::EncapsulatEnvelope(const std::string &data, const std::string& pubstr, std::string& strEncTxt, std::string& cipherText, std::string& out)
{
   EVP_PKEY*pkeyDer = NULL;
   GetEDPubKeyByBytes(pubstr, pkeyDer);


    std::string data256 = Getsha256hash(data);
    if(ED25519SignMessage(data256, rsaPkey, out) == false)
    {
        return -1;
    }

    std::string aesKey;
    if(Encrypt(data,strEncTxt,aesKey) == false)
    {
        return -2;
    }
    
    if(EncryptByRSA(aesKey, pkeyDer, cipherText) != 0)
    {
        return -3;
    }

    return 0;
}


int Envelop::OpenEnvelope(const std::string& cipherText, const std::string& out, const std::string& strEncTxt, const std::string& pub_str, std::string& message)
{
    std::string key;
    if(DecryptByRSA(cipherText, rsaPkey, key) != 0)
    {
        return -1;
    }
    
    if(Decrypt(strEncTxt,message,key) == false)
    {
        return -2;
    }
    
    Account account;
	if(MagicSingleton<AccountManager>::GetInstance()->GetAccountPubByBytes(pub_str, account) == false){
		return -3;
	}

	std::string messageHash = Getsha256hash(message);
    std::string signature = out;
	if(account.Verify(messageHash, signature) == false){
		return -4;
	}

    return 0;
}



void TestFunction2()
{
    std::string data = R"()";
    Envelop enve;

    std::string message;
    for(int i = 0; i < 1; ++i)
    {
        std::string data_1 = data + std::to_string(i);

        std::string pubstr,strEncTxt,cipherText,sign_message;

        if(enve.EncapsulatEnvelope(data_1,pubstr,strEncTxt,cipherText,sign_message) != 0)
        {
            errorL("EncapsulatEnvelope fail !");
            return;
        }

       
        if(enve.OpenEnvelope(cipherText, sign_message, strEncTxt, pubstr, message) != 0)
        {
            errorL("OpenEnvelope fail !");
            return;
        }
    }
    infoL("message:%s" , message);

    return;
}




