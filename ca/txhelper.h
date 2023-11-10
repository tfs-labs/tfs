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
#include "../proto/ca_protomsg.pb.h"
#include "../proto/transaction.pb.h"
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
        vrfAgentType_vrf ,
        vrfAgentType_local ,
        vrfAgentType_unknow,
    };

    static const uint32_t kMaxVinSize;

    TxHelper() = default;
    ~TxHelper() = default;

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
						uint64_t& total,std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare>& setOutUtxos);

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
                            uint64_t height,CTransaction& outTx,TxHelper::vrfAgentType &type,
                            Vrf & information);

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
    static int CreateStakeTransaction(const std::string & fromAddr,uint64_t stakeAmount,uint64_t height,
                        TxHelper::pledgeType pledgeType,CTransaction &outTx,
                        std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                        Vrf & information, double commission);

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
    static int CreatUnstakeTransaction(const std::string& fromAddr,const std::string& utxoHash,uint64_t height,
                                        CTransaction &outTx, std::vector<TxHelper::Utxo> & outVin,
                                        TxHelper::vrfAgentType &type ,Vrf & information);

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
                                    uint64_t investAmount,uint64_t height,
                                    TxHelper::InvestType investType,CTransaction & outTx,
                                    std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                    Vrf & information);

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
                                        const std::string& utxoHash,uint64_t height,CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                        Vrf & information);

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
    static int CreateBonusTransaction(const std::string& Addr,uint64_t height,CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,
                                        Vrf & information);

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
    static int CreateEvmDeployContractTransaction(const std::string &fromAddr, const std::string &OwnerEvmAddr,
                                       const std::string &code, uint64_t height,
                                       const nlohmann::json &contractInfo, 
                                       CTransaction &outTx,TxHelper::vrfAgentType &type, Vrf &information);

    /**
     * @brief       Create a Evm Call Contract Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       txHash: 
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
    static int CreateEvmCallContractTransaction(const std::string &fromAddr, const std::string &toAddr, 
                                    const std::string &txHash,const std::string &strInput, const std::string &OwnerEvmAddr, 
                                    uint64_t height,CTransaction &outTx, TxHelper::vrfAgentType &type, 
                                    Vrf &information,const uint64_t contractTip,const uint64_t contractTransfer);

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
    static void GetTxStartIdentity(const std::vector<std::string> &fromaddr,const uint64_t &height,const uint64_t &currentTime,TxHelper::vrfAgentType &type);
    
    /**
     * @brief       Get the Initiator Type object
     * 
     * @param       fromaddr: 
     * @param       type: 
     */
    static void GetInitiatorType(const std::vector<std::string> &fromaddr, TxHelper::vrfAgentType &type);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateTxTransaction(const std::vector<std::string>& fromAddr,
									const std::map<std::string, int64_t> & toAddr, void* ack);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       stakeAmount: 
     * @param       pledgeType: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateStakeTransaction(const std::string & fromAddr, uint64_t stakeAmount,  int32_t pledgeType, void* ack, double commission);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       utxoHash: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreatUnstakeTransaction(const std::string& fromAddr, const std::string& utxoHash, void* ack);
    
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       investAmount: 
     * @param       investType: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateInvestTransaction(const std::string & fromAddr,
                                    const std::string& toAddr,uint64_t investAmount, int32_t investType, void* ack);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       utxoHash: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateDisinvestTransaction(const std::string& fromAddr,
                                    const std::string& toAddr, const std::string& utxoHash, void* ack);

    /**
     * @brief       
     * 
     * @param       fromAddr: Initiator
     * @param       toAddr: Recipient
     * @param       amount: 
     * @param       multiSignPub: Multi-Sig address public key
     * @param       signAddrList: Record the federation node
     * @param       signThreshold: The number of consensus numbers for multiple signs
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateDeclareTransaction(const std::string & fromaddr, //Initiator
                                    const std::string & toAddr, //Recipient
                                    uint64_t amount, 
                                    const std::string & multiSignPub, //Multi-Sig address public key
                                    const std::vector<std::string> & signAddrList, // Record the federation node
                                    uint64_t signThreshold, //The number of consensus numbers for multiple signs
                                    void* ack);

    /**
     * @brief       
     * 
     * @param       Addr: 
     * @param       ack: 
     * @return      std::string 
     */
    static std::string ReplaceCreateBonusTransaction(const std::string& Addr, void* ack);

    /**
     * @brief       
     * 
     * @param       outTx: 
     * @param       height: 
     * @param       info: 
     * @param       type: 
     * @return      int 
     */
    static int SendMessage(CTransaction & outTx,int height,Vrf &info,TxHelper::vrfAgentType type);

    /**
     * @brief       Get the Eligible Nodes object
     * 
     * @return      std::string 
     */
    static std::string GetEligibleNodes();
    
};

#endif



