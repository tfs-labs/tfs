//
// Created by root on 2024/4/30.
//

#ifndef TFS_EVM_HOST_H
#define TFS_EVM_HOST_H

#include <evmc/evmc.hpp>
#include <trie.h>
#include "evm_host_data.hpp"

/**
 * @note Due to the Ethereum Virtual Machine (EVM) requiring access to most of variables
 *       within the host during contract deployment, setting these variables to private
 *       may cause the deployment to fail. To avoid this, member variables are
 *       temporarily set to public, and proper access control should be implemented
 *       in future versions.
 */

class EvmHost : public evmc::Host
{
public:
    explicit EvmHost(ContractDataCache *contractDataCache = nullptr);

    bool account_exists(const evmc::address &addr) const noexcept override;

    evmc::bytes32 get_storage(const evmc::address &addr, const evmc::bytes32 &key) const noexcept override;

    evmc_storage_status
    set_storage(const evmc::address &addr, const evmc::bytes32 &key, const evmc::bytes32 &value) noexcept override;

    evmc::uint256be get_balance(const evmc::address &addr) const noexcept override;

    size_t get_code_size(const evmc::address &addr) const noexcept override;

    evmc::bytes32 get_code_hash(const evmc::address &addr) const noexcept override;

    size_t copy_code(const evmc::address &addr, size_t code_offset, uint8_t *buffer_data,
                     size_t buffer_size) const noexcept override;

    bool selfdestruct(const evmc::address &addr, const evmc::address &beneficiary) noexcept override;

    evmc::Result call(const evmc_message &msg) noexcept override;

    evmc_tx_context get_tx_context() const noexcept override;

    evmc::bytes32 get_block_hash(int64_t block_number) const noexcept override;

    void emit_log(const evmc::address &addr, const uint8_t *data, size_t data_size, const evmc::bytes32 topics[],
                  size_t topics_count) noexcept override;

    evmc_access_status access_account(const evmc::address &addr) noexcept override;

    evmc_access_status access_storage(const evmc::address &addr, const evmc::bytes32 &key) noexcept override;

    evmc::bytes32 get_transient_storage(const evmc::address &addr, const evmc::bytes32 &key) const noexcept override;

    void set_transient_storage(const evmc::address &addr, const evmc::bytes32 &key,
                               const evmc::bytes32 &value) noexcept override;

//    evmc::bytes GetCode();
//
//    void SetCode(const evmc::bytes &code);
//
//    int64_t GetGasCost();
//
//    void SetGasCost(int64_t cost);
//
//    std::string GetOutput();

//    void SetOutput(const std::string &output);

//        inline std::vector<TransferInfo> GetTransferInfo();
//    void SetNonceCache(int64_t nonce);

//    std::map<std::string, std::string> GetCreatedContract() const;
    //    inline ContractDataCache* getContractDataCache();

    // The set of all accounts in the Host, organized by their addresses.
    mutable std::unordered_map<evmc::address, TfsAccount> accounts;

    // The EVMC transaction context to be returned by get_tx_context().
    evmc_tx_context tx_context = {};

    // The block header hash value to be returned by get_block_hash().
    evmc::bytes32 block_hash = {};

    // The call result to be returned by the call() method.
    evmc_result call_result = {};

    // The record of all block numbers for which get_block_hash() was called.
    mutable std::vector<int64_t> recorded_blockhashes;

    // The record of all account accesses.
    mutable std::vector<evmc::address> recorded_account_accesses;

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

    // The copy of call inputs for the recorded_calls record.
    std::vector<evmc::bytes> m_recorded_calls_inputs;

    evmc::bytes code;
    int64_t gas_cost;
    std::string output;
    std::string outputError;

    std::map<std::string, uint64_t> DBspendMap;
    int64_t nonceCache;
    std::map<std::string/* contract address */, std::string/* contract runtime code */> createdContract;


    void record_account_access(const evmc::address &addr) const;

    int getContractTransfer(uint64_t amount, const std::string &fromAddr, const std::string &toAddr,
                            std::vector<TransferInfo> &transferInfo);

    std::vector<TransferInfo> coin_transferrings;
    ContractDataCache *contractDataCache;
    static std::string calculateCreate2Address(const evmc_message& message) noexcept;

    evmc::Result
    createContract(const evmc_message &msg, uint64_t amount, const std::string &fromAddr, const std::string &contractAddress);

    std::set<std::string> loadContract;
};


#endif //TFS_EVM_HOST_H
