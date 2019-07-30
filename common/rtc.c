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
#include <user_interface.h>
#include "rtc.h"
#include "debug.h"

static uint8_t ICACHE_FLASH_ATTR rtc_check(uint8_t offset, uint16_t len) {
	if (!offset || !len) {
		return 0;
	}
	if (len < 4) {
		return 0;
	}
	if ((offset < 64) || (offset >= 192)) {
		return 0;
	}
	/*TODO: Chacke boundary*/
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rtc_write(uint8_t offset, void* data, uint16_t len) {
	uint32_t magic = RTC_MAGIC;
	if (!data) {
		return 0;
	}
	if (!rtc_check(offset, len)) {
		return 0;
	}
	memcpy(data, &magic, sizeof(magic));
	if (!system_rtc_mem_write(offset, data, len)) {
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rtc_read(uint8_t offset, void* data, uint16_t len) {
	uint32_t magic = RTC_MAGIC;
	if (!data) {
		return 0;
	}
	if (!rtc_check(offset, len)) {
		return 0;
	}
	if (!system_rtc_mem_read(offset, data, len)) {
		return 0;
	}
	if (memcmp(data, &magic, sizeof(magic))) {
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rtc_erase(uint8_t offset, void* data, uint16_t len) {
	if (!data) {
		return 0;
	}
	if (!rtc_check(offset, len)) {
		return 0;
	}
	memset(data, 0, len);
	if (!system_rtc_mem_write(offset, data, len)) {
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rtc_erase_all(void) {
	uint32_t pad = 0xFFFFFFFF;
	uint8_t i;
	for (i = RTC_IP_OFFSET; i < 192; i++) {
		if (!system_rtc_mem_write(i, &pad, sizeof(pad))) {
			return 0;
		}
	}
	return 1;
}
