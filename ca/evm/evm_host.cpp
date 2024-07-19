//
// Created by root on 2024/4/30.
//

#include <db_api.h>
#include <utils/contract_utils.h>
#include <precompiles.hpp>
#include <ethash/keccak.hpp>
#include "evm_host.h"
#include "transaction.h"
#include "evm_manager.h"

EvmHost::EvmHost(ContractDataCache *contractDataCache)
{
    if (contractDataCache != nullptr)
    {
        this->contractDataCache = contractDataCache;
    }
    else
    {
        this->contractDataCache = nullptr;
    }
}

// Record an account access.
// @param addr  The address of the accessed account.
void EvmHost::record_account_access(const evmc::address &addr) const
{
    if (recorded_account_accesses.empty())
        recorded_account_accesses.reserve(max_recorded_account_accesses);

    if (recorded_account_accesses.size() < max_recorded_account_accesses)
        recorded_account_accesses.emplace_back(addr);

    auto found = accounts.find(addr);
    if (found != accounts.end())
    {
        if (!found->second.code.empty())
        {
            return;
        }
    }

    DBReader db_reader;
    std::string contractAddress = evm_utils::EvmAddrToString(addr);

    evmc::bytes code;
    if (evm_utils::GetContractCode(contractAddress, code) < 0)
    {
        ERRORLOG("fail to get contract code");
        return;
    }
    if (!code.empty())
    {
        accounts[addr].set_code(code);
    }
}

int EvmHost::getContractTransfer(uint64_t amount, const std::string &fromAddr, const std::string &toAddr,
                                 std::vector<TransferInfo> &transferInfo)
{
    if (amount > 0)
    {
        for (auto &iter : coin_transferrings)
        {
            if (iter.amount == 0)
            {
                continue;
            }
            if (iter.to == fromAddr)
            {
                if (iter.amount >= amount)
                {
                    iter.amount = iter.amount - amount;
                    transferInfo.emplace_back(iter.from, toAddr, amount);
                    amount = 0;
                    break;
                }
                else
                {
                    if (iter.from != toAddr)
                    {
                        transferInfo.emplace_back(iter.from, toAddr, iter.amount);
                    }
                    amount = amount - iter.amount;
                    iter.amount = 0;
                    uint64_t balance = 0;
                    GetBalanceByUtxo(fromAddr, balance);
                    DBspendMap[fromAddr] += amount;
                    if (balance < DBspendMap[fromAddr])
                    {
                        ERRORLOG("fromAddr:{}, balance:{} < amount:{} failed!", fromAddr, balance,
                                 DBspendMap[fromAddr]);
                        return -1;
                    }
                }
            }
        }
        transferInfo.emplace_back(fromAddr, toAddr, amount);
    }
    return 0;
}

// Returns true if an account exists (EVMC Host method).
bool EvmHost::account_exists(const evmc::address &addr) const noexcept
{
    record_account_access(addr);
    return accounts.count(addr) != 0;
}

evmc::bytes32 EvmHost::get_storage(const evmc::address &addr, const evmc::bytes32 &key) const noexcept
{
    record_account_access(addr);

    const auto account_iter = accounts.find(addr);
    if (account_iter == accounts.end())
        return {};

    const auto storage_iter = account_iter->second.storage.find(key);
    if (storage_iter != account_iter->second.storage.end())
    {
        std::string k = evmc::hex({key.bytes, sizeof(key.bytes)});
        auto mptv = account_iter->second.storageRoot->Get(k);
        if (!mptv.empty())
        {
            return evmc::from_hex<evmc::bytes32>(mptv.c_str()).value_or(evmc::bytes32{});
        }

        return storage_iter->second.value;
    }
    else
    {
        std::string k = evmc::hex({key.bytes, sizeof(key.bytes)});
        auto value = account_iter->second.storageRoot->Get(k);
        if (!value.empty())
        {
            return evmc::from_hex<evmc::bytes32>(value.c_str()).value_or(evmc::bytes32{});
        }
    }
    return {};
}

