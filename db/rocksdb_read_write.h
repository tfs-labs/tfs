#ifndef TFS_DB_ROCKSDBREADWRITE_H_
#define TFS_DB_ROCKSDBREADWRITE_H_

#include "db/rocksdb.h"
#include <memory>
#include <string>
#include <vector>

class RocksDBReadWriter
{
public:
    RocksDBReadWriter(std::shared_ptr<RocksDB> db, const std::string &txn_name);
    ~RocksDBReadWriter();
    RocksDBReadWriter(RocksDBReadWriter &&) = delete;
    RocksDBReadWriter(const RocksDBReadWriter &) = delete;
    RocksDBReadWriter &operator=(RocksDBReadWriter &&) = delete;
    RocksDBReadWriter &operator=(const RocksDBReadWriter &) = delete;
    /**
     * @brief
     * 
     * @return      true
     * @return      false
     */
    bool TransactionInit();
    /**
     * @brief
     * 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool TransactionCommit(rocksdb::Status &retStatus);
    /**
     * @brief
     * 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool TransactionRollBack(rocksdb::Status &retStatus);
    /**
     * @brief
     * 
     * @param       keys:
     * @param       values: 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool MultiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus);
    /**
     * @brief
     * 
     * @param       key:
     * @param       value: 
     * @param       retStatus:
     * @param       firstOrLast:
     * @return      true
     * @return      false
     */
    bool MergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus, bool firstOrLast = false);
    /**
     * @brief
     * 
     * @param       key:
     * @param       value: 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool RemoveMergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus);
    /**
     * @brief
     * 
     * @param       key:
     * @param       value: 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool ReadData(const std::string &key, std::string &value, rocksdb::Status &retStatus);
    /**
     * @brief
     * 
     * @param       key:
     * @param       value: 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool WriteData(const std::string &key, const std::string &value, rocksdb::Status &retStatus);
    /**
     * @brief
     * 
     * @param       key:
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool DeleteData(const std::string &key, rocksdb::Status &retStatus);

private:
    /**
     * @brief
     * 
     * @param       key:
     * @param       value:
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool ReadForUpdate(const std::string &key, std::string &value, rocksdb::Status &retStatus);

    std::string txn_name_;
    std::shared_ptr<RocksDB> rocksdb_;
    rocksdb::Transaction *txn_;
    rocksdb::WriteOptions write_options_;
    rocksdb::ReadOptions read_options_;
};
#endif
