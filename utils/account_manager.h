/**
 * *****************************************************************************
 * @file        account_manager.h
 * @brief       
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _EDManager_
#define _EDManager_

#include <iostream>
#include <string>
#include <filesystem>
#include <dirent.h>

#include "hex_code.h"
#include "utils/time_util.h"
#include "magic_singleton.h"
#include "../ca/global.h"
#include "../include/logging.h"
#include "utils/json.hpp"
#include "utils/bip39.h"
#include "include/scope_guard.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>

/**
 * @brief       
 * 
 */
static const int PrimeSeedNum = 16;
static const int DerivedSeedNum = 32;
class Account
{
    public:
        Account(const bool isNeedInitialize) :_pkey(nullptr, &EVP_PKEY_free){
            _GenerateRandomSeed();
            _GeneratePkey();
        }
        Account():_pkey(nullptr, &EVP_PKEY_free),_pubStr(), _priStr(), _Addr(), _seed{}{}

        /**
         * @brief       Construct a new Account object
         * 
         * @param       bs58Addr: 
         */
        Account(const std::string &addressFileName):_pkey(nullptr, &EVP_PKEY_free)
        {
            _GenerateFilePkey(addressFileName);
        }

        /**
         * @brief       Construct a new Account object
         * 
         * @param       other: 
         */
        Account(const Account& other);

        /**
         * @brief       Construct a new Account object
         * 
         * @param       other: 
         */
        Account(Account&& other) noexcept;

        /**
         * @brief       
         * 
         * @param       other: 
         * @return      Account& 
         */
        Account& operator=(const Account& other);

        /**
         * @brief       
         * 
         * @param       other: 
         * @return      Account& 
         */
        Account& operator=(Account&& other) noexcept;

        /**
         * @brief       Destroy the Account object
         * 
         */
        ~Account(){ _pkey.reset(); };

        /**
         * @brief       
         * 
         * @param       message: 
         * @param       signature: 
         * @return      true 
         * @return      false 
         */
        bool Sign(const std::string &message, std::string &signature);

        /**
         * @brief       
         * 
         * @param       message: 
         * @param       signature: 
         * @return      true 
         * @return      false 
         */
        bool Verify(const std::string &message, std::string &signature);
        
        /**
         * @brief       Get the Key object
         * 
         * @return      EVP_PKEY* 
         */
        EVP_PKEY * GetKey () const
        {
            return _pkey.get();
        }

        /**
         * @brief       Set the Key object
         * 
         * @param       key: 
         */
        void SetKey(EVP_PKEY * key)
        {
            _pkey.reset(key);
        }
        
        /**
         * @brief       Get the Pub Str object
         * 
         * @return      std::string 
         */
        std::string GetPubStr() const
        {
            return _pubStr;
        }
        
        /**
         * @brief       Set the Pub Str object
         * 
         * @param       str: 
         */
        void SetPubStr(std::string &str)
        {
            _pubStr = str;
        }

        /**
         * @brief       Get the Pri Str object
         * 
         * @return      std::string 
         */
        std::string GetPriStr() const
        {
            return _priStr;
        }

        /**
         * @brief       Set the Pri Str object
         * 
         * @param       str: 
         */
        void SetPriStr(std::string &str)
        {
            _priStr = str;
        }

        /**
         * @brief       Get the Base 5 8 object
         * 
         * @return      std::string 
         */
        std::string GetAddr() const
        {
            return _Addr;
        }

        /**
         * @brief       Set the Base 5 8 object
         * 
         * @param       addr: 
         */
        void SetAddr(std::string & addr)
        {
            _Addr = addr;
        }

        uint8_t* GetSeed()
        {
            return _seed;
        }

        void SetSeed(const uint8_t* seed)
        {
            for(int i = 0;i < PrimeSeedNum ; i++)
            {
                _seed[i] =seed[i];
            }            
        }
        
    private:
        /**
         * @brief  Generate a public key through a pointer
         * 
         * 
         */
        void _GeneratePubStr(EVP_PKEY* pkeyPtr);

        /**
         * @brief       _GeneratePriStr
         * 
         */
        void _GeneratePriStr(EVP_PKEY* pkeyPtr);

        /**
         * @brief      _GenerateAddr 
         * 
         */
        void _GenerateAddr();
        
        /**
         * @brief      _GenerateRandomSeed 
         * 
         */
        void _GenerateRandomSeed();
        
        /**
         * @brief     _GeneratePkey  
         * 
         */
        void _GeneratePkey();

        /**
         * @brief       _GenerateFilePkey
         * 
         * @param       base58Address 
         */
        void _GenerateFilePkey(const std::string &base58Address);
    private:
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> _pkey;
        std::string _pubStr;
        std::string _priStr;
        std::string _Addr;
        uint8_t  _seed[PrimeSeedNum];
};

/**
 * @brief       
 * 
 */
class AccountManager
{
    public:
        AccountManager();
        ~AccountManager() = default;

        /**
         * @brief       
         * 
         * @param       account: 
         * @return      int 
         */
        int AddAccount(Account &account);

        /**
         * @brief       
         * 
         */
        void PrintAllAccount() const;

