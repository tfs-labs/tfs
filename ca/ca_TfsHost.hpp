#ifndef __CA_TFSHOST_H__
#define __CA_TFSHOST_H__

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include <db/db_api.h>
#include "utils/console.h"
#include "mpt/trie.h"
#include "include/logging.h"
#include "ca_global.h"
#include "utils/json.hpp"
#include "ca_transaction.h"
#include "utils/ContractUtils.h"
// The string of bytes.
using bytes = std::basic_string<uint8_t>;
using namespace evmc;
#define PseudoInfinite 100000000000000000

// Extended value (by dirty flag) for account storage.
struct storage_value
{
    // The storage value.
    bytes32 value;

    // True means this value has been modified already by the current transaction.
    bool dirty{false};

    // Is the storage key cold or warm.
    evmc_access_status access_status{EVMC_ACCESS_COLD};

    // Default constructor.
    storage_value() noexcept = default;

    // Constructor.
    storage_value(const bytes32& _value, bool _dirty = false) noexcept  // NOLINT
      : value{_value}, dirty{_dirty}
    {}

    // Constructor with initial access status.
    storage_value(const bytes32& _value, evmc_access_status _access_status) noexcept
      : value{_value}, access_status{_access_status}
    {}
};

// account.
struct TfsAccount
{
    TfsAccount()
    {
        StorageRoot = std::make_shared<trie>();
    }
    void CreateTrie(const std::string& rootHash ,const std::string& ContractAddr)
    {
        if(rootHash.empty())
        {
            StorageRoot = std::make_shared<trie>(ContractAddr);
        }
        else
        {
            StorageRoot = std::make_shared<trie>(rootHash, ContractAddr);
        }
        
    }
    // The account nonce.
    int nonce = 0;

    // The account code.
    bytes code;

    // The code hash. Can be a value not related to the actual code.
    bytes32 codehash;

    // The account balance.
    uint256be balance;

    // The account storage map.
    std::unordered_map<bytes32, storage_value> storage;

    std::shared_ptr<trie> StorageRoot;

    void set_code(const bytes& code_) noexcept
    {
        code = code_;
        const auto* data = reinterpret_cast<const uint8_t*> (code_.c_str());
        std::string hash = keccak256(data, code_.length());
        dev::bytes by = dev::fromHex(hash);
        memcpy(codehash.bytes, &by[0], by.size() * sizeof(uint8_t));
    }    
    
    // Helper method for setting balance by numeric type.
    void set_balance(uint64_t x) noexcept
    {
    }
};

// EVMC Host implementation.
class TfsHost : public Host
{
public:
    TfsHost() = default;
    // LOG record.
    struct log_record
    {
        // The address of the account which created the log.
        address creator;

        // The data attached to the log.
        bytes data;

        // The log topics.
        std::vector<bytes32> topics;

        // Equal operator.
        bool operator==(const log_record& other) const noexcept
        {
            return creator == other.creator && data == other.data && topics == other.topics;
        }
    };

    // SELFDESTRUCT record.
    struct selfdestruct_record
    {
        // The address of the account which has self-destructed.
        address selfdestructed;

        // The address of the beneficiary account.
        address beneficiary;

        // Equal operator.
        bool operator==(const selfdestruct_record& other) const noexcept
        {
            return selfdestructed == other.selfdestructed && beneficiary == other.beneficiary;
        }
    };

    // The set of all accounts in the Host, organized by their addresses.
    std::unordered_map<address, TfsAccount> accounts;

    // The EVMC transaction context to be returned by get_tx_context().
    evmc_tx_context tx_context = {};

    // The block header hash value to be returned by get_block_hash().
    bytes32 block_hash = {};

    // The call result to be returned by the call() method.
    evmc_result call_result = {};

    // The record of all block numbers for which get_block_hash() was called.
    mutable std::vector<int64_t> recorded_blockhashes;

    // The record of all account accesses.
    mutable std::vector<address> recorded_account_accesses;

    // The maximum number of entries in recorded_account_accesses record.
    // This is arbitrary value useful in fuzzing when we don't want the record to explode.
    static constexpr auto max_recorded_account_accesses = 200;

    // The record of all call messages requested in the call() method.
    std::vector<evmc_message> recorded_calls;

    // The maximum number of entries in recorded_calls record.
    // This is arbitrary value useful in fuzzing when we don't want the record to explode.
    static constexpr auto max_recorded_calls = 100;

    // The record of all LOGs passed to the emit_log() method.
    std::vector<log_record> recorded_logs;

    // The record of all SELFDESTRUCTs from the selfdestruct() method.
    std::vector<selfdestruct_record> recorded_selfdestructs;
    
