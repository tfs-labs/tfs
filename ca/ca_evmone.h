#ifndef __CA_EVMONE_H__
#define __CA_EVMONE_H__

#include <string>
#include <unordered_map>

#include <evmc/evmc.hpp>
#include <ca_TfsHost.hpp>

namespace Evmone
{
    int ContractInfoAdd(const TfsHost& host, nlohmann::json& jTxInfo, global::ca::TxType TxType);
    int
    FillCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<std::pair<std::string, uint64_t>> &transferrings,
                  const nlohmann::json &jTxInfo, uint64_t height, int64_t gasCost, CTransaction &outTx,
                  TxHelper::vrfAgentType &type, Vrf &info_);
    int FillDeployOutTx(const std::string &fromAddr, const std::string &toAddr,
                        const std::vector<std::pair<std::string, uint64_t>> &transferrings,
                        const nlohmann::json &jTxInfo, int64_t gasCost, uint64_t height, CTransaction &outTx,
                        TxHelper::vrfAgentType &type, Vrf &info_);

    int DeployContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &code_str,
                       std::string &strOutput, TfsHost &host, int64_t &gasCost);
    int
    CallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                 const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost);
    void getStorage(const TfsHost& host, nlohmann::json& jStorage);
}

void test_address_mapping();
#endif
