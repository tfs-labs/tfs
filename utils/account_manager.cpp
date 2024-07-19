#include "account_manager.h"
#include <evmone/evmone.h>
#include "contract_utils.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>
#include "openssl/rand.h"
#include "utils/hex_code.h"
#include <dirent.h>
#include <string>
#include <charconv>
#include "../utils/keccak_cryopp.hpp"
//file Initialize _pkey
static const std::string EDCertPath = "./cert/";
//Account Initialize 
Account::Account(const Account& other):
    _pkey(nullptr, &EVP_PKEY_free),
    _pubStr(other._pubStr),
    _priStr(other._priStr),
    _Addr(other._Addr)
{
    for (size_t i = 0; i < sizeof(_seed); ++i) {
        _seed[i] = std::move(other._seed[i]);
    }
    if (other._pkey) {
        EVP_PKEY *pkeyDup = EVP_PKEY_dup(other._pkey.get());
        _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(pkeyDup, &EVP_PKEY_free);
    }
}

Account::Account(Account&& other) noexcept :
    _pkey(std::move(other._pkey)),
    _pubStr(std::move(other._pubStr)),
    _priStr(std::move(other._priStr)),
    _Addr(std::move(other._Addr))
{
        for (size_t i = 0; i < sizeof(_seed); ++i) {
        _seed[i] = std::move(other._seed[i]);
    }
}

//self copy
Account& Account::operator=(const Account& other)
{
    if (this != &other) {
        _pubStr = other._pubStr;
        _priStr = other._priStr;
        _Addr = other._Addr;
        for (size_t i = 0; i < sizeof(_seed); ++i) {
            _seed[i] = std::move(other._seed[i]);
        }
        if (other._pkey) {
            EVP_PKEY *pkeyDup = EVP_PKEY_dup(other._pkey.get());
            _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(pkeyDup, &EVP_PKEY_free);
        } else {
            _pkey.reset();
        }

    }
    return *this;
}
//self copy
Account& Account::operator=(Account&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    _pkey = std::move(other._pkey);
    _pubStr = std::move(other._pubStr);
    _priStr = std::move(other._priStr);
    _Addr = std::move(other._Addr);
    for (size_t i = 0; i < sizeof(_seed); ++i) {
        _seed[i] = std::move(other._seed[i]);
    }
    return *this;
}

bool Account::Sign(const std::string &message, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char * sig_name = message.c_str();
    unsigned char *sigValue = NULL;
    size_t sig_len = strlen(sig_name);

    ON_SCOPE_EXIT{
        if(mdctx != NULL){EVP_MD_CTX_free(mdctx);}
        if(sigValue != NULL){OPENSSL_free(sigValue);}
    };

    // Create the Message Digest Context 
    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }
    
    // Initialise the DigestSign operation
    if(1 != EVP_DigestSignInit(mdctx, NULL, NULL, NULL, _pkey.get())) 
    {
        return false;
    }

    size_t tmpMLen = 0;
    if( 1 != EVP_DigestSign(mdctx, NULL, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    sigValue = (unsigned char *)OPENSSL_malloc(tmpMLen);

    if( 1 != EVP_DigestSign(mdctx, sigValue, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }
    
    std::string hashString((char*)sigValue, tmpMLen);
    signature = hashString;

    return true;
}

bool Account::Verify(const std::string &message, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char *msg = message.c_str();
    unsigned char *sig = (unsigned char *)signature.data();
    size_t slen = signature.size();
    size_t msg_len = strlen(msg);

    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }
    

    /* Initialize `key` with a public key */
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, _pkey.get())) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (1 != EVP_DigestVerify(mdctx, sig, slen ,(const unsigned char *)msg, msg_len)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}

void Account::_GeneratePubStr(EVP_PKEY* pkeyPtr)
{
    //The binary of the resulting public key is stored in a string serialized
    if(!_pkey)
    {
        return;
    }

    unsigned char *pkeyDer = NULL;
    int publen = i2d_PUBKEY(pkeyPtr ,&pkeyDer);

    for(int i = 0; i < publen; ++i)
    {
        _pubStr += pkeyDer[i];
    }
    OPENSSL_free(pkeyDer);
}

