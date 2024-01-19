/**
 * *****************************************************************************
 * @file        transaction.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __CA_TRANSACTION__
#define __CA_TRANSACTION__

#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <regex>

#include "utils/base58.h"
#include "utils/cycliclist.hpp"
#include "global.h"
#include "proto/block.pb.h"
#include "proto/transaction.pb.h"
#include "proto/block.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "proto/interface.pb.h"
#include "net/msg_queue.h"
#include "../net/interface.h"
#include "ca/block_stroage.h"
#include "block_monitor.h"
#include "ca/txhelper.h"

/**
 * @brief       Get the Balance By Utxo object
 * 
 * @param       address: 
 * @param       balance: 
 * @return      int 
 */
int GetBalanceByUtxo(const std::string & address,uint64_t &balance);

typedef enum emTransactionType{
	kTransactionType_Unknown = -1,	// Unknown
	kTransactionType_Genesis = 0, 	// Genesis Deal
	kTransactionType_Tx,			// Normal trading
} TransactionType;

/**
 * @brief       Get the Transaction Type object
 * 
 * @param       tx: 
 * @return      TransactionType 
 */
TransactionType GetTransactionType(const CTransaction & tx);

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       msgdata: 
 * @return      int 
 */
int HandleTx( const std::shared_ptr<TxMsgReq>& msg, const MsgData& msgdata );

int HandleContractTx( const std::shared_ptr<ContractTxMsgReq>& msg, const MsgData& msgdata );


/**
 * @brief       
 * 
 * @param       msg: 
 * @param       outTx: 
 * @return      int 
 */
int DoHandleTx( const std::shared_ptr<TxMsgReq>& msg, CTransaction & outTx);
int DoHandleTx_V33_1(const std::shared_ptr<TxMsgReq> &msg, CTransaction &outTx);

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       msgdata: 
 * @return      int 
 */
int HandleBlock(const std::shared_ptr<BlockMsg>& msg, const MsgData& msgdata);

/**
 * @brief       
 * 
 * @param       block: 
 * @return      int 
 */
int AddBlockSign(CBlock &block);

/**
 * @brief       
 * 
 * @param       block: 
 * @return      int 
 */
int VerifyBlockSign(const CBlock &block);

/**
 * @brief       
 * 
 * @param       msg: 
 * @return      int 
 */
int DoHandleBlock(const std::shared_ptr<BlockMsg>& msg);
// int DoHandleBlock_V33_1(const std::shared_ptr<BlockMsg>& msg);

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       msgdata: 
 * @return      int 
 */
int HandleBuildBlockBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgdata );

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       msg: 
 * @param       nextNodes: 
 * @return      int 
 */
int FindSignNode(const CTransaction & tx, const std::shared_ptr<TxMsgReq> &msg, std::unordered_set<std::string> & nextNodes);

/**
 * @brief       Get the Block Packager object
 * 
 * @param       packager: 
 * @param       hash: 
 * @param       info: 
 * @return      int 
 */
int GetBlockPackager(std::string &packager,const std::string & hash,Vrf & info);



/**
 * @brief       Get the Contract Block Packager object
 * 
 * @param       packager: 
 * @param       txTime: 
 * @param       txHeight: 
 * @param       packager:
 * @param       info: 
 * @return      int 
 */

int GetVrfDataSourceByTime(const uint64_t& txTime, const uint64_t& txHeight, std::string &txHash, std::vector<std::string>& targetAddrs);

int CalculateThePackerByTime(const uint64_t& txTime, const uint64_t& txHeight, std::string& packager, std::string& proof, std::string &txHash);

int GetContractBlockPackager(const uint64_t& txTime, const uint64_t& txHeight, std::string& packager, Vrf& info);

int IsContractVrfVerifyNode(const CTransaction& tx, const uint64_t& height, const Vrf& vrfInfo);
/**
 * @brief       
 * 
 * @param       address: 
 * @param       stakeamount: 
 * @param       stakeType: 
 * @return      int 
 */
int SearchStake(const std::string &address, uint64_t &stakeamount,  global::ca::StakeType stakeType);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       vrfInfo: 
 * @return      int 
 */
