
#include "RSA_TEXT.h"
#include "utils/tmplog.h"
bool RSAEnCode(const std::string& data, envelop* enve,const std::string& rsa_pubstr, std::string& message)
{
    Base64 base_;
    rsa_code rsa_encode;
    std::string pubstr = enve->getPubstr();
    std::string strEncTxt;
    std::string cipher_text;
    std::string sign_message;

    if (rsa_pubstr.empty()) {
        rsa_encode.isEcode = "0";
        rsa_encode.strEncTxt = base_.Encode((const unsigned char*)data.c_str(), data.size());
        return true;
    }


    rsa_encode.strpub = base_.Encode((const unsigned char*)pubstr.c_str(), pubstr.size());
    int ret = 0;
    ret = enve->EncapsulatEnvelope(data, rsa_pubstr, strEncTxt, cipher_text, sign_message);
    if (ret != 0) {

        errorL("EncapsulatEnvelope error:%s",ret);
        return false;
    }
 
    rsa_encode.sign_message = base_.Encode((const unsigned char*)sign_message.c_str(), sign_message.size());
    rsa_encode.cipher_text = base_.Encode((const unsigned char*)cipher_text.c_str(), cipher_text.size());
    rsa_encode.strEncTxt = base_.Encode((const unsigned char*)strEncTxt.c_str(), strEncTxt.size());
    message = rsa_encode.paseToString();
    return true;
}

bool RSADeCode(const std::string& data, envelop* enve,std::string & rsa_pubstr, std::string& message) {

    rsa_code code;
    if (code.paseFromJson(data) == false) {
        return false;
   }
    Base64 base_;
    int ret = 0;

    if (code.isEcode=="0") {
        message = code.strEncTxt;
        return true;
    }
    
    ret=enve->OpenEnvelope(base_.Decode(code.cipher_text.c_str(), code.cipher_text.size()),
        base_.Decode(code.sign_message.c_str(), code.sign_message.size()),
        base_.Decode(code.strEncTxt.c_str(), code.strEncTxt.size()),
        base_.Decode(code.strpub.c_str(), code.strpub.size()), message);

    if (ret != 0) {
        errorL("OpenEnvelope error:%s", ret);
        return false;
    }

    return true;
}