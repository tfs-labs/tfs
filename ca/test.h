/**
 * *****************************************************************************
 * @file        test.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef __CA_TEST_H__
#define __CA_TEST_H__
#include <string>

#include "utils/json.hpp"
#include "proto/block.pb.h"
#include "proto/transaction.pb.h"

/**
 * @brief       
 * 
 * @param       start: 
 * @param       end: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int PrintRocksdb(uint64_t start, uint64_t end, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       block: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int PrintBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int PrintTx(const CTransaction & tx, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       num: 
 * @param       preHashFlag: 
 * @return      std::string 
 */
std::string PrintBlocks(int num = 0, bool preHashFlag = false);

/**
 * @brief       
 * 
 * @param       num: 
 * @param       preHashFlag: 
 * @return      std::string 
 */
std::string PrintBlocksHash(int num = 0, bool preHashFlag = false);

/**
 * @brief       
 * 
 * @param       startNum: 
 * @param       num: 
 * @param       preHashFlag: 
 * @return      std::string 
 */
std::string PrintRangeBlocks(int startNum = 0,int num = 0, bool preHashFlag = false);

/**
 * @brief       
 * 
 * @param       strHeader: 
 * @param       blocks: 
 */

std::string TxInvet(const CTransaction& tx);
void BlockInvert(const std::string& strHeader, nlohmann::json& blocks);
nlohmann::json FixTxField(const nlohmann::json& inTx);

/**
 * @brief       
 * 
 * @param       where: 
 * @return      std::string 
 */

int PrintContractBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream);
std::string PrintContractBlocks(int num, bool preHashFlag);
std::string PrintRangeContractBlocks(int startNum,int num, bool preHashFlag);
std::string PrintCache(int where);
#endif