void Account::_GeneratePriStr(EVP_PKEY* pkeyPtr)
{
    size_t len = 80;
    char pkeyData[80] = {0};

    if( EVP_PKEY_get_raw_private_key(pkeyPtr, (unsigned char *)pkeyData, &len) == 0)
    {
        return;
    }

    std::string data(reinterpret_cast<char*>(pkeyData), len);
    _priStr = data;
}


void Account::_GenerateAddr()
{
    _Addr = GenerateAddr(_pubStr);
}

void Account::_GenerateRandomSeed()
{
    RAND_bytes(_seed,16);
}

void Account::_GeneratePkey()
{
    _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY* pkeyPtr = _pkey.get();
    //do we neet do continue a sha256
    uint8_t * seed = GetSeed();

    uint8_t outputArr[SHA256_DIGEST_LENGTH];
    sha256_hash(seed, PrimeSeedNum, outputArr);

    pkeyPtr = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, outputArr, DerivedSeedNum);
    if (!pkeyPtr) {
    ERRORLOG("Failed to create OpenSSL private key from seed.");
    }
    if(!_pkey)
    {
        ERRORLOG("generatepk false");
        return;
    }
    _pkey.reset(pkeyPtr);
    _GeneratePubStr(pkeyPtr);
    _GeneratePriStr(pkeyPtr);
    _GenerateAddr();
}

void Account::_GenerateFilePkey(const std::string &base58Address)
{
    _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), &EVP_PKEY_free);
    std::string priFileFormat = EDCertPath + base58Address;
    const char * priPath = priFileFormat.c_str();

    
    BIO* priBioFile = BIO_new_file(priPath, "r");
    if (!priBioFile)
    {
        BIO_free(priBioFile);
        printf("Error: priBioFile err\n");
        return;
    }
    
    uint8_t seedGetRead[PrimeSeedNum] = {0};

    int bytesRead = BIO_read(priBioFile, seedGetRead, PrimeSeedNum);
    if (bytesRead != PrimeSeedNum)
    {
        printf("Error: BIO_read err\n");
        BIO_free(priBioFile);
        return;
    }

    BIO_free(priBioFile);

    SetSeed(seedGetRead);
    uint8_t outputArr[SHA256_DIGEST_LENGTH];
    sha256_hash(seedGetRead, PrimeSeedNum, outputArr);

    EVP_PKEY* pkeyPtr = _pkey.get();
    pkeyPtr = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, outputArr, DerivedSeedNum);
    if (!pkeyPtr) {
    std::cerr << "Failed to create OpenSSL private key from seed." << std::endl;
    }

    if(!_pkey)
    {
        std::cout << "_pkey read from file false" << std::endl;
        return;
    }
    _pkey.reset(pkeyPtr);
    _GeneratePubStr(pkeyPtr);
    _GeneratePriStr(pkeyPtr);
    _Addr = base58Address;
    
}

AccountManager::AccountManager()
{
    _init();
}

int AccountManager::AddAccount(Account &account)
{
    auto iter = _accountList.find(account.GetAddr());
    if(iter != _accountList.end())
    {
        std::cout << "bs58Addr repeat" << std::endl;
        return -1;
    }
    _accountList.emplace(account.GetAddr(), std::make_shared<Account>(account));
    return 0;
}

void AccountManager::PrintAllAccount() const
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        if (iter->first == _defaultAddr)
        {
            
            std::cout << "0x"+ iter->first<< " [default]" << std::endl;
        }
        else
        {
            std::cout << "0x"+iter->first << std::endl;
        }
        ++iter;
    }
}

void AccountManager::DeleteAccount(const std::string& addr)
{
    if(addr == _defaultAddr)
    {
        std::cout << "Default account cannot be deleted " << std::endl;
        return;
    }

    auto iter = _accountList.find(addr);
    if (iter == _accountList.end()) 
    {
        std::cout << addr << " not found Delete failed" << std::endl;
        return;
    }

    _accountList.erase(iter);
    
    std::filesystem::path filename = EDCertPath + addr ;

    try {
        if (std::filesystem::remove(filename))
            std::cout << addr << " deleted successfully" << std::endl;
        else
            std::cout << "Failed to delete " << addr << std::endl;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << e.what();
    }

}

