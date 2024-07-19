#include "db/db_api.h"
#include "utils/magic_singleton.h"
#include "db/cache.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "ca/global.h"


// Block-related interfaces
const std::string kBlockHash2BlockHeightKey = "blkhs2blkht_";
const std::string kBlockHeight2BlockHashKey = "blkht2blkhs_";
const std::string kBlockHeight2SumHash = "blkht2sumhs_";
const std::string kTopThousandSumhashKey = "topthousandsumhs_";
const std::string kBlockHeight2thousandSumHash = "thousandsblkht2sumhs_";
const std::string kBlockHash2BlcokRawKey = "blkhs2blkraw_";
const std::string kBlockTopKey = "blktop_";
const std::string kAddress2UtxoKey = "addr2utxo_";
const std::string kTransactionHash2TransactionRawKey = "txhs2txraw_";
const std::string kTransactionHash2BlockHashKey = "txhs2blkhs_";

// Transaction inquiry related
const std::string kAddress2TransactionRawKey = "addr2txraw_";
const std::string kAddress2BlcokHashKey = "addr2blkhs_";
const std::string kAddress2TransactionTopKey = "addr2txtop_";


// Application-tier queries
const std::string kAddress2BalanceKey = "addr2bal_";
const std::string kStakeAddrKey = "stakeaddr_";
const std::string kMutliSignKey = "mutlisign_";
const std::string kBonusUtxoKey = "bonusutxo_";
const std::string kBonusAddrKey = "bonusaddr_";
const std::string kBonusAddr2InvestAddrKey = "bonusaddr2investaddr_";
const std::string kInvestAddr2BonusAddrKey = "investaddr2bonusaddr_";
const std::string kBonusAddrInvestAddr2InvestAddrUtxo = "invest_nodeaddr2investaddrutxo_";
const std::string kInvestUtxoKey = "investutxo_";


const std::string kM2 = "M2_";
const std::string kDM = "DM_"; //Deflationary Mechanism
const std::string kTotalInvestAmount = "totalinvestamount_";
const std::string kInitVersionKey = "initver_";
const std::string kSignNumberKey = "signnumber_";
const std::string kBlockNumberKey = "blocknumber_";
const std::string kSignAddrKey = "signaddr_";
const std::string kburnAmountKey = "burnamount_";

const std::string kEvmAllDeployerAddr = "allevmdeployeraddr_";
const std::string kWasmAllDeployerAddr = "allwasmdepoyeraddr_";
// const std::string kDeployerAddr2DeployUtxo = "deployeraddr2deployutxo_";
const std::string kDeployerAddr2ContractAddr = "deployeraddr2contractaddr_";
const std::string kContractAddr2ContractCode = "contractaddr2contractcode_";
const std::string kContractAddr2DeployUtxo = "contractaddr2deployutxo_";
const std::string kContractAddr2LatestUtxo = "contractaddr2latestutxo_";
const std::string kLatestContractBlockHash = "latestcontractblockhash_";
const std::string kContractMptK = "contractmpt_";
bool DBInit(const std::string &db_path)
{
    MagicSingleton<RocksDB>::GetInstance()->SetDBPath(db_path);
    rocksdb::Status ret_status;
    if (!MagicSingleton<RocksDB>::GetInstance()->InitDB(ret_status))
    {
        ERRORLOG("rocksdb init fail {}", ret_status.ToString());
        return false;
    }
    return true;
}
void DBDestory()
{
    MagicSingleton<RocksDB>::DesInstance();
}

DBReader::DBReader() : db_reader_(MagicSingleton<RocksDB>::GetInstance())
{
}

DBStatus DBReader::GetBlockHashesByBlockHeight(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes)
{
    std::vector<std::string> keys;
    std::vector<std::string> values;
    std::vector<std::string> hashes;
    for (uint64_t index_height = startHeight; index_height <= endHeight; ++index_height)
    {
        keys.push_back(kBlockHeight2BlockHashKey + std::to_string(index_height));
    }
    auto ret = MultiReadData(keys, values);
    if (values.empty() && DBStatus::DB_SUCCESS != ret)
    {
        return ret;
    }
    blockHashes.clear();
    for (auto &value : values)
    {
        hashes.clear();
        StringUtil::SplitString(value, "_", hashes);
        blockHashes.insert(blockHashes.end(), hashes.begin(), hashes.end());
    }
    return ret;
}
DBStatus DBReader::GetBlocksByBlockHash(const std::vector<std::string> &blockHashes, std::vector<std::string> &blocks)
{
    std::vector<std::string> keys;
    for (auto &hash : blockHashes)
    {
        keys.push_back(kBlockHash2BlcokRawKey + hash);
    }
    return MultiReadData(keys, blocks);
}

