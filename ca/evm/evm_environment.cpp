//
// Created by root on 2024/4/24.
//

#include <utils/string_util.h>
#include "evm_environment.h"
#include "db/db_api.h"
#include "include/logging.h"
#include "utils/contract_utils.h"
#include "utils/util.h"
#include "transaction.h"
#include "evm_manager.h"
#include <string>

namespace
{
    int64_t GetChainId()
    {
        static int64_t chainId = 0;
        if (chainId != 0)
        {
            return chainId;
        }

        std::string blockHash;
        DBReader dbReader;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashByBlockHeight(0, blockHash))
        {
            ERRORLOG("fail to read genesis block hash")
            return -1;
        }

        std::string genesis_block_prefix = blockHash.substr(0, 8);
        chainId = StringUtil::StringToNumber(genesis_block_prefix);
        return chainId;
    }


    int MakeHost(global::ca::TxType contractType, const std::string &sender, const std::string &recipient,
                 uint64_t transferAmount, const evmc::bytes &code, EvmHost &host, int64_t blockTimestamp,
                 int64_t blockPrevRandao, int64_t blockNumber)
    {

        int result = evm_environment::MakeTxContext(sender, host.tx_context, blockTimestamp, blockPrevRandao, blockNumber);
        if (result < 0)
        {
            return -1;
        }

        int64_t nonce = evm_environment::GetNonce(sender);
        if (nonce < 0)
        {
            return -2;
        }
        host.nonceCache = nonce;

        host.coin_transferrings.emplace_back(sender, recipient, transferAmount);


        if (contractType != global::ca::TxType::kTxTypeDeployContract)
        {
            host.accounts[evm_utils::StringToEvmAddr(recipient)].set_code(code);
        }
        std::string contractRootHash;
        if (contractType != global::ca::TxType::kTxTypeDeployContract)
        {
            result = GetContractRootHash(recipient, contractRootHash, host.contractDataCache);
            if (result != 0)
            {
                return -3;
            }
        }

        host.accounts[evm_utils::StringToEvmAddr(recipient)].CreateTrie(contractRootHash, recipient,
                                                                        host.contractDataCache);
        return 0;
    }

    int MakeMessage(global::ca::TxType contractType, const evmc_address &sender, const evmc_address &recipient,
                    const evmc::bytes &input, const uint64_t &contractTransfer, evmc_message &message)
    {
        message.sender = sender;
        message.recipient = recipient;

        DBReader dbReader;
        int64_t balance = 0;
        if (dbReader.GetBalanceByAddress(evm_utils::EvmAddrToString(sender), balance) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("can't get balance of {}", balance)
            return -1;
        }
        message.gas = balance;

        if (contractType == global::ca::TxType::kTxTypeDeployContract)
        {
            message.kind = EVMC_CREATE;
        }
        else if (contractType == global::ca::TxType::kTxTypeCallContract)
        {
            message.kind = EVMC_CALL;
            message.input_data = input.data();
            message.input_size = input.size();
            dev::u256 value = contractTransfer;
            if (value > 0)
            {
                dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
                memcpy(message.value.bytes, &by[0], by.size() * sizeof(uint8_t));
            }
        }
        return 0;
    }

    int RPC_MakeMessage(global::ca::TxType contractType, const evmc_address &sender, const evmc_address &recipient,
                    const evmc::bytes &input, const uint64_t &contractTransfer, evmc_message &message)
    {
        message.sender = sender;
        message.recipient = recipient;

        message.gas = 111657576591;

        if (contractType == global::ca::TxType::kTxTypeDeployContract)
        {
            message.kind = EVMC_CREATE;
        }
        else if (contractType == global::ca::TxType::kTxTypeCallContract)
        {
            message.kind = EVMC_CALL;
            message.input_data = input.data();
            message.input_size = input.size();
            dev::u256 value = contractTransfer;
            if (value > 0)
            {
                dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
                memcpy(message.value.bytes, &by[0], by.size() * sizeof(uint8_t));
            }
        }
        return 0;
    }
}

