#include "main.h"

#include <regex>
#include <iostream>

#include "include/logging.h"
#include "common/global.h"
#include "utils/time_util.h"
#include "net/net_api.h"
#include "ca/ca.h"
#include "ca/ca_global.h"
#include "utils/hexcode.h"
#include "ca/ca_AdvancedMenu.h"
#include "ca/ca_algorithm.h"
#include "utils/MagicSingleton.h"
#include "ca_test.h"
#include "utils/base58.h"
#include "interface.pb.h"
#include "ca/ca_interface.h"
#include "utils/AccountManager.h"
#include "ca/ca_block_http_callback.h"









void menu()
{
	ca_print_basic_info();
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
				ca_cleanup();
				exit(0);
				return;		
			case 1:
				handle_transaction();
				break;
			case 2:
				handle_stake();
				break;
			case 3:
				handle_unstake();
				break;
			case 4:
				handle_invest();
                break;
			case 5:
				handle_disinvest();
                break;
			case 6:
				handle_bonus();
                break;
			case 7:
				ca_print_basic_info();
				break;
      		case 8:
				handle_deploy_contract();
				break;
			case 9:
				handle_call_contract();
				break;
			case 10:
				handle_AccountManger();
				break;
			default:
                std::cout << "Invalid input." << std::endl;
                continue;
		}

		sleep(1);
	}
}
	
bool init()
{
	// Initialize the random number seed
	srand(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
	
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

	return  ca_init() && net_com::net_init();
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

    DBReadWriter db_read_writer;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != db_read_writer.GetBlockTop(top))
    {
        //  Initialize Block 0. Here, the simplified processing directly reads the key information and saves it to the database
        std::string str_block0 = global::ca::kGenesisBlockRaw;

		std::string serBlock = Hex2Str(str_block0);

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

        db_read_writer.SetBlockHeightByBlockHash(block.hash(), block.height());
        db_read_writer.SetBlockHashByBlockHeight(block.height(), block.hash(), true);
        db_read_writer.SetBlockByBlockHash(block.hash(), block.SerializeAsString());
        db_read_writer.SetBlockTop(0);

		
		for(int i = 0; i < tx.utxo().vout_size(); ++i)
		{
			if(!CheckBase58Addr(tx.utxo().vout(i).addr()))
			{
				continue;
			}
			db_read_writer.SetUtxoHashsByAddress(tx.utxo().vout(i).addr(), tx.hash());
			if(tx.utxo().vout(i).addr() == global::ca::kInitAccountBase58Addr)
			{
				db_read_writer.SetUtxoValueByUtxoHashs(tx.hash(), tx.utxo().vout(i).addr(), std::to_string(tx.utxo().vout(i).value()));
				db_read_writer.SetBalanceByAddress(tx.utxo().vout(i).addr(), tx.utxo().vout(i).value());
			}
			else
			{
				db_read_writer.SetUtxoValueByUtxoHashs(tx.hash(), tx.utxo().vout(i).addr(), std::to_string(tx.utxo().vout(i).value()));
				db_read_writer.SetBalanceByAddress(tx.utxo().vout(i).addr(), tx.utxo().vout(i).value());
			}
		}

        db_read_writer.SetTransactionByHash(tx.hash(), tx.SerializeAsString());
        db_read_writer.SetBlockHashByTransactionHash(tx.hash(), block.hash());

        db_read_writer.SetBalanceByAddress(tx.utxo().owner().at(0), tx.utxo().vout(0).value());

        uint64_t totalCirculation = global::ca::kM2 * global::ca::kDecimalNum;
        db_read_writer.SetTotalCirculation(totalCirculation);
    }
    db_read_writer.SetInitVer(global::kVersion);
    if (DBStatus::DB_SUCCESS != db_read_writer.TransactionCommit())
    {
        ERRORLOG("(rocksdb init) TransactionCommit failed !");
        return false;
    }
    return true;
}


bool Check()
{
	DBReader db_reader;
    uint64_t top = 0;
	std::string hash;
	std::string blockraw;
    if (DBStatus::DB_SUCCESS != db_reader.GetBlockHashByBlockHeight(top,hash))
    {
		return false;
	}

	if (DBStatus::DB_SUCCESS != db_reader.GetBlockByBlockHash(hash,blockraw))
    {
		return false;
	}

	CBlock db_block;
    db_block.ParseFromString(blockraw);

	if(db_block.time() != global::ca::kGenesisTime)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	std::string serBlock = Hex2Str(global::ca::kGenesisBlockRaw);

	CBlock gensis_block;
	gensis_block.ParseFromString(serBlock);

	if(gensis_block.hash() != hash)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	return true;
}