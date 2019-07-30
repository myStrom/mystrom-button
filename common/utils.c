// Copyright 2016-2019 myStrom AG
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include "utils.h"
#include "debug.h"

uint16_t ICACHE_FLASH_ATTR utils_mac_bytes_to_string(unsigned char* result, unsigned char* mac_bytes) {
	uint8_t i;
	int8_t j;
	uint8_t idx = 0;
	uint8_t tmp;
	for (i = 0; i < 6; ++i) {
		for (j = 1; j >= 0; --j) {
			if (j)
				tmp = mac_bytes[i] >> 4;
			else
				tmp = mac_bytes[i] & 0x0f;

			result[idx++] = tmp + (tmp < 10 ? 48 : 55);
		}
	}
	result[idx] = 0;
	return idx;
}

uint8_t ICACHE_FLASH_ATTR utils_hex_to_digit(char data) {
	if ((data >= '0') && (data <= '9')) {
		return data - '0';
	} else if ((data >= 'A') && (data <= 'F')) {
		return data + 10 - 'A';
	} else if ((data >= 'a') && (data <= 'f')) {
		return data + 10 - 'a';
	}
	return 0;
}

void ICACHE_FLASH_ATTR utils_mac_string_to_bytes(const char* str, uint8_t* bytes) {
	uint8_t i;
	if (!bytes || !str) {
		return;
	}
	for (i = 0; i < 6; i++) {
		bytes[i] = utils_hex_to_digit(str[i*2]) << 4;
		bytes[i] |= utils_hex_to_digit(str[i*2 + 1]) & 0x0F;
	}
}
