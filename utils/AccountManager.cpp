#include "AccountManager.h"


Account::Account()
{
    pkey = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if(ctx == nullptr)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if(EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        std::cout << "keygen init fail" << std::endl;
    }

    if(EVP_PKEY_keygen(ctx, &pkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        std::cout << "keygen fail\n" << std::endl;
    }

    _GetPubStr();
    _GetPriStr();
    _GetBase58Addr(Base58Ver::kBase58Ver_Normal);
    EVP_PKEY_CTX_free(ctx);
}

Account::Account(Base58Ver ver)
{
    pkey = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if(ctx == nullptr)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    if(EVP_PKEY_keygen_init(ctx) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        std::cout << "keygen init fail" << std::endl;
    }

    if(EVP_PKEY_keygen(ctx, &pkey) <= 0)
    {
        EVP_PKEY_CTX_free(ctx);
        std::cout << "keygen fail\n" << std::endl;
    }

    _GetPubStr();
    _GetPriStr();
    _GetBase58Addr(ver);
    EVP_PKEY_CTX_free(ctx);
}

static const std::string EDCertPath = "./cert/";
Account::Account(const std::string &bs58Addr)
{
    std::string priFileFormat = EDCertPath + bs58Addr + ".private";
    const char * priPath = priFileFormat.c_str();

    //Read public key from PEM file
    BIO* priBioFile = BIO_new_file(priPath, "rb");

    pkey = PEM_read_bio_PrivateKey(priBioFile, NULL, 0, NULL);
    if (!pkey)  
    {
        printf("Error：PEM_write_bio_EC_PUBKEY err\n");
        return ;
    }
    if(priBioFile != NULL) BIO_free(priBioFile);

    base58Addr = bs58Addr;

    _GetPubStr();
    _GetPriStr();
}


bool Account::Sign(const std::string &message, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char * sig_name = message.c_str();

    unsigned char *sig_value = NULL;
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

    sig_value = (unsigned char *)OPENSSL_malloc(tmpMLen);

    if( 1 != EVP_DigestSign(mdctx, sig_value, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    std::string hashString((char*)sig_value, tmpMLen);
    signature = hashString;

    OPENSSL_free(sig_value);
    EVP_MD_CTX_free(mdctx);
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
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey)) 
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

void Account::_GetPubStr()
{
    //The binary of the resulting public key is stored in a string serialized
    unsigned char *pkey_der = NULL;
    int publen = i2d_PUBKEY(pkey ,&pkey_der);

    for(int i = 0; i < publen; ++i)
    {
        pubStr += pkey_der[i];
    }

    delete pkey_der;
}

void Account::_GetPriStr()
{
    size_t len = 80;
    char pkey_data[80] = {0};
    if( EVP_PKEY_get_raw_private_key(pkey, (unsigned char *)pkey_data, &len) == 0)
    {
        return;
    }

    std::string data(pkey_data, len);
    priStr = data;
}


void Account::_GetBase58Addr(Base58Ver ver)
{
    base58Addr = GetBase58Addr(pubStr, ver);
}

AccountManager::AccountManager()
{
    _init();
}

int AccountManager::AddAccount(Account & account)
{
    auto iter = _accountList.find(account.base58Addr);
    if(iter != _accountList.end())
    {
        std::cout << "bs58Addr repeat" << std::endl;
        return -1;
    }

    _accountList.insert(make_pair(account.base58Addr, account));
    return 0;
}

void AccountManager::PrintAllAccount() const
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        if (iter->first == defaultBase58Addr)
        {
            std::cout << iter->first << " [default]" << std::endl;
        }
        else
        {
            std::cout << iter->first << std::endl;
        }
        ++iter;
    }
}

void AccountManager::DeleteAccount(const std::string& base58addr)
{
    if(base58addr == defaultBase58Addr)
    {
        std::cout << "Default account cannot be deleted " << std::endl;
        return;
    }

    auto iter = _accountList.find(base58addr);
    if (iter == _accountList.end()) 
    {
        std::cout << base58addr << " not found Delete failed" << std::endl;
        return;
    }

    EVP_PKEY_free(_accountList.at(base58addr).pkey);
    _accountList.erase(iter);
    
    std::filesystem::path filename = EDCertPath + base58addr + ".private";

    try {
        if (std::filesystem::remove(filename))
            std::cout << base58addr << " deleted successfully" << std::endl;
        else
            std::cout << "Failed to delete " << base58addr << std::endl;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << e.what();
    }

}

void AccountManager::SetDefaultBase58Addr(const std::string & bs58Addr)
{
    defaultBase58Addr = bs58Addr;
}

std::string AccountManager::GetDefaultBase58Addr() const
{
    return defaultBase58Addr;
}

