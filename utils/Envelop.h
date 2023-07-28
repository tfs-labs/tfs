#ifndef _ENVELOP_
#define _ENVELOP_

#include <openssl/evp.h>
#include <string>
#include "AccountManager.h"

class envelop
{
private:
    EVP_PKEY *rsaPkey;
    
public:
    envelop(/* args */);
    ~envelop();
public:


    
std::string getPubstr();
bool Encrypt(const std::string &in, std::string &out,  std::string &key);
bool Decrypt(const std::string &in, std::string &out, const std::string &key);
int EncryptByRSA(const std::string &data, EVP_PKEY * &pkey, std::string & ciphertext);
int DecryptByRSA(const std::string &enc, EVP_PKEY *pkey, std::string & ciphertext);
int EncapsulatEnvelope(const std::string &data, const std::string& pubstr, std::string& strEncTxt, std::string& cipher_text, std::string& out);
int OpenEnvelope(const std::string& cipher_text, const std::string& out, const std::string& strEncTxt, const std::string& pub_str, std::string& message);


};
void TestFunction2();




#endif