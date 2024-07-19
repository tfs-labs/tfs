/**
 * *****************************************************************************
 * @file        interface.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _TFS_CA_INTERFACE_H_
#define _TFS_CA_INTERFACE_H_

#include "proto/sdk.pb.h"
#include "utils/return_ack.h"
#include "proto/interface.pb.h"


/**
 * @brief       Get the Block Req Impl object
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetBlockReqImpl(const std::shared_ptr<GetBlockReq>& req, GetBlockAck & ack);

/**
 * @brief       Get the Balance Req Impl object
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetBalanceReqImpl(const std::shared_ptr<GetBalanceReq>& req, GetBalanceAck & ack);


/**
 * @brief       Stake list requests
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetStakeListReqImpl(const std::shared_ptr<GetStakeListReq>& req, GetStakeListAck & ack);

/**
 * @brief       List of investments
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetInvestListReqImpl(const std::shared_ptr<GetInvestListReq>& req, GetInvestListAck & ack);

/**
 * @brief       utxo Get UTXO
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetUtxoReqImpl(const std::shared_ptr<GetUtxoReq>& req, GetUtxoAck & ack);

/**
 * @brief       Query all investment accounts and amounts on the investee node
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetAllInvestAddressReqImpl(const std::shared_ptr<GetAllInvestAddressReq>& req, GetAllInvestAddressAck & ack);

/**
 * @brief       Get all investable nodes
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetAllStakeNodeListReqImpl(const std::shared_ptr<GetAllStakeNodeListReq>& req, GetAllStakeNodeListAck & ack);

/**
 * @brief       Get a list of signatures
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetSignCountListReqImpl(const std::shared_ptr<GetSignCountListReq>& req, GetSignCountListAck & ack);

/**
 * @brief       Calculate the commission
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetHeightReqImpl(const std::shared_ptr<GetHeightReq>& req, GetHeightAck & ack);

/**
 * @brief       Check the current claim amount
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetBonusListReqImpl(const std::shared_ptr<GetBonusListReq> & req, GetBonusListAck & ack);

/**
 * @brief       Query whether the transaction is linked
 * 
 * @param       msg: 
 * @param       ack: 
 * @return      int 
 */
int SendConfirmTransactionReq(const std::shared_ptr<ConfirmTransactionReq>& msg,  ConfirmTransactionAck & ack);

/**
 * @brief       Get the Rest Invest Amount Req Impl object
 * 
 * @param       msg: 
 * @param       ack: 
 * @return      int 
 */
int GetRestInvestAmountReqImpl(const std::shared_ptr<GetRestInvestAmountReq>& msg,  GetRestInvestAmountAck & ack);

/**
 * @brief       Get the block
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetBlockReq(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgData);

/**
 * @brief       Get the balance
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetBalanceReq(const std::shared_ptr<GetBalanceReq>& req, const MsgData & msgData);

/**
 * @brief       Get node information
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetNodeInfoReqReq(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgData);

/**
 * @brief       Stake list requests
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetStakeListReq(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgData);

/**
 * @brief       List of investments
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetInvestListReq(const std::shared_ptr<GetInvestListReq>& req, const MsgData & msgData);
/**
 * @brief       utxo Get UTXO
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetUtxoReq(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgData);

/**
 * @brief       Query all investment accounts and amounts on the investee node
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetAllInvestAddressReq(const std::shared_ptr<GetAllInvestAddressReq>& req, const MsgData & msgData);

/**
 * @brief       Get all investable nodes
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetAllStakeNodeListReq(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgData);

/**
 * @brief      Get a list of signatures 
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetSignCountListReq(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgData);

/**
 * @brief       Calculate the commission
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetHeightReq(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgData);

/**
 * @brief       Check the current claim amount
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetBonusListReq(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgData);

/**
 * @brief       Query transaction chain up
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int HandleConfirmTransactionReq(const std::shared_ptr<ConfirmTransactionReq>& msg, const MsgData & msgData);

/**
 * @brief       
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetRestInvestAmountReq(const std::shared_ptr<GetRestInvestAmountReq>& req, const MsgData & msgData);

/**
 * @brief       
 * 
 */
void RegisterInterface();

/**
 * @brief       
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetSDKAllNeedReq(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgData);
#endif