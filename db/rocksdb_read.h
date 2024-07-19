#ifndef TFS_DB_ROCKSDBREAD_H_
#define TFS_DB_ROCKSDBREAD_H_

#include "db/rocksdb.h"
#include <memory>
#include <string>
#include <vector>

class RocksDBReader
{
public:
    RocksDBReader(std::shared_ptr<RocksDB> rocksdb);
    ~RocksDBReader() = default;
    RocksDBReader(RocksDBReader &&) = delete;
    RocksDBReader(const RocksDBReader &) = delete;
    RocksDBReader &operator=(RocksDBReader &&) = delete;
    RocksDBReader &operator=(const RocksDBReader &) = delete;
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
     * @return      true
     * @return      false
     */
    bool ReadData(const std::string &key, std::string &value, rocksdb::Status &retStatus);

private:
    rocksdb::ReadOptions read_options_;
    std::shared_ptr<RocksDB> rocksdb_;
};

#endif
