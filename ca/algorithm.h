/**
 * *****************************************************************************
 * @file        algorithm.h
 * @brief       
 * @date        2023-09-25
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef TFS_CA_ALGORITHM_H_
#define TFS_CA_ALGORITHM_H_

#include <utils/json.hpp>
#include "global.h"
#include "db/db_api.h"
#include "proto/block.pb.h"

namespace ca_algorithm
{
/**
 * @brief       Get the Abnormal Sign Addr List By Period object
 * 
 * @param       curTime: 
 * @param       abnormal_addr_list: 
 * @param       addr_sign_cnt: 
 * @return      int 
 */
int GetAbnormalSignAddrListByPeriod(uint64_t &curTime, std::map<std::string, double> &addr_percent, std::unordered_map<std::string, uint64_t> & addrSignCnt);

/**
 * @brief       Obtain the time (nanosecond) of pledge transaction with pledge limit of more than 500 according to the addressGet the Pledge Time By Addr object
                When the return value is less than 0, the function execution fails
                Equal to 0 means no pledge
                Greater than 0 means pledge time

 * 
 * @param       addr: 
 * @param       stakeType: 
 * @param       db_reader_ptr: 
 * @return      int64_t 
 */
int64_t GetPledgeTimeByAddr(const std::string &addr, global::ca::StakeType stakeType, DBReader *dbReaderPtr = nullptr);


/**
 * @brief       
 * 
 * @param       tx: 
 * @return      std::string 
 */
std::string CalcTransactionHash(CTransaction tx);


/**
 * @brief       
 * 
 * @param       block: 
 * @return      std::string 
 */
std::string CalcBlockHash(CBlock block);

/**
 * @brief       
 * 
 * @param       cblock: 
 * @return      std::string 
 */
std::string CalcBlockMerkle(CBlock cblock);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       turn_on_missing_block_protocol: 
 * @param       missing_utxo: 
 * @return      int 
 */
int DoubleSpendCheck(const CTransaction &tx, bool turn_on_missing_block_protocol, std::string* missing_utxo = nullptr);

/**
 * @brief       Verification transaction
 * 
 * @param       tx: 
 * @return      int 
 */
int MemVerifyTransactionTx(const CTransaction &tx);

/**
 * @brief       Verification transaction
 * 
 * @param       tx: 
 * @param       tx_height: 
 * @param       turn_on_missing_block_protocol: 
 * @param       verify_abnormal: 
 * @return      int 
 */
int VerifyTransactionTx(const CTransaction &tx, uint64_t txHeight, bool turnOnMissingBlockProtocol = false, bool verifyAbnormal = true);

/**
 * @brief       Verification transaction
 * 
 * @param       tx: 
 * @param       tx_height: 
 * @param       turn_on_missing_block_protocol: 
 * @param       verify_abnormal: 
 * @return      int 
 */
int VerifyTransactionTx_V33_1(const CTransaction &tx, uint64_t txHeight, bool turnOnMissingBlockProtocol = false, bool verifyAbnormal = true);

/**
 * @brief       
 * 
 * @param       block: 
 * @return      int 
 */
int VerifyPreSaveBlock(const CBlock &block);

/**
 * @brief       
 * 
 * @param       sign: 
 * @param       serHash: 
 * @return      int 
 */
int VerifySign(const CSign & sign, const std::string & serHash);

/**
 * @brief       Check block
 * 
 * @param       block: 
 * @param       isVerify: 
 * @param       blockStatus: 
 * @return      int 
 */
int MemVerifyBlock(const CBlock& block, bool isVerify = true, BlockStatus* blockStat = nullptr);

bool VerifyDirtyContract(const std::vector<std::string> &verifyCalledContract, const std::vector<std::string> &calledContract);

int VerifyContractStorage(const nlohmann::json& txInfo, const nlohmann::json& expectedTxInfo);

int VerifyContractBlock(const CBlock &block,BlockMsg * msg = nullptr);
int verifyContractDependenciesTx(const std::map<std::string, CTransaction>& ContractTxs, std::map<std::string,std::vector<std::string>>& dependTx, const CBlock &block, nlohmann::json& blockData);
/**
 * @brief       Check block
 * 
 * @param       block: 
 * @param       turnOnMissingBlockProtocol: 
 * @param       verifyAbnormal: 
 * @param       isVerify: 
 * @param       blockStatus: 
 * @return      int 
 */