// Gets the height of the data block by the block hash
DBStatus DBReader::GetBlockHeightByBlockHash(const std::string &blockHash, unsigned int &blockHeight)
{
    if (blockHash.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::string db_key = kBlockHash2BlockHeightKey + blockHash;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        blockHeight = std::stoul(value);
    }
    return ret;
}


DBStatus DBReader::GetBlockHashByBlockHeight(uint64_t blockHeight, std::string &hash)
{
    std::vector<std::string> hashes;
    auto ret = GetBlockHashsByBlockHeight(blockHeight, hashes);
    if (DBStatus::DB_SUCCESS == ret && (!hashes.empty()))
    {
        hash = hashes.at(0);
    }
    return ret;
}


// Multiple block hashes are obtained by the height of the data block
DBStatus DBReader::GetBlockHashsByBlockHeight(uint64_t blockHeight, std::vector<std::string> &hashes)
{
    std::string db_key = kBlockHeight2BlockHashKey + std::to_string(blockHeight);
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", hashes);
    }
    return ret;
}


// The block information is obtained through the block hash
DBStatus DBReader::GetBlockByBlockHash(const std::string &blockHash, std::string &block)
{
    if (blockHash.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::string db_key = kBlockHash2BlcokRawKey + blockHash;
    return ReadData(db_key, block);
}

// Get Sum hash per 100 heights
DBStatus DBReader::GetSumHashByHeight(uint64_t height, std::string& sumHash)
{
    if (height % 100 != 0 || height == 0)
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::string db_key = kBlockHeight2SumHash + std::to_string(height);
    return ReadData(db_key, sumHash); 
}


DBStatus DBReader::GetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight, std::string &sumHash)
{
    if (blockHeight % 1000 != 0 || blockHeight == 0)
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::string db_key = kBlockHeight2thousandSumHash + std::to_string(blockHeight);
    return ReadData(db_key, sumHash); 
}


DBStatus DBReader::GetTopThousandSumhash(uint64_t &thousandNum)
{
    std::string value;
    auto ret = ReadData(kTopThousandSumhashKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        thousandNum = std::stoul(value);
    }
    return ret;
}



DBStatus DBReader::GetBlockTop(uint64_t &blockHeight)
{
    std::string value;
    auto ret = ReadData(kBlockTopKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        blockHeight = std::stoul(value);
    }
    return ret;
}


// Get the Uxo hash by address (there are multiple utxohashes)
DBStatus DBReader::GetUtxoHashsByAddress(const std::string &address, std::vector<std::string> &utxoHashs)
{
    std::string db_key = kAddress2UtxoKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxoHashs);
    }
    return ret;
}

DBStatus DBReader::GetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, std::string &balance)
{
    std::string db_key = address + "_" + utxoHash;
    return ReadData(db_key, balance);
}

// Obtain the transaction raw data by the transaction hash
DBStatus DBReader::GetTransactionByHash(const std::string &txHash, std::string &txRaw)
{
    std::string db_key = kTransactionHash2TransactionRawKey + txHash;
    return ReadData(db_key, txRaw);
}

// Get the block hash by transaction hash
DBStatus DBReader::GetBlockHashByTransactionHash(const std::string &txHash, std::string &blockHash)
{
    std::string db_key = kTransactionHash2BlockHashKey + txHash;
    return ReadData(db_key, blockHash);
}

// Get block transactions by transaction address
DBStatus DBReader::GetTransactionByAddress(const std::string &address, const uint32_t txNum, std::string &txRaw)
{
    std::string db_key = kAddress2TransactionRawKey + address + "_" + std::to_string(txNum);
    return ReadData(db_key, txRaw);
}


// Gets the block hash by the transaction address
DBStatus DBReader::GetBlockHashByAddress(const std::string &address, const uint32_t txNum, std::string &blockHash)
{
    std::string db_key = kAddress2BlcokHashKey + address + "_" + std::to_string(txNum);
    return ReadData(db_key, blockHash);
}


// Get the maximum height of the transaction by the transaction address
DBStatus DBReader::GetTransactionTopByAddress(const std::string &address, unsigned int &txIndex)
{
    std::string db_key = kAddress2TransactionTopKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        txIndex = std::stoul(value);
    }
    return ret;
}


// Get the account balance by the transaction address
DBStatus DBReader::GetBalanceByAddress(const std::string &address, int64_t &balance)
{
    std::string db_key = kAddress2BalanceKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        balance = std::stol(value);
    }
    return ret;
}


