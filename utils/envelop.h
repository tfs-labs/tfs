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


/**
 * @brief
 * 
 * @return      string
*/
std::string GetPubstr();
/**
 * @brief
 * 
 * @param       input:
 * @param       out:
 * @param       key:
 * @return      true
 * @return      false
*/
bool Encrypt(const std::string &input, std::string &out,  std::string &key);
/**
 * @brief
 * 
 * @param       input:
 * @param       out:
 * @param       key:
 * @return      true
 * @return      false
*/
bool Decrypt(const std::string &input, std::string &out, const std::string &key);
/**
 * @brief
 * 
 * @param       data:
 * @param       pkey:
 * @param       ciphertext:
 * @return      int
*/
int EncryptByRSA(const std::string &data, EVP_PKEY * &pkey, std::string & ciphertext);
/**
 * @brief
 * 
 * @param       enc:
 * @param       pkey:
 * @param       ciphertext:
 * @return      int
*/
int DecryptByRSA(const std::string &enc, EVP_PKEY *pkey, std::string & ciphertext);
/**
 * @brief
 * 
 * @param       data:
 * @param       pubstr:
 * @param       strEncTxt:
 * @param       cipherText:
 * @param       out:
 * @return      int
*/
int EncapsulatEnvelope(const std::string &data, const std::string& pubstr, std::string& strEncTxt, std::string& cipherText, std::string& out);
/**
 * @brief
 * 
 * @param       cipherText:
 * @param       out:
 * @param       strEncTxt:
 * @param       pubStr:
 * @param       message:
 * @return      int
*/
int OpenEnvelope(const std::string& cipherText, const std::string& out, const std::string& strEncTxt, const std::string& pubStr, std::string& message);


};
/**
 * @brief
*/ 
void TestFunction2();




#endif