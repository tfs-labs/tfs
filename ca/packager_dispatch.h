#ifndef _PACKAGER_DISPATCH_
#define _PACKAGER_DISPATCH_
#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>
#include <utils/json.hpp>
#include <shared_mutex>
#include "../proto/transaction.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/block.pb.h"
#include "utils/timer.hpp"
#include "utils/magic_singleton.h"
#include "include/logging.h"


class packDispatch
{
public:
    packDispatch(/* args */) = default;
    ~packDispatch() = default;
public:
    void Add(const std::string& contractHash, const std::vector<std::string>& dependentContracts,const CTransaction &msg);
    void GetDependentData(std::map<uint32_t, std::map<std::string, CTransaction>>& dependentContractTxMap, std::map<std::string, CTransaction> &nonContractTxMap);
private:

    std::mutex _packDispatchMutex;
    struct Hash_Depend
    {
        uint64_t time;
        std::unordered_map<std::string, std::vector<std::string>> hash_dep;
        std::unordered_map<std::string, CTransaction> hash_tx; 
    };

    Hash_Depend _packDispatchDependent;
};

#endif