    std::vector<std::pair<std::string, uint64_t>> coin_transferrings;

private:
    // The copy of call inputs for the recorded_calls record.
    std::vector<bytes> m_recorded_calls_inputs;

    // Record an account access.
    // @param addr  The address of the accessed account.
    void record_account_access(const address& addr) const
    {
        if (recorded_account_accesses.empty())
            recorded_account_accesses.reserve(max_recorded_account_accesses);

        if (recorded_account_accesses.size() < max_recorded_account_accesses)
            recorded_account_accesses.emplace_back(addr);
    }

public:
    // Returns true if an account exists (EVMC Host method).
    bool account_exists(const address& addr) const noexcept override
    {
        record_account_access(addr);
        return accounts.count(addr) != 0;
    }

    // Get the account's storage value at the given key (EVMC Host method).
    bytes32 get_storage(const address& addr, const bytes32& key)const noexcept override
    {
        record_account_access(addr);

        const auto account_iter = accounts.find(addr);
        if (account_iter == accounts.end())
            return {};

        const auto storage_iter = account_iter->second.storage.find(key);
        if (storage_iter != account_iter->second.storage.end())
        {
            std::string k = evmc::hex({key.bytes,sizeof(key.bytes)});
            auto mptv = account_iter->second.StorageRoot->Get(k);
            if(!mptv.empty())
            {
                return evmc::literals::internal::from_hex<evmc::bytes32>(mptv.c_str());
            }

            return storage_iter->second.value;
        }
        else
        {
            std::string k = evmc::hex({key.bytes,sizeof(key.bytes)});
            auto value = account_iter->second.StorageRoot->Get(k);
            if(!value.empty())
            {
                return evmc::literals::internal::from_hex<evmc::bytes32>(value.c_str());
            }
        }
        return {};
    }

    // Set the account's storage value (EVMC Host method).
    evmc_storage_status set_storage(const address& addr,
                                    const bytes32& key,
                                    const bytes32& value) noexcept override
    {
        record_account_access(addr);

        // Get the reference to the old value.
        // This will create the account in case it was not present.
        // This is convenient for unit testing and standalone EVM execution to preserve the
        // storage values after the execution terminates.
        auto& old = accounts[addr].storage[key];

        evmc_storage_status status{};
        std::string mptk = evmc::hex({key.bytes,sizeof(key.bytes)});
        auto mptv = accounts[addr].StorageRoot->Get(mptk);

        //There is a value in the database to initialize the storage in the memory
        if(!mptv.empty())
        {
            old.value = evmc::literals::internal::from_hex<evmc::bytes32>(mptv.c_str());
            if(old.value != value)
            {  
                accounts[addr].StorageRoot->Update(evmc::hex({key.bytes,sizeof(key.bytes)}), evmc::hex({value.bytes,sizeof(value.bytes)}));
            }
            old.dirty = true;
            status = EVMC_STORAGE_ADDED;
            return status;
        }
        else
        {
            std::string mptKey = evmc::hex({key.bytes,sizeof(key.bytes)});
            std::string mptValue = evmc::hex({value.bytes,sizeof(value.bytes)});
            accounts[addr].StorageRoot->Update(mptKey, mptValue);
        }

        if (old.value == value)
        {
            //The & & value exists in both the database and storage and has not been modified
            return EVMC_STORAGE_UNCHANGED;
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
            status = EVMC_STORAGE_MODIFIED_AGAIN;

        old.value = value;
        return status;
    }

    // Get the account's balance (EVMC Host method).
    uint256be get_balance(const address& addr) const noexcept override
    {
        record_account_access(addr);

        uint64_t balance64 = 0;
        if (account_exists(addr))
        {
            balance64 = PseudoInfinite;
            uint256be balance = uint256be{};
            for (std::size_t i = 0; i < sizeof(balance64); ++i)
                balance.bytes[sizeof(balance) - 1 - i] = static_cast<uint8_t>(balance64 >> (8 * i));            
            return balance;    
        }
        
        if(GetBalanceByUtxo(evm_utils::EvmAddrToBase58(addr),balance64) == 0)
        {
            uint256be balance = uint256be{};
            for (std::size_t i = 0; i < sizeof(balance64); ++i)
                balance.bytes[sizeof(balance) - 1 - i] = static_cast<uint8_t>(balance64 >> (8 * i));            
            return balance;
        }

        return uint256be{};
    }

    // Get the account's code size (EVMC host method).
    size_t get_code_size(const address& addr) const noexcept override
    {
        record_account_access(addr);
        const auto it = accounts.find(addr);
        if (it == accounts.end())
            return 0;
        return it->second.code.size();
    }

    // Get the account's code hash (EVMC host method).
    bytes32 get_code_hash(const address& addr) const noexcept override
    {
        record_account_access(addr);
        const auto it = accounts.find(addr);
        if (it == accounts.end())
            return {};
        return it->second.codehash;
    }

    // Copy the account's code to the given buffer (EVMC host method).
    size_t copy_code(const address& addr,
                     size_t code_offset,
                     uint8_t* buffer_data,
                     size_t buffer_size) const noexcept override
    {
        record_account_access(addr);
        const auto it = accounts.find(addr);
        if (it == accounts.end())
            return 0;

        const auto& code = it->second.code;

        if (code_offset >= code.size())
            return 0;

        const auto n = std::min(buffer_size, code.size() - code_offset);

        if (n > 0)
            std::copy_n(&code[code_offset], n, buffer_data);
        return n;
    }

    // Selfdestruct the account (EVMC host method).
    void selfdestruct(const address& addr, const address& beneficiary) noexcept override
    {
        record_account_access(addr);
        recorded_selfdestructs.push_back({addr, beneficiary});
    }

    // Call/create other contract (EVMC host method).
    result call(const evmc_message& msg) noexcept override
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
            auto& call_msg = recorded_calls.back();
            if (call_msg.input_size > 0)
            {
                m_recorded_calls_inputs.emplace_back(call_msg.input_data, call_msg.input_size);
                const auto& input_copy = m_recorded_calls_inputs.back();
                call_msg.input_data = input_copy.data();
            }
        }