int64_t evm_environment::GetBlockNumber()
{
    DBReader dbReader;
    uint64_t top;
    if (DBStatus::DB_SUCCESS != dbReader.GetBlockTop(top))
    {
        ERRORLOG("GetBlockTop for evm block number failed !")
        return -1;
    }

    return Util::Unsign64toSign64(top + 1);
}

int64_t evm_environment::CalculateBlockTimestamp(int64_t predictOffset)
{
    int64_t nowTime = 0;
    try
    {
        nowTime = Util::Unsign64toSign64(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    }
    catch (const std::exception &e)
    {
        ERRORLOG("{}", e.what())
        return -1;
    }
    int64_t evmPrecisionTime = nowTime / 1000000;
    return Util::integerRound(evmPrecisionTime + predictOffset);
}

int64_t evm_environment::GetNonce(const std::string &address)
{
    std::vector<std::string> contractAddresses;
    DBReader dbReader;
    const auto &result = dbReader.GetContractAddrByDeployerAddr(address, contractAddresses);
    if (result == DBStatus::DB_NOT_FOUND)
    {
        return 0;
    }
    else if (result == DBStatus::DB_SUCCESS)
    {
        return Util::Unsign64toSign64(contractAddresses.size());
    }
    else
    {
        ERRORLOG("fail to read genesis block hash")
        return -1;
    }
}

int64_t evm_environment::GetNextNonce(const std::string &address)
{
    int64_t nonce = GetNonce(address);
    if (nonce < 0)
    {
        return -1;
    }
    return nonce + 1;

}

int evm_environment::MakeDeployMessage(const evmc_address &sender, const evmc_address &recipient, evmc_message &message)
{
    return MakeMessage(global::ca::TxType::kTxTypeDeployContract, sender, recipient, *std::unique_ptr<evmc::bytes>(), 0,
                       message);
}

int
evm_environment::MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                                 const uint64_t &contractTransfer, evmc_message &message)
{
    evmc_message msg{};
    int result = MakeMessage(global::ca::TxType::kTxTypeCallContract, sender, recipient, input, contractTransfer, msg);
    if (result == 0)
    {
        message = msg;
    }
    return result;
}

int
evm_environment::RPC_MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                                 const uint64_t &contractTransfer, evmc_message &message)
{
    evmc_message msg{};
    int result = RPC_MakeMessage(global::ca::TxType::kTxTypeCallContract, sender, recipient, input, contractTransfer, msg);
    if (result == 0)
    {
        message = msg;
    }
    return result;
}

int evm_environment::MakeDeployHost(const std::string &sender, const std::string &recipient, EvmHost &host,
                                    int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber,
                                    uint64_t transferAmount)
{
    return MakeHost(global::ca::TxType::kTxTypeDeployContract, sender, recipient, transferAmount,
                    *std::unique_ptr<evmc::bytes>(),
                    host, blockTimestamp, blockPrevRandao, blockNumber);
}

int evm_environment::MakeCallHost(const std::string &sender, const std::string &recipient, uint64_t transferAmount,
                                  const evmc::bytes &code,
                                  EvmHost &host, int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber)
{
    return MakeHost(global::ca::TxType::kTxTypeCallContract, sender, recipient, transferAmount, code, host,
                    blockTimestamp, blockPrevRandao, blockNumber);
}

