#ifndef __CA_TEST_H__
#define __CA_TEST_H__
#include <string>


#include "proto/block.pb.h"
#include "utils/json.hpp"
#include "proto/transaction.pb.h"

int printRocksdb(uint64_t start, uint64_t end, bool isConsoleOutput, std::ostream & stream);
int printBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream);
int printTx(const CTransaction & tx, bool isConsoleOutput, std::ostream & stream);
std::string printBlocks(int num = 0, bool pre_hash_flag = false);
std::string printBlocksHash(int num = 0, bool pre_hash_flag = false);
std::string printRangeBlocks(int startNum = 0,int num = 0, bool pre_hash_flag = false);
void BlockInvert(const std::string & strHeader, nlohmann::json &blocks);
std::string PrintCache(int where);
#endif