int VerifyBlock(const CBlock &block, bool turnOnMissingBlockProtocol = false, bool verifyAbnormal = true, bool isVerify = true, BlockStatus* blockStatus = nullptr,BlockMsg* msg = nullptr);
// int VerifyBlock(const CBlock &block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean, bool verify_abnormal = true);
int VerifyBlock_V33_1(const CBlock &block, bool turnOnMissingBlockProtocol = false, bool verifyAbnormal = true, bool isVerify = true, BlockStatus* blockStatus = nullptr);
/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       block: 
 * @param       saveType: 
 * @param       obtainMean: 
 * @return      int 
 */
int SaveBlock(DBReadWriter &dbWriter, const CBlock &block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean);


/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       block: 
 * @param       saveType: 
 * @param       obtainMean: 
 * @return      int 
 */
int SaveBlock_V33_1(DBReadWriter &dbWriter, const CBlock &block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean);

/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       blockHash: 
 * @return      int 
 */
int DeleteBlock(DBReadWriter &dbWriter, const std::string &blockHash);


/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       blockHash: 
 * @return      int 
 */
int DeleteBlock_V33_1(DBReadWriter &dbWriter, const std::string &blockHash);

/**
 * @brief       When calling, pay attention not to have too much difference between the height and the maximum height. The memory occupation is too large, and the process is easy to be killed
                Rollback to specified height
 * 
 * @param       height: 
 * @return      int 
 */
int RollBackToHeight(uint64_t height);

/**
 * @brief       Rollback specified hash
 * 
 * @param       blockHash: 
 * @return      int 
 */
int RollBackByHash(const std::string &blockHash);

/**
 * @brief       
 * 
 * @param       tx: 
 */
void PrintTx(const CTransaction &tx);

/**
 * @brief       
 * 
 * @param       block: 
 */
void PrintBlock(const CBlock &block);

/**
 * @brief       Calculate the pledge rate and obtain the rate of return
 * 
 * @param       curTime: 
 * @param       bonusAddr: 
 * @param       vlaues: 
 * @return      int 
 */
int CalcBonusValue(uint64_t &curTime, const std::string &bonusAddr,std::map<std::string, uint64_t> & vlaues);

/**
 * @brief       
 * 
 * @return      int 
 */
int CalcBonusValue();


/**
 * @brief       Get the Inflation Rate object
 * 
 * @param       curTime: 
 * @param       StakeRate: 
 * @param       InflationRate: 
 * @return      int 
 */
int GetInflationRate(const uint64_t &curTime, const uint64_t &&StakeRate, double &InflationRate);    

/**
 * @brief       Get the Sum Hash Ceiling Height object
 * 
 * @param       height: 
 * @return      uint64_t Ceiling Height
 */
uint64_t GetSumHashCeilingHeight(uint64_t height);

/**
 * @brief       Get the Sum Hash Floor Height object
 * 
 * @param       height: 
 * @return      uint64_t Floor Height
 */
uint64_t GetSumHashFloorHeight(uint64_t height);

/**
 * @brief       
 * 
 * @param       blockHeight: 
 * @param       dbWriter: 
 * @return      int 
 */
int CalcHeightsSumHash(uint64_t blockHeight, DBReadWriter &dbWriter);

/**
 * @brief       
 * 
 * @param       blockHeight: 
 * @param       dbWriter: 
 * @param       back_hask: 
 * @return      int 
 */
int Calc1000HeightsSumHash(uint64_t blockHeight, DBReadWriter &dbWriter, std::string& back_hask);

/**
 * @brief       
 * 
 * @param       startHeight: 
 * @param       endHeight: 
 * @param       dbWriter: 
 * @param       sumHash: 
 * @return      true 
 * @return      false 
 */
bool CalculateHeightSumHash(uint64_t startHeight, uint64_t endHeight, DBReadWriter &dbWriter, std::string &sumHash);


/**
 * @brief       Get the Commission Percentage object
 * 
 * @param       addr: base58
 * @param       commission: Commission Percentage
 * @return      int 0 success
 */
int GetCommissionPercentage(const std::string& addr, double& commission);

int GetCallContractFromAddr(const CTransaction& transaction, bool isMultiSign, std::string& fromAddr);
}; // namespace ca_algorithm

#endif
