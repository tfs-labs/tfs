/*
 * Copyright 2012-2014 Luke Dashjr
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the standard MIT license.  See COPYING for more details.
 */

#include <arpa/inet.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "utils/base58.h"
#include "utils/sha2.h"
#include <limits.h>
#include <evmc/hex.hpp>
bool (*b58_sha256_impl)(void *, const void *, size_t) = NULL;

static const int8_t b58digits_map[] = {
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6,  7, 8,-1,-1,-1,-1,-1,-1,
	-1, 9,10,11,12,13,14,15, 16,-1,17,18,19,20,21,-1,
	22,23,24,25,26,27,28,29, 30,31,32,-1,-1,-1,-1,-1,
	-1,33,34,35,36,37,38,39, 40,41,42,43,-1,44,45,46,
	47,48,49,50,51,52,53,54, 55,56,57,-1,-1,-1,-1,-1,
};

typedef uint64_t b58_maxint_t;
typedef uint32_t b58_almostmaxint_t;
#define b58_almostmaxint_bits (sizeof(b58_almostmaxint_t) * 8)
static const b58_almostmaxint_t b58_almostmaxint_mask = ((((b58_maxint_t)1) << b58_almostmaxint_bits) - 1);

bool b58tobin(void *bin, size_t *binszp, const char *b58, size_t b58sz)
{
	size_t binsz = *binszp;
	const unsigned char *b58u = (unsigned char *)(void*)b58;
	unsigned char *binu = (unsigned char *)bin;
	size_t outisz = (binsz + sizeof(b58_almostmaxint_t) - 1) / sizeof(b58_almostmaxint_t);
	b58_almostmaxint_t outi[outisz];
	b58_maxint_t t;
	b58_almostmaxint_t c;
	size_t i, j;
	uint8_t bytesleft = binsz % sizeof(b58_almostmaxint_t);
	b58_almostmaxint_t zeromask = bytesleft ? (b58_almostmaxint_mask << (bytesleft * 8)) : 0;
	unsigned zerocount = 0;
	
	if (!b58sz)
		b58sz = strlen(b58);
	
	for (i = 0; i < outisz; ++i) {
		outi[i] = 0;
	}
	
	// Leading zeros, just count
	for (i = 0; i < b58sz && b58u[i] == '1'; ++i)
		++zerocount;
	
	for ( ; i < b58sz; ++i)
	{
		if (b58u[i] & 0x80)
			// High-bit set on invalid digit
			return false;
		if (b58digits_map[b58u[i]] == -1)
			// Invalid base58 digit
			return false;
		c = (unsigned)b58digits_map[b58u[i]];
		for (j = outisz; j--; )
		{
			t = ((b58_maxint_t)outi[j]) * 58 + c;
			c = t >> b58_almostmaxint_bits;
			outi[j] = t & b58_almostmaxint_mask;
		}
		if (c)
			// Output number too big (carry to the next int32)
			return false;
		if (outi[0] & zeromask)
			// Output number too big (last int32 filled too far)
			return false;
	}
	
	j = 0;
	if (bytesleft) {
		for (i = bytesleft; i > 0; --i) {
			*(binu++) = (outi[0] >> (8 * (i - 1))) & 0xff;
		}
		++j;
	}
	
	for (; j < outisz; ++j)
	{
		for (i = sizeof(*outi); i > 0; --i) {
			*(binu++) = (outi[j] >> (8 * (i - 1))) & 0xff;
		}
	}
	
	// Count canonical base58 byte count
	binu = (unsigned char *)bin;
	for (i = 0; i < binsz; ++i)
	{
		if (binu[i])
			break;
		--*binszp;
	}
	*binszp += zerocount;
	
	return true;
}

static
bool my_dblsha256(void *hash, const void *data, size_t datasz)
{
	bu_Hash((unsigned char *)hash, data, datasz);
	return true;
}

static const char b58digits_ordered[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool b58enc(char *b58, size_t *b58sz, const void *data, size_t binsz)
{
	const uint8_t *bin = (uint8_t *)data;
	int carry;
	size_t i, j, high, zcount = 0;
	size_t size;
	
	while (zcount < binsz && !bin[zcount])
		++zcount;
	
	size = (binsz - zcount) * 138 / 100 + 1;
	uint8_t buf[size];
	memset(buf, 0, size);
	
	for (i = zcount, high = size - 1; i < binsz; ++i, high = j)
	{
		for (carry = bin[i], j = size - 1; (j > high) || carry; --j)
		{
			carry += 256 * buf[j];
			buf[j] = carry % 58;
			carry /= 58;
			if (!j) {
				// Otherwise j wraps to maxint which is > high
				break;
			}
		}
	}
	
	for (j = 0; j < size && !buf[j]; ++j);
	
	if (*b58sz <= zcount + size - j)
	{
		*b58sz = zcount + size - j + 1;
		return false;
	}
	
	if (zcount)
		memset(b58, '1', zcount);
	for (i = zcount; j < size; ++i, ++j)
		b58[i] = b58digits_ordered[buf[j]];
	b58[i] = '\0';
	*b58sz = i + 1;
	
	return true;
}

static const char alphabet[] =
"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static const char alphamap[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,
	-1,  9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
	-1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
	47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1
};