// Get the staking address
DBStatus DBReader::GetStakeAddress(std::vector<std::string> &addresses)
{
    std::string value;
    auto ret = ReadData(kStakeAddrKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}

// Get the UTXO of the staking address
DBStatus DBReader::GetStakeAddressUtxo(const std::string &address, std::vector<std::string> &utxos)
{
    std::string db_key = kStakeAddrKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

// Get the multi-Sig address
DBStatus DBReader::GetMutliSignAddress(std::vector<std::string> &addresses)
{
    std::string value;
    auto ret = ReadData(kMutliSignKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}


// Gets the UTXO of the multi-signature address
DBStatus DBReader::GetMutliSignAddressUtxo(const std::string &address,std::vector<std::string> &utxos)
{
    std::string db_key = kMutliSignKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}


// Get the nodes that are invested
DBStatus DBReader::GetBonusaddr(std::vector<std::string> &bonusAddrs)
{
    std::string db_key = kBonusAddrKey;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", bonusAddrs);
    }
    return ret;
}

// Get the investment pledge address Invest_A:X_Y_Z (where A is the investee address, X, Y, Z is the investor's address)
DBStatus DBReader::GetInvestAddrsByBonusAddr(const std::string &bonusAddr, std::vector<std::string> &addresses)
{
    std::string db_key = kBonusAddr2InvestAddrKey + bonusAddr;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}

// Get the investment pledge address Invest_X:A_B_C (where X is the investor's account and A, B, and C are the investee addresses)
DBStatus DBReader::GetBonusAddrByInvestAddr(const std::string &address, std::vector<std::string> &nodes)
{
    std::string db_key = kInvestAddr2BonusAddrKey + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", nodes);
    }
    return ret;
}

// Obtain the UTXO Invest_A_X:u1_u2_u3 of the account that invests in pledged assets
DBStatus DBReader::GetBonusAddrInvestUtxosByBonusAddr(const std::string & addr,const std::string & address, std::vector<std::string> &utxos)
{
    std::string db_key = kBonusAddrInvestAddr2InvestAddrUtxo + addr + "_" + address;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

DBStatus DBReader::GetBonusUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos)
{
    std::string value;
    auto ret = ReadData(kBonusUtxoKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

DBStatus DBReader::GetInvestUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos)
{
    std::string value;
    auto ret = ReadData(kInvestUtxoKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

//  Get Number of signatures By period
DBStatus DBReader::GetSignNumberByPeriod(const uint64_t &period, const std::string &address, uint64_t &SignNumber)
{
    std::string value;
    auto ret = ReadData(kSignNumberKey + std::to_string(period) + address, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        SignNumber = std::stoull(value);
    }
    return ret;
}

//  Get Number of blocks By period
DBStatus DBReader::GetBlockNumberByPeriod(const uint64_t &period, uint64_t &BlockNumber)
{
    std::string value;
    auto ret = ReadData(kBlockNumberKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        BlockNumber = std::stoull(value);
    }
    return ret;
}

//  Set Addr of signatures By period
DBStatus DBReader::GetSignAddrByPeriod(const uint64_t &period, std::vector<std::string> &SignAddrs)
{
    std::string value;
    auto ret = ReadData(kSignAddrKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", SignAddrs);
    }
    return ret;
}

DBStatus DBReader::GetburnAmountByPeriod(const uint64_t &period, uint64_t &burnAmount)
{
    std::string value;
    auto ret = ReadData(kburnAmountKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        burnAmount = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::GetDM(uint64_t &totalBurn)
{
    std::string value;
    auto ret = ReadData(kDM, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        totalBurn = std::stoull(value);
    }
    return ret;
}

// Get the total amount of stake
DBStatus DBReader::GetTotalInvestAmount(uint64_t &Total)
{
    std::string value;
    auto ret = ReadData(kTotalInvestAmount, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        Total = std::stoull(value);
    }
    return ret;
}

// Get the total amount in circulation
DBStatus DBReader::GetM2(uint64_t &Total)
{
    std::string value;
    auto ret = ReadData(kM2, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        Total = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::GetAllEvmDeployerAddr(std::vector<std::string> &deployerAddr)
{
    std::string value;
    auto ret = ReadData(kEvmAllDeployerAddr, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value,  "_", deployerAddr);
    }
    return ret;
}

//DBStatus DBReader::GetAllWasmDeployerAddr(std::vector<std::string> &deployerAddr)
//{
//    std::string value;
//    auto ret = ReadData(kWasmAllDeployerAddr, value);
//    if (DBStatus::DB_SUCCESS == ret)
//    {
//        StringUtil::SplitString(value,  "_", deployerAddr);
//    }
//    return ret;
//}

DBStatus DBReader::GetContractAddrByDeployerAddr(const std::string &deployerAddr, std::vector<std::string> &contractAddr)
{
    std::string db_key = kDeployerAddr2ContractAddr + deployerAddr;
    std::string value;
    auto ret = ReadData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", contractAddr);
    }
    return ret;
}

DBStatus DBReader::GetContractCodeByContractAddr(const std::string &contractAddr, std::string &contractCode)
{
    std::string db_key = kContractAddr2ContractCode + contractAddr;
    return ReadData(db_key, contractCode);
}
DBStatus DBReader::GetContractDeployUtxoByContractAddr(const std::string &contractAddr, std::string &contractDeployUtxo)
{
    std::string db_key = kContractAddr2DeployUtxo + contractAddr;
    return ReadData(db_key, contractDeployUtxo);
}

DBStatus DBReader::GetLatestUtxoByContractAddr(const std::string &contractAddr, std::string &Utxo)
{
    std::string db_key = kContractAddr2LatestUtxo + contractAddr;
    return ReadData(db_key, Utxo);
}

DBStatus DBReader::GetMptValueByMptKey(const std::string &mptKey, std::string &MptValue)
{
    std::string db_key = kContractMptK + mptKey;
    return ReadData(db_key, MptValue);
}

DBStatus DBReader::GetInitVer(std::string &version)
{
    std::string tmpversion;
    auto ret = ReadData(kInitVersionKey, tmpversion);
    if (DBStatus::DB_SUCCESS == ret)
    {
        version = tmpversion;
    }
    return ret;
}

DBStatus DBReader::MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values)
{
    if (keys.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::vector<std::string> cache_values;
    std::vector<rocksdb::Slice> db_keys;
    std::vector<std::string> str;
    for(auto t : keys)
    {
        str.push_back(t);
    }
    
    std::vector<std::string> db_keys_str;
    for (auto key : keys)
    {
        db_keys_str.push_back(key);
        db_keys.push_back(db_keys_str.back());
    }

    std::string value;
    std::vector<rocksdb::Status> ret_status;
    if (db_reader_.MultiReadData(db_keys, values, ret_status))
    {
        if (db_keys.size() != values.size())
        {
            return DBStatus::DB_ERROR;
        }
        values.insert(values.end(), cache_values.begin(), cache_values.end());
        return DBStatus::DB_SUCCESS;
    }
    else
    {
        for (auto status : ret_status)
        {
            if (status.IsNotFound())
            {
                return DBStatus::DB_NOT_FOUND;
            }
        }
    }
    return DBStatus::DB_ERROR;
}

DBStatus DBReader::ReadData(const std::string &key, std::string &value)
{
    if (key.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }

    rocksdb::Status ret_status;
    if (db_reader_.ReadData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    else if (ret_status.IsNotFound())
    {
        value.clear();
        return DBStatus::DB_NOT_FOUND;
    }
    return DBStatus::DB_ERROR;
}

DBReadWriter::DBReadWriter(const std::string &txn_name) : db_read_writer_(MagicSingleton<RocksDB>::GetInstance(), txn_name)
{
    auto_oper_trans = false;
    ReTransactionInit();
}

DBReadWriter::~DBReadWriter()
{
    TransactionRollBack();
}
DBStatus DBReadWriter::ReTransactionInit()
{
    auto ret = TransactionRollBack();
    if (DBStatus::DB_SUCCESS != ret)
    {
        return ret;
    }
    auto_oper_trans = true;
    if (!db_read_writer_.TransactionInit())
    {
        ERRORLOG("transction init error");
        return DBStatus::DB_ERROR;
    }
    return DBStatus::DB_SUCCESS;
}

DBStatus DBReadWriter::TransactionCommit()
{
    rocksdb::Status ret_status;
    if (db_read_writer_.TransactionCommit(ret_status))
    {
        auto_oper_trans = false;
        return DBStatus::DB_SUCCESS;
    }
    ERRORLOG("TransactionCommit faild:{}:{}", ret_status.code(), ret_status.ToString());
    return DBStatus::DB_ERROR;
}


// Sets the height of the data block by block hash
DBStatus DBReadWriter::SetBlockHeightByBlockHash(const std::string &blockHash, const unsigned int blockHeight)
{
    std::string db_key = kBlockHash2BlockHeightKey + blockHash;
    return WriteData(db_key, std::to_string(blockHeight));
}


// Removes the block height from the database by block hashing
DBStatus DBReadWriter::DeleteBlockHeightByBlockHash(const std::string &blockHash)
{
    std::string db_key = kBlockHash2BlockHeightKey + blockHash;
    return DeleteData(db_key);
}

// Hash data blocks by block height (multiple block hashes at the same height at the same time of concurrency)
DBStatus DBReadWriter::SetBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash, bool isMainBlock)
{
    std::string db_key = kBlockHeight2BlockHashKey + std::to_string(blockHeight);
    return MergeValue(db_key, blockHash, isMainBlock);
}


// Remove the hash of a block in the database by block height
DBStatus DBReadWriter::RemoveBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash)
{
    std::string db_key = kBlockHeight2BlockHashKey + std::to_string(blockHeight);
    return RemoveMergeValue(db_key, blockHash);
}


// Set blocks by block hash
DBStatus DBReadWriter::SetBlockByBlockHash(const std::string &blockHash, const std::string &block)
{
    std::string db_key = kBlockHash2BlcokRawKey + blockHash;
    return WriteData(db_key, block);
}


// Remove blocks inside a data block by block hashing
DBStatus DBReadWriter::DeleteBlockByBlockHash(const std::string &blockHash)
{
    std::string db_key = kBlockHash2BlcokRawKey + blockHash;
    return DeleteData(db_key);
}


// Set Sum hash per 100 heights
DBStatus DBReadWriter::SetSumHashByHeight(uint64_t height, const std::string& sumHash)
{
    std::string db_key = kBlockHeight2SumHash + std::to_string(height);
    return WriteData(db_key, sumHash);
}

// Remove Sum hash per 100 heights
DBStatus DBReadWriter::RemoveSumHashByHeight(uint64_t height)
{
    std::string db_key = kBlockHeight2SumHash + std::to_string(height);
    return DeleteData(db_key);
}


//Set  Sum hash per 1000 heights
DBStatus DBReadWriter::SetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight ,const std::string &sumHash)
{
    std::string db_key = kBlockHeight2thousandSumHash + std::to_string(blockHeight);
    return WriteData(db_key, sumHash);
}

//Set  Sum hash per 1000 heights
DBStatus DBReadWriter::RemoveCheckBlockHashsByBlockHeight(const uint64_t &blockHeight)
{
    std::string db_key = kBlockHeight2thousandSumHash + std::to_string(blockHeight);
    return DeleteData(db_key);
}


DBStatus DBReadWriter::SetTopThousandSumhash(const uint64_t &thousandNum)
{
    return WriteData(kTopThousandSumhashKey, std::to_string(thousandNum));
}

DBStatus DBReadWriter::RemoveTopThousandSumhash(const uint64_t &thousandNum)
{
    return DeleteData(kTopThousandSumhashKey);
}

// Set the highest block
DBStatus DBReadWriter::SetBlockTop(const unsigned int blockHeight)
{
    return WriteData(kBlockTopKey, std::to_string(blockHeight));
}


// Set the UTXO hash by address
DBStatus DBReadWriter::SetUtxoHashsByAddress(const std::string &address, const std::string &utxoHash)
{
    std::string db_key = kAddress2UtxoKey + address;
    return MergeValue(db_key, utxoHash);
}


// Remove the Utoucho hash by address
DBStatus DBReadWriter::RemoveUtxoHashsByAddress(const std::string &address, const std::string &utxoHash)
{
    std::string db_key = kAddress2UtxoKey + address;
    return RemoveMergeValue(db_key, utxoHash);
}


DBStatus DBReadWriter::SetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance)
{
    std::string db_key = address + "_" + utxoHash;
    return WriteData(db_key, balance);
}


DBStatus DBReadWriter::RemoveUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance)
{
    std::string db_key = address + "_" + utxoHash;
    return DeleteData(db_key);
}


// Set the transaction raw data by transaction hash
DBStatus DBReadWriter::SetTransactionByHash(const std::string &txHash, const std::string &txRaw)
{
    std::string db_key = kTransactionHash2TransactionRawKey + txHash;
    return WriteData(db_key, txRaw);
}


// Remove the transaction raw data from the database by transaction hash
DBStatus DBReadWriter::DeleteTransactionByHash(const std::string &txHash)
{
    std::string db_key = kTransactionHash2TransactionRawKey + txHash;
    return DeleteData(db_key);
}


// Set the block hash by transaction hash
DBStatus DBReadWriter::SetBlockHashByTransactionHash(const std::string &txHash, const std::string &blockHash)
{
    std::string db_key = kTransactionHash2BlockHashKey + txHash;
    return WriteData(db_key, blockHash);
}

// Remove the block hash from the database by transaction hashing
DBStatus DBReadWriter::DeleteBlockHashByTransactionHash(const std::string &txHash)
{
    std::string db_key = kTransactionHash2BlockHashKey + txHash;
    return DeleteData(db_key);
}


// Set up block transactions by transaction address
DBStatus DBReadWriter::SetTransactionByAddress(const std::string &address, const uint32_t txNum, const std::string &txRaw)
{
    std::string db_key = kAddress2TransactionRawKey + address + "_" + std::to_string(txNum);
    return WriteData(db_key, txRaw);
}

// Remove transaction data from the database by transaction address
DBStatus DBReadWriter::DeleteTransactionByAddress(const std::string &address, const uint32_t txNum)
{
    std::string db_key = kAddress2TransactionRawKey + address + "_" + std::to_string(txNum);
    return DeleteData(db_key);
}


// Set the block hash by transaction address
DBStatus DBReadWriter::SetBlockHashByAddress(const std::string &address, const uint32_t txNum, const std::string &blockHash)
{
    std::string db_key = kAddress2BlcokHashKey + address + "_" + std::to_string(txNum);
    return WriteData(db_key, blockHash);
}


// Remove the block hash in the database by the transaction address
DBStatus DBReadWriter::DeleteBlockHashByAddress(const std::string &address, const uint32_t txNum)
{
    std::string db_key = kAddress2BlcokHashKey + address + "_" + std::to_string(txNum);
    return DeleteData(db_key);
}


// Set the maximum height of the transaction by the transaction address
DBStatus DBReadWriter::SetTransactionTopByAddress(const std::string &address, const unsigned int txIndex)
{
    std::string db_key = kAddress2TransactionTopKey + address;
    return WriteData(db_key, std::to_string(txIndex));
}


// Set the account balance by the transaction address
DBStatus DBReadWriter::SetBalanceByAddress(const std::string &address, int64_t balance)
{
    std::string db_key = kAddress2BalanceKey + address;
    return WriteData(db_key, std::to_string(balance));
}

DBStatus DBReadWriter::DeleteBalanceByAddress(const std::string &address)
{
    std::string db_key = kAddress2BalanceKey + address;
    return DeleteData(db_key);
}


// Set the staking address
DBStatus DBReadWriter::SetStakeAddresses(const std::string &address)
{
    return MergeValue(kStakeAddrKey, address);
}


// Remove the staking address from the database
DBStatus DBReadWriter::RemoveStakeAddresses(const std::string &address)
{
    return RemoveMergeValue(kStakeAddrKey, address);
}


// Set up a multi-Sig address
DBStatus DBReadWriter::SetMutliSignAddresses(const std::string &address)
{
    return MergeValue(kMutliSignKey, address);
}


// Remove the multi-Sig address from the database
DBStatus DBReadWriter::RemoveMutliSignAddresses(const std::string &address)
{
    return RemoveMergeValue(kMutliSignKey, address);
}


// Set up UTXO for the Holddown Asset Account
DBStatus DBReadWriter::SetStakeAddressUtxo(const std::string &stakeAddr, const std::string &utxo)
{
    std::string db_key = kStakeAddrKey + stakeAddr;
    return MergeValue(db_key, utxo);
}

// Remove the UTXO from the data
DBStatus DBReadWriter::RemoveStakeAddressUtxo(const std::string &stakeAddr, const std::string &utxo)
{
    std::string db_key = kStakeAddrKey + stakeAddr;
    return RemoveMergeValue(db_key, utxo);
}


// Set up a UTXO for a multi-Sig asset account
DBStatus DBReadWriter::SetMutliSignAddressUtxo(const std::string &address, const std::string &utxo)
{
    std::string db_key = kMutliSignKey + address;
    return MergeValue(db_key, utxo);
}


// Remove UTXO from multi-sig data
DBStatus DBReadWriter::RemoveMutliSignAddressUtxo(const std::string &address, const std::string &utxos)
{
    std::string db_key = kMutliSignKey + address;
    return RemoveMergeValue(db_key, utxos);
}


// Set up the node to be invested
DBStatus DBReadWriter::SetBonusAddr(const std::string &bonusAddr)
{
    std::string db_key = kBonusAddrKey;
    return MergeValue(db_key, bonusAddr);
}


// Remove the delegated staking address from the database
DBStatus DBReadWriter::RemoveBonusAddr(const std::string &bonusAddr)
{
    std::string db_key = kBonusAddrKey;
    return RemoveMergeValue(db_key, bonusAddr);
}


// Set the delegated staking address Invest_A:X_Y_Z
DBStatus DBReadWriter::SetInvestAddrByBonusAddr(const std::string &bonusAddr, const std::string& investAddr)
{
    std::string db_key = kBonusAddr2InvestAddrKey + bonusAddr;
    return MergeValue(db_key, investAddr);
}


// Remove the delegated staking address from the database
DBStatus DBReadWriter::RemoveInvestAddrByBonusAddr(const std::string &bonusAddr, const std::string& investAddr)
{
    std::string db_key = kBonusAddr2InvestAddrKey + bonusAddr;
    return RemoveMergeValue(db_key, investAddr);
}


// Set which node the investor invested in
DBStatus DBReadWriter::SetBonusAddrByInvestAddr(const std::string &investAddr, const std::string& bonusAddr)
{
    std::string db_key = kInvestAddr2BonusAddrKey + investAddr;
    return MergeValue(db_key, bonusAddr);
}


// Remove the node to which the investor invested
DBStatus DBReadWriter::RemoveBonusAddrByInvestAddr(const std::string &investAddr, const std::string& bonusAddr)
{
    std::string db_key = kInvestAddr2BonusAddrKey + investAddr;
    return RemoveMergeValue(db_key, bonusAddr);
}


// Set the UTXO Invest_A_X:u1_u2_u3 corresponding to the node where you invest in yourself
DBStatus DBReadWriter::SetBonusAddrInvestAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &investAddr, const std::string &utxo)
{
    MagicSingleton<BonusAddrCache>::GetInstance()->isDirty(bonusAddr);
    std::string db_key = kBonusAddrInvestAddr2InvestAddrUtxo + bonusAddr + "_" + investAddr;
    return MergeValue(db_key, utxo);
}


// Remove the UTXO corresponding to the node where you invested in it
DBStatus DBReadWriter::RemoveBonusAddrInvestAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &investAddr, const std::string &utxo)
{
    MagicSingleton<BonusAddrCache>::GetInstance()->isDirty(bonusAddr);
    std::string db_key = kBonusAddrInvestAddr2InvestAddrUtxo + bonusAddr + "_" + investAddr;
    return RemoveMergeValue(db_key, utxo);
}

