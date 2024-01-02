/**
 * *****************************************************************************
 * @file        http_api.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#include "../net/http_server.h"
#include "test.h"
#include "../net/unregister_node.h"
#include "net/httplib.h"

/**
 * @brief       
 * 
 */
void CaRegisterHttpCallbacks();

#ifndef NDEBUG

/**
 * @brief       
 * 
 * @param       req 
 * @param       res 
 */
void ApiGetLogLine(const Request & req,Response & res);
void GetAllSystemInfo(const Request &req, Response &res);
std::string ApiGetCpuInfo();
std::string ApiGetSystemInfo();
std::string GetNetRate();
std::string GetProcessInfo();
std::string ApiTime();
void ApiGetRealBandwidth(const Request &req, Response &res);
void ApiGetBlockInfo(const Request &req, Response &res);
void ApiGetTxInfo(const Request &req,Response &res);
void ApiPrintBlock(const Request & req, Response & res);
void ApiInfo(const Request & req, Response & res);
void ApiGetBlock(const Request & req, Response & res);
void ApiStartAutoTx(const Request & req, Response & res);
void ApiEndAutotx(const Request & req, Response & res);
void ApiStatusAutoTx(const Request & req, Response & res);
void ApiPub(const Request & req, Response & res);
void ApiFilterHeight(const Request &req, Response &res);
void ApiJsonRpc(const Request & req, Response & res);
void ApiTestEcho(const Request & req, Response & res);
void ApiGetRatesInfo(const Request &req,Response &res);
void ApiPrintAllBlocks(const Request &req,Response &res);
void ApiPrintCalc1000SumHash(const Request &req,Response &res);
void ApiSetCalc1000TopHeight(const Request &req,Response &res);
void ApiPrintContractBlock(const Request & req, Response & res);
#endif

bool ApiStatusAutoTxTest(Response & res);
void ApiEndAutoTxTest(Response & res);
void JsonRpcGetHeight(const Request &req,Response &res);
void JsonrpcGetBalance(const Request &req, Response &res) ;
void GetStakeUtxo(const Request & req, Response & res);
void GetDisinvestUtxo(const Request & req, Response & res);
void GetTransaction(const Request & req, Response & res);
void GetStake(const Request & req, Response & res);
void GetUnstake(const Request & req, Response & res);
void GetInvest(const Request & req, Response & res);
void GetDisinvest(const Request & req, Response & res);
void GetDeclare(const Request & req, Response & res);
void GetBonus(const Request & req, Response & res);
void GetRsaPub(const Request & req, Response & res);
void DeployContract(const Request & req, Response & res);
void CallContract(const Request & req, Response & res);
void SendMessage(const Request & req, Response & res);
void SendContractMessage(const Request & req,Response & res);
void GetIsOnChain(const Request & req, Response & res);
void GetDeployer(const Request & req, Response & res);
void GetDeployerUtxo(const Request & req, Response & res);

// void get_base58_from_evm(const Request & req, Response & res);
// void get_evmaddr_from_pubstr(const Request & req,Response & res);
void GetRestInvest(const Request & req, Response & res);
void ApiIp(const Request &req, Response & res);
void GetAllStakeNodeListAcknowledge(const Request & req,Response & res);
void ApiGetRatesInfo(const Request &req,Response &res);
// void TfsRpcParse(const Request &req,Response &res);


enum RPCERROR:int
{
    RPCVERSION_ERROR=847, 
    FIELD_REEOR,
    PASE_ERROR,
    NUMBER_ERROR,
    TRANSACTION_ERROR
};

#define tfsrpc
#ifdef tfsrpc

#endif
