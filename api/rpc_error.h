/**
 * *****************************************************************************
 * @file        rpc_error.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _RPC_ERROR_H
#define _RPC_ERROR_H
// for error massages
#include <string>
#include <tuple>

/**
 * @brief       Set the Rpc Error object
 * 
 * @param       errorCode 
 * @param       errorMessage 
 */
void SetRpcError(const std::string & errorCode,const std::string & errorMessage);

/**
 * @brief       Get the Rpc Error object
 * 
 * @return      std::pair<std::string,std::string> 
 */
std::pair<std::string,std::string> GetRpcError();

/**
 * @brief       
 * 
 */
void RpcErrorClear();

#endif