evmc_storage_status
EvmHost::set_storage(const evmc::address &addr, const evmc::bytes32 &key, const evmc::bytes32 &value) noexcept
{
    record_account_access(addr);

    auto &old = accounts[addr].storage[key];

    evmc_storage_status status{};
    std::string mptk = evmc::hex({key.bytes, sizeof(key.bytes)});
    auto mptv = accounts[addr].storageRoot->Get(mptk);

    if (!mptv.empty())
    {
        old.value = evmc::from_hex<evmc::bytes32>(mptv.c_str()).value_or(evmc::bytes32{});
        if (old.value != value)
        {
            accounts[addr].storageRoot->Update(evmc::hex({key.bytes, sizeof(key.bytes)}),
                                               evmc::hex({value.bytes, sizeof(value.bytes)}));
        }
        old.dirty = true;
        status = EVMC_STORAGE_ADDED;
        return status;
    }
    else
    {
        std::string mptKey = evmc::hex({key.bytes, sizeof(key.bytes)});
        std::string mptValue = evmc::hex({value.bytes, sizeof(value.bytes)});
        accounts[addr].storageRoot->Update(mptKey, mptValue);
    }

    if (old.value == value)
    {
        return EVMC_STORAGE_ASSIGNED;;
    }
    if (!old.dirty)
    {
        old.dirty = true;
        if (!old.value)
            status = EVMC_STORAGE_ADDED;
        else if (value)
            status = EVMC_STORAGE_MODIFIED;
        else
            status = EVMC_STORAGE_DELETED;
    }
    else
        status = EVMC_STORAGE_ASSIGNED;

    old.value = value;
    return status;
}

evmc::uint256be EvmHost::get_balance(const evmc::address &addr) const noexcept
{
    record_account_access(addr);

    uint64_t amount = 0;
    auto Addr = evm_utils::EvmAddrToString(addr);
    if (GetBalanceByUtxo(Addr, amount) != 0)
    {
        amount = 0;
    }

    for (auto &iter : coin_transferrings)
    {
        if (iter.to == Addr)
        {
            amount += iter.amount;
        }
    }

    auto found = DBspendMap.find(Addr);
    if (found != DBspendMap.end() && found->second <= amount)
    {
        amount -= found->second;
    }

    evmc::uint256be balance = evmc::uint256be{};
    for (std::size_t i = 0; i < sizeof(amount); ++i)
        balance.bytes[sizeof(balance) - 1 - i] = static_cast<uint8_t>(amount >> (8 * i));

    return balance;
}

size_t EvmHost::get_code_size(const evmc::address &addr) const noexcept
{
    record_account_access(addr);
    const auto it = accounts.find(addr);
    if (it == accounts.end())
    {
        DEBUGLOG("can't find code of address {}", evm_utils::EvmAddrToString(addr));
        return 0;
    }

    return it->second.code.size();
}

evmc::bytes32 EvmHost::get_code_hash(const evmc::address &addr) const noexcept
{
    record_account_access(addr);
    const auto it = accounts.find(addr);
    if (it == accounts.end())
        return {};
    return it->second.codehash;
}

size_t EvmHost::copy_code(const evmc::address &addr, size_t code_offset, uint8_t *buffer_data,
                          size_t buffer_size) const noexcept
{
    record_account_access(addr);
    const auto it = accounts.find(addr);
    if (it == accounts.end())
        return 0;

    const auto &code = it->second.code;

    if (code_offset >= code.size())
        return 0;

    const auto n = std::min(buffer_size, code.size() - code_offset);

    if (n > 0)
        std::copy_n(&code[code_offset], n, buffer_data);
    return n;
}

bool EvmHost::selfdestruct(const evmc::address &addr, const evmc::address &beneficiary) noexcept
{
    record_account_access(addr);
    recorded_selfdestructs.push_back({addr, beneficiary});

    std::string fromAddr = evm_utils::EvmAddrToString(addr);
    std::string toAddr = evm_utils::EvmAddrToString(beneficiary);

    evmc::uint256be value = get_balance(addr);
    dev::bytes by2(value.bytes, value.bytes + sizeof(value.bytes) / sizeof(uint8_t));
    uint64_t amount = dev::toUint64(dev::fromBigEndian<dev::u256>(by2));

    for (auto &iter : coin_transferrings)
    {
        if (iter.amount == 0)
        {
            continue;
        }
        if (iter.to == fromAddr)
        {
            amount -= iter.amount;
            iter.to = toAddr;
        }
    }

    DEBUGLOG("fromAddr:{}, toAddr:{}, amount:{}", fromAddr, toAddr, amount);
    coin_transferrings.emplace_back(fromAddr, toAddr, amount);
    return true;
}

