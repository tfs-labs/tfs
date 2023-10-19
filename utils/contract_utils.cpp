#include "contract_utils.h"
#include "utils/base58.h"
#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include <iostream>
std::string evm_utils::ToChecksumAddress(const std::string& hash, const std::string& address)
{
    std::string ret;
	for (int i = 0; i < address.length(); i++)
	{
        std::string str;
        str = str + hash[i];
		if (stoi(str, 0, 16) >= 8)
		{
			ret += std::toupper(address[i]);
		}
		else
		{
			ret += address[i];
		}
	}
	return ret;
}

evmc_address evm_utils::StringToEvmAddr(const std::string& addr)
{
    const char *s = addr.data();
    evmc_address evmAddr = literals::internal::from_hex<address>(s);
    return evmAddr;
}

std::string evm_utils::EvmAddrToString(const evmc_address& addr)
{
    return hex({addr.bytes,sizeof(addr.bytes)});
}

std::string evm_utils::EvmAddrToBase58(const evmc_address& addr)
{
    using bytes = std::basic_string<uint8_t>;
    bytes str2=bytes(addr.bytes, sizeof(addr.bytes));
    char buf[2048] = {0};
    size_t bufLen = sizeof(buf);
    b58check_enc(buf, &bufLen, 0x00, from_hex(hex(str2).data()).data(), RIPEMD160_DIGEST_LENGTH);
    std::string bs58Addr;
    bs58Addr.append(buf, bufLen - 1 );
    return bs58Addr;
}

evmc_address evm_utils::PubStrToEvmAddr(const std::string& pub)
{
    std::string evmAddress = GenerateEvmAddr(pub);
    const char *s = evmAddress.data();
    evmc_address evmaddr = literals::internal::from_hex<address>(s);
    return evmaddr;
}

std::string evm_utils::GenerateEvmAddr(const std::string& pub)
{
    std::string md160 = GetMd160(pub);

    return md160;
}

std::string evm_utils::GenerateContractAddr(const std::string& input)
{
    return EvmAddrToBase58(GenerateEvmAddr(input));
}

std::string evm_utils::EvmAddrToBase58(const std::string& addr)
{
    evmc_address evmAddr = StringToEvmAddr(addr);
    return EvmAddrToBase58(evmAddr);
}

std::string evm_utils::GetEvmAddr(const std::string& pub)
{
    std::string md160 = GenerateEvmAddr(pub);
    std::string evmAddress = EvmAddrToChecksum(md160);
    return "0x" + evmAddress;
}

std::string evm_utils::EvmAddrToChecksum(const std::string& addr)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*> (addr.c_str());
    std::string hash = keccak256(data, addr.length());
    if(hash.length() < 64)
    {
       hash = "0" + hash;
    }
    return ToChecksumAddress(hash, addr);
}

