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
                  const std::vector<transferInfo> &transferrings,
                  const nlohmann::json &jTxInfo, uint64_t height, int64_t gasCost, CTransaction &outTx,
                  TxHelper::vrfAgentType &type, Vrf &info_, const uint64_t& contractTip);
    int
    GenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<transferInfo> &transferrings, int64_t gasCost, 
                  CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign = true);

    int VerifyUtxo(const CTransaction& tx, const CTransaction& callOutTx);

    int FillDeployOutTx(const std::string &fromAddr, const std::string &toAddr,
                        const std::vector<transferInfo> &transferrings,
                        const nlohmann::json &jTxInfo, int64_t gasCost, uint64_t height, CTransaction &outTx,
                        TxHelper::vrfAgentType &type, Vrf &info_);

    int DeployContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &code_str,
                       std::string &strOutput, TfsHost &host, int64_t &gasCost);
    int
    CallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                 const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost, const uint64_t& contractTransfer);
    void getStorage(const TfsHost& host, nlohmann::json& jStorage, std::set<address>& dirtyContract);

    void getSelfdestructs(const TfsHost& host, nlohmann::json& jSelfdestructs);

    int genVin(const std::vector<std::string>& vecfromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign = true);
}

void test_address_mapping();
#endif
