#ifndef TFS_DB_ROCKSDB_H_
#define TFS_DB_ROCKSDB_H_

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "rocksdb/utilities/transaction.h"
#include "rocksdb/utilities/transaction_db.h"
#include <mutex>

class BackgroundErrorListener : public rocksdb::EventListener
{
public:
    void OnBackgroundError(rocksdb::BackgroundErrorReason reason, rocksdb::Status* errorStatus) override;
};

class RocksDBReader;
class RocksDBReadWriter;
class RocksDB
{
public:
    RocksDB();
    ~RocksDB();
    RocksDB(RocksDB &&) = delete;
    RocksDB(const RocksDB &) = delete;
    RocksDB &operator=(RocksDB &&) = delete;
    RocksDB &operator=(const RocksDB &) = delete;
    /**
     * @brief
     * 
     * @param       dbPath: 
     */
    void SetDBPath(const std::string &dbPath);
    /**
     * @brief
     * 
     * @param       retStatus: 
     * @return      true
     * @return      false
     */
    bool InitDB(rocksdb::Status &retStatus);
     /**
     * @brief
     */
    void DestoryDB();
    /**
     * @brief
     * 
     * @return      true
     * @return      false
     */
    bool IsInitSuccess();
    /**
     * @brief
     * @param       info:
     */
    void GetDBMemoryUsage(std::string& info);

private:
    friend class BackgroundErrorListener;
    friend class RocksDBReader;
    friend class RocksDBReadWriter;
    std::string db_path_;
    rocksdb::TransactionDB *db_;
    std::mutex is_init_success_mutex_;
    bool is_init_success_;
};



#endif
