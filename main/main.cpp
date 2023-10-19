#include "main.h"

#include <regex>
#include <iostream>

#include "include/logging.h"
#include "common/global.h"
#include "utils/time_util.h"
#include "net/api.h"
#include "ca/ca.h"
#include "ca/global.h"
#include "utils/hex_code.h"
#include "ca/advanced_menu.h"
#include "ca/algorithm.h"
#include "utils/magic_singleton.h"
#include "test.h"
#include "utils/base58.h"
#include "interface.pb.h"
#include "ca/interface.h"
#include "utils/account_manager.h"
#include "ca/block_http_callback.h"

void Menu()
{
	PrintBasicInfo();
	while (true)
	{
		std::cout << std::endl << std::endl;
		std::cout << "1.Transaction" << std::endl;
		std::cout << "2.Stake" << std::endl;
		std::cout << "3.Unstake" << std::endl;
		std::cout << "4.Delegate" << std::endl;
		std::cout << "5.Withdraw" << std::endl;
		std::cout << "6.Get Bonus"  << std::endl;
        std::cout << "7.PrintAccountInfo" << std::endl;
		std::cout << "8.Deploy contract"  << std::endl;
		std::cout << "9.Call contract"  << std::endl;
		std::cout << "10.Account Manager" << std::endl;
		std::cout << "0.Exit" << std::endl;

		std::string strKey;
		std::cout << "Please input your choice: "<< std::endl;
		std::cin >> strKey;	    
		std::regex pattern("^[0-9]|([1][0])$");
		if(!std::regex_match(strKey, pattern))
        {
            std::cout << "Invalid input." << std::endl;
            continue;
        }
        int key = std::stoi(strKey);
		switch (key)
		{			
			case 0:
				std::cout << "Exiting, bye!" << std::endl;
				CaCleanup();
				exit(0);
				return;		
			case 1:
				HandleTransaction();
				break;
			case 2:
				HandleStake();
				break;
			case 3:
				HandleUnstake();
				break;
			case 4:
				HandleInvest();
                break;
			case 5:
				HandleDisinvest();
                break;
			case 6:
				HandleBonus();
                break;
			case 7:
				PrintBasicInfo();
				break;
      		case 8:
				HandleDeployContract();
				break;
			case 9:
				HandleCallContract();
				break;
			case 10:
				HandleAccountManger();
				break;
			default:
                std::cout << "Invalid input." << std::endl;
                continue;
		}
		sleep(1);
	}
}
	
bool Init()
{
	// Initialize the random number seed
	srand(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	
	// Initialize configuration
	// if (!InitConfig())

	if(!InitConfig())
	{
		ERRORLOG("Failed to initialize log!");
		std::cout << "Failed to initialize log!" << std::endl;
		return false;
	}


	// Initialize log
	if (!InitLog())
	{
		ERRORLOG("Failed to initialize log!");
		std::cout << "Failed to initialize log!" << std::endl;
		return false;
	}

	// Initialize account
    if(!InitAccount())
	{
		ERRORLOG("Failed to initialize certification!");
		std::cout << "Failed to initialize certification!" << std::endl;
		return false;		
	}
	
	// Initialize database
    if(!InitRocksDb())
	{
		ERRORLOG("Failed to initialize database!");
		std::cout << "Failed to initialize database!" << std::endl;
		return false;		
	}

	return  CaInit() && net_com::InitializeNetwork();
}

bool InitConfig()
{
	bool flag = false;
	MagicSingleton<Config>::GetInstance(flag);
	return flag;
}

bool InitLog()
{
	Config::Log log = {};
	MagicSingleton<Config>::GetInstance()->GetLog(log);
	if(!LogInit(log.path, log.console, log.level))
	{
		return false;
	}
	return true;
}

bool InitAccount()
{
	MagicSingleton<AccountManager>::GetInstance();

	return true;
}

bool InitRocksDb()
{
    if (!DBInit("./data.db"))
    {
        return false;
    }

    DBReadWriter dbReadWriter;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReadWriter.GetBlockTop(top))
    {
        //  Initialize Block 0. Here, the simplified processing directly reads the key information and saves it to the database
        std::string strBlock = global::ca::kGenesisBlockRaw;

		std::string serBlock = Hex2Str(strBlock);

        CBlock block;
        if(!block.ParseFromString(serBlock))
		{
			return false;
		}
        
        if (block.txs_size() == 0)
        {
            return false;
        }
        CTransaction tx = block.txs(0);
        if (tx.utxo().vout_size() == 0)
        {
            return false;
        }

        dbReadWriter.SetBlockHeightByBlockHash(block.hash(), block.height());
        dbReadWriter.SetBlockHashByBlockHeight(block.height(), block.hash(), true);
        dbReadWriter.SetBlockByBlockHash(block.hash(), block.SerializeAsString());
        dbReadWriter.SetBlockTop(0);

		
		for(int i = 0; i < tx.utxo().vout_size(); ++i)
		{
			if(!CheckBase58Addr(tx.utxo().vout(i).addr()))
			{
				continue;
			}
			dbReadWriter.SetUtxoHashsByAddress(tx.utxo().vout(i).addr(), tx.hash());
			if(tx.utxo().vout(i).addr() == global::ca::kInitAccountBase58Addr)
			{
				dbReadWriter.SetUtxoValueByUtxoHashs(tx.hash(), tx.utxo().vout(i).addr(), std::to_string(tx.utxo().vout(i).value()));
				dbReadWriter.SetBalanceByAddress(tx.utxo().vout(i).addr(), tx.utxo().vout(i).value());
			}
			else
			{
				dbReadWriter.SetUtxoValueByUtxoHashs(tx.hash(), tx.utxo().vout(i).addr(), std::to_string(tx.utxo().vout(i).value()));
				dbReadWriter.SetBalanceByAddress(tx.utxo().vout(i).addr(), tx.utxo().vout(i).value());
			}
		}

        dbReadWriter.SetTransactionByHash(tx.hash(), tx.SerializeAsString());
        dbReadWriter.SetBlockHashByTransactionHash(tx.hash(), block.hash());

        dbReadWriter.SetBalanceByAddress(tx.utxo().owner().at(0), tx.utxo().vout(0).value());

        uint64_t totalCirculation = global::ca::kM2 * global::ca::kDecimalNum;
        dbReadWriter.SetTotalCirculation(totalCirculation);
    }
    dbReadWriter.SetInitVer(global::kVersion);
    if (DBStatus::DB_SUCCESS != dbReadWriter.TransactionCommit())
    {
        ERRORLOG("(rocksdb init) TransactionCommit failed !");
        return false;
    }
    return true;
}


bool Check()
{
	DBReader dbReader;
    uint64_t top = 0;
	std::string hash;
	std::string blockRaw;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashByBlockHeight(top,hash))
    {
		return false;
	}

	if (DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(hash,blockRaw))
    {
		return false;
	}

	CBlock dbBlock;
    dbBlock.ParseFromString(blockRaw);

	if(dbBlock.time() != global::ca::kGenesisTime)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	std::string serBlock = Hex2Str(global::ca::kGenesisBlockRaw);

	CBlock gensisBlock;
	gensisBlock.ParseFromString(serBlock);

	if(gensisBlock.hash() != hash)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	return true;
}