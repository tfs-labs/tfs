/**
 * *****************************************************************************
 * @file        ca.h
 * @brief       
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef _RPC_CREATE_TRANSACTION_H_
#define _RPC_CREATE_TRANSACTION_H_

#include "ca/txhelper.h"
#include "rpc_tx.h"

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       ack: 
    * @return      
    */
void ReplaceCreateTxTransaction(const std::vector<std::string>& fromAddr, 
                                const std::map<std::string, int64_t> & toAddr, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       stakeAmount: 
    * @param       pledgeType: 
    * @param       ack: 
    * @return      
    */
void ReplaceCreateStakeTransaction(const std::string & fromAddr, uint64_t stakeAmount,  
                                        int32_t pledgeType, txAck* ack, double commissionRate, bool isFindUtxo, const std::string& encodedInfo);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       utxoHash: 
    * @param       ack: 
    * @return      std::string 
    */
void ReplaceCreatUnstakeTransaction(const std::string& fromAddr, const std::string& utxoHash, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       investAmount: 
    * @param       investType: 
    * @param       ack: 
    * @return     
    */
void ReplaceCreateInvestTransaction(const std::string & fromAddr,
                                const std::string& toAddr,uint64_t investAmount, int32_t investType, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       utxoHash: 
    * @param       ack: 
    * @return      
    */
void ReplaceCreateDisinvestTransaction(const std::string& fromAddr,
                                const std::string& toAddr, const std::string& utxoHash, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       Addr: 
    * @param       ack: 
    * @return      
    */
void ReplaceCreateBonusTransaction(const std::string& Addr, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       outTx: 
    * @param       height: 
    * @param       info: 
    * @param       type: 
    * @return      int 
    */
int SendMessage(CTransaction & outTx,int height,Vrf &info,TxHelper::vrfAgentType type);



#endif
