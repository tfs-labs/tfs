#ifndef __DISPATCHER_H_
#define __DISPATCHER_H_
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include "./transaction.h"
#include "./global.h"
#include "transaction_cache.h"
#include "interface.h"
#include "./utils/console.h"

class ContractDispatcher{

    public:
        ContractDispatcher() = default;
        ~ContractDispatcher() = default;

        void AddContractInfo(const std::string& contractHash, const std::vector<std::string>& dependentContracts);
        void AddContractMsgReq(const std::string& contractHash, const ContractTxMsgReq &msg);
        void RemoveContractInfo(const std::string& contractName);
        void ClearContractInfoCache();
        void Start();
        void Stop();
        void Process();
        
    private:
        constexpr static int _precedingContractBlockLookupInterval = 10;
        constexpr static int _precedingContractBlockLookupStartTime = 3 * 1000000;
        constexpr static int _precedingContractBlockLookupEndtime = _precedingContractBlockLookupInterval * 1000000;
        constexpr static int _contractBlockPackingTime = _precedingContractBlockLookupInterval * 1000000;

        struct msgInfo
        {
            std::vector<TxMsgReq> txMsgReq;
            std::set<std::string> nodelist;
            Vrf info;
        };

        void _DispatcherProcessingFunc();

        bool HasDuplicate(const std::vector<std::string>& v1, const std::vector<std::string>& v2);
        std::vector<std::vector<TxMsgReq>> GetDependentData();
        std::vector<std::vector<TxMsgReq>> GroupDependentData(const std::vector<std::vector<TxMsgReq>> & txMsgVec);
        int DistributionContractTx(std::multimap<std::string, msgInfo>& distribution);
        int SendTxInfoToPackager(const std::string &packager, const Vrf &info, std::vector<TxMsgReq> &txsmsg,const std::set<std::string> nodelist);

    private:
        std::thread _dispatcherThread;
        std::mutex _contractInfoCacheMutex;
        std::mutex _contractMsgMutex;
        std::mutex _contractHandleMutex;

        std::unordered_map<std::string, std::vector<std::string>> _contractDependentCache; 
        std::unordered_map<std::string, TxMsgReq> _contractMsgReqCache;
};

#endif