void AccountManager::SetDefaultAddr(const std::string & bs58Addr)
{
    _defaultAddr = bs58Addr;
}

std::string AccountManager::GetDefaultAddr() const
{
    return _defaultAddr;
}

int AccountManager::SetDefaultAccount(const std::string & bs58Addr)
{
    if (_accountList.size() == 0)
    {
        return -1;
    }

    if (bs58Addr.size() == 0)
    {
        _defaultAddr = _accountList.begin()->first;
        return 0;
    }

    auto iter = _accountList.find(bs58Addr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found bs58Addr {} in the _accountList ",bs58Addr);
        return -2;
    }
    _defaultAddr = bs58Addr;
    
    return 0;
}

bool AccountManager::IsExist(const std::string & bs58Addr)
{
    auto iter = _accountList.find(bs58Addr);
    if(iter == _accountList.end())
    {
        return false;
    }
    return true;
}

int AccountManager::FindAccount(const std::string & bs58Addr, Account & account)
{
    auto iter = _accountList.find(bs58Addr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found bs58Addr {} in the _accountList ",bs58Addr);
        return -1;
    }
    
    account = *iter->second;
    return 0;
}

int AccountManager::GetDefaultAccount(Account & account)
{
    auto iter = _accountList.find(_defaultAddr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found DefaultKeyBs58Addr {} in the _accountList ", _defaultAddr);
        return -1;
    }
    account = *iter->second;

    return 0;
}

void AccountManager::GetAccountList(std::vector<std::string> & _list)
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        _list.push_back(iter->first);
        iter++;
    }
}


bool AccountManager::GetAccountPubByBytes(const std::string &pubStr, Account &account)
{
    unsigned char* bufPtr = (unsigned char *)pubStr.data();
    const unsigned char *pk_str = bufPtr;
    int lenPtr = pubStr.size();
    
    if(lenPtr == 0)
    {
        ERRORLOG("public key Binary is empty");
        return false;
    }

    EVP_PKEY *peerPubKey = d2i_PUBKEY(NULL, &pk_str, lenPtr);

    if(peerPubKey == nullptr)
    {
        return false;
    }
    account.SetKey(peerPubKey);

    return true;
}


int AccountManager::SavePrivateKeyToFile(const std::string & Addr)
{
    std::string priFileFormat = EDCertPath + Addr;
    const char * path =  priFileFormat.c_str();

    Account account(true);
    if(FindAccount(Addr, account) != 0)
    {
        ERRORLOG("SavePrivateKeyToFile find account fail: {}", Addr);
        return -1;
    }

    uint8_t seedGet[PrimeSeedNum] = {0};

    uint8_t * seed = account.GetSeed();
    for (int i = 0; i < PrimeSeedNum; ++i) {
         seedGet[i] = static_cast<uint8_t>(seed[i]);
     }

    //Store the private key to the specified path
    BIO* priBioFile = BIO_new_file(path, "w");
    if (!priBioFile)
    {
        printf("Error: priBioFile err\n");
        BIO_free(priBioFile);
        return -2;
    }
    std::stringstream ss;
    for (int i = 0; i < PrimeSeedNum; i++) {
            if (i > 0) {
        ss << "-";
    }
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(seedGet[i]);
    }
    std::string hexString = ss.str();
    std::vector<uint8_t> binaryData(PrimeSeedNum);
    for (int i = 0; i < PrimeSeedNum; i++) {
        const char* start = hexString.c_str() + i * 3;
            uint8_t value;
        std::from_chars(start, start + 2, value, 16);
    binaryData[i] = value;
    }
    
    if (BIO_write(priBioFile, binaryData.data(), PrimeSeedNum) != PrimeSeedNum)
    {
        printf("Error: BIO_write err\n");
        BIO_free(priBioFile);
        return -3;
    }
    
    BIO_free(priBioFile);
    return 0;
}

