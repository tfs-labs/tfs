#include "block_monitor.h"

#include "utils/console.h"
#include "utils/vrf.hpp"
#include "db/db_api.h"
#include "transaction.h"

uint32_t BlockMonitor::_maxSendSize = 100;
void BlockMonitor::checkTxSuccessRate()
{
	std::unique_lock<std::mutex> lck(mutex_);
	DBReader reader;
	int DropshippingTxVecFail = 0;
	std::vector<std::string> failTxhashVec;
	for(const auto& txHash : DropshippingTxVec)
	{
		std::string blockHash;
		reader.GetBlockHashByTransactionHash(txHash, blockHash);

		if (blockHash.empty())
		{
			failTxhashVec.push_back(txHash);
			DropshippingTxVecFail++;
			continue;
		}
	}
	
	std::ofstream outputFile("./failTxhash.txt");
	if (outputFile.is_open())
	{
		for(const auto& it : failTxhashVec)
		{
			outputFile << it << "\n";
		}
		outputFile.close();
	}
	double successRate = (1 - static_cast<double>(DropshippingTxVecFail) / DropshippingTxVec.size()) * 100;
	DEBUGLOG(GREEN "addDropshippingTxVec Txsize: {}, DropshippingTxVecFail size:{}, SuccessRate:{}%" RESET, DropshippingTxVec.size(), DropshippingTxVecFail, successRate );
	std::cout<< GREEN << "addDropshippingTxVec Txsize:" << DropshippingTxVec.size() << RESET << std::endl;
	std::cout<< GREEN <<"DropshippingTxVecFail size:" << DropshippingTxVecFail<< RESET << std::endl;
	std::cout<< GREEN <<"SuccessRate:" << successRate << "%" << RESET << std::endl;
	DropshippingTxVec.clear();

	int DoHandleTxVecFail = 0;
	for(const auto& txHash : DoHandleTxVec)
	{
		std::string blockHash;
		reader.GetBlockHashByTransactionHash(txHash, blockHash);

		if (blockHash.empty())
		{
			DoHandleTxVecFail++;
			continue;
		}
	}
	DEBUGLOG(GREEN "addDoHandleTxTxVec Txsize: {}, DoHandleTxVecFailFail size:{}" RESET, DoHandleTxVec.size(), DoHandleTxVecFail);
	DoHandleTxVec.clear();
}

void BlockMonitor::addDropshippingTxVec(const std::string& txHash)
{
	std::unique_lock<std::mutex> lck(mutex_);
	DropshippingTxVec.push_back(txHash);
}
void BlockMonitor::addDoHandleTxTxVec(const std::string& txHash)
{
	std::unique_lock<std::mutex> lck(mutex_);
	DoHandleTxVec.push_back(txHash);
}


int BlockMonitor::SendBroadcastAddBlock(std::string strBlock, uint64_t blockHeight,uint64_t sendSize)
{
    std::vector<Node> list;
	list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	{
		// Filter nodes that meet the height
		std::vector<Node> tmpList;
		if (blockHeight != 0)
		{
			for (auto & item : list)
			{
				if (item.height >= blockHeight - 1)
				{
					tmpList.push_back(item);
				}
			}
			list = tmpList;
		}
	}

    BuildBlockBroadcastMsg buildBlockBroadcastMsg;
    buildBlockBroadcastMsg.set_version(global::kVersion);
	buildBlockBroadcastMsg.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
	buildBlockBroadcastMsg.set_blockraw(strBlock);
    buildBlockBroadcastMsg.set_flag(1);

	//VRF protocol for filling block flow
	CBlock block;
    block.ParseFromString(strBlock);
	std::pair<std::string,Vrf> blockVrf;
	MagicSingleton<VRF>::GetInstance()->getVrfInfo(block.hash(), blockVrf);
	Vrf *blockVrfinfo = buildBlockBroadcastMsg.mutable_blockvrfinfo();
	blockVrfinfo->CopyFrom(blockVrf.second);

    for(auto &tx : block.txs())
    {
        if(GetTransactionType(tx) != kTransactionType_Tx)
        {
            continue;
        }

        CTransaction copyTx = tx;
        copyTx.clear_hash();
        copyTx.clear_verifysign();
        std::string txHash = Getsha256hash(copyTx.SerializeAsString());
        // const std::string& txHash = tx.hash();
        std::pair<std::string,Vrf> vrfPair;
        if(!MagicSingleton<VRF>::GetInstance()->getVrfInfo(txHash, vrfPair))
        {
            ERRORLOG("getVrfInfo failed! tx hash {}", txHash);
            return -1;
        }
        Vrf *vrfinfo  = buildBlockBroadcastMsg.add_vrfinfo();
        vrfinfo ->CopyFrom(vrfPair.second);

        uint64_t handleTxHeight =  block.height() - 1;
        TxHelper::vrfAgentType type = TxHelper::GetVrfAgentType(tx, handleTxHeight);
        if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
        {
            continue;
        }

        if(!MagicSingleton<VRF>::GetInstance()->getTxVrfInfo(txHash, vrfPair))
        {
            ERRORLOG("getTxVrfInfo failed! tx hash {}", txHash);
            return -2;
        }

        Vrf *txvrfinfo  = buildBlockBroadcastMsg.add_txvrfinfo();
        vrfPair.second.mutable_vrfdata()->set_txvrfinfohash(txHash);
        txvrfinfo ->CopyFrom(vrfPair.second);
    }

    bool isSucceed = net_com::BroadBroadcastMessage(buildBlockBroadcastMsg);
	if(isSucceed == false)
	{
        ERRORLOG("Broadcast BuildBlockBroadcastMsg failed!");
        return -3;
	}

	std::set<std::string> cast_address;
	for(auto addr : buildBlockBroadcastMsg.castaddrs()){
		cast_address.insert(addr);
	}

	MagicSingleton<BlockStroage>::GetInstance()->AddBlockStatus(block.hash(), block, cast_address);

	DEBUGLOG("***********net broadcast time{}",MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	return 0;
}
