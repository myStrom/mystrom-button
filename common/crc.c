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


#include <c_types.h>
#include "crc.h"

Crc_T* ICACHE_FLASH_ATTR crc_init(Crc_T* crc, uint64_t poly, uint64_t init_value) {
	uint8_t len;
	if (!crc || !poly) {
		return NULL;
	}
	len = sizeof(poly)*8 - 1;
	while (!(poly & ((uint64_t)1 << len))) {
		len--;
	}
	crc->poly = poly;
	crc->power = len;
	crc->init_value = init_value;
	crc->last_value = crc->init_value;
	return crc;
}

uint64_t ICACHE_FLASH_ATTR crc_calculate(Crc_T* crc, uint64_t value) {
	uint64_t poly;
	uint8_t i;
	if (!crc) {
		return 0;
	}
	poly = crc->poly << (crc->power - 1);
	value <<= crc->power;
	value |= crc->last_value;
	for (i = ((crc->power * 2) - 1); i >= crc->power; i--) {
		if (value & ((uint64_t)1 << i)) {
			value ^= poly;
		}
		poly >>= 1;
	}
	crc->last_value = value;
	return crc->last_value;
}

void ICACHE_FLASH_ATTR crc_reset(Crc_T* crc) {
	if (!crc) {
		return;
	}
	crc->last_value = crc->init_value;
}

uint64_t ICACHE_FLASH_ATTR crc_last(Crc_T* crc) {
	if (!crc) {
		return 0;
	}
	return crc->last_value;
}

uint64_t ICACHE_FLASH_ATTR crc_check(Crc_T* crc, const uint8_t* buffer, uint16_t length) {
	uint16_t i;
	if (!crc || !buffer || !length) {
		return 0;
	}
	for (i = 0; i < length; i++) {
		crc_calculate(crc, buffer[i]);
	}
	return crc_last(crc);
}
