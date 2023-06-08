#ifndef _TFS_CA_INTERFACE_H_
#define _TFS_CA_INTERFACE_H_

#include "utils/ReturnAckCode.h"
#include "proto/interface.pb.h"
#include "net/msg_queue.h"
#include "proto/sdk.pb.h"


//  Get the block
int GetBlockReqImpl(const std::shared_ptr<GetBlockReq>& req, GetBlockAck & ack);
//  Get the balance
int GetBalanceReqImpl(const std::shared_ptr<GetBalanceReq>& req, GetBalanceAck & ack);
//  Stake list requests
int GetStakeListReqImpl(const std::shared_ptr<GetStakeListReq>& req, GetStakeListAck & ack);
//  List of investments
int GetInvestListReqImpl(const std::shared_ptr<GetInvestListReq>& req, GetInvestListAck & ack);
// utxo Get UTXO
int GetUtxoReqImpl(const std::shared_ptr<GetUtxoReq>& req, GetUtxoAck & ack);
//  Query all investment accounts and amounts on the investee node
int GetAllInvestAddressReqImpl(const std::shared_ptr<GetAllInvestAddressReq>& req, GetAllInvestAddressAck & ack);
//  Get all investable nodes
int GetAllStakeNodeListReqImpl(const std::shared_ptr<GetAllStakeNodeListReq>& req, GetAllStakeNodeListAck & ack);
//  Get a list of signatures
int GetSignCountListReqImpl(const std::shared_ptr<GetSignCountListReq>& req, GetSignCountListAck & ack);
//  Calculate the commission
int GetHeightReqImpl(const std::shared_ptr<GetHeightReq>& req, GetHeightAck & ack);
//  Check the current claim amount
int GetBonusListReqImpl(const std::shared_ptr<GetBonusListReq> & req, GetBonusListAck & ack);
//  Query whether the transaction is linked
int SendCheckTxReq(const std::shared_ptr<IsOnChainReq>& msg,  IsOnChainAck & ack);
// 
int GetRestInvestAmountReqImpl(const std::shared_ptr<GetRestInvestAmountReq>& msg,  GetRestInvestAmountAck & ack);

//  Get the block
int HandleGetBlockReq(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgdata);
//  Get the balance
int HandleGetBalanceReq(const std::shared_ptr<GetBalanceReq>& req, const MsgData & msgdata);
//   Get node information
int HandleGetNodeInfoReqReq(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgdata);
//  Stake list requests
int HandleGetStakeListReq(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgdata);
//  List of investments
int HandleGetInvestListReq(const std::shared_ptr<GetInvestListReq>& req, const MsgData & msgdata);
//  Failed transactions
// utxo Get UTXO
int HandleGetUtxoReq(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgdata);
//  Query all investment accounts and amounts on the investee node
int HandleGetAllInvestAddressReq(const std::shared_ptr<GetAllInvestAddressReq>& req, const MsgData & msgdata);
//  Get all investable nodes
int HandleGetAllStakeNodeListReq(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgdata);
//  Get a list of signatures
int HandleGetSignCountListReq(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgdata);
//  Calculate the commission
int HandleGetHeightReq(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgdata);
//  Check the current claim amount
int HandleGetBonusListReq(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgdata);
//  Query transaction chain up
int HandleIsOnChainReq(const std::shared_ptr<IsOnChainReq>& msg, const MsgData & msgdata);
// 
int HandleGetRestInvestAmountReq(const std::shared_ptr<GetRestInvestAmountReq>& req, const MsgData & msgdata);

void RegisterInterface();
int HandleGetSDKAllNeedReq(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgdata);
#endif