evmc::Result EvmHost::call(const evmc_message &msg) noexcept
{

    record_account_access(msg.recipient);
    if (recorded_calls.empty())
    {
        recorded_calls.reserve(max_recorded_calls);
        m_recorded_calls_inputs.reserve(max_recorded_calls);  // Iterators will not invalidate.
    }

    if (recorded_calls.size() < max_recorded_calls)
    {
        recorded_calls.emplace_back(msg);
        auto &call_msg = recorded_calls.back();
        if (call_msg.input_size > 0)
        {
            m_recorded_calls_inputs.emplace_back(call_msg.input_data, call_msg.input_size);
            const auto &input_copy = m_recorded_calls_inputs.back();
            call_msg.input_data = input_copy.data();
        }
    }

    evmc_result evmc_success_result = evmc::make_result(EVMC_SUCCESS, msg.gas, 0, 0, 0);
    evmc_result evmc_failure_result = evmc::make_result(EVMC_FAILURE, msg.gas, 0, 0, 0);

    auto found = std::find_if(recorded_selfdestructs.begin(), recorded_selfdestructs.end(),
                              [msg](const selfdestruct_record &item) {

                                  if (item.selfdestructed == msg.code_address)
                                  {
                                      return true;
                                  }
                                  return false;
                              });
    if (found != recorded_selfdestructs.end())
    {
        DEBUGLOG("Contract selfdestructs addr:{}", evm_utils::EvmAddrToString(found->selfdestructed));
        return evmc::Result{evmc_failure_result};
    }


    dev::bytes by2(msg.value.bytes, msg.value.bytes + sizeof(msg.value.bytes) / sizeof(uint8_t));
    uint64_t amount = dev::toUint64(dev::fromBigEndian<dev::u256>(by2));
    std::string fromAddr = evm_utils::EvmAddrToString(msg.sender);
    std::string toAddr = evm_utils::EvmAddrToString(msg.code_address);

    std::string input = evmc::hex({msg.input_data, msg.input_size});

    if (msg.kind == EVMC_CREATE)
    {
        ++nonceCache;
        std::string contractAddress = GenerateAddr(fromAddr + std::to_string(nonceCache));
		return createContract(msg, amount, fromAddr, contractAddress);

    }
    else if (msg.kind == EVMC_CREATE2)
    {
        ++nonceCache;
        std::string contractAddress = calculateCreate2Address(msg);
        return createContract(msg, amount, fromAddr, contractAddress);

    }

    std::vector<TransferInfo> tmp_transferrings;
    if (getContractTransfer(amount, fromAddr, toAddr, tmp_transferrings) != 0)
    {
        return evmc::Result{evmc_failure_result};
    }

    coin_transferrings.insert(coin_transferrings.end(), tmp_transferrings.begin(), tmp_transferrings.end());

    if (msg.input_size <= 0)
    {
        DEBUGLOG("empty input data, sender: {}, code_address: {}", fromAddr, toAddr);
        return evmc::Result{evmc_success_result};
    }


    if (auto precompiled_result = evmone::state::call_precompile(EVMC_MAX_REVISION,
                                                                 msg); precompiled_result.has_value())
    {
        return std::move(*precompiled_result);
    }


    std::string contractAddress = evm_utils::EvmAddrToString(msg.code_address);
    evmc::bytes code;
    bool newlyCreatedContract = false;
    auto codeFound = createdContract.find(contractAddress);
    if (codeFound != createdContract.end())
    {
        code = evm_utils::StringTobytes(codeFound->second);
        newlyCreatedContract = true;
    }
    else
    {
        int foundResult = evm_utils::GetContractCode(contractAddress, code);
        if (foundResult > 0)
        {
            return evmc::Result{evmc_success_result};
        }
        else if (foundResult < 0)
        {
            ERRORLOG("fail to get contract code");
            return evmc::Result{evmc_failure_result};
        }
        else
        {
            if (code.empty())
            {
                ERRORLOG("contract code is empty");
                return evmc::Result{evmc_failure_result};
            }
        }
    }
    bool loadedContract = loadContract.find(contractAddress) != loadContract.end();

    if (msg.kind == EVMC_CALL && !newlyCreatedContract && !loadedContract)
    {
        DEBUGLOG("EVMC_CALL:");
        std::string rootHash;
        int ret = GetContractRootHash(contractAddress, rootHash, contractDataCache);
        if (ret != 0)
        {
            return evmc::Result{evmc_failure_result};
        }
        this->accounts[msg.recipient].CreateTrie(rootHash, contractAddress, contractDataCache);
		loadContract.insert(contractAddress);
    }
    else if (msg.kind == EVMC_DELEGATECALL)
    {
        DEBUGLOG("EVMC_DELEGATECALL:");
    }
    struct evmc_vm *pvm = evmc_create_evmone();
    if (!pvm)
    {
        return evmc::Result{evmc_failure_result};
    }
    if (!evmc_is_abi_compatible(pvm))
    {
        return evmc::Result{evmc_failure_result};
    }
    evmc::VM vm{pvm};

    evmc::Result re = vm.execute(*this, EVMC_MAX_REVISION, msg, code.data(), code.size());
    DEBUGLOG("ContractAddress: {} , Result: {}", contractAddress, re.status_code);
    if (re.status_code != EVMC_SUCCESS)
    {
        ERRORLOG("Evmone execution failed!");
        auto strOutput = std::string_view(reinterpret_cast<const char *>(re.output_data), re.output_size);
        DEBUGLOG("Output:   {}\n", strOutput);
        return re;
    }
    std::string strOutput = std::move(evmc::hex({re.output_data, re.output_size}));
    DEBUGLOG("Output:   {}\n", strOutput);
    return re;
}