int IsVrfVerifyNode(const CTransaction& tx, const Vrf& vrfInfo);

/**
 * @brief       
 * 
 * @param       identity: 
 * @param       msg: 
 * @return      int 
 */
int IsVrfVerifyNode(const std::string identity, const std::shared_ptr<TxMsgReq> &msg);

/**
 * @brief       
 * 
 * @param       tx: 
 * @return      TxHelper::vrfAgentType 
 */
TxHelper::vrfAgentType IsNeedAgent(const CTransaction & tx);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       msg: 
 * @return      int 
 */
int SendTxMsg(const CTransaction & tx, const std::shared_ptr<TxMsgReq>& msg);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       msg: 
 * @return      int 
 */
int UpdateTxMsg(CTransaction & tx, const std::shared_ptr<TxMsgReq> &msg);

/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       utxoHash: 
 * @param       stakedAmount: 
 * @return      int 
 */
int IsQualifiedToUnstake(const std::string& fromAddr, const std::string& utxoHash, uint64_t& stakedAmount);

/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       toAddr: 
 * @param       investAmount: 
 * @return      int 
 */
int CheckInvestQualification(const std::string& fromAddr, 
						const std::string& toAddr, uint64_t investAmount);

/**
 * @brief       
 * 
 * @param       BonusAddr: 
 * @param       txTime: 
 * @param       verifyAbnormal: 
 * @return      int 
 */
int CheckBonusQualification(const std::string& BonusAddr, const uint64_t& txTime, bool verifyAbnormal = true);

/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       toAddr: 
 * @param       utxoHash: 
 * @param       investedAmount: 
 * @return      int 
 */
int IsQualifiedToDisinvest(const std::string& fromAddr, const std::string& toAddr,
						const std::string& utxoHash, uint64_t& investedAmount);

/**
 * @brief       
 * 
 * @param       tx: 
 * @return      int 
 */
int VerifyTxTimeOut(const CTransaction &tx);
int VerifyTxTimeOut_V33_1(const CTransaction &tx);

/**
 * @brief       
 * 
 * @param       utxo: 
 * @return      true 
 * @return      false 
 */
bool IsMoreThan30DaysForUnstake(const std::string& utxo);

/**
 * @brief       
 * 
 * @param       utxo: 
 * @return      true 
 * @return      false 
 */
bool IsMoreThan1DayForDivest(const std::string& utxo);

/**
 * @brief       
 * 
 * @param       BonusAddr: 
 * @return      int 
 */
int VerifyBonusAddr(const std::string & BonusAddr);

/**
 * @brief       Get the Investment Amount And Duration object
 * 
 * @param       bonusAddr: 
 * @param       curTime: 
 * @param       zeroTime: 
 * @param       mpInvestAddr2Amount: 
 * @return      int 
 */
int GetInvestmentAmountAndDuration(const std::string & bonusAddr,const uint64_t &curTime,const uint64_t &zeroTime,std::map<std::string, std::pair<uint64_t,uint64_t>> &mpInvestAddr2Amount);

/**
 * @brief       Get the Total Circulation Yesterday object
 * 
 * @param       curTime: 
 * @param       totalCirculation: 
 * @return      int 
 */
int GetTotalCirculationYesterday(const uint64_t &curTime, uint64_t &totalCirculation);

/**
 * @brief       Get the Total Investment Yesterday object
 * 
 * @param       curTime: 
 * @param       totalinvest: 
 * @return      int 
 */
int GetTotalInvestmentYesterday(const uint64_t &curTime, uint64_t &totalinvest);

/**
 * @brief       Get the Total Burn Yesterday object
 * 
 * @param       curTime: 
 * @param       TotalBrun: 
 * @return      int 
 */
int GetTotalBurnYesterday(const uint64_t &curTime, uint64_t &TotalBrun);

/**
 * @brief       
 * 
 */
void NotifyNodeHeightChange();

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       msgdata: 
 * @return      int 
 */
