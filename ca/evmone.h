/**
 * *****************************************************************************
 * @file        evmone.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef __CA_EVMONE_H__
#define __CA_EVMONE_H__

#include <string>
#include <unordered_map>

#include <evmc/evmc.hpp>
#include <tfs_host.hpp>

namespace Evmone
{
    /**
     * @brief       
     * 
     * @param       host: 
     * @param       jTxInfo: 
     * @param       TxType: 
     * @return      int 
     */
    int ContractInfoAdd(const TfsHost& host, nlohmann::json& jTxInfo, global::ca::TxType TxType);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       transferrings: 
     * @param       jTxInfo: 
     * @param       height: 
     * @param       gasCost: 
     * @param       outTx: 
     * @param       type: 
     * @param       info_: 
     * @param       contractTip: 
     * @return      int 
     */
    int FillCallOutTx(const std::string &fromAddr, const std::string &toAddr, const std::vector<TransferInfo> &transferrings,
                    const nlohmann::json &jTxInfo, uint64_t height, int64_t gasCost, CTransaction &outTx,TxHelper::vrfAgentType &type, 
                    Vrf &info_, const uint64_t& contractTip);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       transferrings: 
     * @param       gasCost: 
     * @param       outTx: 
     * @param       contractTip: 
     * @param       utxoHashs: 
     * @param       isGenSign: 
     * @return      int 
     */
    int GenCallOutTx(const std::string &fromAddr, const std::string &toAddr,const std::vector<TransferInfo> &transferrings, 
                    int64_t gasCost, CTransaction& outTx, const uint64_t& contractTip, std::vector<std::string>& utxoHashs, bool isGenSign = true);

    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       callOutTx: 
     * @return      int 
     */
    int VerifyUtxo(const CTransaction& tx, const CTransaction& callOutTx);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       transferrings: 
     * @param       jTxInfo: 
     * @param       gasCost: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       info_: 
     * @return      int 
     */
    int FillDeployOutTx(const std::string &fromAddr, const std::string &toAddr,const std::vector<TransferInfo> &transferrings,
                    const nlohmann::json &jTxInfo, int64_t gasCost, uint64_t height, CTransaction &outTx,TxHelper::vrfAgentType &type, Vrf &info_);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       ownerEvmAddr: 
     * @param       code_str: 
     * @param       strOutput: 
     * @param       host: 
     * @param       gasCost: 
     * @return      int 
     */
    int DeployContract(const std::string &fromAddr, const std::string &ownerEvmAddr, const std::string &code_str, std::string &strOutput, TfsHost &host, int64_t &gasCost);
    
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       ownerEvmAddr: 
     * @param       strDeployer: 
     * @param       strDeployHash: 
     * @param       strInput: 
     * @param       strOutput: 
     * @param       host: 
     * @param       gasCost: 
     * @param       contractTransfer: 
     * @return      int 
     */
    int CallContract(const std::string &fromAddr, const std::string &ownerEvmAddr, const std::string &strDeployer, 
                    const std::string &strDeployHash,const std::string &strInput, std::string &strOutput, 
                    TfsHost &host, int64_t &gasCost, const uint64_t& contractTransfer);

    /**
     * @brief       Get the Storage object
     * 
     * @param       host: 
     * @param       jStorage: 
     * @param       dirtyContract: 
     */
    void GetStorage(const TfsHost& host, nlohmann::json& jStorage, std::set<address>& dirtyContract);

    /**
     * @brief       Get the Selfdestructs object
     * 
     * @param       host: 
     * @param       jSelfdestructs: 
     */
    void GetSelfdestructs(const TfsHost& host, nlohmann::json& jSelfdestructs);

    /**
     * @brief       
     * 
     * @param       vecfromAddr: 
     * @param       txUtxo: 
     * @param       utxoHashs: 
     * @param       total: 
     * @param       isSign: 
     * @return      int 
     */
    int GenVin(const std::vector<std::string>& vecfromAddr,CTxUtxo * txUtxo, std::vector<std::string>& utxoHashs, uint64_t& total, bool isSign = true);
}

/**
 * @brief       
 * 
 * @param       msg: 
 * @param       code: 
 * @param       host: 
 * @param       strOutput: 
 * @param       gasCost: 
 * @return      int 
 */
int ExecuteByEvmone(const evmc_message &msg, const evmc::bytes &code, TfsHost &host, std::string &strOutput,
                    int64_t &gasCost);

/**
 * @brief       
 * 
 */
void TestAddressMapping();
#endif
