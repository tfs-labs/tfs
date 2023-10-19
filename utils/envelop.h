/**
 * *****************************************************************************
 * @file        Envelop.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _ENVELOP_
#define _ENVELOP_

#include <openssl/evp.h>
#include <string>
#include "account_manager.h"

class Envelop
{
private:
    EVP_PKEY *rsaPkey;
    
public:
    Envelop(/* args */);
    ~Envelop();
public:


    
std::string GetPubstr();
bool Encrypt(const std::string &in, std::string &out,  std::string &key);
bool Decrypt(const std::string &in, std::string &out, const std::string &key);
int EncryptByRSA(const std::string &data, EVP_PKEY * &pkey, std::string & ciphertext);
int DecryptByRSA(const std::string &enc, EVP_PKEY *pkey, std::string & ciphertext);
int EncapsulatEnvelope(const std::string &data, const std::string& pubstr, std::string& strEncTxt, std::string& cipherText, std::string& out);
int OpenEnvelope(const std::string& cipherText, const std::string& out, const std::string& strEncTxt, const std::string& pubStr, std::string& message);


};
void TestFunction2();




#endif