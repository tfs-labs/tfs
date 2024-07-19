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
#include <future>
#include <chrono>
#include <ostream>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>


namespace evm_utils
{
    using namespace evmc;
    
    /**
     * @brief       
     * 
     * @param       address: 
     * @return      std::string 
     */
    std::string ToChecksumAddress(const std::string& address);

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
    std::string EvmAddrTo(const evmc_address& addr);

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
     * @param       input: 
     * @return      std::string 
     */
    std::string GenerateContractAddr(const std::string& input);

    // /**
    //  * @brief       
    //  * 
    //  * @param       addr: 
    //  * @return      std::string 
    //  */
    // std::string EvmAddrTo(const std::string& addr);

    /**
     * @brief       Get the Evm Addr object
     * 
     * @param       pub: 
     * @return      std::string 
     */
    std::string GetEvmAddr(const std::string& pub);

    evmc_uint256be Uint32ToEvmcUint256be(uint32_t x);

    uint32_t EvmcUint256beToUint32(evmc_uint256be value);

    bytes StringTobytes(const std::string& content);

    std::string BytesToString(const bytes& content);

    int GetContractCode(const std::string& contractAddress, bytes& code);
}


#endif