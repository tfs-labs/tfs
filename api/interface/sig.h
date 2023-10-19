/**
 * *****************************************************************************
 * @file        sig.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __SIG_RPC_
#define __SIG_RPC_
#include <string>
#include "proto/transaction.pb.h"

/**
 * @brief       
 * 
 * @param       addr 
 * @param       message 
 * @param       signature 
 * @param       pub 
 * @return      true 
 * @return      false 
 */
bool JsonrpcGetSigvalue(const std::string& addr, const std::string& message, std::string & signature, std::string& pub);

/**
 * @brief       
 * 
 * @param       addr 
 * @param       tx 
 * @return      int 
 */
int AddMutilSignRpc(const std::string & addr, CTransaction &tx);
#endif