// Set up a bonus transaction
DBStatus DBReadWriter::SetBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return MergeValue(kBonusUtxoKey + std::to_string(period), utxo);
}
// Remove a bonus transaction
DBStatus DBReadWriter::RemoveBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return RemoveMergeValue(kBonusUtxoKey + std::to_string(period), utxo);
}

// Set up an investment transaction
DBStatus DBReadWriter::SetInvestUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return MergeValue(kInvestUtxoKey + std::to_string(period), utxo);
}
// Remove an investment transaction
DBStatus DBReadWriter::RemoveInvestUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return RemoveMergeValue(kInvestUtxoKey + std::to_string(period),utxo);
}

DBStatus DBReadWriter::SetEvmDeployerAddr(const std::string &deployerAddr)
{
    return MergeValue(kEvmAllDeployerAddr, deployerAddr);
}
DBStatus DBReadWriter::RemoveEvmDeployerAddr(const std::string &deployerAddr)
{
    return RemoveMergeValue(kEvmAllDeployerAddr, deployerAddr);
}

//DBStatus DBReadWriter::SetWasmDeployerAddr(const std::string &deployerAddr)
//{
//    return MergeValue(kWasmAllDeployerAddr, deployerAddr);
//}
//
//DBStatus DBReadWriter::RemoveWasmDeployerAddr(const std::string &deployerAddr)
//{
//    return RemoveMergeValue(kWasmAllDeployerAddr, deployerAddr);
//}

