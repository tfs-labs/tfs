#ifndef TFS_DB_DB_API_H_
#define TFS_DB_DB_API_H_

#include "db/rocksdb_read.h"
#include "db/rocksdb_read_write.h"
#include "proto/block.pb.h"
#include <string>
#include <vector>

bool DBInit(const std::string &path);
void DBDestory();

enum DBStatus
{
    DB_SUCCESS = 0,    //success
    DB_ERROR = 1,      //error
    DB_PARAM_NULL = 2, //Illegal parameter
    DB_NOT_FOUND = 3,  //Can't find
    DB_IS_EXIST = 4,
    DB_DESERIALIZATION_FAILED = 5
};
class DBReader
{
public:
    DBReader();
    ~DBReader() = default;
    DBReader(DBReader &&) = delete;
    DBReader(const DBReader &) = delete;
    DBReader &operator=(DBReader &&) = delete;
    DBReader &operator=(const DBReader &) = delete;
    
    //Obtain MutliSign address
    DBStatus GetMutliSignAddress(std::vector<std::string> &addresses);
    //Get utxo of MutliSign address
    DBStatus GetMutliSignAddressUtxo(const std::string &address, std::vector<std::string> &utxos);
    //Get hash according to height range
    DBStatus GetBlockHashesByBlockHeight(uint64_t start_height, uint64_t end_height, std::vector<std::string> &block_hashes);
    //Get blocks according to hash
    DBStatus GetBlocksByBlockHash(const std::vector<std::string> &block_hashes, std::vector<std::string> &blocks);
    //Get the height of the data block through the block hash
    DBStatus GetBlockHeightByBlockHash(const std::string &blockHash, unsigned int &blockHeight);
    //Obtain the hash of a single block by the height of the data block
    DBStatus GetBlockHashByBlockHeight(uint64_t blockHeight, std::string &hash);
    //Multiple block hashes are obtained by the height of the data block
    DBStatus GetBlockHashsByBlockHeight(uint64_t blockHeight, std::vector<std::string> &hashes);
    //Get data block information through block hash
    DBStatus GetBlockByBlockHash(const std::string &blockHash, std::string &block);
    //Get Sum hash per global::ca::sum_hash_range heights
    DBStatus GetSumHashByHeight(uint64_t height, std::string& sumHash);
    //Get Sum CheckBlockHash 
    DBStatus GetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight, std::string &sumHash);
    //Get ComHash
    DBStatus GetBlockComHashHeight(uint64_t &thousandNum);
    //Get highest block
    DBStatus GetBlockTop(uint64_t &blockHeight);
    //Obtain the utxo hash through the address (there are multiple utxohashes)
    DBStatus GetUtxoHashsByAddress(const std::string &address, std::vector<std::string> &utxoHashs);
    //Get utxo hash value from utxo hash
    DBStatus GetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, std::string &balance);
    //Obtain transaction original data through transaction hash
    DBStatus GetTransactionByHash(const std::string &txHash, std::string &txRaw);
    //Get block hash from transaction hash
    DBStatus GetBlockHashByTransactionHash(const std::string &txHash, std::string &blockHash);
    //Get block transaction by transaction address
    DBStatus GetTransactionByAddress(const std::string &address, const uint32_t txNum, std::string &txRaw);
    //Get block hash from transaction address
    DBStatus GetBlockHashByAddress(const std::string &address, const uint32_t txNum, std::string &blockHash);
    //Obtain the highest transaction height through the transaction address
    DBStatus GetTransactionTopByAddress(const std::string &address, unsigned int &txIndex);
    //Obtain account balance through transaction address
    DBStatus GetBalanceByAddress(const std::string &address, int64_t &balance);
    //Obtain stake address
    DBStatus GetStakeAddress(std::vector<std::string> &addresses);
    //Get utxo of stake address
    DBStatus GetStakeAddressUtxo(const std::string &address, std::vector<std::string> &utxos);
    //Get the Bonus addr
    DBStatus GetBonusaddr(std::vector<std::string> &addr);
    //Get the investment stake address invest_ A:X_ Y_ Z
    DBStatus GetInvestAddrsByBonusAddr(const std::string &addr, std::vector<std::string> &addresses);
    //Obtain which nodes the investor has invested in
    DBStatus GetBonusAddrByInvestAddr(const std::string &address, std::vector<std::string> &nodes);
    //Get the utxo invest of the investment stake account_ A_ X:u1_ u2_ u3
    DBStatus GetBonusAddrInvestUtxosByBonusAddr(const std::string &addr,const std::string &address, std::vector<std::string> &utxos);
    //Get all Bonus transactions utxo
    DBStatus GetBonusUtxoByPeriod(const uint64_t &Period, std::vector<std::string> &utxos);
    //Get all investment transactions utxo
    DBStatus GetInvestUtxoByPeriod(const uint64_t &Period, std::vector<std::string> &utxos);
    // Get Number of signatures By Period
    DBStatus GetSignNumberByPeriod(const uint64_t &Period, const std::string &address, uint64_t &SignNumber);
    // Get Number of blocks By Period
    DBStatus GetBlockNumberByPeriod(const uint64_t &Period, uint64_t &BlockNumber);
    // Set Addr of signatures By Period
    DBStatus GetSignAddrByPeriod(const uint64_t &Period, std::vector<std::string> &SignAddrs);
    // Set Addr of tburnAmount By Period
    DBStatus GetburnAmountByPeriod(const uint64_t &Period, uint64_t &burnAmount);

    //Obtain the total amount of stake
    DBStatus GetTotalInvestAmount(uint64_t &Total);
    //Obtain total circulation
    DBStatus GetM2(uint64_t &Total);

    DBStatus GetDM(uint64_t &Totalburn);

    DBStatus GetAllDeployerAddr(std::vector<std::string> &DeployerAddr);
    DBStatus GetDeployUtxoByDeployerAddr(const std::string &DeployerAddr,std::vector<std::string> &DeployUtxo);

    DBStatus GetContractDeployUtxoByContractAddr(const std::string &ContractAddr, std::string &ContractDeployUtxo);

    DBStatus GetLatestUtxoByContractAddr(const std::string &ContractAddr, std::string &Utxo);
    DBStatus GetMptValueByMptKey(const std::string &MptKey, std::string &MptValue);

    virtual DBStatus MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);
    virtual DBStatus ReadData(const std::string &key, std::string &value);