int AccountManager::GetMnemonic(const std::string & bs58Addr, std::string & mnemonic)
{
    Account account(true);
    Account defaultAccount;
    if(!FindAccount(bs58Addr, account))
    {
        GetDefaultAccount(defaultAccount);
        account = defaultAccount;
    }
    
    if(account.GetPriStr().size() <= 0)
    {
        return 0;
    }

    char out[1024]={0};
    int ret = mnemonic_from_data(account.GetSeed(), 16, out, 1024); 
    std::string data(out);
    mnemonic = data;
    return ret;
}

int AccountManager::ImportMnemonic(const std::string & mnemonic)
{
    char out[33] = {0};
    int outLen = 0;


	if(mnemonic_check((char *)mnemonic.c_str(), out, &outLen) == 0)
    {
        return -1;
     }
    uint8_t mnmonicHex[65] = {0};

    char mnemonic_hex[65] = {0};
	encode_hex(mnemonic_hex, out, outLen);

	std::string mnemonic_key;
	mnemonic_key.append(mnemonic_hex, outLen * 2);
    uint8_t seed[16]={0};
    string_to_hex_array(mnemonic_hex, seed, PrimeSeedNum);

    ImportPrivateKeyHex(seed);
    
    return 0;
}


int AccountManager::GetPrivateKeyHex(const std::string & bs58Addr, std::string & privateKeyHex)
{
    Account account(true);
    Account defaultAccount;

    if(!FindAccount(bs58Addr, account))
    {
        GetDefaultAccount(defaultAccount);
        account = defaultAccount;
    }

	if(account.GetPriStr().size() <= 0)
    {
        return -1;
    }
	
    unsigned int privateKeyLen = sizeof(privateKeyHex);
	if(privateKeyHex.empty() || privateKeyLen < account.GetPriStr().size() * 2)
    {
        privateKeyLen = sizeof(account.GetSeed());
        return -2;
    }
	
    std::string strPriHex = Str2Hex(account.GetPriStr());
    privateKeyHex = strPriHex;    
    return 0;
}

int AccountManager::ImportPrivateKeyHex(uint8_t *inputSeed)
{
    uint8_t outputArr[SHA256_DIGEST_LENGTH];
    sha256_hash(inputSeed, PrimeSeedNum, outputArr);

    std::string pubStr_;
    std::string priStr_;


    EVP_PKEY * pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, outputArr, DerivedSeedNum);
    if(pkey == nullptr)
    {
        return -1;
    }

    unsigned char *pkeyDer = NULL;
    int publen = i2d_PUBKEY(pkey ,&pkeyDer);

    for(int i = 0; i < publen; ++i)
    {
        pubStr_ += pkeyDer[i];
    }
    size_t len = 80;
    char pkeyData[80] = {0};

    if( EVP_PKEY_get_raw_private_key(pkey, (unsigned char *)pkeyData, &len) == 0)
    {
        return -2;
    }

    std::string data(reinterpret_cast<char*>(pkeyData), len);
    priStr_ = data;
    
    std::string Addr = GenerateAddr(pubStr_);
    Account acc;
    acc.SetKey(pkey);
    acc.SetPubStr(pubStr_);
    acc.SetPriStr(priStr_); 
    acc.SetAddr(Addr);
    acc.SetSeed(inputSeed);

    std::cout << "final pubStr " << Str2Hex(acc.GetPubStr()) << std::endl;
    std::cout << "final priStr " << Str2Hex(acc.GetPriStr()) << std::endl;

    MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);

    int ret =  MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.GetAddr());
    if(ret != 0)
    {
        ERRORLOG("SavePrivateKey failed!");
        return -3;
    }

	return 0;
}

std::string Getsha256hash(const std::string & text)
{
	unsigned char mdStr[65] = {0};
	SHA256((const unsigned char *)text.c_str(), text.size(), mdStr);
 
	char tmp[3] = {0};

    std::string encodedHexStr;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(tmp, "%02x", mdStr[i]);
        encodedHexStr += tmp;
	}

    return encodedHexStr;
}

