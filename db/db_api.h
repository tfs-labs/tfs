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
    
    /**
     * @brief       Obtain MutliSign address
     * 
     * @param       addresses:
     * @return      DBStatus
     */
    DBStatus GetMutliSignAddress(std::vector<std::string> &addresses);
    /**
     * @brief       Get utxo of MutliSign address
     * 
     * @param       address:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus GetMutliSignAddressUtxo(const std::string &address, std::vector<std::string> &utxos);
    /**
     * @brief       Get hash according to height range
     * 
     * @param       startHeight:
     * @param       endHeight:
     * @param       blockHashes:
     * @return      DBStatus
     */
    DBStatus GetBlockHashesByBlockHeight(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes);
    /**
     * @brief       Get blocks according to hash
     * 
     * @param       blockHashes:
     * @param       blocks:
     * @return      DBStatus
     */
    DBStatus GetBlocksByBlockHash(const std::vector<std::string> &blockHashes, std::vector<std::string> &blocks);
    /**
     * @brief       Get the height of the data block through the block hash
     * 
     * @param       blockHash:
     * @param       blockHeight:
     * @return      DBStatus
     */
    DBStatus GetBlockHeightByBlockHash(const std::string &blockHash, unsigned int &blockHeight);
    /**
     * @brief       Obtain the hash of a single block by the height of the data block
     * 
     * @param       blockHeight:
     * @param       hash:
     * @return      DBStatus
     */
    DBStatus GetBlockHashByBlockHeight(uint64_t blockHeight, std::string &hash);
    /**
     * @brief       Multiple block hashes are obtained by the height of the data block
     * 
     * @param       blockHeight:
     * @param       hashes:
     * @return      DBStatus
     */
    DBStatus GetBlockHashsByBlockHeight(uint64_t blockHeight, std::vector<std::string> &hashes);
    /**
     * @brief       Get data block information through block hash
     * 
     * @param       blockHash:
     * @param       block:
     * @return      DBStatus
     */
    DBStatus GetBlockByBlockHash(const std::string &blockHash, std::string &block);
    /**
     * @brief       Get Sum hash per global::ca::sum_hash_range heights
     * 
     * @param       height:
     * @param       sumHash:
     * @return      DBStatus
     */
    DBStatus GetSumHashByHeight(uint64_t height, std::string& sumHash);
    /**
     * @brief       The block hash to check is obtained by the block height
     * 
     * @param       blockHeight:
     * @param       sumHash:
     * @return      DBStatus
     */
    DBStatus GetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight, std::string &sumHash);
    /**
     * @brief       By block common hash height
     * 
     * @param       thousandNum:
     * @return      DBStatus
     */
    DBStatus GetTopThousandSumhash(uint64_t &thousandNum);
    /**
     * @brief       Get highest block
     * 
     * @param       blockHeight:
     * @return      DBStatus
     */
    DBStatus GetBlockTop(uint64_t &blockHeight);
    /**
     * @brief       Obtain the utxo hash through the address (there are multiple utxohashes)
     * 
     * @param       address:
     * @param       utxoHashs:
     * @return      DBStatus
     */
    DBStatus GetUtxoHashsByAddress(const std::string &address, std::vector<std::string> &utxoHashs);
    /**
     * @brief       Get utxo hash value from utxo hash
     * 
     * @param       utxoHash:
     * @param       address:
     * @param       balance:
     * @return      DBStatus
     */
    DBStatus GetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, std::string &balance);
    /**
     * @brief       Obtain transaction original data through transaction hash
     * 
     * @param       txHash:
     * @param       txRaw:
     * @return      DBStatus
     */
    DBStatus GetTransactionByHash(const std::string &txHash, std::string &txRaw);
    /**
     * @brief       Get block hash from transaction hash
     * 
     * @param       txHash:
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus GetBlockHashByTransactionHash(const std::string &txHash, std::string &blockHash);
    /**
     * @brief       Get block transaction by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       txRaw:
     * @return      DBStatus
     */
    DBStatus GetTransactionByAddress(const std::string &address, const uint32_t txNum, std::string &txRaw);
    /**
     * @brief       Get block hash from transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus GetBlockHashByAddress(const std::string &address, const uint32_t txNum, std::string &blockHash);
    /**
     * @brief       Obtain the highest transaction height through the transaction address
     * 
     * @param       address:
     * @param       txIndex:
     * @return      DBStatus
     */
    DBStatus GetTransactionTopByAddress(const std::string &address, unsigned int &txIndex);
    /**
     * @brief       Obtain account balance through transaction address
     * 
     * @param       address:
     * @param       balance:
     * @return      DBStatus
     */
    DBStatus GetBalanceByAddress(const std::string &address, int64_t &balance);
    /**
     * @brief       Obtain stake address
     * 
     * @param       addresses:
     * @return      DBStatus
     */
    DBStatus GetStakeAddress(std::vector<std::string> &addresses);
    /**
     * @brief       Get utxo of stake address
     * 
     * @param       address:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus GetStakeAddressUtxo(const std::string &address, std::vector<std::string> &utxos);
    /**
     * @brief       Get the Bonus addr
     * 
     * @param       bonusAddrs:
     * @return      DBStatus
     */
    DBStatus GetBonusaddr(std::vector<std::string> &bonusAddrs);
    /**
     * @brief       Get the investment stake address invest_ A:X_ Y_ Z
     * 
     * @param       bonusAddr: bonus address
     * @param       addresses:
     * @return      DBStatus
     */
    DBStatus GetInvestAddrsByBonusAddr(const std::string &bonusAddr, std::vector<std::string> &investAddr);
    /**
     * @brief       Obtain which nodes the investor has invested in
     * 
     * @param       address:
     * @param       nodes:
     * @return      DBStatus
     */
    DBStatus GetBonusAddrByInvestAddr(const std::string &address, std::vector<std::string> &nodes);
    /**
     * @brief       Get the utxo invest of the investment stake account_ A_ X:u1_ u2_ u3
     * 
     * @param       addr:
     * @param       address:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus GetBonusAddrInvestUtxosByBonusAddr(const std::string &addr,const std::string &address, std::vector<std::string> &utxos);
    /**
     * @brief       Get all Bonus transactions utxo
     * 
     * @param       period:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus GetBonusUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos);
    /**
     * @brief       Get all investment transactions utxo
     * 
     * @param       period:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus GetInvestUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos);
    /**
     * @brief       Get Number of signatures By period
     * 
     * @param       period:
     * @param       address:
     * @param       signNumber:
     * @return      DBStatus
     */
    DBStatus GetSignNumberByPeriod(const uint64_t &period, const std::string &address, uint64_t &signNumber);
    /**
     * @brief       Get Number of blocks By period
     * 
     * @param       period:
     * @param       blockNumber:
     * @return      DBStatus
     */
    DBStatus GetBlockNumberByPeriod(const uint64_t &period, uint64_t &blockNumber);
    /**
     * @brief       Set Addr of signatures By period
     * 
     * @param       period:
     * @param       SignAddrs:
     * @return      DBStatus
     */
    DBStatus GetSignAddrByPeriod(const uint64_t &period, std::vector<std::string> &SignAddrs);
    /**
     * @brief       Set Addr of tburnAmount By period
     * 
     * @param       period:
     * @param       burnAmount:
     * @return      DBStatus
     */
    DBStatus GetburnAmountByPeriod(const uint64_t &period, uint64_t &burnAmount);
    /**
     * @brief       Obtain the total amount of invest
     * 
     * @param       Total:
     * @return      DBStatus
     */
    DBStatus GetTotalInvestAmount(uint64_t &Total);
    /**
     * @brief
     * 
     * @param       Total:
     * @return      DBStatus
     */
    DBStatus GetM2(uint64_t &Total);
    /**
     * @brief
     * 
     * @param       totalBurn:
     * @return      DBStatus
     */
    DBStatus GetDM(uint64_t &totalBurn);
    /**
     * @brief       Gets the address deployed via the evm virtual machine
     * 
     * @param       deployerAddr:
     * @return      DBStatus
     */
    DBStatus GetAllEvmDeployerAddr(std::vector<std::string> &deployerAddr);
    /**
     * @brief       Gets the address deployed via the wasm virtual machine
     * 
     * @param       deployerAddr:
     * @return      DBStatus
     */