private:
    RocksDBReader db_reader_;
};

class DBReadWriter : public DBReader
{
public:
    DBReadWriter(const std::string &txn_name = std::string());
    virtual ~DBReadWriter();
    DBReadWriter(DBReadWriter &&) = delete;
    DBReadWriter(const DBReadWriter &) = delete;
    DBReadWriter &operator=(DBReadWriter &&) = delete;
    DBReadWriter &operator=(const DBReadWriter &) = delete;

    DBStatus ReTransactionInit();
    DBStatus TransactionCommit();

    //Set the height of the data block through the block hash
    DBStatus SetBlockHeightByBlockHash(const std::string &blockHash, const unsigned int blockHeight);
    //Remove the block height in the database through the block hash
    DBStatus DeleteBlockHeightByBlockHash(const std::string &blockHash);
    //Set the hash of data blocks through the block height (there may be multiple block hashes at the same height in case of concurrency)
    DBStatus SetBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash, bool is_mainblock = false);
    //Remove the hash of the block in the database by the block height
    DBStatus RemoveBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash);
    //Set block by block hash
    DBStatus SetBlockByBlockHash(const std::string &blockHash, const std::string &block);
    //Remove the block inside the data block through the block hash
    DBStatus DeleteBlockByBlockHash(const std::string &blockHash);
    //Set Sum hash per global::ca::sum_hash_range heights
    DBStatus SetSumHashByHeight(uint64_t height, const std::string& sumHash);
    //Remove Sum hash per global::ca::sum_hash_range heights
    DBStatus RemoveSumHashByHeight(uint64_t height);   
    //Set Sum CheckBlockHash 
    DBStatus SetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight ,const std::string &sumHash);
    //Remove Sum CheckBlockHash 
    DBStatus RemoveCheckBlockHashsByBlockHeight(const uint64_t &blockHeight);
    //Set ComHash
    DBStatus SetBlockComHashHeight(const uint64_t &thousandNum);
    //Remove ComHash
    DBStatus RemoveBlockComHashHeight(const uint64_t &thousandNum);   
    //Set highest block
    DBStatus SetBlockTop(const unsigned int blockHeight);
    //Set Stake address
    DBStatus SetMutliSignAddresses(const std::string &address);
    //Remove the stake address from the database
    DBStatus RemoveMutliSignAddresses(const std::string &address);
    //Setting up the utxo account of encumbered assets
    DBStatus SetMutliSignAddressUtxo(const std::string &address, const std::string &utxo);
    //Remove utxo from data
    DBStatus RemoveMutliSignAddressUtxo(const std::string &address, const std::string &utxos);
    //Set utxo hash by address
    DBStatus SetUtxoHashsByAddress(const std::string &address, const std::string &utxoHash);
    //Remove utxo hash by address
    DBStatus RemoveUtxoHashsByAddress(const std::string &address, const std::string &utxoHash);
    //Set utxo value by utxo hash
    DBStatus SetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance);
    //Remove utxo hash value by utxo hash
    DBStatus RemoveUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance);
    //Set transaction raw data through transaction hash
    DBStatus SetTransactionByHash(const std::string &txHash, const std::string &txRaw);
    //Remove the original transaction data in the database through the transaction hash
    DBStatus DeleteTransactionByHash(const std::string &txHash);
    //Set block hash through transaction hash
    DBStatus SetBlockHashByTransactionHash(const std::string &txHash, const std::string &blockHash);
    //Remove the block hash in the database through the transaction hash
    DBStatus DeleteBlockHashByTransactionHash(const std::string &txHash);
    //Set block transaction by transaction address
    DBStatus SetTransactionByAddress(const std::string &address, const uint32_t txNum, const std::string &txRaw);
    //Remove the transaction data in the database through the transaction address
    DBStatus DeleteTransactionByAddress(const std::string &address, const uint32_t txNum);
    //Set block hash by transaction address
    DBStatus SetBlockHashByAddress(const std::string &address, const uint32_t txNum, const std::string &blockHash);
    //Remove the block hash in the database through the transaction address
    DBStatus DeleteBlockHashByAddress(const std::string &address, const uint32_t txNum);
    //Set the maximum transaction height through the transaction address
    DBStatus SetTransactionTopByAddress(const std::string &address, const unsigned int txIndex);
    //Set account balance through transaction address
    DBStatus SetBalanceByAddress(const std::string &address, int64_t balance);
    //Set account balance through transaction address
    DBStatus DeleteBalanceByAddress(const std::string &address);
    //Set Stake address
    DBStatus SetStakeAddresses(const std::string &address);
    //Remove the stake address from the database
    DBStatus RemoveStakeAddresses(const std::string &address);
    //Set the bonus addr
    DBStatus SetBonusAddr(const std::string &addr);
    //Remove the bonus addr
    DBStatus RemoveBonusAddr(const std::string &addr);    
    //Set investment stake address_ A:X_ Y_ Z
    DBStatus SetInvestAddrByBonusAddr(const std::string &addr, const std::string& address);
    //Remove the investment stake address from the database
    DBStatus RemoveInvestAddrByBonusAddr(const std::string &addr, const std::string& address);
    //Set investment pledge address_ A:X_ Y_ Z
    DBStatus SetBonusAddrByInvestAddr(const std::string &address, const std::string& node);
    //Remove the investment stake address from the database
    DBStatus RemoveBonusAddrByInvestAddr(const std::string &address, const std::string& node);
    //Set the utxo invest of the investment pledge account_ A_ X:u1_ u2_ u3
    DBStatus SetBonusAddrInvestAddrUtxoByBonusAddr(const std::string &addr,const std::string &address, const std::string &utxo);
    //Remove utxo from investment stake data
    DBStatus RemoveBonusAddrInvestAddrUtxoByBonusAddr(const std::string &addr,const std::string &address, const std::string &utxo);

    DBStatus SetDM(uint64_t &Totalburn);
    //Set the total amount of entrusted stake
    DBStatus SetTotalInvestAmount(uint64_t &Totalinvest);
    //Set total circulation
    DBStatus SetTotalCirculation(uint64_t &TotalCirculation);
    //Setting up the utxo account of encumbered assets
    DBStatus SetStakeAddressUtxo(const std::string &address, const std::string &utxo);
    //Remove utxo from data
    DBStatus RemoveStakeAddressUtxo(const std::string &address, const std::string &utxos);

    //Set up a bonus transaction
    DBStatus SetBonusUtxoByPeriod(const uint64_t &Period, const std::string &utxo);
    //Remove a bonus transaction
    DBStatus RemoveBonusUtxoByPeriod(const uint64_t &Period, const std::string &utxo);

    //Set up an investment transaction
    DBStatus SetInvestUtxoByPeriod(const uint64_t &Period, const std::string &utxo);
    //Remove an investment transaction
    DBStatus RemoveInvestUtxoByPeriod(const uint64_t &Period, const std::string &utxo);

    // Set Number of signatures By Period
    DBStatus SetSignNumberByPeriod(const uint64_t &Period, const std::string &address, const uint64_t &SignNumber);
    // Remove Number of signatures By Period
    DBStatus RemoveSignNumberByPeriod(const uint64_t &Period, const std::string &address);

    //  Set Number of blocks By Period
    DBStatus SetBlockNumberByPeriod(const uint64_t &Period, const uint64_t &BlockNumber);
    //  Remove Number of blocks By Period
    DBStatus RemoveBlockNumberByPeriod(const uint64_t &Period);

    // Set Addr of signatures By Period
    DBStatus SetSignAddrByPeriod(const uint64_t &Period, const std::string &addr);
    // Remove Addr of signatures By Period
    DBStatus RemoveSignAddrberByPeriod(const uint64_t &Period, const std::string &addr);

    DBStatus SetburnAmountByPeriod(const uint64_t &Period, const uint64_t &burnAmount);
    DBStatus RemoveburnAmountByPeriod(const uint64_t &Period, const uint64_t &burnAmount);


    DBStatus SetDeployerAddr(const std::string &DeployerAddr);
    DBStatus RemoveDeployerAddr(const std::string &DeployerAddr);
    DBStatus SetDeployUtxoByDeployerAddr(const std::string &DeployerAddr,const std::string &DeployUtxo);
    DBStatus RemoveDeployUtxoByDeployerAddr(const std::string &DeployerAddr,const std::string &DeployUtxo);

    DBStatus SetContractDeployUtxoByContractAddr(const std::string &ContractAddr, const std::string &ContractDeployUtxo);

    DBStatus RemoveContractDeployUtxoByContractAddr(const std::string &ContractAddr);

    DBStatus SetLatestUtxoByContractAddr(const std::string &ContractAddr, const std::string &Utxo);
    DBStatus RemoveLatestUtxoByContractAddr(const std::string &ContractAddr);
    DBStatus SetMptValueByMptKey(const std::string &MptKey, const std::string &MptValue);
    DBStatus RemoveMptValueByMptKey(const std::string &MptKey);

    //Record the program version that initializes the database
    DBStatus SetInitVer(const std::string &version);

private:
    DBStatus TransactionRollBack();
    virtual DBStatus MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);
    virtual DBStatus ReadData(const std::string &key, std::string &value);
    DBStatus MergeValue(const std::string &key, const std::string &value, bool first_or_last = false);
    DBStatus RemoveMergeValue(const std::string &key, const std::string &value);
    DBStatus WriteData(const std::string &key, const std::string &value);
    DBStatus DeleteData(const std::string &key);

//    std::mutex key_mutex_;
    std::set<std::string> delete_keys_;

    RocksDBReadWriter db_read_writer_;
    bool auto_oper_trans;
};

#endif
