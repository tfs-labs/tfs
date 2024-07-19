/**
 * *****************************************************************************
 * @file        txhelper.h
 * @brief       
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */

#ifndef _TXHELPER_H_
#define _TXHELPER_H_

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include <utils/json.hpp>
#include <google/protobuf/util/json_util.h>
#include <evmc/evmc.h>
#include "proto/ca_protomsg.pb.h"
#include "proto/transaction.pb.h"

class EvmHost;
/**
 * @brief       
 */
class TxHelper
{
public:

    struct Utxo
    {
        std::uint64_t value;
        std::string addr;
        std::string hash;
        std::uint32_t n;
    };

    class UtxoCompare
    {
    public:
        bool operator()(const Utxo& utxo1, const Utxo& utxo2) const
        {
            return utxo1.value < utxo2.value;
        }
    };

    typedef enum emPledgeType {
        kPledgeType_Unknown = -1,		// unknown
        kPledgeType_Node = 0,			// Node stake
    } pledgeType;

     typedef enum emInvestType {
        kInvestType_Unknown = -1,		// unknown
        kInvestType_NetLicence = 0,	    //NetLicence
    } InvestType;

    enum vrfAgentType
    {
        vrfAgentType_defalut = 0, 
        vrfAgentType_vrf,
        vrfAgentType_local ,
    };

    static const uint32_t kMaxVinSize;

    TxHelper() = default;
    ~TxHelper() = default;

    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       txUtxoHeight: 
     * @return      int 
     */
    static int GetTxUtxoHeight(const CTransaction &tx, uint64_t& txUtxoHeight);

    /**
     * @brief       Get the Tx Owner object
     * 
     * @param       tx: 
     * @return      std::vector<std::string> 
     */
    static std::vector<std::string> GetTxOwner(const CTransaction& tx);

    /**
     * @brief       Get the Utxos object
     * 
     * @param       address: 
     * @param       utxos: 
     * @return      int 
     */
    static int GetUtxos(const std::string & address, std::vector<TxHelper::Utxo>& utxos);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       height: 
     * @return      int 
     */
    static int Check(const std::vector<std::string>& fromAddr,uint64_t height);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       needUtxoAmount: 
     * @param       total: 
     * @param       setOutUtxos: 
     * @return      int 
     */
    static int FindUtxo(const std::vector<std::string>& fromAddr,const uint64_t needUtxoAmount,
						uint64_t& total,std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare>& setOutUtxos, bool isFindUtxo = false);

    static int SendConfirmUtxoHashReq(const std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare>& setOutUtxos);

    /**
     * @brief       Create a Tx Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateTxTransaction(const std::vector<std::string>& fromAddr,
                            const std::map<std::string, int64_t> & toAddr,
                            const std::string& encodedInfo,
                            uint64_t height,CTransaction& outTx,TxHelper::vrfAgentType &type,
                            Vrf & information, bool isFindUtxo = false);

    /**
     * @brief       Create a Stake Transaction object
     * 
     * @param       fromAddr: 
     * @param       stakeAmount: 
     * @param       height: 
     * @param       pledgeType: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @param       commission: commission percentage (0.05 - 0.2)
     * @return      int 
     */
    static int CreateStakeTransaction(const std::string & fromAddr,uint64_t stakeAmount, const std::string& encodedInfo, uint64_t height,
                        TxHelper::pledgeType pledgeType,CTransaction &outTx,
                        std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                        Vrf & information, double commissionRate, bool isFindUtxo = false);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       utxoHash: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreatUnstakeTransaction(const std::string& fromAddr,const std::string& utxoHash, const std::string& encodedInfo, uint64_t height,
                                        CTransaction &outTx, std::vector<TxHelper::Utxo> & outVin,
                                        TxHelper::vrfAgentType &type ,Vrf & information, bool isFindUtxo = false);

