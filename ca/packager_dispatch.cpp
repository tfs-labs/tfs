#include "packager_dispatch.h"
#include "utils/timer.hpp"
#include "include/logging.h"
#include "dispatchtx.h"

void packDispatch::Add(const std::string& contractHash, const std::vector<std::string>& dependentContracts, const CTransaction &tx)
{
    std::unique_lock<std::mutex> locker(_packDispatchMutex);
	_packDispatchDependent.time = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    _packDispatchDependent.hash_dep.insert(std::make_pair(contractHash, dependentContracts));
	_packDispatchDependent.hash_tx.insert(std::make_pair(contractHash, tx));
    DEBUGLOG("packDispatch Add ...");
}

void packDispatch::GetDependentData(std::map<uint32_t, std::map<std::string, CTransaction>>& dependentContractTxMap, std::map<std::string, CTransaction> &nonContractTxMap)
{

    std::unique_lock<std::mutex> locker(_packDispatchMutex);
    DEBUGLOG("DependencyGrouping");
    std::vector<std::set<std::string>> res;

	for (const auto& [key, values] : _packDispatchDependent.hash_dep) {
		std::set<std::string> commonKeys;
		commonKeys.insert(key);

		for (const auto& [otherKey, otherValues] : _packDispatchDependent.hash_dep) {
			if (key == otherKey)  continue;

			if (MagicSingleton<ContractDispatcher>::GetInstance()->HasDuplicate(values, otherValues) == true)
			{
				commonKeys.insert(otherKey);
			}
		}

		if (!commonKeys.empty()) {
			bool foundDuplicate = false;

			for (auto& itemSet : res) {
                
				std::set<std::string> intersection;
				std::set_intersection(
					itemSet.begin(), itemSet.end(),
					commonKeys.begin(), commonKeys.end(),
					std::inserter(intersection, intersection.begin())
				);

				if (!intersection.empty()) {
					foundDuplicate = true;
					itemSet.insert(commonKeys.begin(), commonKeys.end());
					break;
				}
			}

			if (!foundDuplicate) {
				res.push_back(commonKeys);
			}
		}
	}

	
	int n = 1;
    for(const auto & hashContainer : res)
    {
        for(const auto & hash : hashContainer)
        {
			if(hashContainer.size() == 1)
			{
				nonContractTxMap[hash] = _packDispatchDependent.hash_tx.at(hash);
			}
			else
			{
				dependentContractTxMap[n].emplace(hash, _packDispatchDependent.hash_tx.at(hash));
			}
        }
		n++;
    }
    return ;
}
