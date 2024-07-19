/*
 * @Author: lyw 15035612538@163.com
 * @Date: 2024-05-13 18:08:23
 * @LastEditors: lyw 15035612538@163.com
 * @LastEditTime: 2024-05-24 09:26:26
 * @FilePath: /tfs_sdk/tfs_sdk/src/envelop/keccak256.cpp
 * @Description:https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%A
 */
#include "../utils/keccak_cryopp.hpp"
#include <cryptopp/keccak.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <stdio.h>
#include <sstream>
#include <iomanip>
std::string Keccak256Crypt(const std::string& input) 
{
    using namespace CryptoPP;
    std::string digest;
    Keccak_256 hash;
    StringSource ss(input, true, 
        new HashFilter(hash,
            new HexEncoder(
                new StringSink(digest)
            )
        )
    );

    return digest;
}

