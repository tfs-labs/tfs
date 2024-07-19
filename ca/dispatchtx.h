#ifndef __DISPATCHER_H_
#define __DISPATCHER_H_
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>

#include "ca/global.h"
#include "ca/interface.h"
#include "ca/transaction.h"
#include "ca/transaction_cache.h"

class ContractDispatcher{

    public:
        ContractDispatcher() = default;
        ~ContractDispatcher() = default;
        /**
        * @brief       
        * 
        * @param       contractHash: 
        * @param       dependentContracts: 
        */
        void AddContractInfo(const std::string& contractHash, const std::vector<std::string>& dependentContracts);
        /**
        * @brief       
        * 
        * @param       contractHash: 
        * @param       msg: 
        */
        void AddContractMsgReq(const std::string& contractHash, const ContractTxMsgReq &msg);
        /**
        * @brief       
        */
        void Process();

        /**
        * @brief       
        * 
        * @param       v1: 
        * @param       v2: 
        * @return      true
        * @return      false  
        */
        bool HasDuplicate(const std::vector<std::string>& v1, const std::vector<std::string>& v2);
        void setValue(const uint64_t& newValue);
        
    private:
        constexpr static int _contractWaitingTime = 3 * 1000000;

        struct msgInfo
        {
            std::vector<TxMsgReq> txMsgReq;//Transaction information protocol
            std::set<std::string> nodelist;//A list of nodes when making a transaction
            Vrf info; //vrf information
        };
        /**
        * @brief       
        */
        void _DispatcherProcessingFunc();

        /**
        * @brief       
        * 
        * @return      std::vector<std::vector<TxMsgReq>> 
        */
        std::vector<std::vector<TxMsgReq>> GetDependentData();
        /**
        * @brief       
        * 
        * @param       txMsgVec:
        * @return      std::vector<std::vector<TxMsgReq>>  
        */
        std::vector<std::vector<TxMsgReq>> GroupDependentData(const std::vector<std::vector<TxMsgReq>> & txMsgVec);
        /**
        * @brief       
        * 
        * @param       distribution:
        * @return      int 
        */
        int DistributionContractTx(std::multimap<std::string, msgInfo>& distribution);
        /**
        * @brief       Message the transaction information to the packer
        * 
        * @param       packager:
        * @param       info:
        * @param       txsMsg:
        * @param       nodeList:
        * @return      int 
        */
        int SendTxInfoToPackager(const std::string &packager, const Vrf &info, std::vector<TxMsgReq> &txsMsg, const std::set<std::string> nodeList);

    private:
        std::thread _dispatcherThread;
        std::mutex _contractInfoCacheMutex;
        std::mutex _contractMsgMutex;
        std::mutex _contractHandleMutex;
        std::mutex _mtx;

        bool isFirst = false;
        uint64_t timeValue;

        std::unordered_map<std::string/*txHash*/, std::vector<std::string>/*Contract dependency address*/> _contractDependentCache; //The data received from the initiator is stored
        std::unordered_map<std::string, TxMsgReq> _contractMsgReqCache; //hash TxMsgReq

 

};

#endif