#ifndef _EDManager_
#define _EDManager_

#include <iostream>
#include <string>
#include <filesystem>
#include <dirent.h>

#include "base58.h"
#include "hexcode.h"
#include "utils/time_util.h"
#include "MagicSingleton.h"
#include "../ca/ca_global.h"
#include "../include/logging.h"
#include "utils/json.hpp"
#include "utils/bip39.h"

#include "../openssl/include/openssl/evp.h"
#include "../openssl/include/openssl/ec.h"
#include "../openssl/include/openssl/pem.h"
#include "../openssl/include/openssl/core_names.h"

class Account
{
    public:
        Account();
        Account(Base58Ver ver);
        Account(const std::string &bs58Addr);
        ~Account() = default;

        bool Sign(const std::string &message, std::string &signature);
        bool Verify(const std::string &message, std::string &signature);

        EVP_PKEY * GetKey () const
        {
            return pkey;
        }

        void SetKey(EVP_PKEY * key)
        {
            pkey = key;
        }
        
        std::string GetPubStr() const
        {
            return pubStr;
        }
        
        void SetPubStr(std::string &str)
        {
            pubStr = str;
        }

        std::string GetPriStr() const
        {
            return priStr;
        }

        void SetPriStr(std::string &str)
        {
            priStr = str;
        }

        std::string GetBase58() const
        {
            return base58Addr;
        }

        void SetBase58(std::string & base58)
        {
            base58Addr = base58;
        }

    private:
        void GeneratePubStr();
        void GeneratePriStr();
        void GenerateBase58Addr(Base58Ver ver);
    private:
        EVP_PKEY *pkey;
        std::string pubStr;
        std::string priStr;
        std::string base58Addr;

};

class AccountManager
{
    public:
        AccountManager();
        ~AccountManager() = default;

        int AddAccount(Account & account);
        void PrintAllAccount() const;
        void DeleteAccount(const std::string& base58addr);
        void SetDefaultBase58Addr(const std::string & bs58Addr);
        std::string GetDefaultBase58Addr() const;
        int SetDefaultAccount(const std::string & bs58Addr);
        bool IsExist(const std::string & bs58Addr);
        int FindAccount(const std::string & bs58Addr, Account & account);
        int GetDefaultAccount(Account & account);
        void GetAccountList(std::vector<std::string> & base58_list);
        int SavePrivateKeyToFile(const std::string & base58Addr);

        int GetMnemonic(const std::string & bs58Addr, std::string & mnemonic);
        int ImportMnemonic(const std::string & mnemonic);
        
        int GetPrivateKeyHex(const std::string & bs58Addr, std::string & privateKeyHex);
        int ImportPrivateKeyHex(const std::string & privateKeyHex);

    private:
        std::string defaultBase58Addr;
        std::map<std::string /*base58addr*/,Account> _accountList;
        
        int _init();
};

void TestED25519Time();
void testEDFunction();

//base64 Encryption and decryption
std::string Base64Encode(const std::string & data_source);
std::string Base64Decode(const std::string & data_source);

std::string getsha256hash(const std::string & text);

bool ED25519SignMessage(const std::string &message, EVP_PKEY* pkey, std::string &signature);
bool ED25519VerifyMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature);

bool GetEDPubKeyByBytes(const std::string &pubStr, EVP_PKEY* &pKey);

#endif
