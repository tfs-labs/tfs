/*
 * @Author: HaoXuDong 2848973813@qq.com
 * @Date: 2023-12-06 16:49:30
 * @LastEditors: HaoXuDong 2848973813@qq.com
 * @LastEditTime: 2023-12-06 16:49:31
 * @FilePath: /tfs/ca/transaction_entity_V33_1.h
 * @Description: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * *****************************************************************************
 * @file        transaction_entity.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _TransactionEntity_V33_1_H_
#define _TransactionEntity_V33_1_H_

#include <iostream>

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"

class TransactionEntity_V33_1
{
    private:
        CTransaction _transaction;
        TxMsgReq _msg;
        time_t _timestamp;
    
    public:
        TransactionEntity_V33_1(CTransaction transaction, TxMsgReq msg, time_t timestamp)
			 : _transaction(transaction), _msg(msg), _timestamp(timestamp){};
		TransactionEntity_V33_1() = default;
        ~TransactionEntity_V33_1() = default;

		/**
		 * @brief       Get the transaction object
		 * 
		 * @return      CTransaction 
		 */
        CTransaction GetTransaction() const 
		{
			return _transaction;
		}

		/**
		 * @brief       Get the txmsg object
		 * 
		 * @return      TxMsgReq 
		 */
		TxMsgReq GetTxmsg() const
		{
			return _msg;
		}

		/**
		 * @brief       Get the timestamp object
		 * 
		 * @return      time_t 
		 */
		time_t GetTimestamp() const
		{
			return _timestamp;
		}

		/**
		 * @brief       Set the transaction object
		 * 
		 * @param       transaction: 
		 */
		void SetTransaction(const CTransaction& transaction)
		{
			_transaction = transaction;
		}

		/**
		 * @brief       Set the txmsg object
		 * 
		 * @param       msg: 
		 */
		void SetTxmsg(const TxMsgReq& msg) 
		{
			_msg = msg;
		}

		/**
		 * @brief       Set the timestamp object
		 * 
		 * @param       timestamp: 
		 */
		void SetTimestamp(const time_t& timestamp)
		{
			_timestamp = timestamp;
		}
};

#endif
