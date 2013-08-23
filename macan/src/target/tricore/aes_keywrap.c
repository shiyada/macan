/*
 *  Copyright 2014 Czech Technical University in Prague
 *
 *  Authors: Michal Sojka <sojkam1@fel.cvut.cz>
 *           Radek Matějka <radek.matejka@gmail.com>
 *           Ondřej Kulatý <kulatond@fel.cvut.cz>
 *
 *  This file is part of MaCAN.
 *
 *  MaCAN is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  MaCAN is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MaCAN.	If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <common.h>
#include "she.h"

/* ToDo:
 * 	consider endianness
 */

uint32_t bswap32(uint32_t x)
{
	return	((x << 24) & 0xff000000 ) |
		((x <<  8) & 0x00ff0000 ) |
		((x >>  8) & 0x0000ff00 ) |
		((x >> 24) & 0x000000ff );
}

/**
 * aes_wrap() - AES key wrap algorithm
 * @ctx:     AES context with _encryption_ key set
 * @length:  length of src
 * @dst:     cipher text will be written to, i.e. (length + 8) bytes
 * @src:     plain text
 *
 * aes_wrap() ciphers data at src to produce cipher text at dst. It is
 * implemented as specified in RFC 3394.
 */
void aes_wrap(uint8_t *key, size_t length, uint8_t *dst, const uint8_t *src)
{
	uint8_t a[8] = { 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };
	uint8_t b[16];
	size_t block_size = 16;
	uint32_t t, n;
	int i, j, k;

	assert((length % 8) == 0);

	memcpy(dst + 8, src, length);
	n = length / 8;

	for (j = 0; j < 6; j++) {
		for (i = 1; i < n + 1; i++) {
			memcpy(b, a, 8);
			memcpy(b + 8, dst + (8 * i), 8);
			aes_encrypt(key, block_size, b, b);

			memcpy(a, b, 8);
			t = (n*j) + i;
			t = bswap32(t);
			for (k = 0; k < 4; k++) {
				*(a + 4 + k) = *(a + 4 + k) ^ *(((uint8_t *)&t) + k);
			}

			memcpy(dst + (8 * i), b + 8, 8);   /* ToDo: write to dst */
		}
	}

	memcpy(dst, a, 8);
}

/* ToDo: suit for TC1798 */
/**
 * aes_unwrap() - AES key unwrap algorithm
 * @ctx:     AES context with _decryption_ key set
 * @length:  length of src in bytes
 * @dst:     plain text will be written to, i.e. (length - 8) bytes
 * @src:     cipher text
 * @tmp:     auxiliary buffer with length size,
 *
 * The function unwraps key data stored at src. An auxiliary buffer tmp is
 * required, although it is possible to supply src as tmp.
 */
int aes_unwrap(uint8_t *key, size_t length, uint8_t *dst, uint8_t *src, uint8_t *tmp)
{
	uint8_t iv[8] = { 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };
	uint8_t b[16];
	size_t block_size = 16;
	uint32_t t, n;
	uint32_t *af;
	int i, j, k;

	assert((length % 8) == 0 && length > 0);

	memcpy(tmp, src, length);
	n = length / 8 - 1;

	for (j = 5; j >= 0; j--) {
		for (i = n; i > 0; i--) {
			t = (n*j) + i;
			t = bswap32(t);
			for (k = 0; k < 4; k++) {
				*(tmp + 4 + k) = *(tmp + 4 + k) ^ *(((uint8_t *)&t) + k);
			}

			memcpy(b, tmp, 8);
			memcpy(b + 8, tmp + (8 * i), 8);
			aes_decrypt(key, block_size, b, b);

			memcpy(tmp, b, 8);
			memcpy(tmp + (8 * i), b + 8, 8);   /* ToDo: write to dst */
		}
	}

	if (!memchk(tmp, iv, 8))
		return 1;
	memcpy(dst, tmp + 8, length - 8);

	return 0;
}