//    DBStatus GetAllWasmDeployerAddr(std::vector<std::string> &deployerAddr);
    /**
     * @brief       Get the deployment utxo by the deployment address
     * 
     * @param       deployerAddr:
     * @param       contractAddr:
     * @return      DBStatus
     */
    DBStatus GetContractAddrByDeployerAddr(const std::string &deployerAddr, std::vector<std::string> &contractAddr);

    DBStatus GetContractCodeByContractAddr(const std::string &contractAddr, std::string &contractCode);
    /**
     * @brief       Get the utxo of the contract deployment from the contract address
     * 
     * @param       contractAddr:
     * @param       contractDeployUtxo:
     * @return      DBStatus
     */
    DBStatus GetContractDeployUtxoByContractAddr(const std::string &contractAddr, std::string &contractDeployUtxo);
    /**
     * @brief       Get the latest utxo from the contract address
     * 
     * @param       contractAddr:
     * @param       Utxo:
     * @return      DBStatus
     */
    DBStatus GetLatestUtxoByContractAddr(const std::string &contractAddr, std::string &Utxo);
    /**
     * @brief       The mpt value is obtained by the mpt key
     * 
     * @param       mptKey:
     * @param       mptValue:
     * @return      DBStatus
     */
    DBStatus GetMptValueByMptKey(const std::string &mptKey, std::string &mptValue);
    /**
	 * @brief       Read the program version that initializes the database
	 *
	 * @param       version:
	 * @return      DBStatus
	 */
	DBStatus GetInitVer(std::string &version);
    /**
     * @brief       
     * 
     * @param       keys:
     * @param       values:
     * @return      DBStatus
     */
    virtual DBStatus MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);
    /**
     * @brief       
     * 
     * @param       key:
     * @param       value:
     * @return      DBStatus
     */
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
    /**
     * @brief       
     * 
     * @return      DBStatus
     */
    DBStatus ReTransactionInit();
    /**
     * @brief       
     * 
     * @return      DBStatus
     */
    DBStatus TransactionCommit();

    /**
     * @brief       Set the height of the data block through the block hash
     * 
     * @param       blockHash:
     * @param       blockHeight:
     * @return      DBStatus
     */
    DBStatus SetBlockHeightByBlockHash(const std::string &blockHash, const unsigned int blockHeight);
    /**
     * @brief       Remove the block height in the database through the block hash
     * 
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus DeleteBlockHeightByBlockHash(const std::string &blockHash);
    /**
     * @brief       Set the hash of data blocks through the block height (there may be multiple block hashes at the same height in case of concurrency)
     * 
     * @param       blockHeight:
     * @param       blockHash:
     * @param       isMainBlock:
     * @return      DBStatus
     */
    DBStatus SetBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash, bool isMainBlock = false);
    /**
     * @brief       Remove the hash of the block in the database by the block height
     * 
     * @param       blockHeight:
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus RemoveBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash);
    /**
     * @brief       Set block by block hash
     * 
     * @param       blockHash:
     * @param       block:
     * @return      DBStatus
     */
    DBStatus SetBlockByBlockHash(const std::string &blockHash, const std::string &block);
    /**
     * @brief       Remove the block inside the data block through the block hash
     * 
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus DeleteBlockByBlockHash(const std::string &blockHash);
    /**
     * @brief       Set Sum hash per global::ca::sum_hash_range heights
     * 
     * @param       height:
     * @param       sumHash:
     * @return      DBStatus
     */
    DBStatus SetSumHashByHeight(uint64_t height, const std::string& sumHash);
    /**
     * @brief       Remove Sum hash per global::ca::sum_hash_range heights
     * 
     * @param       height:
     * @return      DBStatus
     */
    DBStatus RemoveSumHashByHeight(uint64_t height);   
    /**
     * @brief       Check the block hash by the block height setting
     * 
     * @param       blockHeight:
     * @param       sumHash:
     * @return      DBStatus
     */
    DBStatus SetCheckBlockHashsByBlockHeight(const uint64_t &blockHeight ,const std::string &sumHash);
    /**
     * @brief       Check the block hash by block height deletion
     * 
     * @param       blockHeight:
     * @return      DBStatus
     */
    DBStatus RemoveCheckBlockHashsByBlockHeight(const uint64_t &blockHeight);
    /**
     * @brief       Set the block hash sum height
     * 
     * @param       thousandNum:
     * @return      DBStatus
     */
    DBStatus SetTopThousandSumhash(const uint64_t &thousandNum);
    /**
     * @brief       remove the block hash sum height
     * 
     * @param       thousandNum:
     * @return      DBStatus
     */
    DBStatus RemoveTopThousandSumhash(const uint64_t &thousandNum);   
    /**
     * @brief       Set highest block
     * 
     * @param       blockHeight:
     * @return      DBStatus
     */
    DBStatus SetBlockTop(const unsigned int blockHeight);
    /**
     * @brief       Set Stake address
     * 
     * @param       address:
     * @return      DBStatus
     */
    DBStatus SetMutliSignAddresses(const std::string &address);
    /**
     * @brief       Remove the stake address from the database
     * 
     * @param       address:
     * @return      DBStatus
     */
    DBStatus RemoveMutliSignAddresses(const std::string &address);
    /**
     * @brief       Setting up the utxo account of encumbered assets
     * 
     * @param       address:
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus SetMutliSignAddressUtxo(const std::string &address, const std::string &utxo);
    /**
     * @brief       Remove utxo from data
     * 
     * @param       address:
     * @param       utxos:
     * @return      DBStatus
     */
    DBStatus RemoveMutliSignAddressUtxo(const std::string &address, const std::string &utxos);
    /**
     * @brief       Set utxo hash by address
     * 
     * @param       address:
     * @param       utxoHash:
     * @return      DBStatus
     */
    DBStatus SetUtxoHashsByAddress(const std::string &address, const std::string &utxoHash);
    /**
     * @brief       Remove utxo hash by address
     * 
     * @param       address:
     * @param       utxoHash:
     * @return      DBStatus
     */
    DBStatus RemoveUtxoHashsByAddress(const std::string &address, const std::string &utxoHash);
    /**
     * @brief       Set utxo value by utxo hash
     * 
     * @param       utxoHash:
     * @param       address:
     * @param       balance:
     * @return      DBStatus
     */
    DBStatus SetUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance);
    /**
     * @brief       Remove utxo hash value by utxo hash
     * 
     * @param       utxoHash:
     * @param       address:
     * @param       balance:
     * @return      DBStatus
     */
    DBStatus RemoveUtxoValueByUtxoHashs(const std::string &utxoHash, const std::string &address, const std::string &balance);
    /**
     * @brief       Set transaction raw data through transaction hash
     * 
     * @param       txHash:
     * @param       txRaw:
     * @return      DBStatus
     */
    DBStatus SetTransactionByHash(const std::string &txHash, const std::string &txRaw);
    /**
     * @brief       Remove the original transaction data in the database through the transaction hash
     * 
     * @param       txHash:
     * @return      DBStatus
     */
    DBStatus DeleteTransactionByHash(const std::string &txHash);
    /**
     * @brief       Set block hash through transaction hash
     * 
     * @param       txHash:
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus SetBlockHashByTransactionHash(const std::string &txHash, const std::string &blockHash);
    /**
     * @brief       Remove the block hash in the database through the transaction hash
     * 
     * @param       txHash:
     * @return      DBStatus
     */
    DBStatus DeleteBlockHashByTransactionHash(const std::string &txHash);
    /**
     * @brief       Set block transaction by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       txRaw:
     * @return      DBStatus
     */
    DBStatus SetTransactionByAddress(const std::string &address, const uint32_t txNum, const std::string &txRaw);
    /**
     * @brief       Remove the transaction data in the database through the transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @return      DBStatus
     */
    DBStatus DeleteTransactionByAddress(const std::string &address, const uint32_t txNum);
    /**
     * @brief       Set block hash by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       blockHash:
     * @return      DBStatus
     */
    DBStatus SetBlockHashByAddress(const std::string &address, const uint32_t txNum, const std::string &blockHash);
    /**
     * @brief       Remove the block hash in the database through the transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @return      DBStatus
     */
    DBStatus DeleteBlockHashByAddress(const std::string &address, const uint32_t txNum);
    /**
     * @brief       Set the maximum transaction height through the transaction address
     * 
     * @param       address:
     * @param       txIndex:
     * @return      DBStatus
     */
    DBStatus SetTransactionTopByAddress(const std::string &address, const unsigned int txIndex);
    /**
     * @brief       Set account balance through transaction address
     * 
     * @param       address:
     * @param       balance:
     * @return      DBStatus
     */
    DBStatus SetBalanceByAddress(const std::string &address, int64_t balance);
    /**
     * @brief       Remove account balance through transaction address
     * 
     * @param       address:
     * @return      DBStatus
     */
    DBStatus DeleteBalanceByAddress(const std::string &address);
    /**
     * @brief       Set Stake address
     * 
     * @param       address:
     * @return      DBStatus
     */
    DBStatus SetStakeAddresses(const std::string &address);
    /**
     * @brief       Remove the stake address from the database
     * 
     * @param       address:
     * @return      DBStatus
     */
    DBStatus RemoveStakeAddresses(const std::string &address);
    /**
     * @brief       Set the bonus addr
     * 
     * @param       bonusAddr:
     * @return      DBStatus
     */
    DBStatus SetBonusAddr(const std::string &bonusAddr);
    /**
     * @brief       Remove the bonus addr
     * 
     * @param       bonusAddr:
     * @return      DBStatus
     */
    DBStatus RemoveBonusAddr(const std::string &bonusAddr);    
    /**
     * @brief       Set the investment address through the bonus address
     * 
     * @param       bonusAddr:       Address of bonus
     * @param       investAddr:    Address of investment
     * @return      DBStatus
     */
    DBStatus SetInvestAddrByBonusAddr(const std::string &bonusAddr, const std::string& investAddr);
    /**
     * @brief       Remove the investment stake address from the database
     * 
     * @param       bonusAddr:      Address of bonus
     * @param       investAddr:     Address of investment
     * @return      DBStatus
     */
    DBStatus RemoveInvestAddrByBonusAddr(const std::string &bonusAddr, const std::string& investAddr);
    /**
     * @brief       Set the bonus address through the investment address
     * 
     * @param       investAddr:     Address of investment
     * @param       bonusAddr:      Address of bonus
     * @return      DBStatus
     */
    DBStatus SetBonusAddrByInvestAddr(const std::string &investAddr, const std::string& bonusAddr);
    /**
     * @brief       Remove the investment stake address from the database
     * 
     * @param       investAddr:     Address of investment
     * @param       bonusAddr:      Address of bonus
     * @return      DBStatus
     */
    DBStatus RemoveBonusAddrByInvestAddr(const std::string &investAddr, const std::string& bonusAddr);
    /**
     * @brief       Set the utxo invest of the investment pledge account_ A_ X:u1_ u2_ u3
     * 
     * @param       bonusAddr:      Address of bonus
     * @param       investAddr:     Address of investment
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus SetBonusAddrInvestAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &investAddr, const std::string &utxo);
    /**
     * @brief       Remove utxo from investment stake data
     * 
     * @param       bonusAddr:      Address of bonus
     * @param       investAddr:     Address of investment
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus RemoveBonusAddrInvestAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &investAddr, const std::string &utxo);
    /**
     * @brief       
     *
     * @param       totalBurn:
     * @return      DBStatus
     */ 
    DBStatus SetDM(uint64_t &totalBurn);
    /**
     * @brief       Set the total amount of entrusted stake
     *
     * @param       totalInvest:
     * @return      DBStatus
     */ 
    DBStatus SetTotalInvestAmount(uint64_t &totalInvest);
    /**
     * @brief       Set total circulation
     *
     * @param       totalCirculation:
     * @return      DBStatus
     */ 
    DBStatus SetTotalCirculation(uint64_t &totalCirculation);
    /**
     * @brief       Setting up the utxo account of encumbered assets
     *
     * @param       stakeAddr:        Address of pledge
     * @param       utxo:
     * @return      DBStatus
     */ 
    DBStatus SetStakeAddressUtxo(const std::string &stakeAddr, const std::string &utxo);
    /**
     * @brief       Remove utxo from data
     *
     * @param       stakeAddr:        Address of pledge
     * @param       utxo:
     * @return      DBStatus
     */ 
    DBStatus RemoveStakeAddressUtxo(const std::string &stakeAddr, const std::string &utxo);
    /**
     * @brief       Set up a bonus transaction
     *
     * @param       period:
     * @param       utxo:
     * @return      DBStatus
     */ 
    DBStatus SetBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo);
    /**
     * @brief       Remove a bonus transaction
     *
     * @param       period:
     * @param       utxo:
     * @return      DBStatus
     */ 
    DBStatus RemoveBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo);
    /**
     * @brief       Set up an investment transaction
     *
     * @param       period:
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus SetInvestUtxoByPeriod(const uint64_t &period, const std::string &utxo);
    /**
     * @brief       Remove an investment transaction
     *
     * @param       period:
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus RemoveInvestUtxoByPeriod(const uint64_t &period, const std::string &utxo);
    /**
     * @brief       Set Number of signatures By period
     *
     * @param       period:
     * @param       address:
     * @param       signNumber:
     * @return      DBStatus
     */
    DBStatus SetSignNumberByPeriod(const uint64_t &period, const std::string &address, const uint64_t &signNumber);
    /**
     * @brief       Remove Number of signatures By period
     *
     * @param       period:
     * @param       address:
     * @return      DBStatus
     */
    DBStatus RemoveSignNumberByPeriod(const uint64_t &period, const std::string &address);
    /**
     * @brief       Set Number of blocks By period
     *
     * @param       period:
     * @param       blockNumber:
     * @return      DBStatus
     */
    DBStatus SetBlockNumberByPeriod(const uint64_t &period, const uint64_t &blockNumber);
    /**
     * @brief       Remove Number of blocks By period
     *
     * @param       period:
     * @return      DBStatus
     */
    DBStatus RemoveBlockNumberByPeriod(const uint64_t &period);
    /**
     * @brief       Set Addr of signatures By period
     *
     * @param       period:
     * @param       addr:
     * @return      DBStatus
     */
    DBStatus SetSignAddrByPeriod(const uint64_t &period, const std::string &addr);
    /**
     * @brief       Remove Addr of signatures By period
     *
     * @param       period:
     * @param       addr:
     * @return      DBStatus
     */
    DBStatus RemoveSignAddrberByPeriod(const uint64_t &period, const std::string &addr);
    /**
     * @brief       Set the burn amount according to period
     *
     * @param       period:
     * @param       burnAmount:
     * @return      DBStatus
     */
    DBStatus SetburnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount);
    /**
     * @brief       Remove the burn amount according to period
     *
     * @param       period:
     * @param       burnAmount:
     * @return      DBStatus
     */
    DBStatus RemoveburnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount);
    /**
     * @brief       Set the address of the evm virtual machine deployer
     *
     * @param       deployerAddr:
     * @return      DBStatus
     */
    DBStatus SetEvmDeployerAddr(const std::string &deployerAddr);
    /**
     * @brief       Remove the address of the evm virtual machine deployer
     *
     * @param       deployerAddr:
     * @return      DBStatus
     */
    DBStatus RemoveEvmDeployerAddr(const std::string &deployerAddr);
    /**
     * @brief       Set the address of the wasm virtual machine deployer
     *
     * @param       deployerAddr:
     * @return      DBStatus
     */
    DBStatus SetWasmDeployerAddr(const std::string &deployerAddr);
    /**
     * @brief       Remove the address of the wasm virtual machine deployer
     *
     * @param       deployerAddr:
     * @return      DBStatus
     */
    DBStatus RemoveWasmDeployerAddr(const std::string &deployerAddr);
    /**
     * @brief       The deployed utxo is set by the address of the deployer
     *
     * @param       deployerAddr:
     * @param       contractAddr:
     * @return      DBStatus
     */
    DBStatus SetContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr);
    /**
     * @brief       The deployed utxo is remove by the address of the deployer
     *
     * @param       deployerAddr:
     * @param       contractAddr:
     * @return      DBStatus
     */
    DBStatus RemoveContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr);

    DBStatus SetContractCodeByContractAddr(const std::string &contractAddr, const std::string &contractCode);

    DBStatus RemoveContractCodeByContractAddr(const std::string &contractAddr);
    /**
     * @brief       Set the utxo of the deployment contract via the contract address
     *
     * @param       contractAddr:
     * @param       contractDeployUtxo:
     * @return      DBStatus
     */
    DBStatus SetContractDeployUtxoByContractAddr(const std::string &contractAddr, const std::string &contractDeployUtxo);
    /**
     * @brief       Remove the utxo of the deployment contract via the contract address
     *
     * @param       contractAddr:
     * @param       contractDeployUtxo:
     * @return      DBStatus
     */
    DBStatus RemoveContractDeployUtxoByContractAddr(const std::string &contractAddr);
    /**
     * @brief       Set the latest utxo via the contract address
     *
     * @param       contractAddr:
     * @param       utxo:
     * @return      DBStatus
     */
    DBStatus SetLatestUtxoByContractAddr(const std::string &contractAddr, const std::string &utxo);
    /**
     * @brief       Remove the latest utxo via the contract address
     *
     * @param       contractAddr:
     * @return      DBStatus
     */
    DBStatus RemoveLatestUtxoByContractAddr(const std::string &contractAddr);
    /**
     * @brief       The mpt value is set by the mpt key
     *
     * @param       mptKey:
     * @param       mptValue:
     * @return      DBStatus
     */
    DBStatus SetMptValueByMptKey(const std::string &mptKey, const std::string &mptValue);
    /**
     * @brief       The mpt value is remove by the mpt key
     *
     * @param       mptKey:
     * @return      DBStatus
     */
    DBStatus RemoveMptValueByMptKey(const std::string &mptKey);
    /**
     * @brief       Record the program version that initializes the database
     *
     * @param       version:
     * @return      DBStatus
     */
    DBStatus SetInitVer(const std::string &version);