DBStatus DBReadWriter::SetContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr)
{
    std::string db_key = kDeployerAddr2ContractAddr + deployerAddr;
    return MergeValue(db_key, contractAddr);
}

DBStatus DBReadWriter::RemoveContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr)
{
    std::string db_key = kDeployerAddr2ContractAddr + deployerAddr;
    return RemoveMergeValue(db_key, contractAddr);
}

DBStatus DBReadWriter::SetContractCodeByContractAddr(const std::string &contractAddr, const std::string &contractCode)
{
    std::string db_key = kContractAddr2ContractCode + contractAddr;
    return WriteData(db_key, contractCode);
}

DBStatus DBReadWriter::RemoveContractCodeByContractAddr(const std::string &contractAddr)
{
    std::string db_key = kContractAddr2ContractCode + contractAddr;
    return DeleteData(db_key);
}

DBStatus DBReadWriter::SetContractDeployUtxoByContractAddr(const std::string &contractAddr, const std::string &contractDeployUtxo)
{
    std::string db_key = kContractAddr2DeployUtxo + contractAddr;
    return WriteData(db_key, contractDeployUtxo);
}

DBStatus DBReadWriter::RemoveContractDeployUtxoByContractAddr(const std::string &contractAddr)
{
    std::string db_key = kContractAddr2DeployUtxo + contractAddr;
    return DeleteData(db_key);
}

