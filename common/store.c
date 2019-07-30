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
#include "store.h"
#include "crc.h"
#include "debug.h"
#include "color.h"
#include "array_size.h"
#include "slash.h"
#include "url_storage.h"

#define STORE_SECTOR_A 0xFE
#define STORE_SECTOR_B 0xFF

#define STORE_CRC_POLY 0x1EDC6F41
#define STORE_SIZE_BEFORE_VERSION ((uint16_t)304)
#define STORE_HEADER_SIZE ((uint16_t)8)
#define STORE_MAX_SIZE ((uint16_t)1024)

struct Store_T store;

struct {
	uint32_t counter : 20;
	uint32_t current : 1;
	uint32_t a : 1;
	uint32_t b : 1;
} static store_valid;

extern Url_Storage_T url_storage;

static const uint8_t esp_init_data_default_bin[128] __attribute__ ((aligned(4))) = {
	0x05, 0x00, 0x04, 0x02, 0x05, 0x05, 0x05, 0x02, 0x05, 0x00, 0x04, 0x05, 0x05, 0x04, 0x05, 0x05,
	0x04, 0xFE, 0xFD, 0xFF, 0xF0, 0xF0, 0xF0, 0xE0, 0xE0, 0xE0, 0xE1, 0x0A, 0x00, 0x00, 0xF8, 0x00,
	0xF8, 0xF8, 0x52, 0x4E, 0x4A, 0x44, 0x40, 0x38, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x05,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xE1, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x93, 0x43, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void ICACHE_FLASH_ATTR store_handle_old_format(uint32_t len) {
	debug_describe_P("Handle old format here");
}

static uint8_t ICACHE_FLASH_ATTR store_load(uint8_t id, uint32_t* counter) {
	Crc_T crc;
	uint32_t valid_crc;
	uint32_t read_len = sizeof(store);
	if (id > 1) {
		return 0;
	}
	memset(&store, 0, sizeof(store));
	crc_init(&crc, STORE_CRC_POLY, 0);
	debug_printf("Read form section: %d\n", id ? STORE_SECTOR_B : STORE_SECTOR_A);
	if (spi_flash_read((id ? STORE_SECTOR_B : STORE_SECTOR_A) * SPI_FLASH_SEC_SIZE, (uint32_t*)&store, STORE_HEADER_SIZE) != SPI_FLASH_RESULT_OK) {
		return 0;
	}
	debug_value(store.len);
	if (!store.len) {
		debug_describe_P("Store in very old format");
		read_len = STORE_SIZE_BEFORE_VERSION;
	} else {
		if ((store.len > sizeof(store)) || (store.len < STORE_SIZE_BEFORE_VERSION)) {
			debug_describe_P(COLOR_RED "Store in not handled version" COLOR_END);
			return 0;
		} else {
			if (store.len < sizeof(store)) {
				debug_describe_P("Store in old format");
				read_len = store.len;
			} else {
				debug_describe_P("Store in current format");
				read_len = sizeof(store);
			}
		}
	}
	memset(&store, 0, sizeof(store));
	if (spi_flash_read((id ? STORE_SECTOR_B : STORE_SECTOR_A) * SPI_FLASH_SEC_SIZE, (uint32_t*)&store, read_len) != SPI_FLASH_RESULT_OK) {
		return 0;
	}
	valid_crc = store.crc;
	store.crc = 0;
	if ((crc_check(&crc, (uint8_t*)&store, read_len) & 0xFFFFFFFF) != valid_crc) {
		debug_printf("Bad CRC in store %s\n", id ? "B" : "A");
		return 0;
	}
	if (counter) {
		*counter = store.counter;
	}
	if (read_len < sizeof(store)) {
		store_handle_old_format(read_len);
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR store_larger(uint32_t a, uint32_t b, uint8_t* id) {
	if (!id) {
		return 0;
	}
	if (a == b) {
		return 0;
	}
	if ((a == 0x000FFFFF) && !b) {
		*id = 1;
		return 1;
	}
	if ((b == 0x000FFFFF) && !a) {
		*id = 0;
		return 1;
	}
	/*diff abs*/
	if (a > b) {
		if ((a - b) > 1) {
			return 0;
		}
		*id = 0;
		return 1;
	}
	if ((b - a) > 1) {
		return 0;
	}
	*id = 1;
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR store_store(uint8_t id, uint32_t counter) {
	Crc_T crc;
	debug_printf("Write store id: %d, counter: %u\n", (int)id, counter);
	if (spi_flash_erase_sector(id ? STORE_SECTOR_B : STORE_SECTOR_A) != SPI_FLASH_RESULT_OK) {
		debug_describe_P("Unable to erase sector");
		return 0;
	}
	crc_init(&crc, STORE_CRC_POLY, 0);
	store.crc = 0;
	store.counter = counter;
	store.len = sizeof(store);
	store.version = STORE_VERSION;
	crc_check(&crc, (uint8_t*)&store, sizeof(store));
	store.crc = crc_last(&crc);
	if (spi_flash_write((id ? STORE_SECTOR_B : STORE_SECTOR_A) * SPI_FLASH_SEC_SIZE, (uint32_t*)&store, sizeof(store)) == SPI_FLASH_RESULT_OK) {
		#ifdef PRINT_SAVE_NET
		debug_printf("SSID: %s\n", store.connect.station.ssid);
		debug_printf("PWD: %s\n", store.connect.station.password);
		debug_printf("BSSID: " MACSTR "\n", MAC2STR(store.connect.station.bssid));
		debug_printf("SET: %d\n", (int)store.connect.station.bssid_set);
		#endif
		return 1;
	}
	debug_describe_P("Unable to write sector");
	return 0;
}

static uint8_t ICACHE_FLASH_ATTR store_select(uint8_t* id) {
	uint32_t counter_a;
	uint32_t counter_b;
	if (!id) {
		return 0;
	}
	store_valid.a = 0;
	store_valid.b = 0;
	if (store_load(0, &counter_a)) {
		if (store_load(1, &counter_b)) {
			/*two valid store, select newest*/
			if (store_larger(counter_a, counter_b, id)) {
				store_valid.a = 1;
				store_valid.b = 1;
				debug_describe_P("Store A and B valid");
				return 1;
			}
			return 0;
		}
		store_valid.a = 1;
		*id = 0;
		debug_describe_P("Only store A valid");
		return 1;
	}
	if (store_load(1, NULL)) {
		store_valid.b = 1;
		*id = 1;
		debug_describe_P("Only store B valid");
		return 1;
	}
	return 0;
}

// This function detects that the configuration using old method (not general) has been defined by the HAEngine
// and automatically migrates it to general configuration
static uint8_t ICACHE_FLASH_ATTR store_hae_auto_migration(void) {
	Slash_T slash_struct;
	char* check_url = NULL;
	char* url = NULL;
	char* match_url = NULL;
	uint8_t i;
	uint8_t ret_val = 0;
	for (i = 0; i < __URL_TYPE_MAX - 1; i++) {
		if (!(url = url_storage_read(&url_storage, i))) {
			return 0;
		}
		free(url);
	}
	url = url_storage_read(&url_storage, URL_TYPE_SINGLE);
	if (!url) {
		goto done;
	}
	Slash_T *slash = slash_init_delimeters(&slash_struct, url, ":/");
	char *method = slash_next(slash);
	if (method == NULL || strcmp(method, "get")) {
		goto done;
	}
	char *address = slash_next(slash);
	if (address == NULL) {
		goto done;
	}
	char *port = slash_next(slash);
	if (port == NULL || strcmp(port, "8888")) {
		goto done;
	}
	char *uri = slash_next(slash);
	if (uri == NULL || strcmp(uri, "single")) {
		goto done;
	}

	uint32_t check_url_len = strlen(url) + 10;
	check_url = malloc(check_url_len + 1);
	if (!check_url) {
		goto done;
	}

	if (!(match_url = url_storage_read(&url_storage, URL_TYPE_TOUCH))) {
		goto done;
	}
	snprintf(check_url, check_url_len, "%s://%s:%s/%s", method, address, port, "touch");
	if (strcmp(check_url, match_url)) {
		goto done;
	}
	free(match_url);
	
	if (!(match_url = url_storage_read(&url_storage, URL_TYPE_DOUBLE))) {
		goto done;
	}
	snprintf(check_url, check_url_len, "%s://%s:%s/%s", method, address, port, "double");
	if (strcmp(check_url, match_url)) {
		goto done;
	}
	free(match_url);

	if (!(match_url = url_storage_read(&url_storage, URL_TYPE_LONG))) {
		goto done;
	}
	snprintf(check_url, check_url_len, "%s://%s:%s/%s", method, address, port, "long");
	if (strcmp(check_url, match_url)) {
		goto done;
	}

	url_storage_write(&url_storage, URL_TYPE_SINGLE, "");
	url_storage_write(&url_storage, URL_TYPE_DOUBLE, "");
	url_storage_write(&url_storage, URL_TYPE_LONG, "");
	url_storage_write(&url_storage, URL_TYPE_TOUCH, "");

	snprintf(check_url, check_url_len, "%s://%s:8887/generic", method, address);
	url_storage_write(&url_storage, URL_TYPE_GENERIC, check_url);
	ret_val = 1;
	debug_describe(COLOR_MAGENTA "HAE URL migrate" COLOR_END);
done:
	free(match_url);
	free(check_url);
	free(url);
	return ret_val;
}

void ICACHE_FLASH_ATTR store_init(void) {
	/*load data to RAM*/
	uint32_t counter;
	uint8_t id;
	memset(&store, 0, sizeof(store));
	if (store_select(&id) && store_load(id, &counter)) {
		store_valid.current = id;
		store_valid.counter = counter;
		debug_printf("Store load: %u, %u\n", (uint32_t)id, counter);
		uint8_t migrated = 0;
		switch (store.version) {
			case 0:
				break;
		}
		migrated += store_hae_auto_migration();

		if (migrated) {
			store_save();
		}
	} else {
		debug_describe_P(COLOR_RED "CORRUPT STORE" COLOR_END);
		memset(&store, 0, sizeof(store));
		store_valid.counter = 0;
		store_valid.current = 0;
		store.version = STORE_VERSION;
		store_save();
	}
}

uint8_t ICACHE_FLASH_ATTR store_save(void) {
	store_valid.counter++;
	store_valid.current++;
	return store_store(store_valid.current, store_valid.counter);
}

void ICACHE_FLASH_ATTR store_erase(void) {
	spi_flash_erase_sector(STORE_SECTOR_A);
	spi_flash_erase_sector(STORE_SECTOR_B);
	store_init();
}