    /**
     * @brief       Create a Invest Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       investAmount: 
     * @param       height: 
     * @param       investType: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateInvestTransaction(const std::string & fromAddr,const std::string& toAddr,
                                    uint64_t investAmount, const std::string& encodedInfo, uint64_t height,
                                    TxHelper::InvestType investType,CTransaction & outTx,
                                    std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                    Vrf & information, bool isFindUtxo = false);

    /**
     * @brief       Create a Disinvest Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       utxoHash: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateDisinvestTransaction(const std::string& fromAddr,const std::string& toAddr,
                                        const std::string& utxoHash, const std::string& encodedInfo, uint64_t height,CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                        Vrf & information, bool isFindUtxo = false);

    /**
     * @brief       Create a Bonus Transaction object
     * 
     * @param       Addr: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateBonusTransaction(const std::string& Addr, const std::string& encodedInfo, uint64_t height,CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                        Vrf & information, bool isFindUtxo = false);
										
    static int MakeEvmDeployContractTransaction(uint64_t height, CTransaction &outTx,
                                                std::vector<std::string> &dirtyContract,
                                                TxHelper::vrfAgentType &type, Vrf &information, EvmHost &host,
                                                evmc_message &message, bool isFindUtxo, const std::string& encodedInfo,
                                                std::function<int(CTransaction &,std::string &)> signFunction);

    static int
    MakeEvmCallContractTransaction(uint64_t height, CTransaction &outTx, std::vector<std::string> &dirtyContract,
                                   TxHelper::vrfAgentType &type, Vrf &information, EvmHost &host,
                                   evmc_message &message, const std::string &strInput, const uint64_t contractTip,
                                   const uint64_t contractTransfer, const std::string &toAddr, bool isFindUtxo,
                                   const std::string& encodedInfo,
                                   std::function<int(CTransaction &,std::string &)> signFunction);


    /**
     * @brief       Create a Evm Deploy Contract Transaction object
     * 
     * @param       fromAddr: 
     * @param       OwnerEvmAddr: 
     * @param       code: 
     * @param       height: 
     * @param       contractInfo: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
	 
    static int CreateEvmDeployContractTransaction(uint64_t height, CTransaction &outTx, std::vector<std::string> &dirtyContract,
                                       TxHelper::vrfAgentType &type, Vrf &information, std::string &code,
                                       const std::string &from, uint64_t transfer, const std::string &to,  
                                       const std::string& encodedInfo, bool isFindUtxo = false, bool isRpc = false);

//    static int CreateWasmDeployContractTransaction(const std::string &fromaddr, const std::string &OwnerWasmAddr,
//                                    const std::string &code, uint64_t height,
//                                    CTransaction &outTx,TxHelper::vrfAgentType &type, Vrf &information);

    /**
     * @brief       Create a Evm Call Contract Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       strInput:
     * @param       OwnerEvmAddr: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @param       contractTip: 
     * @param       contractTransfer: 
     * @return      int 
     */
    static int
    CreateEvmCallContractTransaction(const std::string &from, const std::string &toAddr, const std::string &strInput,
                                     const std::string& encodedInfo,
                                     uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                     Vrf &information,
                                     const uint64_t contractTip, const uint64_t contractTransfer,
                                     std::vector<std::string> &dirtyContract, const std::string &to,
                                     bool isFindUtxo = false,bool isRpc = false);

//    static int CreateWasmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr, const std::string &txHash,
//                                const std::string &strInput, uint64_t height,CTransaction &outTx, TxHelper::vrfAgentType &type,
//                                Vrf &information,const uint64_t contractTip, const std::string &contractFunName);
    int RPC_CreateEvmCallContractTransaction(const std::string &from, const std::string &toAddr,
                                               const std::string &strInput,
                                               uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                               Vrf &information,
                                               const uint64_t contractTip, const uint64_t contractTransfer,
                                               std::vector<std::string> &dirtyContract, const std::string &to,
                                               bool isFindUtxo,bool isRpc);
    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       tx: 
     * @return      int 
     */
    static int AddMutilSign(const std::string & addr, CTransaction &tx);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       tx: 
     * @return      int 
     */
    static int AddVerifySign(const std::string & addr, CTransaction &tx);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       message: 
     * @param       signature: 
     * @param       pub: 
     * @return      int 
     */
    static int Sign(const std::string & addr, const std::string & message, 
                    std::string & signature, std::string & pub);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @return      true 
     * @return      false 
     */
    static bool IsNeedAgent(const std::vector<std::string> & fromAddr);

    /**
     * @brief       
     * 
     * @param       tx: 
     * @return      true 
     * @return      false 
     */
    static bool IsNeedAgent(const CTransaction &tx);

    /**
     * @brief       Whether the time gap with the current highly recent block is within timeout
     * 
     * @param       txTime: 
     * @param       timeout: 
     * @param       preHeight: 
     * @return      true 
     * @return      false 
     */
    static bool CheckTxTimeOut(const uint64_t & txTime, const uint64_t & timeout,const uint64_t & preHeight);

    /**
     * @brief       Get the Vrf Agent Type object
     * 
     * @param       tx: 
     * @param       preHeight: 
     * @return      TxHelper::vrfAgentType 
     */
    static TxHelper::vrfAgentType GetVrfAgentType(const CTransaction &tx, uint64_t &preHeight);

    /**
     * @brief       Get the Tx Start Identity object
     * 
     * @param       fromaddr: 
     * @param       height: 
     * @param       currentTime: 
     * @param       type: 
     */
    static void GetTxStartIdentity(const uint64_t &height,const uint64_t &currentTime,TxHelper::vrfAgentType &type);

    /**
    * @brief       Get the Eligible Nodes object
    * 
    * @return      std::string 
    */
    static std::string GetEligibleNodes();

    static std::string GetIdentityNodes();
};

#endif