DBStatus DBReadWriter::SetLatestUtxoByContractAddr(const std::string &contractAddr, const std::string &utxo)
{
    std::string db_key = kContractAddr2LatestUtxo + contractAddr;
    return WriteData(db_key, utxo);
}

DBStatus DBReadWriter::RemoveLatestUtxoByContractAddr(const std::string &contractAddr)
{
    std::string db_key = kContractAddr2LatestUtxo + contractAddr;
    return DeleteData(db_key);
}

DBStatus DBReadWriter::SetMptValueByMptKey(const std::string &mptKey, const std::string &MptValue)
{
    std::string db_key = kContractMptK + mptKey;
    return WriteData(db_key, MptValue);
}
DBStatus DBReadWriter::RemoveMptValueByMptKey(const std::string &mptKey)
{
    std::string db_key = kContractMptK + mptKey;
    return DeleteData(db_key);
}
//  Set Number of signatures By period
DBStatus DBReadWriter::SetSignNumberByPeriod(const uint64_t &period, const std::string &address, const uint64_t &SignNumber)
{
    std::string db_key = kSignNumberKey + std::to_string(period) + address;
    return WriteData(db_key, std::to_string(SignNumber));
}

//  Remove Number of signatures By period
DBStatus DBReadWriter::RemoveSignNumberByPeriod(const uint64_t &period, const std::string &address)
{
    std::string db_key = kSignNumberKey + std::to_string(period) + address;
    return DeleteData(db_key);
}