evmc::Result EvmHost::createContract(const evmc_message &msg, uint64_t amount, const std::string &fromAddr,
                                     const std::string &contractAddress)
                                     {
    evmc_result evmc_success_result = evmc::make_result(EVMC_SUCCESS, msg.gas, 0, 0, 0);
    evmc_result evmc_failure_result = evmc::make_result(EVMC_FAILURE, msg.gas, 0, 0, 0);
    if (amount >= 0)
    {
        std::vector<TransferInfo> tmp_transferrings;
        if (getContractTransfer(amount, fromAddr, contractAddress, tmp_transferrings) != 0)
        {
            return evmc::Result{evmc_failure_result};
        }
        coin_transferrings.insert(coin_transferrings.end(), tmp_transferrings.begin(), tmp_transferrings.end());
    }

    std::string runtimeCode;
    evmc_message createMessage = msg;
    createMessage.recipient = evm_utils::StringToEvmAddr(contractAddress);
    accounts[createMessage.recipient].CreateTrie("", evm_utils::EvmAddrToString(createMessage.recipient));
    int result = Evmone::SynchronousExecuteByEvmone(createMessage, {createMessage.input_data, createMessage.input_size}, *this, runtimeCode);
    if (result != 0)
    {
        return evmc::Result{evmc_failure_result};
    }
    createdContract[contractAddress] = runtimeCode;
    evmc_success_result.create_address = evm_utils::StringToEvmAddr(contractAddress);
    auto codeConvertResult = evmc::from_hex(runtimeCode);
    if(!codeConvertResult.has_value())
    {
        ERRORLOG("fail to convert code to hex , code: {}", runtimeCode);
        return evmc::Result{evmc_failure_result};
    }
    accounts[createMessage.recipient].set_code(codeConvertResult.value());
    return evmc::Result{evmc_success_result};
}


evmc_tx_context EvmHost::get_tx_context() const noexcept
{
    return tx_context;
}

