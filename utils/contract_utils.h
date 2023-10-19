/**
 * *****************************************************************************
 * @file        contract_utils.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _CONTRACTUTILS_H_
#define _CONTRACTUTILS_H_

#include <evmc/evmc.hpp>

namespace evm_utils
{
    using namespace evmc;
    
    /**
     * @brief       
     * 
     * @param       hash: 
     * @param       address: 
     * @return      std::string 
     */
    std::string ToChecksumAddress(const std::string& hash, const std::string& address);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      evmc_address 
     */
    evmc_address StringToEvmAddr(const std::string& addr);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      std::string 
     */
    std::string EvmAddrToString(const evmc_address& addr);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      std::string 
     */
    std::string EvmAddrToBase58(const evmc_address& addr);

    /**
     * @brief       
     * 
     * @param       pub: 
     * @return      evmc_address 
     */
    evmc_address PubStrToEvmAddr(const std::string& pub);

    /**
     * @brief       
     * 
     * @param       pub: 
     * @return      std::string 
     */
    std::string GenerateEvmAddr(const std::string& pub);

    /**
     * @brief       
     * 
     * @param       input: 
     * @return      std::string 
     */
    std::string GenerateContractAddr(const std::string& input);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      std::string 
     */
    std::string EvmAddrToBase58(const std::string& addr);

    /**
     * @brief       Get the Evm Addr object
     * 
     * @param       pub: 
     * @return      std::string 
     */
    std::string GetEvmAddr(const std::string& pub);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      std::string 
     */
    std::string EvmAddrToChecksum(const std::string& addr);
}


#endif