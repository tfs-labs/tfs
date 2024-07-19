#ifndef _HTTP_RPC_
#define _HTTP_RPC_
#include "../../net/http_server.h"
#include "../../net/unregister_node.h"
#include "../../net/httplib.h"
#include "test.h"


void _CaRegisterHttpCallbacks();

/**
 * @brief  
 * @param  req: 
 * @param  res: 
 */
void _ApiJsonRpc(const Request & req, Response & res);
/**
 * @brief  
 * @param  req: 
 * @param  res: 
 */
void _ApiGetTxInfo(const Request &req,Response &res);

/**
 * @brief  Get the Stake object
 * @param  req: 
 * @param  res: 
 */
void _GetStake(const Request & req, Response & res);

/**
 * @brief  Get the Unstake object
 * @param  req: 
 * @param  res: 
 */
void _GetUnstake(const Request & req, Response & res);

/**
 * @brief  Get the Invest object
 * @param  req: 
 * @param  res: 
 */
void _GetInvest(const Request & req, Response & res);

/**
 * @brief  Get the Disinvest object
 * @param  req: 
 * @param  res: 
 */
void _GetDisinvest(const Request & req, Response & res);

/**
 * @brief  Get stake rates info and other messages
 * @param  req: 
 * @param  res: 
 */
void _ApiGetRatesInfo(const Request &req,Response &res);
/**
 * @brief  Get the Bonus object
 * @param  req: 
 * @param  res: 
 */
void _GetBonus(const Request & req, Response & res);


/**
 * @brief  Get the Disinvest Utxo object
 * @param  req: 
 * @param  res: 
 */
void _GetDisinvestUtxo(const Request & req, Response & res);
/**
 * @brief  Get the Stake Utxo object
 * @param  req: 
 * @param  res: 
 */
void _GetStakeUtxo(const Request & req, Response & res);
/**
 * @brief  Get the Transaction object
 * @param  req: 
 * @param  res: 
 */
void _GetTransaction(const Request & req, Response & res);
/**
 * @brief  
 * @param  req: 
 * @param  res: 
 */
void _DeployContract(const Request & req, Response & res);
/**
 * @brief  
 * @param  req: 
 * @param  res: 
 */
void _CallContract(const Request & req, Response & res);
/**
 * @brief  
 * @param  req: 
 * @param  res: 
 */
void _ApiGetAllBonusInfo(const Request &req,Response &res);
/**
 * @brief  Get the All Stake Node List Acknowledge object
 * @param  req: 
 * @param  res: 
 */
void _GetAllStakeNodeListnowledge(const Request & req,Response & res);
/**
 * @brief  Confirm whether a high level transaction is on the chain through the whole network
 * @param  req: 
 * @param  res: 
 */
void _ConfirmTransaction(const Request & req, Response & res);
/**
 * @brief  Get the BlockNumber object
 * @param  req: 
 * @param  res: 
 */
void _GetBlockNumber(const Request &req, Response &res);
/**
 * @brief  Get the Version object
 * @param  req: 
 * @param  res: 
 */
void _GetVersion(const Request &req, Response &res);
/**
 * @brief  Get the Balance object
 * @param  req: 
 * @param  res: 
 */
void _GetBalance(const Request &req, Response &res);
/**
 * @brief  Get the TransactionCount object
 * @param  req: 
 * @param  res: 
 */
void _GetBlockTransactionCountByHash(const Request &req, Response &res);
/**
 * @brief  Get the Accounts object
 * @param  req: 
 * @param  res: 
 */
void _GetAccounts(const Request &req, Response &res);
/**
 * @brief  Get the ChainId object
 * @param  req: 
 * @param  res: 
 */
void _GetChainId(const Request &req, Response &res);
/**
 * @brief  Get the PeerList object
 * @param  req: 
 * @param  res: 
 */
void _GetPeerList(const Request &req, Response &res) ;
/**
 * @brief  Get the Transaction object
 * @param  req: 
 * @param  res: 
 */
void _ApiIp(const Request &req, Response &res);
/**
 * @brief  Get the Transaction object
 * @param  req: 
 * @param  res: 
 */
void _ApiPub(const Request &req, Response &res);

void _ApiGetTransactionInfo(const Request &req,Response &res);
void _ApiGetBlockByHash(const Request &req,Response &res);
void _ApiGetBlockByHeight(const Request &req,Response &res);
void _ApiSendMessage(const Request &req, Response &res);
void _ApiSendContractMessage(const Request &req, Response &res);
void _ApiGetDelegateInfoReq(const Request &req, Response &res);

void _ApiGetBlock(const Request & req, Response & res);
void _ApiPrintBlock(const Request & req, Response & res);
void _GetRequesterIP(const Request &req, Response & res);
#endif