//  Set Number of blocks By period
DBStatus DBReadWriter::SetBlockNumberByPeriod(const uint64_t &period, const uint64_t &BlockNumber)
{
    std::string db_key = kBlockNumberKey + std::to_string(period);
    return WriteData(db_key, std::to_string(BlockNumber));
}


//  Remove Number of blocks By period
DBStatus DBReadWriter::RemoveBlockNumberByPeriod(const uint64_t &period)
{
    std::string db_key = kBlockNumberKey + std::to_string(period);
    return DeleteData(db_key);
}

//  Set Addr of signatures By period
DBStatus DBReadWriter::SetSignAddrByPeriod(const uint64_t &period, const std::string &addr)
{
    return MergeValue(kSignAddrKey + std::to_string(period), addr);
}

//  Remove Addr of signatures By period
DBStatus DBReadWriter::RemoveSignAddrberByPeriod(const uint64_t &period, const std::string &addr)
{
    return RemoveMergeValue(kSignAddrKey + std::to_string(period), addr);
}

DBStatus DBReadWriter::SetburnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount)
{
    return WriteData(kburnAmountKey + std::to_string(period), std::to_string(burnAmount));
}
DBStatus DBReadWriter::RemoveburnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount)
{
    return DeleteData(kburnAmountKey + std::to_string(period));
}