int HandleMultiSignTxReq(const std::shared_ptr<MultiSignTxReq>& msg, const MsgData &msgdata );

/**
 * @brief       
 * 
 * @param       tx: 
 * @return      true 
 * @return      false 
 */
bool IsMultiSign(const CTransaction & tx);

/**
 * @brief       
 * 
 * @param       msg: 
 * @return      int 
 */
int VerifyTxMsgReq(const TxMsgReq & msg);

/**
 * @brief       
 * 
 * @param       vrfNodelist:
 * @param       tx: 
 * @param       randNum: 
 * @param       targetAddr: 
 * @return      int 
 */
int VerifyTxFlowSignNode(const vector<Node>& vrfNodelist, const uint64_t& vrfTxHeight, const CTransaction &tx , const double & randNum, std::string & targetAddr);

/**
 * @brief       
 * 
 * @param       tx: 
 * @return      int 
 */
int VerifyTxTimeOut(const CTransaction &tx);

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       msgdata: 
 * @return      int 
 */
int HandleAddBlockAck(const std::shared_ptr<BuildBlockBroadcastMsgAck>& msg, const MsgData& msgdata);

/**
 * @brief       
 * 
 * @param       txMsg: 
 * @param       tx: 
 * @return      int 
 */
int DropshippingTx(const std::shared_ptr<TxMsgReq> & txMsg,const CTransaction &tx);

int DropCallShippingTx(const std::shared_ptr<ContractTxMsgReq> & txMsg,const CTransaction &tx);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       gas: 
 * @return      int 
 */
int CalculateGas(const CTransaction &tx , uint64_t &gas );
// int GenerateGas(const CTransaction &tx, uint64_t voutSize, uint64_t &gas );

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       voutSize: 
 * @param       gas: 
 * @return      int 
 */
int GenerateGas(const CTransaction &tx, const uint64_t voutSize, uint64_t &gas);

/**
 * @brief       Set the Vrf object
 * 
 * @param       dest: 
 * @param       proof: 
 * @param       pub: 
 * @param       data: 
 */
void SetVrf(Vrf & dest,const std::string & proof, const std::string & pub,const std::string & data);

/**
 * @brief       Get the Vrfdata object
 * 
 * @param       vrf: 
 * @param       hash: 
 * @param       range: 
 * @return      int 
 */
int getVrfdata(const Vrf & vrf,std::string & hash, int & range);

/**
 * @brief       Get the Vrfdata object
 * 
 * @param       vrf: 
 * @param       hash: 
 * @param       targetAddr: 
 * @return      int 
 */
int getVrfdata(const Vrf &vrf, std::string &hash, std::string &targetAddr);

/**
 * @brief       
 * 
 * @param       block: 
 */
void ClearVRF(const CBlock &block);

int verifyVrfDataSource(const std::vector<Node>& vrfNodelist, const uint64_t& vrfTxHeight, bool txConsensusStatus = false);

bool CheckTxConsensusStatus(const CTransaction &tx);

int GetContractRootHash(const std::string& contractAddress, std::string& rootHash);

/**
 * @brief       
 * 
 * @param		vrfNodelist:
 * @param       tx: 
 * @param       outAddrs: 
 */
static void FilterConsensusNodeList(const vector<std::string>& vrfNodelist, const CTransaction & tx, std::vector<std::string> &outAddrs);

/**
 * @brief       
 * 
 * @param       endPos: 
 * @param       list: 
 * @param       targetAddrs: 
 * @return      int 
 */
static int FilterSendList(int & endPos,Cycliclist<std::string> &list, std::vector<std::string> &targetAddrs);

bool IsContractBlock(const CBlock & block);

static int CalculatePackNode(const std::vector<Node> &nodes, const double &randNum, const bool& isVerify, std::vector<std::string>& targetAddrs);

int FindContractPackNode(const std::string & txHash, std::string &targetAddr, Vrf& vrfInfo,std::set<std::string> & out_nodelist);

int VerifyContractPackNode(const std::string& dispatchNodeAddr, const double& randNum, const std::string& targetAddr,const std::vector<Node> & _vrfNodelist);

#endif
