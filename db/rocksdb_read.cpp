#include "db/rocksdb_read.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "db/db_api.h"
#include "rocksdb/db.h"

RocksDBReader::RocksDBReader(std::shared_ptr<RocksDB> rocksdb)
{
    rocksdb_ = rocksdb;
}

bool RocksDBReader::MultiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus)
{
    retStatus.clear();
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (keys.empty())
    {
        ERRORLOG("key is empty");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    {
        retStatus = rocksdb_->db_->MultiGet(read_options_, keys, &values);
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
                TRACELOG("rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         key, status.code(), status.subcode(), status.severity(), status.ToString());
            }
            else
            {
                ERRORLOG("rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         key, status.code(), status.subcode(), status.severity(), status.ToString());
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

bool RocksDBReader::ReadData(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->IsInitSuccess())
    {
        ERRORLOG("rocksdb not init");
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
        retStatus = rocksdb_->db_->Get(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("rocksdb ReadData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        DBDestory();
        exit(-1);
    }
    return false;
}