evmc::bytes32 EvmHost::get_block_hash(int64_t block_number) const noexcept
{
    recorded_blockhashes.emplace_back(block_number);
    return block_hash;
}

void EvmHost::emit_log(const evmc::address &addr, const uint8_t *data, size_t data_size, const evmc::bytes32 topics[],
                       size_t topics_count) noexcept
{
    recorded_logs.push_back({addr, {data, data_size}, {topics, topics + topics_count}});
}

evmc_access_status EvmHost::access_account(const evmc::address &addr) noexcept
{
    const auto already_accessed =
            std::find(recorded_account_accesses.begin(), recorded_account_accesses.end(), addr) !=
            recorded_account_accesses.end();

    record_account_access(addr);

    using namespace evmc;
    if (addr >= 0x0000000000000000000000000000000000000001_address &&
        addr <= 0x0000000000000000000000000000000000000009_address)
        return EVMC_ACCESS_WARM;

    return already_accessed ? EVMC_ACCESS_WARM : EVMC_ACCESS_COLD;
}

evmc_access_status EvmHost::access_storage(const evmc::address &addr, const evmc::bytes32 &key) noexcept
{
    auto &value = accounts[addr].storage[key];
    const auto access_status = value.access_status;
    value.access_status = EVMC_ACCESS_WARM;
    return access_status;
}

/// @copydoc evmc_host_interface::get_transient_storage
evmc::bytes32 EvmHost::get_transient_storage(const evmc::address &addr, const evmc::bytes32 &key) const noexcept
{
    return {};
}

/// @copydoc evmc_host_interface::set_transient_storage
void
EvmHost::set_transient_storage(const evmc::address &addr, const evmc::bytes32 &key, const evmc::bytes32 &value) noexcept
{

}

std::string EvmHost::calculateCreate2Address(const evmc_message& message) noexcept
{
    static constexpr uint8_t preventCollisionConstant = 0xff;
    static constexpr size_t evmAddressSize = 20;
    static constexpr size_t create2SaltSize = 32;
    static constexpr size_t keccakHashSize = 32;
    static constexpr size_t create2SeedSize = sizeof preventCollisionConstant + evmAddressSize + create2SaltSize + keccakHashSize;

    uint8_t create2Seed[create2SeedSize] = {0};

    create2Seed[0] = preventCollisionConstant;
    uint32_t offset = sizeof preventCollisionConstant;

    std::memcpy(create2Seed + offset, message.sender.bytes, evmAddressSize);
    offset += evmAddressSize;

    std::memcpy(create2Seed + offset, message.create2_salt.bytes, create2SaltSize);
    offset += create2SaltSize;

    uint8_t* initCodeHash = ethash::keccak256(message.input_data, message.input_size).bytes;
    std::memcpy(create2Seed + offset, initCodeHash, keccakHashSize);

    uint8_t* addressBase = ethash::keccak256(create2Seed,create2SeedSize).bytes;
    static constexpr size_t evmAddressOffset = keccakHashSize - evmAddressSize;
    evmc_address address = {0};
    std::memcpy(address.bytes, addressBase + evmAddressOffset, sizeof address);

    std::string contractAddress = evm_utils::EvmAddrToString(address);
    return contractAddress;
}

//evmc::bytes EvmHost::GetCode()
//{
//    return code;
//}
//
//void EvmHost::SetCode(const evmc::bytes &code)
//{
//    this->code = code;
//}
//
//int64_t EvmHost::GetGasCost()
//{
//    return gas_cost;
//}
//
//void EvmHost::SetGasCost(int64_t cost)
//{
//    this->gas_cost = cost;
//}
//
//std::string EvmHost::GetOutput()
//{
//    return output;
//}
//
//void EvmHost::SetOutput(const std::string &output)
//{
//    this->output = output;
//}

//std::vector<TransferInfo> EvmHost::GetTransferInfo()
//{
//    return coin_transferrings;
//}
//
//void EvmHost::SetNonceCache(int64_t nonce)
//{
//    nonceCache = nonce;
//}
//
//std::map<std::string, std::string> EvmHost::GetCreatedContract() const
//{
//    return createdContract;
//}
