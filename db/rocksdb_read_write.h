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

    bool TransactionInit();
    bool TransactionCommit(rocksdb::Status &ret_status);
    bool TransactionRollBack(rocksdb::Status &ret_status);

    bool MultiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &ret_status);
    bool MergeValue(const std::string &key, const std::string &value, rocksdb::Status &ret_status, bool first_or_last = false);
    bool RemoveMergeValue(const std::string &key, const std::string &value, rocksdb::Status &ret_status);
    bool ReadData(const std::string &key, std::string &value, rocksdb::Status &ret_status);
    bool WriteData(const std::string &key, const std::string &value, rocksdb::Status &ret_status);
    bool DeleteData(const std::string &key, rocksdb::Status &ret_status);

private:
    bool ReadForUpdate(const std::string &key, std::string &value, rocksdb::Status &ret_status);

    std::string txn_name_;
    std::shared_ptr<RocksDB> rocksdb_;
    rocksdb::Transaction *txn_;
    rocksdb::WriteOptions write_options_;
    rocksdb::ReadOptions read_options_;
};
#endif
