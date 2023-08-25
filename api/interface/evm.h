#ifndef __RPC_EVM__
#define  __RPC_EVM__
#include "ca_TfsHost.hpp"
#include <string>

namespace rpc_evm{
    int rpc_GenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<transferInfo> &transferrings, int64_t gasCost, 
                  CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign=true);

    int rpc_genVin(const std::vector<std::string>& vecfromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign);

    int rpc_FillOutTx(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType tx_type,
          const std::vector<transferInfo> &transferrings, const nlohmann::json &jTxInfo,
          uint64_t height, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info_, const uint64_t& contractTip);
    int rpc_CreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                                 const std::string &code, uint64_t height,
                                                 const nlohmann::json &contractInfo, CTransaction &outTx,
                                                 TxHelper::vrfAgentType &type, Vrf &info_);

    int rpc_CreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr,
                                               const std::string &txHash,
                                               const std::string &strInput, const std::string &OwnerEvmAddr,
                                               uint64_t height,
                                               CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info_,
											   const uint64_t contractTip,const uint64_t contractTransfer);


}
#endif