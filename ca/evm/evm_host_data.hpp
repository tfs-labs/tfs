//
// Created by root on 2024/4/30.
//

#ifndef TFS_EVM_HOST_DATA_HPP
#define TFS_EVM_HOST_DATA_HPP

/**
 * @brief       Extended value (by dirty flag) for account storage.
 *
 */

#include <evmone/evmone.h>
#include "../utils/keccak_cryopp.hpp"
struct StorageValue
{
    // The storage value.
    evmc::bytes32 value;

    // True means this value has been modified already by the current transaction.
    bool dirty{false};

    // Is the storage key cold or warm.
    evmc_access_status access_status{EVMC_ACCESS_COLD};

    // Default constructor.
    StorageValue() noexcept = default;

    // Constructor.
    StorageValue(const evmc::bytes32 &_value, bool _dirty = false) noexcept
            : value{_value}, dirty{_dirty}
    {}

    // Constructor with initial access status.
    StorageValue(const evmc::bytes32 &_value, evmc_access_status _access_status) noexcept
            : value{_value}, access_status{_access_status}
    {}
};


/**
 * @brief       account.
 *
 */
struct TfsAccount
{
    TfsAccount()
    {
        storageRoot = std::make_shared<Trie>();
    }

    void CreateTrie(const std::string &rootHash, const std::string &ContractAddr)
    {
        if (rootHash.empty())
        {
            storageRoot = std::make_shared<Trie>(ContractAddr);
        }
        else
        {
            storageRoot = std::make_shared<Trie>(rootHash, ContractAddr);
        }
    }

    void CreateTrie(const std::string &rootHash, const std::string &ContractAddr, ContractDataCache *contractDataCache)
    {
        if (rootHash.empty())
        {
            storageRoot = std::make_shared<Trie>(ContractAddr, contractDataCache);
        }
        else
        {
            storageRoot = std::make_shared<Trie>(rootHash, ContractAddr, contractDataCache);
        }

    }

    // The account nonce.
    int nonce = 0;

    // The account code.
    evmc::bytes code;

    // The code hash. Can be a value not related to the actual code.
    evmc::bytes32 codehash;

    // The account balance.
    evmc::uint256be balance;

    // The account storage map.
    std::unordered_map<evmc::bytes32, StorageValue> storage;

    std::shared_ptr<Trie> storageRoot;

    void set_code(const evmc::bytes &code_) noexcept
    {
        code = code_;
        
        const std::string& input = std::string(reinterpret_cast<const char*>(code_.data()), code_.size());
        // const auto *data = reinterpret_cast<const uint8_t *> (code_.c_str());
        std::string hash = Keccak256Crypt(input);
        // std::string hash = keccak256(data, code_.length());
        dev::bytes by = dev::fromHex(hash);
        memcpy(codehash.bytes, &by[0], by.size() * sizeof(uint8_t));
    }

    // Helper method for setting balance by numeric type.
    void set_balance(uint64_t x) noexcept
    {
    }
};

struct TransferInfo
{
    TransferInfo(std::string from, std::string to, uint64_t amount)
    {
        this->from = from;
        this->to = to;
        this->amount = amount;
    }

    std::string from;
    std::string to;
    uint64_t amount;
};

// LOG record.
struct log_record
{
    // The address of the account which created the log.
    evmc::address creator;

    // The data attached to the log.
    evmc::bytes data;

    // The log topics.
    std::vector<evmc::bytes32> topics;

    // Equal operator.
    bool operator==(const log_record &other) const noexcept
    {
        return creator == other.creator && data == other.data && topics == other.topics;
    }
};

// SELFDESTRUCT record.
struct selfdestruct_record
{
    // The address of the account which has self-destructed.
    evmc::address selfdestructed;

    // The address of the beneficiary account.
    evmc::address beneficiary;

    // Equal operator.
    bool operator==(const selfdestruct_record &other) const noexcept
    {
        return selfdestructed == other.selfdestructed && beneficiary == other.beneficiary;
    }
};


#endif //TFS_EVM_HOST_DATA_HPP
