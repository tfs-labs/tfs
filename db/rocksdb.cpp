#include "db/rocksdb.h"
#include "utils/magic_singleton.h"
#include "include/logging.h"
#include "db/db_api.h"
#include "ca/ca.h"

void BackgroundErrorListener::OnBackgroundError(rocksdb::BackgroundErrorReason reason, rocksdb::Status *errorStatus)
{
    if (errorStatus != nullptr)
    {
        ERRORLOG("RocksDB Background Error {} code:({}),subcode:({}),severity:({}),info:({})", reason,
                 errorStatus->code(), errorStatus->subcode(), errorStatus->severity(), errorStatus->ToString());
    }
    else
    {
        ERRORLOG("RocksDB Background Error {}", reason);
    }
    DBDestory();
    exit(-1);
}

RocksDB::RocksDB()
{
    db_ = nullptr;
    is_init_success_ = false;
}

RocksDB::~RocksDB()
{
    DestoryDB();
    db_ = nullptr;
    std::lock_guard<std::mutex> lock(is_init_success_mutex_);
    is_init_success_ = false;
}

void RocksDB::SetDBPath(const std::string &dbPath)
{
    db_path_ = dbPath;
}

bool RocksDB::InitDB(rocksdb::Status &retStatus)
{
    if (is_init_success_)
    {
        return false;
    }

    rocksdb::Options options;
    options.create_if_missing = true;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    auto listener = std::make_shared<BackgroundErrorListener>();
    options.listeners.push_back(listener);
    rocksdb::TransactionDBOptions txn_db_options;
    retStatus = rocksdb::TransactionDB::Open(options, txn_db_options, db_path_, &db_);
    if (retStatus.ok())
    {
        is_init_success_ = true;
    }
    else
    {
        ERRORLOG("rocksdb {} Open failed code:({}),subcode:({}),severity:({}),info:({})",
                 db_path_, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    return is_init_success_;
}
void RocksDB::DestoryDB()
{
    {
        std::lock_guard<std::mutex> lock(is_init_success_mutex_);
        is_init_success_ = false;
    }

    rocksdb::Status retStatus;
    if (nullptr != db_)
    {
        retStatus = db_->Close();
        if (!retStatus.ok())
        {
            ERRORLOG("rocksdb {} Close failed code:({}),subcode:({}),severity:({}),info:({})",
                     db_path_, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
            return;
        }
        delete db_;
    }
    db_ = nullptr;
}
bool RocksDB::IsInitSuccess()
{
    std::lock_guard<std::mutex> lock(is_init_success_mutex_);
    return is_init_success_;
}

void RocksDB::GetDBMemoryUsage(std::string& info)
{
    std::string block_cache_usage;
    if (db_->GetProperty("rocksdb.block-cache-usage", &block_cache_usage))
    {
        info.append("block_cache_usage: ").append(block_cache_usage).append("\n");
    }

    std::string estimate_table_readers_mem;
    if (db_->GetProperty("rocksdb.estimate-table-readers-mem", &estimate_table_readers_mem))
    {
        info.append("estimate_table_readers_mem: ").append(estimate_table_readers_mem).append("\n");
    }

    std::string cur_size_all_mem_tables;
    if (db_->GetProperty("rocksdb.cur-size-all-mem-tables", &cur_size_all_mem_tables))
    {
        info.append("cur_size_all_mem_tables: ").append(cur_size_all_mem_tables).append("\n");
    }

    std::string block_cache_pinned_usage;
    if (db_->GetProperty("rocksdb.block-cache-pinned-usage", &block_cache_pinned_usage))
    {
        info.append("block_cache_pinned_usage: ").append(block_cache_pinned_usage).append("\n");
    }
}