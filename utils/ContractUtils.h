#ifndef _CONTRACTUTILS_H_
#define _CONTRACTUTILS_H_

#include <evmc/evmc.hpp>

namespace evm_utils
{
    using namespace evmc;
    
    std::string toChecksumAddress(const std::string& hash, const std::string& address);

    evmc_address stringToEvmAddr(const std::string& addr);

    std::string EvmAddrToString(const evmc_address& addr);

    std::string EvmAddrToBase58(const evmc_address& addr);

    evmc_address pubStrToEvmAddr(const std::string& pub);

    std::string generateEvmAddr(const std::string& pub);

    std::string EvmAddrToBase58(const std::string& addr);

    std::string getEvmAddr(const std::string& pub);

    std::string EvmAddrToChecksum(const std::string& addr);
}


#endif