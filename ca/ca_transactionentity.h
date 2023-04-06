#ifndef _TransactionEntity_H_
#define _TransactionEntity_H_

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"
#include <iostream>

class TransactionEntity
{
    private:
        CTransaction transaction_;
        TxMsgReq msg_;
        time_t timestamp_;
    
    public:
        TransactionEntity(CTransaction transaction, TxMsgReq msg, time_t timestamp)
			 : transaction_(transaction), msg_(msg), timestamp_(timestamp){};
		TransactionEntity() = default;
        ~TransactionEntity() = default;

        CTransaction get_transaction() const 
		{
			return transaction_;
		}

		TxMsgReq get_txmsg() const
		{
			return msg_;
		}

		time_t get_timestamp() const
		{
			return timestamp_;
		}

		void set_transaction(const CTransaction& transaction)
		{
			transaction_ = transaction;
		}

		void set_txmsg(const TxMsgReq& msg) 
		{
			msg_ = msg;
		}

		void set_timestamp(const time_t& timestamp)
		{
			timestamp_ = timestamp;
		}
};

#endif
