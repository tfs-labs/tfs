/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include <stdio.h>

#include "utils/bip39.h"
#include "hmac.h"
#include "utils/sha2.h"
#include "bip39_english.h"
#include "../include/logging.h"
#include "utils/hex_code.h"

int mnemonic_from_data(const uint8_t *data, int len, char *out, size_t outLen)
{
	if (len % 4 || len < 16 || len > 32) {
		return 0;
	}

	uint8_t bits[32 + 1];

	memset(out, 0x00, outLen);

	sha256_Raw(data, len, bits);
	// checkSum
	bits[len] = bits[0];
	// data
	memcpy(bits, data, len);

	int mlen = len * 3 / 4;


	int i, j, idx;
	char *p = out;
	for (i = 0; i < mlen; i++) {
		idx = 0;
		for (j = 0; j < 11; j++) {
			idx <<= 1;
			idx += (bits[(i * 11 + j) / 8] & (1 << (7 - ((i * 11 + j) % 8)))) > 0;
		}
		if(outLen < strlen(p) + strlen(wordlist[idx]))
			return 0;
		strcpy(p, wordlist[idx]);
		p += strlen(wordlist[idx]);
		*p = (i < mlen - 1) ? ' ' : 0;
		p++;
	}

	return mlen;
}

int mnemonic_check(const char *mnemonic, char *out, int *outLen)
{
	if (!mnemonic) {
		return 0;
	}

	uint32_t i, n;

	i = 0; n = 0;
	while (mnemonic[i]) {
		if (mnemonic[i] == ' ') {
			n++;
		}
		i++;
	}
	n++;
	// check number of words
	if (n != 12 && n != 18 && n != 24) {
		return 0;
	}

	char current_word[10];
	uint32_t j, k, ki, bi;
	uint8_t bits[32 + 1];
	memset(bits, 0, sizeof(bits));
	i = 0; bi = 0;
	while (mnemonic[i]) {
		j = 0;
		while (mnemonic[i] != ' ' && mnemonic[i] != 0) {
			if (j >= sizeof(current_word)) {
				return 0;
			}
			current_word[j] = mnemonic[i];
			i++; j++;
		}
		current_word[j] = 0;
		if (mnemonic[i] != 0) i++;
		k = 0;
		for (;;) {
			if (!wordlist[k]) { // word not found
				return 0;
			}
			if (strcmp(current_word, wordlist[k]) == 0) { // word found on index k
				for (ki = 0; ki < 11; ki++) {
					if (k & (1 << (10 - ki))) {
						bits[bi / 8] |= 1 << (7 - (bi % 8));
					}
					bi++;
				}
				break;
			}
			k++;
		}
	}
	if (bi != n * 11) {
		return 0;
	}

	
	memcpy(out, bits, n * 4 / 3);
	*outLen = n * 4 / 3;
	bits[32] = bits[n * 4 / 3];
	sha256_Raw(bits, n * 4 / 3, bits);
	if (n == 12) {
		return (bits[0] & 0xF0) == (bits[32] & 0xF0); // compare first 4 bits
	} else
	if (n == 18) {
		return (bits[0] & 0xFC) == (bits[32] & 0xFC); // compare first 6 bits
	} else
	if (n == 24) {
		return bits[0] == bits[32]; // compare 8 bits
	}
	return 0;
}

int import_mnemonic(const std::string & mnemonic,uint8_t* seed){
    char out[33] = {0};
    int outLen = 0;
	if(mnemonic_check((char *)mnemonic.c_str(), out, &outLen) == 0)
    {
        return -1;
    }
    char mnemonicHex[65] = {0};
	encode_hex(mnemonicHex, out, outLen);
	//std::cout << "mnemonci_hex" << mnemonicHex <<std::endl;
	std::string mnemonicKey;
    //std::cout<<"seed"<<seed<<std::endl;
	mnemonicKey.append(mnemonicHex, outLen * 2);
	seed = (uint8_t *)mnemonicKey.c_str();
    return 0;
}