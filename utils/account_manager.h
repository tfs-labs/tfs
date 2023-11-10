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

#include "base58.h"
#include "hex_code.h"
#include "utils/time_util.h"
#include "magic_singleton.h"
#include "../ca/global.h"
#include "../include/logging.h"
#include "utils/json.hpp"
#include "utils/bip39.h"
#include "include/scope_guard.h"

// #include "../openssl/include/openssl/evp.h"
// #include "../openssl/include/openssl/ec.h"
// #include "../openssl/include/openssl/pem.h"
// #include "../openssl/include/openssl/core_names.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>

/**
 * @brief       
 * 
 */
class Account
{
    public:
        Account();
        /**
         * @brief       Construct a new Account object
         * 
         * @param       ver: 
         */
        Account(Base58Ver ver);

        /**
         * @brief       Construct a new Account object
         * 
         * @param       bs58Addr: 
         */
        Account(const std::string &bs58Addr);

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
        std::string GetBase58() const
        {
            return _base58Addr;
        }

        /**
         * @brief       Set the Base 5 8 object
         * 
         * @param       base58: 
         */
        void SetBase58(std::string & base58)
        {
            _base58Addr = base58;
        }

    private:
        /**
         * @brief       
         * 
         */
        void _GeneratePubStr();

        /**
         * @brief       
         * 
         */
        void _GeneratePriStr();

        /**
         * @brief       
         * 
         * @param       ver: 
         */
        void _GenerateBase58Addr(Base58Ver ver);
    private:
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> _pkey;

        std::string _pubStr;
        std::string _priStr;
        std::string _base58Addr;

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
         * @param       base58addr: 
         */
        void DeleteAccount(const std::string& base58addr);

        /**
         * @brief       Set the Default Base 5 8 Addr object
         * 
         * @param       bs58Addr: 
         */
        void SetDefaultBase58Addr(const std::string & bs58Addr);

        /**
         * @brief       Get the Default Base 5 8 Addr object
         * 
         * @return      std::string 
         */
        std::string GetDefaultBase58Addr() const;

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
         * @param       base58_list: 
         */
        void GetAccountList(std::vector<std::string> & base58_list);

        /**
         * @brief       
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
         * @param       base58Addr: 
         * @return      int 
         */
        int SavePrivateKeyToFile(const std::string & base58Addr);

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
        int ImportPrivateKeyHex(const std::string & privateKeyHex);

    private:
        // friend std::string PrintCache(int where);
        std::string _defaultBase58Addr;
        std::map<std::string /*base58addr*/, std::shared_ptr<Account>> _accountList;
        
        int _init();
};

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

//base64 Encryption and decryption
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

#endif