int AccountManager::_init()
{
    std::string path = EDCertPath;
    if(access(path.c_str(), F_OK))
    {
        if(mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH))
        {
            assert(false);
            return -1;
        }
    }

    DIR *dir;
    struct dirent *ptr;

    if ((dir = opendir(path.c_str())) == NULL)
    {
		ERRORLOG("OPEN DIR  ERROR ..." );
		return -2;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".") == 0 || strcmp(ptr->d_name, "..") ==0)
		{
            continue;
		}
        else
        {
            std::string filename(ptr->d_name);
            if (filename.size() == 0)
            {
                return -3;
            }
            
            int index = filename.find('.');
            std::string Addr = filename.substr(0, index);

            Account account(Addr);
            if(AddAccount(account) != 0)
            {
                return -4;
            }

        }
    }
    closedir(dir);

    if(_accountList.size() == 0)
    {
        Account account(true);
        std::string Addr = account.GetAddr();
        if(AddAccount(account) != 0)
        {
            return -5;
        }

        SetDefaultAccount(Addr);
        if(SavePrivateKeyToFile(Addr) != 0)
        {
            return -6;
        }
    }
    else
    {
        if (IsExist(global::ca::kInitAccountAddr))
        {
            SetDefaultAccount(global::ca::kInitAccountAddr);
        }
        else
        {
            SetDefaultAddr(_accountList.begin()->first);
        }
    }

    return 0;
}

bool isValidAddress(const std::string& address) 
{
    if(MagicSingleton<VerifiedAddressSet>::GetInstance()->isAddressVerified(address))
    {
        return true;
    }
    
    if (address.size() != 40)
    { 
        DEBUGLOG("addresss size is :",address.size());
        return false;
    }

    std::string originalAddress = address;
    std::transform(originalAddress.begin(), originalAddress.end(), originalAddress.begin(), ::tolower);

    std::string hash = Keccak256Crypt(originalAddress);
    std::transform(hash.begin(), hash.end(), hash.begin(), ::tolower);

    for (size_t i = 0; i < originalAddress.size(); ++i) {
        if ((hash[i] >= '8' && originalAddress[i] >= 'a' && originalAddress[i] <= 'f') && address[i] != std::toupper(originalAddress[i]))
        {
            DEBUGLOG("hash[i] >= 8");
            return false;
        }
        else if ((hash[i] < '8' || originalAddress[i] >= '0' && originalAddress[i] <= '9') && address[i] != originalAddress[i])
        {
            DEBUGLOG("hash[i] < 8");
            return false;
        }
            
    }
    MagicSingleton<VerifiedAddressSet>::GetInstance()->markAddressVerified(address);
    return true;
}

std::string GenerateAddr(const std::string& publicKey)
{
    std::string addrCache = MagicSingleton<AddressCache>::GetInstance()->getAddress(publicKey);
    if(addrCache != "")
    {
        return addrCache;
    }
    std::string hash = Keccak256Crypt(publicKey);

    std::string addr = hash.substr(hash.length() - 40);
    std::string checkSumAddr = evm_utils::ToChecksumAddress(addr);
    MagicSingleton<AddressCache>::GetInstance()->addPublicKey(publicKey, checkSumAddr);
    return checkSumAddr;
}

void TestED25519Time()
{
    Account account;
    std::string sigName = "ABC";
    std::string signature = "";
    
    long long startTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    for(int i = 0; i < 1000; ++i)
    {   
        if(account.Sign(sigName, signature) == false)
        {
            std::cout << "evp sign fail" << std::endl;
        }
    }
    long long  endTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::cout << "evp ED25519 sign total time : " << (endTime - startTime) << std::endl;

    long long s1 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    for(int i = 0; i < 1000; ++i)
    {   
        if(account.Verify(sigName, signature) == false)
        {
            std::cout << "evp verify fail" << std::endl;
        }
    }
    long long  e1 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::cout << "evp ED25519 verify sign total time : " << (e1 - s1) << std::endl;
    
}

