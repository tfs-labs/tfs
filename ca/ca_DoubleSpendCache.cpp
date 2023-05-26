#include"ca_DoubleSpendCache.h"

int DoubleSpendCache::AddFromAddr(const std::vector<std::string> &addrUtxo)
{
    std::unique_lock<std::mutex> lck(_double_spend_mutex_);
	const int64_t kTenSecond = (int64_t)1000000 * 10;
	std::string underline = "_";
	uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
	for(auto & j : addrUtxo)
	{
		std::vector<std::string> usingUtxo;
		if(j.find(underline) != string::npos)
		{
			StringUtil::SplitString(j, "_", usingUtxo);
		}
		std::string fromaddr = usingUtxo[0];
		for(auto & item : _pendingAddrs)
		{
			std::vector<std::string> pendingUtxo;
			auto it = std::find(item.first.begin(),item.first.end(),fromaddr);
			if(it != item.first.end())//Description: The same address was found
			{
				if(it->find(underline) != string::npos)
				{
					StringUtil::SplitString(*it, "_", pendingUtxo);
				}

				
				std::sort(usingUtxo.begin(), usingUtxo.end());
				std::sort(pendingUtxo.begin(), pendingUtxo.end());

				std::vector<std::string> v_intersection;
				std::set_intersection(usingUtxo.begin(), usingUtxo.end(), pendingUtxo.begin(), pendingUtxo.end(), std::back_inserter(v_intersection));
			
				if(item.second + kTenSecond > time && v_intersection.size() >= 2)
				{
					for(auto & diff : v_intersection)
					{
					
						if(diff != fromaddr)
						{
							ERRORLOG("utxo:{} is using!");
							std::cout << "utxo:" << diff <<"is using!";
							return -1;
						
						}
					}
				}

			}
		}
	}
	
	_pendingAddrs.insert(std::make_pair(addrUtxo,time));

	return 0;
}

void DoubleSpendCache::CheckLoop()
{
	std::unique_lock<std::mutex> lck(_double_spend_mutex_);
	std::vector<uint64_t> toRemove;
	for(auto & item : _pendingAddrs)
	{
	    const int64_t kTenSecond = (int64_t)1000000 * 10;
		uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
		if(item.second + kTenSecond < time)
		{
			toRemove.push_back(item.second);
		}
	}

	for(auto & timeKey: toRemove)
	{
		for(auto iter = _pendingAddrs.begin(); iter != _pendingAddrs.end();)
		{
			if (iter->second == timeKey)
			{
				iter = _pendingAddrs.erase(iter);
				DEBUGLOG("DoubleSpendCache::Remove ");
			}
			else
			{
				iter++;
			}
		}  
	}
}
