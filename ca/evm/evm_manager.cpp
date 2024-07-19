//
// Created by root on 2024/4/30.
//

#include <utils/account_manager.h>
#include "evm_manager.h"
#include "include/logging.h"
#include "utils/contract_utils.h"
#include "evm_environment.h"

std::optional<evmc::VM> Evmone::GetEvmInstance()
{
    struct evmc_vm *pvm = evmc_create_evmone();
    if (!pvm)
    {
        ERRORLOG("can't create evmone instance")
        return {};
    }
    if (!evmc_is_abi_compatible(pvm))
    {
        ERRORLOG("created evmone instance abi not compatible")
        return {};
    }
    evmc::VM vm{pvm};
    return vm;
}

int
Evmone::SynchronousExecuteByEvmone(const evmc_message &msg, const evmc::bytes &code, EvmHost &host, std::string &output)
{
    auto createResult = Evmone::GetEvmInstance();
    if (!createResult.has_value())
    {
        return -1;
    }

    evmc::VM &vm = createResult.value();

    auto result = vm.execute(host, EVMC_MAX_REVISION, msg, code.data(), code.size());
    DEBUGLOG("ContractAddress: {} , Result: {}", evm_utils::EvmAddrToString(msg.recipient), result.status_code);
    if (result.status_code != EVMC_SUCCESS)
    {
        ERRORLOG("Evmone execution failed!");
        auto strOutput = std::string_view(reinterpret_cast<const char *>(result.output_data), result.output_size);
        DEBUGLOG("Output:   {}\n", strOutput);
        return -2;
    }
    output = std::move(evmc::hex({result.output_data, result.output_size}));
    DEBUGLOG("Output:   {}\n", output);
    return 0;
}

bool Evmone::VerifyContractAddress(const std::string &from, const std::string &contractAddress)
{
    auto result = GetLatestContractAddress(from);
    if (!result.has_value())
    {
        return false;
    }
    return result.value() == contractAddress;
}

std::optional<std::string> Evmone::GetLatestContractAddress(const std::string &from)
{
    int64_t nonce = evm_environment::GetNextNonce(from);
    if (nonce < 0)
    {
        ERRORLOG("can't read nonce of {}", from);
        return {};
    }
    return GenerateAddr(from + std::to_string(nonce));
}

int Evmone::SaveContract(const std::string &deployerAddress, const std::string &deployHash,
                         const std::string &contractAddress, const std::string &contractCode,
                         DBReadWriter &dbReadWriter, global::ca::VmType vmType)
{

    if (DBStatus::DB_SUCCESS != dbReadWriter.SetContractDeployUtxoByContractAddr(contractAddress, deployHash))
    {
        return -84;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.SetContractCodeByContractAddr(contractAddress, contractCode))
    {
        return -84;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.SetLatestUtxoByContractAddr(contractAddress, deployHash))
    {
        return -90;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.SetContractAddrByDeployerAddr(deployerAddress, contractAddress))
    {
        return -91;
    }
    std::vector<std::string> vecDeployerAddrs;
    DBStatus ret;
    if (vmType == global::ca::VmType::EVM)
    {
        ret = dbReadWriter.GetAllEvmDeployerAddr(vecDeployerAddrs);
    }
//    else if (vmType == global::ca::VmType::WASM)
//    {
//        ret = dbReadWriter.GetAllWasmDeployerAddr(vecDeployerAddrs);
//    }

    if (DBStatus::DB_SUCCESS != ret && DBStatus::DB_NOT_FOUND != ret)
    {
        return -92;
    }
    auto iter = std::find(vecDeployerAddrs.begin(), vecDeployerAddrs.end(), deployerAddress);
    if (iter == vecDeployerAddrs.end())
    {
        if (vmType == global::ca::VmType::EVM)
        {
            if (DBStatus::DB_SUCCESS != dbReadWriter.SetEvmDeployerAddr(deployerAddress))
            {
                return -93;
            }
        }
//        else if (vmType == global::ca::VmType::WASM)
//        {
//            if (DBStatus::DB_SUCCESS != dbReadWriter.SetWasmDeployerAddr(deployerAddress))
//            {
//                return -94;
//            }
//        }
    }
    return 0;
}

int Evmone::DeleteContract(const std::string &deployerAddress, const std::string &contractAddress,
                           DBReadWriter &dbReadWriter, global::ca::VmType vmType)
{
    if (DBStatus::DB_SUCCESS != dbReadWriter.RemoveContractDeployUtxoByContractAddr(contractAddress))
    {
        return -88;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.RemoveContractCodeByContractAddr(contractAddress))
    {
        return -89;
    }
    if (DBStatus::DB_SUCCESS != dbReadWriter.RemoveLatestUtxoByContractAddr(contractAddress))
    {
        return -92;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReadWriter.RemoveContractAddrByDeployerAddr(deployerAddress, contractAddress))
    {
        return -93;
    }

    std::vector<std::string> vecDeployUtxos;
    auto ret = dbReadWriter.GetContractAddrByDeployerAddr(deployerAddress, vecDeployUtxos);
    if (DBStatus::DB_NOT_FOUND == ret || vecDeployUtxos.empty())
    {
        if (vmType == global::ca::VmType::EVM)
        {
            if (DBStatus::DB_SUCCESS != dbReadWriter.RemoveEvmDeployerAddr(deployerAddress))
            {
                return -94;
            }
        }
//        else if (vmType == global::ca::VmType::WASM)
//        {
//            if (DBStatus::DB_SUCCESS != dbReadWriter.RemoveWasmDeployerAddr(deployerAddress))
//            {
//                return -95;
//            }
//        }
    }

    return 0;
}