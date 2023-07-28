#ifndef _RSA_TEXT_H_
#define _RSA_TEXT_H_
#include "utils/Envelop.h"
#include "tx.h"
#include "base64.h"

bool RSAEnCode(const std::string& data, envelop* enve, const std::string& rsa_pubstr, std::string& message);
bool RSADeCode(const std::string& data, envelop* enve,std::string & rsa_pubstr, std::string& message);
#endif