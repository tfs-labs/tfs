/**
 * *****************************************************************************
 * @file        rsa_text.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _RSA_TEXT_H_
#define _RSA_TEXT_H_
#include "utils/envelop.h"
#include "tx.h"
#include "base64.h"

/**
 * @brief       
 * 
 * @param       data 
 * @param       enve 
 * @param       rsa_pubstr 
 * @param       message 
 * @return      true 
 * @return      false 
 */
bool RSAEnCode(const std::string& data, Envelop* enve, const std::string& rsa_pubstr, std::string& message);

/**
 * @brief       
 * 
 * @param       data 
 * @param       enve 
 * @param       rsa_pubstr 
 * @param       message 
 * @return      true 
 * @return      false 
 */
bool RSADeCode(const std::string& data, Envelop* enve,std::string & rsa_pubstr, std::string& message);
#endif