int base58_encode(const char *in, size_t in_len, char *out, size_t *out_len) {
	if (!in_len) {
		*out_len = 0;
		return 0;
	}
	if (!(*out_len)) {
		*out_len = 0;
		return -1;
	}

	// leading zeroes
	size_t total = 0;
	for (size_t i = 0; i < in_len && !in[i]; ++i) {
		if (total == *out_len) {
			*out_len = 0;
			return -1;
		}
		out[total++] = alphabet[0];
	}
	in += total;
	in_len -= total;
	out += total;

	// encoding
	size_t idx = 0;
	for (size_t i = 0; i < in_len; ++i) {
		unsigned int carry = (unsigned char)in[i];
		for (size_t j = 0; j < idx; ++j) {
			carry += (unsigned int)out[j] << 8;
			out[j] = (unsigned char)(carry % 58);
			carry /= 58;
		}
		while (carry > 0) {
			if (total == *out_len) {
				*out_len = 0;
				return -1;
			}
			total++;
			out[idx++] = (unsigned char)(carry % 58);
			carry /= 58;
		}
	}

	// apply alphabet and reverse
	size_t c_idx = idx >> 1;
	for (size_t i = 0; i < c_idx; ++i) {
		char s = alphabet[(unsigned char)out[i]];
		out[i] = alphabet[(unsigned char)out[idx - (i + 1)]];
		out[idx - (i + 1)] = s;
	}
	if ((idx & 1)) {
		out[c_idx] = alphabet[(unsigned char)out[c_idx]];
	}
	*out_len = total;
	return 0;
}

int base58_decode(const char *in, size_t in_len, char *out, size_t *out_len) {
	if (!in_len) {
		*out_len = 0;
		return 0;
	}
	if (!(*out_len)) {
		*out_len = 0;
		return -1;
	}

	// leading ones
	size_t total = 0;
	for (size_t i = 0; i < in_len && in[i] == '1'; ++i) {
		if (total == *out_len) {
			*out_len = 0;
			return -1;
		}
		out[total++] = 0;
	}
	in += total;
	in_len -= total;
	out += total;

	// decoding
	size_t idx = 0;
	for (size_t i = 0; i < in_len; ++i) {
		unsigned int carry = (unsigned int)alphamap[(unsigned char)in[i]];
		if (carry == UINT_MAX) {
			*out_len = 0;
			return -1;
		}
		for (size_t j = 0; j < idx; j++) {
			carry += (unsigned char)out[j] * 58;
			out[j] = (unsigned char)(carry & 0xff);
			carry >>= 8;
		}
		while (carry > 0) {
			if (total == *out_len) {
				*out_len = 0;
				return -1;
			}
			total++;
			out[idx++] = (unsigned char)(carry & 0xff);
			carry >>= 8;
		}
	}

	// apply simple reverse
	size_t c_idx = idx >> 1;
	for (size_t i = 0; i < c_idx; ++i) {
		char s = out[i];
		out[i] = out[idx - (i + 1)];
		out[idx - (i + 1)] = s;
	}
	*out_len = total;
	return 0;
}




bool b58check_enc(char *b58c, size_t *b58c_sz, uint8_t ver, const void *data, size_t datasz)
{
	uint8_t buf[1 + datasz + 0x20];
	uint8_t *hash = &buf[1 + datasz];
	
	buf[0] = ver;
	memcpy(&buf[1], data, datasz);
	if (!my_dblsha256(hash, buf, datasz + 1))
	{
		*b58c_sz = 0;
		return false;
	}
	
	return b58enc(b58c, b58c_sz, buf, 1 + datasz + 4);
}

bool GetBase58Addr(char *b58c, size_t *b58c_sz, uint8_t ver, const void *data, size_t datasz)
{
    unsigned char md160[RIPEMD160_DIGEST_LENGTH];
    bu_Hash160(md160, data, datasz);
    return b58check_enc(b58c, b58c_sz, ver, md160, RIPEMD160_DIGEST_LENGTH);
}
std::string GetMd160(std::string key)
{
	unsigned char md160[RIPEMD160_DIGEST_LENGTH];
	bu_Hash160(md160, key.data(),key.size());
	std::string str = evmc::hex({md160,20});
	return str;
}
std::string GetBase58Addr(std::string key, Base58Ver ver)
{
	char buf[2048] = {0};
	size_t buf_len = sizeof(buf);
	GetBase58Addr(buf, &buf_len, (int)ver, key.data(), key.size());
	std::string str(buf);
	return str;	
}

bool CheckBase58Addr(const std::string & base58Addr, Base58Ver ver)
{
	int length = base58Addr.length();
	if (length != 33 && length != 34)
	{
		return false;
	}

	char firstChar = base58Addr.at(0);
	if (firstChar != '1' &&  firstChar != '3')
	{
		return false;
	}

	if (ver == Base58Ver::kBase58Ver_All && (firstChar != '1' && firstChar != '3'))
	{
		return false;
	}
	else 
	{
		if (ver == Base58Ver::kBase58Ver_Normal && firstChar != '1')
		{
			return false;
		}
		else if (ver == Base58Ver::kBase58Ver_MultiSign && firstChar != '3')
		{
			return false;
		}
	}

	char decodebuf[1024] = { 0 };
	size_t decodebufLen = sizeof(decodebuf);
	int res = base58_decode(base58Addr.c_str(), length, decodebuf, &decodebufLen);
	if(res != 0) 
	{
		return false;
	}

	uint8_t hash[SHA256_DIGEST_LENGTH] = {0};
	my_dblsha256(hash, decodebuf, decodebufLen - 4);
	
	return (memcmp(((unsigned char *)decodebuf) + decodebufLen - 4, hash, 4) == 0);
}