#ifndef __CA_CHECKER__
#define __CA_CHECKER__

#include <vector>
#include <map>

#include "proto/transaction.pb.h"
#include "proto/block.pb.h"
#include "ca_blockhelper.h"
#include "ca/ca_transaction.h"
#include "ca/ca_transactionentity.h"

namespace Checker 
{
    bool CheckConflict(const CTransaction &tx, const std::map<uint64_t, std::list<TransactionEntity>> &cache);
    bool CheckConflict(const CTransaction &tx, const std::set<CBlock, compator::BlockTimeAscending> &blocks);
    bool CheckConflict(const CBlock &block, const std::set<CBlock, compator::BlockTimeAscending> &blocks, std::string* txHashPtr = nullptr);
    bool CheckConflict(const CBlock &block1, const CBlock &block2, std::string* txHashPtr = nullptr);
    bool CheckConflict(const CTransaction &tx1, const CTransaction &tx2);
    void CheckConflict(const CBlock &block, std::vector<CTransaction>& DoubleSpentTransactions);
};
#endif