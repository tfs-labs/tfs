/**
 * *****************************************************************************
 * @file        evm.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __RPC_EVM__
#define  __RPC_EVM__
#include "tfs_host.hpp"
#include <string>



namespace rpc_evm
{

      /**
       * @brief       
       * 
       */
      int RpcGenCallOutTx(const std::string &fromAddr, const std::string &toAddr,
                  const std::vector<TransferInfo> &transFerrings, int64_t gasCost, 
                  CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign=true);

      /**
       * @brief       
       * 
       * @param       vecFromAddr 
       * @param       txUtxo 
       * @param       utxoHashs 
       * @param       total 
       * @param       isSign 
       * @return      int 
       */
      int RpcGenVin(const std::vector<std::string>& vecFromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign);

      /**
       * @brief       
       * 
       */
      int RpcFillOutTx(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType txType,
          const std::vector<TransferInfo> &transFerrings, const nlohmann::json &jTxInfo,
          uint64_t height, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info, const uint64_t& contractTip);
      
      /**
       * @brief       
       * 
       */
      int RpcCreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                                 const std::string &code, uint64_t height,
                                                 const nlohmann::json &contractInfo, CTransaction &outTx,
                                                 TxHelper::vrfAgentType &type, Vrf &info);

      /**
       * @brief       
       * 
       */
      int RpcCreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr,
                                               const std::string &txHash,
                                               const std::string &strInput, const std::string &OwnerEvmAddr,
                                               uint64_t height,
                                               CTransaction &outTx, TxHelper::vrfAgentType &type, Vrf &info,
							      const uint64_t contractTip,const uint64_t contractTransfer,bool istochain);

      /**
       * @brief       
       * 
       * @param       fromAddr 
       * @param       OwnerEvmAddr 
       * @param       strDeployer 
       * @param       strDeployHash 
       * @param       strInput 
       * @param       strOutput 
       * @param       host 
       * @param       gasCost 
       * @param       contractTransfer 
       * @return      int 
       */
      int RpcECallContract(const std::string &fromAddr, const std::string &OwnerEvmAddr, const std::string &strDeployer, const std::string &strDeployHash,
                     const std::string &strInput, std::string &strOutput, TfsHost &host, int64_t &gasCost, const uint64_t& contractTransfer);
}
#endif