void TestEDFunction()
{
    MagicSingleton<AccountManager>::GetInstance();

    Account account;

    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);

    const std::string sig_name = "ABC";
    std::string signature = "";
    if(account.Sign(sig_name, signature) == false)
    {
        std::cout << "evp sign fail" << std::endl;
        return;
    }

    EVP_PKEY* eckey = nullptr;
    if(GetEDPubKeyByBytes(account.GetPubStr(), eckey) == false)
    {
        return;
    }

    if(ED25519VerifyMessage(sig_name, eckey, signature) == false)
    {
        return;
    }

    Account e1;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(e1);
    if(ED25519SignMessage(sig_name, e1.GetKey(), signature) == false)
    {
        return;
    }

    if(ED25519VerifyMessage(sig_name, e1.GetKey(), signature) == false)
    {
        return;
    }
}

std::string Base64Encode(const std::string & dataSource)
{
    const auto predicted_len = 4 * ((dataSource.length() + 2) / 3);
    const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
    const std::vector<unsigned char> vec_chars{dataSource.begin(), dataSource.end()};
    const auto output_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output_buffer.get()), vec_chars.data(), static_cast<int>(vec_chars.size()));

    if (predicted_len != static_cast<unsigned long>(output_len)) 
    {
        ERRORLOG("Base64Encode error");
    }

  return output_buffer.get();
}

std::string Base64Decode(const std::string & dataSource)
{
  const auto predicted_len = 3 * dataSource.length() / 4;
  const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
  const std::vector<unsigned char> vec_chars{dataSource.begin(), dataSource.end()};
  const auto output_len = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(output_buffer.get()), vec_chars.data(), static_cast<int>(vec_chars.size()));

  if (predicted_len != static_cast<unsigned long>(output_len)) 
  {
    ERRORLOG("Base64Decode error");
  }
  return output_buffer.get();
}

bool ED25519SignMessage(const std::string &message, EVP_PKEY* pkey, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char * sig_name = message.c_str();

    unsigned char *sigValue = NULL;
    size_t sig_len = strlen(sig_name);

    // Create the Message Digest Context 
    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }

    if(pkey == NULL)
    {
        return false;
    }
    
    // Initialise the DigestSign operation
    if(1 != EVP_DigestSignInit(mdctx, NULL, NULL, NULL, pkey)) 
    {
        return false;
    }

    size_t tmpMLen = 0;
    if( 1 != EVP_DigestSign(mdctx, NULL, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    sigValue = (unsigned char *)OPENSSL_malloc(tmpMLen);

    if( 1 != EVP_DigestSign(mdctx, sigValue, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    std::string hashString((char*)sigValue, tmpMLen);
    signature = hashString;

    OPENSSL_free(sigValue);
    EVP_MD_CTX_free(mdctx);
    return true;

}

bool ED25519VerifyMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char *msg = message.c_str();
    unsigned char *sig = (unsigned char *)signature.data();
    size_t slen = signature.size();
    size_t msgLen = strlen(msg);

    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }

    /* Initialize `key` with a public key */
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (1 != EVP_DigestVerify(mdctx, sig, slen ,(const unsigned char *)msg, msgLen)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;

}

bool GetEDPubKeyByBytes(const std::string &pubStr, EVP_PKEY* &pKey)
{
    //Generate public key from binary string of public key  
    unsigned char* bufPtr = (unsigned char *)pubStr.data();
    const unsigned char *pk_str = bufPtr;
    int lenPtr = pubStr.size();
    
    if(lenPtr == 0)
    {
        ERRORLOG("public key Binary is empty");
        return false;
    }

    EVP_PKEY *peerPubKey = d2i_PUBKEY(NULL, &pk_str, lenPtr);

    if(peerPubKey == nullptr)
    {
        return false;
    }
    pKey = peerPubKey;
    return true;
}

void sha256_hash(const uint8_t* input, size_t input_len, uint8_t* output) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, input, input_len);
    SHA256_Final(output, &ctx);
}