        /**
         * @brief       
         * 
         * @param       addr: 
         */
        void DeleteAccount(const std::string& addr);

        /**
         * @brief       Set the Default Base 5 8 Addr object
         * 
         * @param       bs58Addr: 
         */
        void SetDefaultAddr(const std::string & bs58Addr);

        /**
         * @brief       Get the Default Base 5 8 Addr object
         * 
         * @return      std::string 
         */
        std::string GetDefaultAddr() const;

        /**
         * @brief       Set the Default Account object
         * 
         * @param       bs58Addr: 
         * @return      int 
         */
        int SetDefaultAccount(const std::string & bs58Addr);

        /**
         * @brief       
         * 
         * @param       bs58Addr: 
         * @return      true 
         * @return      false 
         */
        bool IsExist(const std::string & bs58Addr);

        /**
         * @brief       
         * 
         * @param       bs58Addr: 
         * @param       account: 
         * @return      int 
         */
        int FindAccount(const std::string & bs58Addr, Account & account);

        /**
         * @brief       Get the Default Account object
         * 
         * @param       account: 
         * @return      int 
         */
        int GetDefaultAccount(Account & account);

        /**
         * @brief       Get the Account List object
         * 
         * @param       _list: 
         */
        void GetAccountList(std::vector<std::string> & _list);

        /**
         * @brief       The public key is obtained by public key binary
         * 
         * @param       pubStr: 
         * @param       account: 
         * @return      true 
         * @return      false 
         */
        bool GetAccountPubByBytes(const std::string &pubStr, Account &account);

        /**
         * @brief       
         * 
         * @param       Addr: 
         * @return      int 
         */
        int SavePrivateKeyToFile(const std::string & Addr);

        /**
         * @brief       Get the Mnemonic object
         * 
         * @param       bs58Addr: 
         * @param       mnemonic: 
         * @return      int 
         */
        int GetMnemonic(const std::string & bs58Addr, std::string & mnemonic);

        /**
         * @brief       
         * 
         * @param       mnemonic: 
         * @return      int 
         */
        int ImportMnemonic(const std::string & mnemonic);
        
        /**
         * @brief       Get the Private Key Hex object
         * 
         * @param       bs58Addr: 
         * @param       privateKeyHex: 
         * @return      int 
         */
        int GetPrivateKeyHex(const std::string & bs58Addr, std::string & privateKeyHex);

        /**
         * @brief       
         * 
         * @param       privateKeyHex: 
         * @return      int 
         */
        int ImportPrivateKeyHex(uint8_t *inputSeed);

    private:
        std::string _defaultAddr;
        std::map<std::string /*addr*/, std::shared_ptr<Account>> _accountList;
        
        int _init();
};
class VerifiedAddressSet {
private:
    std::unordered_set<std::string> verifiedAddresses;
    std::mutex mutex;

public:
    void markAddressVerified(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex);
        verifiedAddresses.insert(address);
    }

    bool isAddressVerified(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex);
        return verifiedAddresses.count(address) > 0;
    }
};

class AddressCache {
private:
    std::unordered_map<std::string, std::string> cache;
    std::mutex mutex;

public:
    void addPublicKey(const std::string& publicKey, const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex);
        cache[publicKey] = address;
    }

    std::string getAddress(const std::string& publicKey) {
        std::lock_guard<std::mutex> lock(mutex);
        if (cache.count(publicKey) > 0) {
            return cache[publicKey];
        }
        return "";
    }
};

bool isValidAddress(const std::string& address);

/**
 * @brief       
 * 
 * @param       publicKey: 
 * @return      std::string 
 */
std::string GenerateAddr(const std::string& publicKey);

/**
 * @brief       
 * 
 */
void TestED25519Time();

/**
 * @brief       
 * 
 */
void TestEDFunction();

/**
 * @brief       
 * 
 * @param       dataSource: 
 * @return      std::string 
 */
std::string Base64Encode(const std::string & dataSource);

/**
 * @brief       
 * 
 * @param       dataSource: 
 * @return      std::string 
 */
std::string Base64Decode(const std::string & dataSource);

/**
 * @brief       
 * 
 * @param       text: 
 * @return      std::string 
 */
std::string Getsha256hash(const std::string & text);

/**
 * @brief       
 * 
 * @param       message: 
 * @param       pkey: 
 * @param       signature: 
 * @return      true 
 * @return      false 
 */
bool ED25519SignMessage(const std::string &message, EVP_PKEY* pkey, std::string &signature);

/**
 * @brief       
 * 
 * @param       message: 
 * @param       pkey: 
 * @param       signature: 
 * @return      true 
 * @return      false 
 */
bool ED25519VerifyMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature);

/**
 * @brief       
 * 
 * @param       pubStr: 
 * @param       pKey: 
 * @return      true 
 * @return      false 
 */
bool GetEDPubKeyByBytes(const std::string &pubStr, EVP_PKEY* &pKey);

/**
 * @brief       generater random seed
 * 
 * @return      std::vector<unsigned char> 
 */

void sha256_hash(const uint8_t* input, size_t input_len, uint8_t* output);
#endif
