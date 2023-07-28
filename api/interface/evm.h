#ifndef __interace_evm_
#define __interace_evm_
#include <string>
#include "ca/ca_TfsHost.hpp"
#include <tuple>

namespace interface_evm
{
    int ContractInfoAdd(const TfsHost& host, nlohmann::json& jTxInfo, global::ca::TxType TxType);
    int
    FillCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<std::pair<std::string, uint64_t>> &transferrings,
                  const nlohmann::json &jTxInfo, uint64_t height, int64_t gasCost, CTransaction &outTx,
                  TxHelper::vrfAgentType &type, Vrf &info_,void *ack);
    int FillDeployOutTx(const std::string &fromAddr, const std::string &toAddr,
                        const std::vector<std::pair<std::string, uint64_t>> &transferrings,
                        const nlohmann::json &jTxInfo, int64_t gasCost, uint64_t height, CTransaction &outTx,
                        TxHelper::vrfAgentType &type, Vrf &info_,void *ack);


    int DeployContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &code_str,
                       std::string &strOutput, TfsHost &host, int64_t &gasCost);
    int
    CallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                 const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost);
    void getStorage(const TfsHost& host, nlohmann::json& jStorage);

    int CreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                                 const std::string &code, uint64_t height,
                                                 const nlohmann::json &contractInfo, CTransaction &outTx,
                                                 TxHelper::vrfAgentType &type, Vrf &info_,void * ack);
   std::pair<int,std::string> ReplaceCreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr,
                                               const std::string &txHash,
                                               const std::string &strInput, const std::string &OwnerEvmAddr,
                                               uint64_t height,
                                               CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info_,void * ack);
}



#endif