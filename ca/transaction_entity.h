/**
 * *****************************************************************************
 * @file        transaction_entity.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _TransactionEntity_H_
#define _TransactionEntity_H_

#include <iostream>

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"

class TransactionEntity
{
    private:
        CTransaction _transaction;
        uint64_t _height;
        bool _executedBefore;
    
    public:
        TransactionEntity(CTransaction transaction, uint64_t height, bool executedBefore)
			 : _transaction(transaction), _height(height), _executedBefore(executedBefore){};
		TransactionEntity() = default;
        ~TransactionEntity() = default;

		/**
		 * @brief       Get the transaction object
		 * 
		 * @return      CTransaction 
		 */
        inline CTransaction GetTransaction() const
		{
			return _transaction;
		}

		/**
		 * @brief       Get the txmsg object
		 * 
		 * @return      TxMsgReq 
		 */
		inline uint64_t GetHeight() const
		{
			return _height;
		}

		/**
		 * @brief       Get the timestamp object
		 * 
		 * @return      time_t 
		 */
		inline bool GetExecutedBefore() const
		{
			return _executedBefore;
		}

		/**
		 * @brief       Set the transaction object
		 * 
		 * @param       transaction: 
		 */
		inline void SetTransaction(const CTransaction& transaction)
		{
			_transaction = transaction;
		}

		/**
		 * @brief       Set the txmsg object
		 * 
		 * @param       msg: 
		 */
        inline void SetHeight(uint64_t height)
		{
            _height = height;
		}

		/**
		 * @brief       Set the timestamp object
		 * 
		 * @param       timestamp: 
		 */
        inline void SetExecutedBefore(bool executedBefore)
		{
            _executedBefore = executedBefore;
		}
};

#endif
