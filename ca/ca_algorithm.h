#ifndef TFS_CA_ALGORITHM_H_
#define TFS_CA_ALGORITHM_H_

#include "ca_global.h"
#include "db/db_api.h"
#include "proto/block.pb.h"

namespace ca_algorithm
{
//Get the abnormal account number of the previous day
int GetAbnormalSignAddrListByPeriod(uint64_t &cur_time, std::vector<std::string> &abnormal_addr_list, std::unordered_map<std::string, uint64_t> & addr_sign_cnt);
//Obtain the time (nanosecond) of pledge transaction with pledge limit of more than 500 according to the address
//When the return value is less than 0, the function execution fails
//Equal to 0 means no pledge
//Greater than 0 means pledge time
int64_t GetPledgeTimeByAddr(const std::string &addr, global::ca::StakeType stakeType, DBReader *db_reader_ptr = nullptr);

std::string CalcTransactionHash(CTransaction tx);

std::string CalcBlockHash(CBlock block);

std::string CalcBlockMerkle(CBlock cblock);

int DoubleSpendCheck(const CTransaction &tx, bool turn_on_missing_block_protocol, std::string* missing_utxo = nullptr);
//Verification transaction
int MemVerifyTransactionTx(const CTransaction &tx);
//Verification transaction
int VerifyTransactionTx(const CTransaction &tx, uint64_t tx_height, bool turn_on_missing_block_protocol = false, bool verify_abnormal = true);

//Check block
int MemVerifyBlock(const CBlock& block, bool isVerify = true);
//Check block
int VerifyBlock(const CBlock &block, bool turn_on_missing_block_protocol = false, bool verify_abnormal = true, bool isVerify = true);
int SaveBlock(DBReadWriter &db_writer, const CBlock &block, global::ca::SaveType saveType, global::ca::BlockObtainMean obtainMean);
int DeleteBlock(DBReadWriter &db_writer, const std::string &block_hash);

//When calling, pay attention not to have too much difference between the height and the maximum height. The memory occupation is too large, and the process is easy to be killed
//Rollback to specified height
int RollBackToHeight(uint64_t height);
//Rollback specified hash
int RollBackByHash(const std::string &block_hash);

void PrintTx(const CTransaction &tx);
void PrintBlock(const CBlock &block);

//Calculate the pledge rate and obtain the rate of return
int CalcBonusValue(uint64_t &cur_time, const std::string &bonusAddr,std::map<std::string, uint64_t> & vlaues);
int CalcBonusValue();

//Get annual rate of return
int GetInflationRate(const uint64_t &cur_time, const uint64_t &&StakeRate, double &InflationRate);    

uint64_t GetSumHashCeilingHeight(uint64_t height);
uint64_t GetSumHashFloorHeight(uint64_t height);

int CalcHeightsSumHash(uint64_t block_height, DBReadWriter &db_writer);
}; // namespace ca_algorithm

#endif
