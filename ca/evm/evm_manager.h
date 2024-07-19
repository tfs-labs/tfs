//
// Created by root on 2024/4/30.
//

#ifndef TFS_EVM_MANAGER_H
#define TFS_EVM_MANAGER_H


#include <optional>
#include <db_api.h>
#include "evm_host.h"

namespace Evmone
{
    const std::string contractVersionKeyName = "version";
    const std::string contractVirtualMachineKeyName = "virtualMachine";
    const std::string contractSenderKeyName = "sender";
    const std::string contractRecipientKeyName = "recipient";
    const std::string contractInputKeyName = "input";
    const std::string contractDonationKeyName = "donation";
    const std::string contractTransferKeyName = "transfer";

    const std::string contractOutputKeyName = "output";
    const std::string contractCreationKeyName = "creation";
    const std::string contractLogKeyName = "log";
    const std::string contractStorageKeyName = "storage";
    const std::string contractPreHashKeyName = "preHash";
    const std::string contractDestructionKeyName = "destruction";

    const std::string contractBlockTimestampKeyName = "blockTimestamp";
    const std::string contractBlockPrevRandaoKeyName = "blockPrevRandao";

    const std::string contractDeployerKeyName = "contractDeployer";

    std::optional<evmc::VM> GetEvmInstance();

    int
    SynchronousExecuteByEvmone(const evmc_message &msg, const evmc::bytes &code, EvmHost &host, std::string &output);

    bool VerifyContractAddress(const std::string &from, const std::string &contractAddress);

    std::optional<std::string> GetLatestContractAddress(const std::string &from);

    int SaveContract(const std::string &deployerAddress, const std::string &deployHash,
                     const std::string &contractAddress, const std::string &contractCode,
                     DBReadWriter &dbReadWriter, global::ca::VmType vmType);

    int DeleteContract(const std::string &deployerAddress, const std::string &contractAddress,
                       DBReadWriter &dbReadWriter, global::ca::VmType vmType);
}


#endif //TFS_EVM_MANAGER_H
