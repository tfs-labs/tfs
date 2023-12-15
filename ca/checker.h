/*
 * @Author: HaoXuDong 2848973813@qq.com
 * @Date: 2023-12-06 16:33:00
 * @LastEditors: HaoXuDong 2848973813@qq.com
 * @LastEditTime: 2023-12-06 16:53:29
 * @FilePath: /tfs/ca/checker.h
 */
/**
 * *****************************************************************************
 * @file        checker.h
 * @brief       
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __CA_CHECKER__
#define __CA_CHECKER__

#include <vector>
#include <map>

#include "proto/transaction.pb.h"
#include "proto/block.pb.h"
#include "block_helper.h"
#include "ca/transaction.h"
#include "ca/transaction_entity.h"
#include "ca/transaction_entity_V33_1.h"

namespace Checker 
{
    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       cache: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CTransaction &tx, const std::map<uint64_t ,std::list<CTransaction>> &cache);
    bool CheckConflict_V33_1(const CTransaction &tx, const std::map<uint64_t, std::list<TransactionEntity_V33_1>> &cache);
    /**
     * @brief
     *
     * @param       tx:
     * @param       cache:
     * @return      true
     * @return      false
     */
    bool CheckConflict(const CTransaction &tx, const std::vector<TransactionEntity>  &cache);
    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       blocks: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CTransaction &tx, const std::set<CBlock, compator::BlockTimeAscending> &blocks);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       blocks: 
     * @param       txHashPtr: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CBlock &block, const std::set<CBlock, compator::BlockTimeAscending> &blocks, std::string* txHashPtr = nullptr);
    
    /**
     * @brief       
     * 
     * @param       block1: 
     * @param       block2: 
     * @param       txHashPtr: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CBlock &block1, const CBlock &block2, std::string* txHashPtr = nullptr);

    /**
     * @brief       
     * 
     * @param       tx1: 
     * @param       tx2: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CTransaction &tx1, const CTransaction &tx2);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       doubleSpentTransactions: 
     */
    void CheckConflict(const CBlock &block, std::vector<CTransaction>& doubleSpentTransactions);
};
#endif