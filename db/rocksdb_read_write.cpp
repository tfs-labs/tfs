#include "db/rocksdb_read_write.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "db/db_api.h"

RocksDBReadWriter::RocksDBReadWriter(std::shared_ptr<RocksDB> rocksdb, const std::string &txn_name)
{
    txn_ = nullptr;
    rocksdb_ = rocksdb;
    txn_name_ = txn_name;
}

RocksDBReadWriter::~RocksDBReadWriter()
{

}
bool RocksDBReadWriter::TransactionInit()
{
    txn_ = rocksdb_->db_->BeginTransaction(write_options_);
    if (nullptr == txn_)
    {
        return false;
    }
    return true;
}

bool RocksDBReadWriter::TransactionCommit(rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    retStatus = txn_->Commit();
    if (!retStatus.ok())
    {
        ERRORLOG("{} transction commit failed code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
        return false;
    }
    delete txn_;
    txn_ = nullptr;
    return true;
}

bool RocksDBReadWriter::TransactionRollBack(rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    auto status = txn_->Rollback();
    if (!status.ok())
    {
        ERRORLOG("{} transction rollback failed code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, status.code(), status.subcode(), status.severity(), status.ToString());
        return false;
    }
    delete txn_;
    txn_ = nullptr;
    return true;
}

bool RocksDBReadWriter::MultiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (keys.empty())
    {
        ERRORLOG("key is empty");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    retStatus.clear();
    {
        retStatus = txn_->MultiGet(read_options_, keys, &values);
    }
    bool flag = true;
    for(size_t i = 0; i < retStatus.size(); ++i)
    {
        auto status = retStatus.at(i);
        if (!status.ok())
        {
            flag = false;
            std::string key;
            if(keys.size() > i)
            {
                key = keys.at(i).data();
            }
            if (status.IsNotFound())
            {
                TRACELOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         txn_name_, key, status.code(), status.subcode(), status.severity(), status.ToString());
            }
            else
            {
                ERRORLOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         txn_name_, key, status.code(), status.subcode(), status.severity(), status.ToString());
                if(status.code() == rocksdb::Status::Code::kIOError)
                {
                    DBDestory();
                    exit(-1);
                }
            }
        }
    }
    return flag;

}

bool RocksDBReadWriter::MergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus, bool firstOrLast)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (ReadForUpdate(key, ret_value, retStatus))
    {
        std::vector<std::string> split_values;
        StringUtil::SplitString(ret_value, "_", split_values);
        auto it = std::find(split_values.begin(), split_values.end(), value);
        if(split_values.end() == it)
        {
            if(firstOrLast)
            {
                std::vector<std::string> tmp_values;
                tmp_values.push_back(value);
                tmp_values.insert(tmp_values.end(), split_values.begin(), split_values.end());
                split_values.swap(tmp_values);
            }
            else
            {
                split_values.push_back(value);
            }
        }
        ret_value.clear();
        for(auto split_value : split_values)
        {
            if(split_value.empty())
            {
                continue;
            }
            ret_value += split_value;
            ret_value += "_";
        }
        return WriteData(key, ret_value, retStatus);
    }
    else
    {
        if (retStatus.IsNotFound())
        {
            return WriteData(key, value, retStatus);
        }
    }
    return false;
}

bool RocksDBReadWriter::RemoveMergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (ReadForUpdate(key, ret_value, retStatus))
    {
        std::vector<std::string> split_values;
        StringUtil::SplitString(ret_value, "_", split_values);
        auto it = std::find(split_values.begin(), split_values.end(), value);
        while(split_values.end() != it)
        {
            split_values.erase(it);
            it = std::find(split_values.begin(), split_values.end(), value);
        }
        ret_value.clear();
        for(auto split_value : split_values)
        {
            if(split_value.empty())
            {
                continue;
            }
            ret_value += split_value;
            ret_value += "_";
        }
        if(ret_value.empty())
        {
            return DeleteData(key, retStatus);
        }
        return WriteData(key, ret_value, retStatus);
    }
    else
    {
        if(retStatus.IsNotFound())
        {
            return true;
        }
    }
    return false;
}
bool RocksDBReadWriter::ReadData(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = txn_->Get(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        DBDestory();
        exit(-1);
    }
    return false;
}

bool RocksDBReadWriter::WriteData(const std::string &key, const std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = txn_->Put(key, value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb WriteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb WriteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    return false;
}
bool RocksDBReadWriter::DeleteData(const std::string &key, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = txn_->Delete(key);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb DeleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb DeleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    return false;
}
bool RocksDBReadWriter::ReadForUpdate(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = txn_->GetForUpdate(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb ReadForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb ReadForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        DBDestory();
        exit(-1);
    }
    return false;
}
