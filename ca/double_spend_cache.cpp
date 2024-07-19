#include "ca/double_spend_cache.h"

int DoubleSpendCache::AddFromAddr(const std::pair<std::string,DoubleSpendCache::doubleSpendsuc> &usings)
{
    std::unique_lock<std::mutex> lck(_doubleSpendMutex);
	auto it =  _pending.find(usings.first);
	if(it != _pending.end())
	{

		std::vector<std::string> usingsUtxo(usings.second.utxoVec.begin(), usings.second.utxoVec.end());
		std::vector<std::string> pendingsUtxo(it->second.utxoVec.begin(), it->second.utxoVec.end());
		std::sort(usingsUtxo.begin(),usingsUtxo.end());
		std::sort(pendingsUtxo.begin(),pendingsUtxo.end());

		std::vector<std::string> vIntersection;
		std::set_intersection(usingsUtxo.begin(), usingsUtxo.end(), pendingsUtxo.begin(), pendingsUtxo.end(), std::back_inserter(vIntersection));

		for(auto & diff : vIntersection)
		{
			ERRORLOG("utxo:{} is using!",diff);
			std::cout << "utxo:" << diff << "is using!\n";
			return -1;
		}

	}else{
		_pending.insert(usings);
	}

	return 0;
}


void DoubleSpendCache::Remove(const uint64_t& txtimeKey)
{

	for(auto iter = _pending.begin(); iter != _pending.end();)
	{
		if (iter->second.time == txtimeKey)
		{
			iter = _pending.erase(iter);
		}
		else
		{
			iter++;
		}
	}  
	
}

void DoubleSpendCache::CheckLoop()
{
	std::unique_lock<std::mutex> lck(_doubleSpendMutex);
	std::vector<uint64_t> toRemove;
	for(auto & item : _pending)
	{
	    const int64_t kTenSecond = (int64_t)1000000 * 30;
		uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
		if(item.second.time + kTenSecond < time)
		{
			toRemove.push_back(item.second.time);
		}
	}

	for(auto & txtimeKey: toRemove)
	{
		Remove(txtimeKey);
	}
}



void DoubleSpendCache::Detection(const CBlock & block)
{
	std::unique_lock<std::mutex> lck(_doubleSpendMutex);
	std::vector<uint64_t> toRemove;
	for(auto & tx : block.txs())
	{	
		auto it = _pending.find(tx.utxo().owner(0));
		if(it != _pending.end() && tx.time() == it->second.time)
		{	
			toRemove.push_back(it->second.time);
			DEBUGLOG("Remove pending txhash : {}",tx.hash());
		}
	}

	for(auto & txtimeKey: toRemove)
	{
		Remove(txtimeKey);
	}
}