#include "string.h"
#include "unistd.h"

#include <iostream>

#include "main/main.h"
#include "utils/time_util.h"
#include "utils/console.h"
#include "common/config.h"
#include "ca/ca.h"
#include "ca/ca_global.h"
#include "ca/ca_transaction.h"
#include "utils/single.hpp"
#include "utils/TFSbenchmark.h"

int main(int argc, char* argv[])
{

	if(-1 == init_pid_file()) {
		exit(-1);
	}
 
	/* Ctrl + C */
	if(signal(SIGINT, sigHandler) == SIG_ERR) {
		exit(-1);
	}
 
	/* kill pid / killall name */
	if(signal(SIGTERM, sigHandler) == SIG_ERR) {
		exit(-1);
	}

	//Modify process time UTC
	setenv("TZ","Universal",1);
    tzset();

	// Get the current number of CPU cores and physical memory size
	global::cpu_nums = sysconf(_SC_NPROCESSORS_ONLN);
	uint64_t memory = (uint64_t)sysconf(_SC_PAGESIZE) * (uint64_t)sysconf(_SC_PHYS_PAGES) / 1024 / 1024;
	INFOLOG("Number of current CPU cores:{}", global::cpu_nums);
	INFOLOG("Number of current memory size:{}", memory);
	if(global::cpu_nums < 1 || memory < 0.5 * 1024)
	{
		std::cout << "Current machine configuration:\n"
				  << "The number of processors currently online (available): "<< global::cpu_nums << "\n"
		          << "The memory size: " << memory << " MB" << std::endl;
		std::cout << RED << "Tfs basic configuration:\n"
						 << "The number of processors currently online (available): 8\n"
						 << "The memory size: 16,384 MB" << RESET << std::endl;
		return 1;
	}

	if (argc > 4)
	{
		ERRORLOG("Too many parameters!");
		std::cout << "Too many parameters!" << std::endl;
		return 1;
	}

	bool showMenu = false;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-m") == 0)
		{
			showMenu = true;
		}
		else if (strcmp(argv[i], "-c") == 0)
		{
			MagicSingleton<Config>::GetInstance();
			std::cout << "Init Config file" << std::endl;
			return 2;
		}
		else if (strcmp(argv[i], "--help") == 0)
		{
			ERRORLOG("The parameter is Help!");
			std::cout << argv[0] << ": --help version:" << global::kCompatibleVersion << " \n -m show menu\n -s value, signature fee\n -p value, package fee" << std::endl;
			return 3;
		}
		else if (strcmp(argv[i], "-t") == 0)
		{
			showMenu = true;
			MagicSingleton<TFSbenchmark>::GetInstance()->OpenBenchmark();
		}
		else if (strcmp(argv[i], "-p") == 0)
		{
			showMenu = true;
			MagicSingleton<TFSbenchmark>::GetInstance()->OpenBenchmark2();
		}
		else
		{
			ERRORLOG("Parameter parsing error!");
			std::cout << "Parameter parsing error!" << std::endl;
			return 4;
		}
	}

	if (!init())
	{
		return 5;
	}         

	if (Check() == false)
	{
		return 6;
	}

	if (showMenu)
	{
		menu();
	}
	while (true)
	{
		sleep(9999);
	}
	
	return 0;
}