//
// Created by root on 2024/4/24.
//

#ifndef TFS_EVM_ENVIRONMENT_H
#define TFS_EVM_ENVIRONMENT_H

#include <cstdint>
#include <evmc/evmc.h>
#include <transaction.pb.h>
#include "evm_host.h"

namespace evm_environment
{
//    int64_t GetChainId();

    int64_t GetBlockNumber();

    int64_t GetNonce(const std::string &address);

    int64_t GetNextNonce(const std::string &address);

    int MakeDeployMessage(const evmc_address &sender, const evmc_address &recipient, evmc_message &message);

    int MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                        const uint64_t &contractTransfer, evmc_message &message);
    int RPC_MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
            const uint64_t &contractTransfer, evmc_message &message);

    int MakeDeployHost(const std::string &sender, const std::string &recipient, EvmHost &host,
                       int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber,
                       uint64_t transferAmount);

    int MakeCallHost(const std::string &sender, const std::string &recipient, uint64_t transferAmount,
                     const evmc::bytes &code,
                     EvmHost &host, int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber);

    int MakeTxContext(const std::string &from, evmc_tx_context &txContext, int64_t blockTimestamp,
                      int64_t blockPrevRandao, int64_t blockNumber);

    int64_t GetBlockTimestamp(const CTransaction &transaction);

    int64_t GetBlockPrevRandao(const CTransaction &transaction);

    int64_t CalculateBlockTimestamp(int64_t time);

    int64_t CalculateBlockPrevRandao(const std::string &from);

    int64_t CalculateBlockPrevRandao(const CTransaction &transaction);

    int64_t VerifyEvmParameters(const CTransaction &transaction);
}


#endif //TFS_EVM_ENVIRONMENT_H