int AccountManager::SetDefaultAccount(const std::string & bs58Addr)
{
    if (_accountList.size() == 0)
    {
        return -1;
    }

    if (bs58Addr.size() == 0)
    {
        defaultBase58Addr = _accountList.begin()->first;
        return 0;
    }

    auto iter = _accountList.find(bs58Addr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found bs58Addr {} in the _accountList ",bs58Addr);
        return -2;
    }
    defaultBase58Addr = bs58Addr;
    
    return 0;
}

bool AccountManager::IsExist(const std::string & bs58Addr)
{
    auto iter = _accountList.find(bs58Addr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found bs58Addr {} in the _accountList ",bs58Addr);
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
    account = iter->second;

    return 0;
}

int AccountManager::GetDefaultAccount(Account & account)
{
    auto iter = _accountList.find(defaultBase58Addr);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found DefaultKeyBs58Addr {} in the _accountList ", defaultBase58Addr);
        return -1;
    }
    account = iter->second;

    return 0;
}

void AccountManager::GetAccountList(std::vector<std::string> & base58_list)
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        base58_list.push_back(iter->first);
        iter++;
    }
}

int AccountManager::SavePrivateKeyToFile(const std::string & base58Addr)
{
    std::string priFileFormat = EDCertPath + base58Addr +".private";
    const char * path =  priFileFormat.c_str();

    Account account;
    EVP_PKEY_free(account.pkey);
    if(FindAccount(base58Addr, account) != 0)
    {
        ERRORLOG("SavePrivateKeyToFile find account fail: {}", base58Addr);
        return -1;
    }

    //Store the private key to the specified path
    BIO* priBioFile = BIO_new_file(path, "w");

    if (!priBioFile)
    {
        printf("Error：pBioFile err \n");
        return -2;
    }

    if (!PEM_write_bio_PrivateKey(priBioFile, account.pkey, NULL, NULL, 0, NULL, NULL))  //Write to the private key
    {
        printf("Error：PEM_write_bio_ECPrivateKey err\n");
        return -3;
    }

    BIO_free(priBioFile);
    return 0;
}

int AccountManager::GetMnemonic(const std::string & bs58Addr, std::string & mnemonic)
{
    Account account;
    Account defaultAccount;
    if(!FindAccount(bs58Addr, account))
    {
        GetDefaultAccount(defaultAccount);
        account = defaultAccount;
    }
    
    if(account.priStr.size() <= 0)
    {
        return 0;
    }

    char out[1024]={0};

    int ret = mnemonic_from_data((const uint8_t*)account.priStr.c_str(), account.priStr.size(), out, 1024); 
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

    char mnemonic_hex[65] = {0};
	encode_hex(mnemonic_hex, out, outLen);

	std::string mnemonic_key;
	mnemonic_key.append(mnemonic_hex, outLen * 2);

    ImportPrivateKeyHex(mnemonic_key);
    

    return 0;
}

int AccountManager::GetPrivateKeyHex(const std::string & bs58Addr, std::string & privateKeyHex)
{
    Account account;
    Account defaultAccount;

    if(!FindAccount(bs58Addr, account))
    {
        GetDefaultAccount(defaultAccount);
        account = defaultAccount;
    }

	if(account.priStr.size() <= 0)
    {
        return -1;
    }
	
    unsigned int privateKeyLen = sizeof(privateKeyHex);
	if(privateKeyHex.empty() || privateKeyLen < account.priStr.size() * 2)
    {
        privateKeyLen = account.priStr.size() * 2;
        return -2;
    }
	
    std::string strPriHex = Str2Hex(account.priStr);
    privateKeyHex = strPriHex;
    EVP_PKEY_free(account.pkey);
    EVP_PKEY_free(defaultAccount.pkey);
    
    return 0;
}

int AccountManager::ImportPrivateKeyHex(const std::string & privateKeyHex)
{
    std::string priStr_ = Hex2Str(privateKeyHex);
    std::string pubStr_;
    unsigned char* buf_ptr = (unsigned char *)priStr_.data();
    const unsigned char *pk_str = buf_ptr;

    EVP_PKEY * pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, pk_str, priStr_.size());
    if(pkey == nullptr)
    {
        return -1;
    }

    Account account;
    GetDefaultAccount(account);
    if(EVP_PKEY_eq(pkey,account.pkey))
    {
        std::cout << "is equal" << std::endl;
    }

    unsigned char *pkey_der = NULL;
    int publen = i2d_PUBKEY(pkey ,&pkey_der);

    for(int i = 0; i < publen; ++i)
    {
        pubStr_ += pkey_der[i];
    }

    std::string base58Addr = GetBase58Addr(pubStr_, Base58Ver::kBase58Ver_Normal);
    Account acc;
    EVP_PKEY_free(acc.pkey);
    acc.pkey = pkey;
    acc.pubStr = pubStr_;
    acc.priStr = priStr_; 
    acc.base58Addr = base58Addr;

    std::cout << "final pubStr " << Str2Hex(acc.pubStr) << std::endl;
    std::cout << "final priStr" << Str2Hex(acc.priStr) << std::endl;

    MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
    int ret =  MagicSingleton<AccountManager>::GetInstance()->SavePrivateKeyToFile(acc.base58Addr);
    if(ret != 0)
    {
        ERRORLOG("SavePrivateKey failed!");
        return -2;
    }

	return 0;
}