int evm_environment::MakeTxContext(const std::string &from, evmc_tx_context &txContext, int64_t blockTimestamp,
                                   int64_t blockPrevRandao, int64_t blockNumber)
{
    txContext.tx_gas_price = evm_utils::Uint32ToEvmcUint256be(1);
    txContext.tx_origin = evm_utils::StringToEvmAddr(from);
    txContext.block_coinbase = evm_utils::StringToEvmAddr(
            MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
    txContext.block_timestamp = blockTimestamp;
    txContext.block_gas_limit = std::numeric_limits<int64_t>::max();
    txContext.block_prev_randao = evm_utils::Uint32ToEvmcUint256be(blockPrevRandao);
    txContext.block_base_fee = evm_utils::Uint32ToEvmcUint256be(1);

    txContext.block_number = blockNumber;
    int64_t chainId = GetChainId();
    if (chainId < 0)
    {
        return -2;
    }
    txContext.chain_id = evm_utils::Uint32ToEvmcUint256be(chainId);
    return 0;
}

int64_t evm_environment::CalculateBlockPrevRandao(const std::string &from)
{
    std::multiset<TxHelper::Utxo, TxHelper::UtxoCompare> setOutUtxos;
    uint64_t total;
    int result = TxHelper::FindUtxo(std::vector<std::string>({from}), TxHelper::kMaxVinSize, total, setOutUtxos);
    if (result != 0)
    {
        ERRORLOG("can't found utxo from {}", from)
        return -1;
    }

    std::string utxoString;
    for (const auto &utxo : setOutUtxos)
    {
        DEBUGLOG("PrevRandao hash: {}", utxo.hash)
        utxoString += utxo.hash;
    }
    return StringUtil::StringToNumber(utxoString);
}

int64_t evm_environment::GetBlockTimestamp(const CTransaction &transaction)
{
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        return txInfo[Evmone::contractBlockTimestampKeyName].get<int64_t>();
    }
    catch (...)
    {
        ERRORLOG("fail to get block timestamp from tx {}", transaction.hash())
        return -1;
    }
}

int64_t evm_environment::GetBlockPrevRandao(const CTransaction &transaction)
{
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        return txInfo[Evmone::contractBlockPrevRandaoKeyName].get<int64_t>();
    }
    catch (...)
    {
        ERRORLOG("fail to get block timestamp from tx {}", transaction.hash())
        return -1;
    }
}

int64_t evm_environment::VerifyEvmParameters(const CTransaction &transaction)
{
    const static int64_t blockTimestampTolerance = 20;

    int64_t blockTimestamp = CalculateBlockTimestamp(0);

    int64_t transactionBlockTimestamp = evm_environment::GetBlockTimestamp(transaction);
    if (transactionBlockTimestamp < 0)
    {
        return -5;
    }
    if (std::abs(blockTimestamp - transactionBlockTimestamp) > blockTimestampTolerance)
    {
        ERRORLOG("verify blockTimestamp fail, transaction timestamp: {} block timestamp: {}", transactionBlockTimestamp,
                 blockTimestamp)
        return -1;
    }

    int64_t transactionBlockPrevRandao = evm_environment::GetBlockPrevRandao(transaction);
    if (transactionBlockPrevRandao < 0)
    {
        return -6;
    }
    int64_t blockPrevRandao = CalculateBlockPrevRandao(transaction);
    if (blockPrevRandao < 0)
    {
        return -3;
    }

    if (blockPrevRandao != transactionBlockPrevRandao)
    {
        ERRORLOG("verify blockPrevRandao fail, transaction Randao: {} block Randao: {}", transactionBlockPrevRandao,
                 blockPrevRandao)
        return -4;
    }
    return 0;
}

int64_t evm_environment::CalculateBlockPrevRandao(const CTransaction &transaction)
{
    std::string from;
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
        from = txInfo[Evmone::contractSenderKeyName].get<std::string>();
    }
    catch (const std::exception &e)
    {
        ERRORLOG("parse transaction data fail {}", e.what())
        return -1;
    }

    std::string utxoString;
    for (const auto &vin : transaction.utxo().vin())
    {
        if (!vin.contractaddr().empty())
        {
            continue;
        }

        if (vin.vinsign().pub().empty())
        {
            ERRORLOG("can't read public from transaction {}", transaction.hash())
            return -2;
        }
        if (GenerateAddr(vin.vinsign().pub()) != from)
        {
            continue;
        }

        for (const auto &utxo : vin.prevout())
        {
            DEBUGLOG("PrevRandao hash: {}", utxo.hash())
            utxoString += utxo.hash();
        }
    }
    return StringUtil::StringToNumber(utxoString);
}