        evmc_result r = evmc::make_result(EVMC_FAILURE, 0, nullptr, 0);

        dev::bytes by2(msg.value.bytes, msg.value.bytes + sizeof(msg.value.bytes) / sizeof(uint8_t));
        uint64_t value_ll = dev::toUint64(dev::fromBigEndian<dev::u256>(by2));
        if(value_ll > 0)
        {
            coin_transferrings.emplace_back(evm_utils::EvmAddrToBase58(msg.code_address) , value_ll);
            return result{call_result};
        }

        std::string input = evmc::hex({msg.input_data,msg.input_size});

        DBReader data_reader;
        std::string ContractAddress = evmc::hex({msg.code_address.bytes,sizeof(msg.code_address.bytes)});

        std::string deployHash;
        if(data_reader.GetContractDeployUtxoByContractAddr(ContractAddress, deployHash) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetContractDeployUtxoByContractAddr failed!");
            return result{r};
        }
        std::string txRaw;
        if(data_reader.GetTransactionByHash(deployHash, txRaw) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetTransactionByHash failed!");
            return result{r};
        }
        CTransaction deployTx;
        if(!deployTx.ParseFromString(txRaw))
        {
            ERRORLOG("Transaction Parse failed!");
            return result{r};
        }
        std::string strCode;
        evmc::bytes code;
        try
        {
            nlohmann::json data_json = nlohmann::json::parse(deployTx.data());
            nlohmann::json tx_info = data_json["TxInfo"].get<nlohmann::json>();
            strCode = tx_info["Output"].get<std::string>();
            code = evmc::from_hex(strCode);
        }
        catch(const std::exception& e)
        {
            ERRORLOG("can't parse deploy contract transaction");
            return result{r};
        }

        if(msg.kind == EVMC_CALL)
        {
            DEBUGLOG("EVMC_CALL:");
            std::string rootHash;

            std::string strPrevTxHash;
            auto ret = data_reader.GetLatestUtxoByContractAddr(ContractAddress, strPrevTxHash);
            if(ret != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("GetLatestUtxoByContractAddr failed!");
                return result{r};
            }

            CTransaction PrevTx;
            std::string deployTxRaw;
            ret = data_reader.GetTransactionByHash(strPrevTxHash, deployTxRaw);
            if(ret != DBStatus::DB_SUCCESS)    
            {
                ERRORLOG("GetTransactionByHash failed!");
                return result{r}; 
            }
            if(!PrevTx.ParseFromString(deployTxRaw))
            {
                ERRORLOG("Transaction Parse failed!");
                return result{r}; 
            }
            try
            {
                nlohmann::json jPrevData = nlohmann::json::parse(PrevTx.data());
                nlohmann::json jPrevStorage = jPrevData["TxInfo"]["Storage"];
                if(!jPrevStorage.is_null())
                {
                    auto tx_type = (global::ca::TxType)PrevTx.txtype();
                    if(tx_type == global::ca::TxType::kTxTypeDeployContract)
                    {
                        rootHash = jPrevStorage[std::string("_") + "rootHash"].get<std::string>();
                    }
                    else
                    {
                        rootHash = jPrevStorage[ContractAddress + "_" + "rootHash"].get<std::string>();
                    }
                }
            }
            catch(...)
            {
                ERRORLOG("Parsing failed!");  
                return result{r};
            }

            this->accounts[msg.recipient].CreateTrie(rootHash, ContractAddress);
        }
        else if (msg.kind == EVMC_DELEGATECALL)
        {
            DEBUGLOG("EVMC_DELEGATECALL:");
        }
        struct evmc_vm* pvm = evmc_create_evmone();
        if (!pvm)
        {
            return result{r};
        }
        if (!evmc_is_abi_compatible(pvm))
        {
            return result{r};
        }
        evmc::VM vm{pvm};
        
