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
        // ERRORLOG("{} rocksdb begintransaction fail", txn_name_); 
        return false;
    }
    return true;
}

bool RocksDBReadWriter::TransactionCommit(rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    ret_status = txn_->Commit();
    if (!ret_status.ok())
    {
        ERRORLOG("{} transction commit failed code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
        return false;
    }
    delete txn_;
    txn_ = nullptr;
    return true;
}

bool RocksDBReadWriter::TransactionRollBack(rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
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

bool RocksDBReadWriter::MultiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (keys.empty())
    {
        ERRORLOG("key is empty");
        ret_status.push_back(rocksdb::Status::Aborted());
        return false;
    }
    ret_status.clear();
    {
        ret_status = txn_->MultiGet(read_options_, keys, &values);
    }
    bool flag = true;
    for(size_t i = 0; i < ret_status.size(); ++i)
    {
        auto status = ret_status.at(i);
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

bool RocksDBReadWriter::MergeValue(const std::string &key, const std::string &value, rocksdb::Status &ret_status, bool first_or_last)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (ReadForUpdate(key, ret_value, ret_status))
    {
        std::vector<std::string> split_values;
        StringUtil::SplitString(ret_value, "_", split_values);
        auto it = std::find(split_values.begin(), split_values.end(), value);
        if(split_values.end() == it)
        {
            if(first_or_last)
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
        return WriteData(key, ret_value, ret_status);
    }
    else
    {
        if (ret_status.IsNotFound())
        {
            return WriteData(key, value, ret_status);
        }
    }
    return false;
}

bool RocksDBReadWriter::RemoveMergeValue(const std::string &key, const std::string &value, rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (ReadForUpdate(key, ret_value, ret_status))
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
            return DeleteData(key, ret_status);
        }
        return WriteData(key, ret_value, ret_status);
    }
    else
    {
        if(ret_status.IsNotFound())
        {
            return true;
        }
    }
    return false;
}
bool RocksDBReadWriter::ReadData(const std::string &key, std::string &value, rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    {
        ret_status = txn_->Get(read_options_, key, &value);
    }
    if (ret_status.ok())
    {
        return true;
    }
    if (ret_status.IsNotFound())
    {
        TRACELOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    if(ret_status.code() == rocksdb::Status::Code::kIOError)
    {
        DBDestory();
        exit(-1);
    }
    return false;
}

bool RocksDBReadWriter::WriteData(const std::string &key, const std::string &value, rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    {
        ret_status = txn_->Put(key, value);
    }
    if (ret_status.ok())
    {
        return true;
    }
    if (ret_status.IsNotFound())
    {
        TRACELOG("{} rocksdb WriteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb WriteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    return false;
}
bool RocksDBReadWriter::DeleteData(const std::string &key, rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    {
        ret_status = txn_->Delete(key);
    }
    if (ret_status.ok())
    {
        return true;
    }
    if (ret_status.IsNotFound())
    {
        TRACELOG("{} rocksdb DeleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb DeleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    return false;
}
bool RocksDBReadWriter::ReadForUpdate(const std::string &key, std::string &value, rocksdb::Status &ret_status)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        ret_status = rocksdb::Status::Aborted();
        return false;
    }
    {
        ret_status = txn_->GetForUpdate(read_options_, key, &value);
    }
    if (ret_status.ok())
    {
        return true;
    }
    if (ret_status.IsNotFound())
    {
        TRACELOG("{} rocksdb ReadForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb ReadForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, ret_status.code(), ret_status.subcode(), ret_status.severity(), ret_status.ToString());
    }
    if(ret_status.code() == rocksdb::Status::Code::kIOError)
    {
        DBDestory();
        exit(-1);
    }
    return false;
}