std::string getsha256hash(const std::string & text)
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

            Base58Ver ver;
            if (filename[0] == '1')
            {
                ver = Base58Ver::kBase58Ver_Normal;
            }
            else if (filename[0] == '3')
            {
                ver = Base58Ver::kBase58Ver_MultiSign;
            }
            else
            {
                return -4;
            }
            
            int index = filename.find('.');
            std::string bs58Addr = filename.substr(0, index);
            Account account(bs58Addr);
            if(AddAccount(account) != 0)
            {
                return -5;
            }

        }
    }
    closedir(dir);

    if(_accountList.size() == 0)
    {
        Account account;
        if(AddAccount(account) != 0)
        {
            return -6;
        }

        SetDefaultAccount(account.base58Addr);

        if(SavePrivateKeyToFile(account.base58Addr) != 0)
        {
            return -7;
        }
    }
    else
    {
        if (IsExist(global::ca::kInitAccountBase58Addr))
        {
            SetDefaultAccount(global::ca::kInitAccountBase58Addr);
        }
        else
        {
            SetDefaultBase58Addr(_accountList.begin()->first);
        }
    }

    return 0;
}

void TestED25519Time()
{
    Account account;
    std::string sig_name = "ABC";
    std::string signature = "";
    
    long long start_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    for(int i = 0; i < 1000; ++i)
    {   
        if(account.Sign(sig_name, signature) == false)
        {
            std::cout << "evp sign fail" << std::endl;
        }
    }
    long long  end_time = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    std::cout << "evp ED25519 sign total time : " << (end_time - start_time) << std::endl;

    long long s1 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    for(int i = 0; i < 1000; ++i)
    {   
        if(account.Verify(sig_name, signature) == false)
        {
            std::cout << "evp verify fail" << std::endl;
        }
    }
    long long  e1 = MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp();
    std::cout << "evp ED25519 verify sign total time : " << (e1 - s1) << std::endl;
    
}

void testEDFunction()
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
    if(GetEDPubKeyByBytes(account.pubStr, eckey) == false)
    {
        EVP_PKEY_free(eckey);
        return;
    }

    if(ED25519VerifyMessage(sig_name, eckey, signature) == false)
    {
        EVP_PKEY_free(eckey);
        return;
    }

    Account e1;
    EVP_PKEY_free(e1.pkey);
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(e1);
    if(ED25519SignMessage(sig_name, e1.pkey, signature) == false)
    {
        return;
    }

    if(ED25519VerifyMessage(sig_name, e1.pkey, signature) == false)
    {
        return;
    }
}

std::string Base64Encode(const std::string & data_source)
{
    const auto predicted_len = 4 * ((data_source.length() + 2) / 3);
    const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
    const std::vector<unsigned char> vec_chars{data_source.begin(), data_source.end()};
    const auto output_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output_buffer.get()), vec_chars.data(), static_cast<int>(vec_chars.size()));

    if (predicted_len != static_cast<unsigned long>(output_len)) 
    {
        ERRORLOG("Base64Encode error");
    }

  return output_buffer.get();
}

std::string Base64Decode(const std::string & data_source)
{
  const auto predicted_len = 3 * data_source.length() / 4;
  const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
  const std::vector<unsigned char> vec_chars{data_source.begin(), data_source.end()};
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

    unsigned char *sig_value = NULL;
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

    sig_value = (unsigned char *)OPENSSL_malloc(tmpMLen);

    if( 1 != EVP_DigestSign(mdctx, sig_value, &tmpMLen, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    std::string hashString((char*)sig_value, tmpMLen);
    signature = hashString;

    OPENSSL_free(sig_value);
    EVP_MD_CTX_free(mdctx);
    return true;

}

bool ED25519VerifyMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature)
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
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey)) 
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

bool GetEDPubKeyByBytes(const std::string &pubStr, EVP_PKEY* &pKey)
{
    //Generate public key from binary string of public key  
    unsigned char* buf_ptr = (unsigned char *)pubStr.data();
    const unsigned char *pk_str = buf_ptr;
    int len_ptr = pubStr.size();
    
    if(len_ptr == 0)
    {
        ERRORLOG("public key Binary is empty");
        return false;
    }

    EVP_PKEY *peer_pub_key = d2i_PUBKEY(NULL, &pk_str, len_ptr);

    if(peer_pub_key == nullptr)
    {
        return false;
    }
    pKey = peer_pub_key;
    return true;
}
