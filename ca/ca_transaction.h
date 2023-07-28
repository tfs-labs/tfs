#ifndef __CA_TRANSACTION__
#define __CA_TRANSACTION__

#include "ca_txhelper.h"
#include "utils/base58.h"
#include "utils/Cycliclist.hpp"
#include "ca_global.h"
#include "proto/block.pb.h"
#include "proto/transaction.pb.h"
#include "proto/block.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "proto/interface.pb.h"
#include "net/msg_queue.h"

#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <regex>

#include "../include/net_interface.h"
#include "ca/ca_blockstroage.h"

#include "ca_blockmonitor.h"
#include "ca/ca_txhelper.h"


int GetBalanceByUtxo(const std::string & address,uint64_t &balance);

typedef enum emTransactionType{
	kTransactionType_Unknown = -1,	// Unknown
	kTransactionType_Genesis = 0, 	// Genesis Deal
	kTransactionType_Tx,			// Normal trading
} TransactionType;

TransactionType GetTransactionType(const CTransaction & tx);

int HandleTx( const std::shared_ptr<TxMsgReq>& msg, const MsgData& msgdata );
int DoHandleTx( const std::shared_ptr<TxMsgReq>& msg, CTransaction & outTx);
int HandleBlock(const std::shared_ptr<BlockMsg>& msg, const MsgData& msgdata);

int AddBlockSign(CBlock &block);
int VerifyBlockSign(const CBlock &block);
int DoHandleBlock(const std::shared_ptr<BlockMsg>& msg);
int HandleBuildBlockBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgdata );

int FindSignNode(const CTransaction & tx, const std::shared_ptr<TxMsgReq> &msg,  const int & requiredNodeCount, std::set<std::string> & nextNodes);

int GetBlockPackager(std::string &packager,const std::string & hash,Vrf & info);

int SearchStake(const std::string &address, uint64_t &stakeamount,  global::ca::StakeType stakeType);

int IsVrfVerifyNode(const CTransaction& tx, const Vrf& vrfInfo);
int IsVrfVerifyNode(const std::string identity, const std::shared_ptr<TxMsgReq> &msg);

TxHelper::vrfAgentType IsNeedAgent(const CTransaction & tx);

int SendTxMsg(const CTransaction & tx, const std::shared_ptr<TxMsgReq>& msg);

int UpdateTxMsg(CTransaction & tx, const std::shared_ptr<TxMsgReq> &msg);

int IsQualifiedToUnstake(const std::string& fromAddr, 
						const std::string& utxo_hash, 
						uint64_t& staked_amount);

int CheckInvestQualification(const std::string& fromAddr, 
						const std::string& toAddr, 
						uint64_t invest_amount);

int CheckBonusQualification(const std::string& BonusAddr, const uint64_t& txTime, bool verify_abnormal = true);

int IsQualifiedToDisinvest(const std::string& fromAddr, 
						const std::string& toAddr,
						const std::string& utxo_hash, 
						uint64_t& invested_amount);

int VerifyTxTimeOut(const CTransaction &tx);

bool IsMoreThan30DaysForUnstake(const std::string& utxo);
bool IsMoreThan1DayForDivest(const std::string& utxo);
int VerifyBonusAddr(const std::string & BonusAddr);
int GetInvestmentAmountAndDuration(const std::string & bonusAddr,const uint64_t &cur_time,const uint64_t &zero_time,std::map<std::string, std::pair<uint64_t,uint64_t>> &mpInvestAddr2Amount);
int GetTotalCirculationYesterday(const uint64_t &cur_time, uint64_t &TotalCirculation);
int GetTotalInvestmentYesterday(const uint64_t &cur_time, uint64_t &Totalinvest);
int GetTotalBurnYesterday(const uint64_t &cur_time, uint64_t &TotalBrun);
void NotifyNodeHeightChange();

int HandleMultiSignTxReq(const std::shared_ptr<MultiSignTxReq>& msg, const MsgData &msgdata );

bool IsMultiSign(const CTransaction & tx);

int VerifyTxMsgReq(const TxMsgReq & msg);

int VerifyTxFlowSignNode(const CTransaction &tx , const double & rand_num, const uint64_t & top, const std::string & target_addr);

int VerifyTxTimeOut(const CTransaction &tx);

int HandleAddBlockAck(const std::shared_ptr<BuildBlockBroadcastMsgAck>& msg, const MsgData& msgdata);

int DropshippingTx(const std::shared_ptr<TxMsgReq> & txMsg,const CTransaction &tx);

int CalculateGas(const CTransaction &tx , uint64_t &gas );
// int GenerateGas(const CTransaction &tx, uint64_t voutSize, uint64_t &gas );
int GenerateGas(const CTransaction &tx, const uint64_t voutSize, uint64_t &gas);


void setVrf(Vrf & dest,const std::string & proof, const std::string & pub,const std::string & data);
int getVrfdata(const Vrf & vrf,std::string & hash, int & range);
int getVrfdata(const Vrf &vrf, std::string &hash, std::string &targetAddr);

void ClearVRF(const CBlock &block);

static void filterConsensusNodeList(const CTransaction & tx, std::vector<Node> &outAddrs);
static int filterSendList(int & end_pos,Cycliclist<std::string> &list, std::vector<std::string> &target_addrs);

#endif
