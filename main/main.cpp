#include "main.h"

#include <cstdint>
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
#include "ca/block_stroage.h"


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
				{
					uint64_t selfNodeHeight = 0;
                    DBReader dbReader;
                    auto status = dbReader.GetBlockTop(selfNodeHeight);
                    if (DBStatus::DB_SUCCESS != status)
                    {
						std::cout << "Get block top error,please try again: "<< std::endl;
                        break;
                    }
					if(selfNodeHeight < global::ca::OldVersionSmartContractFailureHeight)
					{
						HandleDeployContract_V33_1();
					}
					else
					{
						HandleDeployContract();
					}
				}
				break;
			case 9:
				{
					uint64_t selfNodeHeight = 0;
                    DBReader dbReader;
                    auto status = dbReader.GetBlockTop(selfNodeHeight);
                    if (DBStatus::DB_SUCCESS != status)
                    {
						std::cout << "Get block top error,please try again: "<< std::endl;
                        break;
                    }
					if(selfNodeHeight <= global::ca::OldVersionSmartContractFailureHeight)
					{
						HandleCallContract_V33_1();
					}
					else
					{
						HandleCallContract();
					}
				}
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

	if(!InitConfig())
	{
		ERRORLOG("Failed to initialize config!");
		std::cout << "Failed to initialize config!" << std::endl;
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
    if(InitRocksDb() != 0)
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

int InitRocksDb()
{
    if (!DBInit("./data.db"))
    {
		ERRORLOG("DBInit is error");
        return -1;
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
			ERRORLOG("block parsefromsring is error");
			return -2;
		}
        
        if (block.txs_size() == 0)
        {
			ERRORLOG("the transactions in block are empty");
            return -3;
        }
        CTransaction tx = block.txs(0);
        if (tx.utxo().vout_size() == 0)
        {
			ERRORLOG("vouts in utxo are empty");
            return -4;
        }

        dbReadWriter.SetBlockHeightByBlockHash(block.hash(), block.height());
        dbReadWriter.SetBlockHashByBlockHeight(block.height(), block.hash(), true);
        dbReadWriter.SetBlockByBlockHash(block.hash(), block.SerializeAsString());
        dbReadWriter.SetBlockTop(0);
//        dbReadWriter.SetLatestContractBlockHash(block.hash());
		
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
	// if (DBStatus::DB_SUCCESS != dbReadWriter.SetInitVer(global::kVersion))
	// {
	// 	ERRORLOG("(rocksdb init) set InitVer failed !");
	// 	return false;
	// }
	std::string version;
	if (DBStatus::DB_SUCCESS != dbReadWriter.GetInitVer(version))
	{
		ERRORLOG("(rocksdb init) get InitVer failed !");
	}
	
	if(global::kVersion != version)
	{
		std::cout <<"Database version now is:"<< version << std::endl;
		if(version == "")
		{
			ERRORLOG("rocksdb date version is empty");
			std::cout << "Please replace the database with the latest "<< std::endl;
			return -5;
		}
		else
		{
		ERRORLOG("(rocksdb init) date version is not qulified current version failed !");
		std::cout <<"Please replace the database with the latest "<<std::endl;
		return -6;
		}
	}
	if (DBStatus::DB_SUCCESS != dbReadWriter.TransactionCommit())
    {
        ERRORLOG("(rocksdb init) TransactionCommit failed !");
        return -7;
    }
    return 0;
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