        evmc::result re = vm.execute(*this, EVMC_MAX_REVISION, msg, code.data(), code.size());
        DEBUGLOG("ContractAddress: {} , Result: {}",ContractAddress, re.status_code);
        if (re.status_code != EVMC_SUCCESS)
        {
            ERRORLOG(RED "Evmone execution failed!" RESET);    
            auto strOutput = std::string_view(reinterpret_cast<const char *>(re.output_data), re.output_size);     
            DEBUGLOG("Output:   {}\n", strOutput);	
            return re;
        }
        std::string strOutput = std::move(evmc::hex({re.output_data, re.output_size}));
        DEBUGLOG("Output:   {}\n", strOutput);
        return re;
    }

    // Get transaction context (EVMC host method).
    evmc_tx_context get_tx_context() const noexcept override { return tx_context; }

    // Get the block header hash (EVMC host method).
    bytes32 get_block_hash(int64_t block_number) const noexcept override
    {
        recorded_blockhashes.emplace_back(block_number);
        return block_hash;
    }

    // Emit LOG (EVMC host method).
    void emit_log(const address& addr,
                  const uint8_t* data,
                  size_t data_size,
                  const bytes32 topics[],
                  size_t topics_count) noexcept override
    {
        recorded_logs.push_back({addr, {data, data_size}, {topics, topics + topics_count}});
    }

    // Record an account access.
    //
    // This method is required by EIP-2929 introduced in ::EVMC_BERLIN. It will record the account
    // access in MockedHost::recorded_account_accesses and return previous access status.
    // This methods returns ::EVMC_ACCESS_WARM for known addresses of precompiles.
    // The EIP-2929 specifies that evmc_message::sender and evmc_message::recipient are always
    // ::EVMC_ACCESS_WARM. Therefore, you should init the MockedHost with:
    //
    //     mocked_host.access_account(msg.sender);
    //     mocked_host.access_account(msg.recipient);
    //
    // The same way you can mock transaction access list (EIP-2930) for account addresses.
    //
    // @param addr  The address of the accessed account.
    // @returns     The ::EVMC_ACCESS_WARM if the account has been accessed before,
    //              the ::EVMC_ACCESS_COLD otherwise.
    evmc_access_status access_account(const address& addr) noexcept override
    {
        // Check if the address have been already accessed.

        const auto already_accessed =
            std::find(recorded_account_accesses.begin(), recorded_account_accesses.end(), addr) !=
            recorded_account_accesses.end();

        record_account_access(addr);

        // Accessing precompiled contracts is always warm.
        if (addr >= 0x0000000000000000000000000000000000000001_address &&
            addr <= 0x0000000000000000000000000000000000000009_address)
            return EVMC_ACCESS_WARM;

        return already_accessed ? EVMC_ACCESS_WARM : EVMC_ACCESS_COLD;
    }

    // Access the account's storage value at the given key.
    //
    // This method is required by EIP-2929 introduced in ::EVMC_BERLIN. In records that the given
    // account's storage key has been access and returns the previous access status.
    // To mock storage access list (EIP-2930), you can pre-init account's storage values with
    // the ::EVMC_ACCESS_WARM flag:
    //
    //     mocked_host.accounts[msg.recipient].storage[key] = {value, EVMC_ACCESS_WARM};
    //
    // @param addr  The account address.
    // @param key   The account's storage key.
    // @return      The ::EVMC_ACCESS_WARM if the storage key has been accessed before,
    //              the ::EVMC_ACCESS_COLD otherwise.
    evmc_access_status access_storage(const address& addr, const bytes32& key) noexcept override
    {
        auto& value = accounts[addr].storage[key];
        const auto access_status = value.access_status;
        value.access_status = EVMC_ACCESS_WARM;
        return access_status;
    }
};

#endif
