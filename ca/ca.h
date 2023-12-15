/**
 * *****************************************************************************
 * @file        ca.h
 * @brief       
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef TFS_CA_H
#define TFS_CA_H

#include <iostream>
#include <thread>
#include <shared_mutex>

#include "proto/ca_protomsg.pb.h"
#include "net/msg_queue.h"
#include "transaction.pb.h"

extern bool bStopTx;
extern bool bIsCreateTx;

void RegisterCallback();
void TestCreateTx(const std::vector<std::string> & addrs, const int & sleepTime);

/**
 * @brief       CA initialization
 * 
 * @return      true success
 * @return      false failure
 */
bool CaInit();

/**
 * @brief       CA cleanup function
 */
void CaCleanup();

/**
 * @brief       Related implementation functions used in the main menu
 */
void PrintBasicInfo();

/**
 * @brief       
 */
void HandleTransaction();

/**
 * @brief       
 */
void HandleStake();

/**
 * @brief       
 */
void HandleUnstake();

/**
 * @brief       
 */
void HandleInvest();

/**
 * @brief       
 */
void HandleDisinvest();

/**
 * @brief       
 */
void HandleBonus();

/**
 * @brief       
 */
void HandleAccountManger();

/**
 * @brief       
 */
void HandleSetdefaultAccount();

/**
 * @brief       
 */
void HandleDeployContract();
void HandleDeployContract_V33_1();

/**
 * @brief       
 */
void HandleCallContract();
void HandleCallContract_V33_1();

/**
 * @brief       
 */
void HandleExportPrivateKey();

/**
 * @brief       NTPcheckout
 * 
 * @return      int 
 */
int CheckNtpTime();

/**
 * @brief       Get the Chain Height object
 * 
 * @param       chainHeight: 
 * @return      int 
 */
int GetChainHeight(unsigned int & chainHeight);


/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string RpcCallContract(void * arg,void *ack);

/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string RpcCallContract_V33_1(void * arg,void *ack);

/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string RpcDeployContract(void * arg,void *ack);


/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string RpcDeployContract_V33_1(void * arg,void *ack);

int SigTx(CTransaction &tx,const std::string & addr);

#endif
