#include "rpc_utils.h"

thread_local std::string threadRpcError;
thread_local int error;

/**
 * @brief       Set the Error object
 * 
 * @param       error 
 */
void SetError(const std::string & error){
    threadRpcError=error;
}

/**
 * @brief       Get the Error object
 * 
 * @return      std::string 
 */
std::string GetError(){
    return threadRpcError;
}

/**
 * @brief       Set the No Error object
 * 
 */
void SetNoError(){
    threadRpcError="Ok";
}

/**
 * @brief       Set the Error Num object
 * 
 * @param       v 
 */
void SetErrorNum(int v){
    error=v;
}

/**
 * @brief       Set the No Error Num object
 * 
 */
void SetNoErrorNum(){
    error=0;
}

/**
 * @brief       Get the Error Num object
 * 
 * @return      int 
 */
int GetErrorNum(){
    return error;
}