private:
    /**
     * @brief       
     *
     * @return      DBStatus
     */
    DBStatus TransactionRollBack();
    /**
     * @brief       
     *
     * @param       keys:
     * @param       values:
     * @return      DBStatus
     */
    virtual DBStatus MultiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);
    /**
     * @brief       
     *
     * @param       key:
     * @param       value:
     * @return      DBStatus
     */
    virtual DBStatus ReadData(const std::string &key, std::string &value);
    /**
     * @brief       
     *
     * @param       key:
     * @param       value:
     * @param       firstOrLast:
     * @return      DBStatus
     */
    DBStatus MergeValue(const std::string &key, const std::string &value, bool firstOrLast = false);
    /**
     * @brief       
     *
     * @param       key:
     * @param       value:
     * @return      DBStatus
     */
    DBStatus RemoveMergeValue(const std::string &key, const std::string &value);
    /**
     * @brief       
     *
     * @param       key:
     * @param       value:
     * @return      DBStatus
     */
    DBStatus WriteData(const std::string &key, const std::string &value);
    /**
     * @brief       
     *
     * @param       key:
     * @return      DBStatus
     */
    DBStatus DeleteData(const std::string &key);

    std::set<std::string> delete_keys_;

    RocksDBReadWriter db_read_writer_;
    bool auto_oper_trans;
};

#endif