DBStatus DBReadWriter::SetDM(uint64_t &totalBurn)
{
    return WriteData(kDM, std::to_string(totalBurn));
}


// Set the total amount of stake
DBStatus DBReadWriter::SetTotalInvestAmount(uint64_t &totalInvest)
{
    return WriteData(kTotalInvestAmount, std::to_string(totalInvest));
}

// Set the total circulation
DBStatus DBReadWriter::SetTotalCirculation(uint64_t &totalCirculation)
{
    return WriteData(kM2, std::to_string(totalCirculation));
}


// Record the version of the program that initialized the database
DBStatus DBReadWriter::SetInitVer(const std::string &version)
{
    return WriteData(kInitVersionKey, version);
}

DBStatus DBReadWriter::TransactionRollBack()
{
    if (auto_oper_trans)
    {
        rocksdb::Status ret_status;
        if (!db_read_writer_.TransactionRollBack(ret_status))
        {
            ERRORLOG("transction rollback code:{} info:{}", ret_status.code(), ret_status.ToString());
            return DBStatus::DB_ERROR;
        }
    }
    return DBStatus::DB_SUCCESS;
}

DBStatus DBReadWriter::MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values)
{
    if (keys.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::vector<rocksdb::Slice> db_keys;
    std::vector<std::string> str;
    for(auto t : keys)
    {
        str.push_back(t);
    }

    std::vector<std::string> db_keys_str;
    for (auto key : keys)
    {
        db_keys_str.push_back(key);
        db_keys.push_back(db_keys_str.back());
    }
    std::vector<rocksdb::Status> ret_status;
    if (db_read_writer_.MultiReadData(db_keys, values, ret_status))
    {
        if (db_keys.size() != values.size())
        {
            return DBStatus::DB_ERROR;
        }
        return DBStatus::DB_SUCCESS;
    }
    else
    {
        for (auto status : ret_status)
        {
            if (status.IsNotFound())
            {
                return DBStatus::DB_NOT_FOUND;
            }
        }
    }
    return DBStatus::DB_ERROR;
}

DBStatus DBReadWriter::ReadData(const std::string &key, std::string &value)
{
    if (key.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    rocksdb::Status ret_status;
    if (db_read_writer_.ReadData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    else if (ret_status.IsNotFound())
    {
        value.clear();
        return DBStatus::DB_NOT_FOUND;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::MergeValue(const std::string &key, const std::string &value, bool firstOrLast)
{
    rocksdb::Status ret_status;
    if (db_read_writer_.MergeValue(key, value, ret_status, firstOrLast))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::RemoveMergeValue(const std::string &key, const std::string &value)
{
    rocksdb::Status ret_status;
    if (db_read_writer_.RemoveMergeValue(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::WriteData(const std::string &key, const std::string &value)
{
    rocksdb::Status ret_status;
    if (db_read_writer_.WriteData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::DeleteData(const std::string &key)
{
    rocksdb::Status ret_status;
    if (db_read_writer_.DeleteData(key, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
