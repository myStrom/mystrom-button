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
#include <osapi.h>
#include <ctype.h>
#include <user_interface.h>
#include "url_storage.h"
#include "crc.h"
#include "debug.h"

#define URL_CRC_POLY 0x741B8CD7

static uint8_t ICACHE_FLASH_ATTR url_storage_check_crc(Url_Storage_T* url, uint32_t stored_crc, uint32_t sector) {
	Crc_T crc;
	uint32_t value;
	uint32_t i;
	if (!url) {
		return 0;
	}
	crc_init(&crc, URL_CRC_POLY, 0);
	for (i = 1; i < (SPI_FLASH_SEC_SIZE / sizeof(value)); i++) {
		if (spi_flash_read((sector * SPI_FLASH_SEC_SIZE) + (i * sizeof(value)), &value, sizeof(value)) != SPI_FLASH_RESULT_OK) {
			return 0;
		}
		crc_calculate(&crc, value);
	}
	if ((crc_last(&crc) & 0xFFFFFFFF) != stored_crc) {
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR url_storage_get_write_sector(Url_Storage_T* url, uint32_t* sector, uint32_t* counter) {
	Url_Storage_Herader_T header[2];
	uint32_t i;
	uint8_t valid[2] = {0, 0};
	if (!url || !sector || !counter) {
		return 0;
	}
	for (i = 0; i < 2; i++) {
		if (spi_flash_read((url->base_sector + i) * SPI_FLASH_SEC_SIZE, (uint32_t*)&header[i], sizeof(Url_Storage_Herader_T)) != SPI_FLASH_RESULT_OK) {
			return 0;
		}
		valid[i] = url_storage_check_crc(url, header[i].crc, url->base_sector + i);
		debug_value(valid[i]);
	}
	if (!valid[0] || !valid[1]) {
		if (!valid[0] && !valid[1]) {
			*sector = 0;
			*counter = 0;
			return 1;
		}
		if (!valid[0]) {
			*sector = 0;
			*counter = header[1].counter + 1;
			return 1;
		}
		*sector = 1;
		*counter = header[0].counter + 1;
		return 1;
	}
	if (header[0].counter < header[1].counter) {
		*sector = 0;
		*counter = header[1].counter + 1;
		return 1;
	}
	*sector = 1;
	*counter = header[0].counter + 1;
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR url_storage_get_read_sector(Url_Storage_T* url, uint32_t* sector) {
	Url_Storage_Herader_T header[2];
	uint32_t i;
	uint8_t valid[2] = {0, 0};
	if (!url || !sector) {
		return 0;
	}
	for (i = 0; i < 2; i++) {
		if (spi_flash_read((url->base_sector + i) * SPI_FLASH_SEC_SIZE, (uint32_t*)&header[i], sizeof(Url_Storage_Herader_T)) != SPI_FLASH_RESULT_OK) {
			return 0;
		}
		valid[i] = url_storage_check_crc(url, header[i].crc, url->base_sector + i);
	}
	if (!valid[0] || !valid[1]) {
		if (!valid[0] && !valid[1]) {
			return 0;
		}
		if (!valid[0]) {
			*sector = 1;
			return 1;
		}
		*sector = 0;
		return 1;
	}
	if (header[0].counter < header[1].counter) {
		*sector = 1;
		return 1;
	}
	*sector = 0;
	return 1;
}

Url_Storage_T* ICACHE_FLASH_ATTR url_storage_init(Url_Storage_T* url, uint32_t base_sector) {
	if (!url) {
		return 0;
	}
	memset(url, 0, sizeof(Url_Storage_T));
	url->base_sector = base_sector;
	return url;
}

uint8_t ICACHE_FLASH_ATTR url_storage_write(Url_Storage_T* url, Url_Storage_Type_T type, const char* str) {
	Crc_T crc;
	uint32_t offset = 0;
	uint32_t in_range_start = 0;
	uint32_t in_range_end = 0;
	uint32_t sector;
	uint32_t counter;
	uint32_t len;
	uint32_t i;
	uint8_t value[4];
	uint32_t value_32;
	if (!url) {
		return 0;
	}
	if (type >= __URL_TYPE_MAX) {
		return 0;
	}
	if (!str) {
		return 0;
	}
	if (!url_storage_get_write_sector(url, &sector, &counter)) {
		return 0;
	}
	debug_value(sector);
	debug_value(counter);
	if (spi_flash_erase_sector(url->base_sector + sector) != SPI_FLASH_RESULT_OK) {
		return 0;
	}
	/*rewrite, start and end must be align to 4*/
	in_range_start = sizeof(Url_Storage_Herader_T) + ((uint32_t)URL_MAX_SIZE * type);
	in_range_end = sizeof(Url_Storage_Herader_T) + ((uint32_t)URL_MAX_SIZE * (type + 1));
	len = strlen(str);
	crc_init(&crc, URL_CRC_POLY, 0);
	crc_calculate(&crc, counter);
	for (i = 8; i < SPI_FLASH_SEC_SIZE; i++) {
		if ((i >= in_range_start) && (i < in_range_end)) {
			offset = i - in_range_start;
			if (!(i % sizeof(value))) {
				memset(value, 0, sizeof(value));
			}
			if (len > offset) {
				value[i % sizeof(value)] = str[offset];
			}
			if ((i % sizeof(value)) == (sizeof(value) - 1)) {
				if (spi_flash_write(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE) + ((i / sizeof(value)) * sizeof(value)), (uint32_t*)value, sizeof(value)) != SPI_FLASH_RESULT_OK) {
					return 0;
				}
				memcpy(&value_32, value, sizeof(value_32));
				crc_calculate(&crc, value_32);
			}
		} else {
			if (!(i % sizeof(value))) {
				if (spi_flash_read(((url->base_sector + ((sector + 1) % 2)) * SPI_FLASH_SEC_SIZE) + i, (uint32_t*)value, sizeof(value)) != SPI_FLASH_RESULT_OK) {
					return 0;
				}
				if (spi_flash_write(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE) + i, (uint32_t*)value, sizeof(value)) != SPI_FLASH_RESULT_OK) {
					return 0;
				}
				memcpy(&value_32, value, sizeof(value_32));
				crc_calculate(&crc, value_32);
			}
		}
	}
	/*counter*/
	if (spi_flash_write(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE) + sizeof(counter), (uint32_t*)&counter, sizeof(counter)) != SPI_FLASH_RESULT_OK) {
		return 0;
	}
	/*crc*/
	value_32 = crc_last(&crc);
	if (spi_flash_write(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE), (uint32_t*)&value_32, sizeof(value_32)) != SPI_FLASH_RESULT_OK) {
		return 0;
	}
	return 1;
}

char* ICACHE_FLASH_ATTR url_storage_read(Url_Storage_T* url, Url_Storage_Type_T type) {
	uint32_t sector = 0;
	uint32_t in_range_start = 0;
	uint32_t in_range_end = 0;
	uint32_t len = 0;
	uint32_t i;
	uint8_t data[4];
	char* res = NULL;
	if (!url || (type >= __URL_TYPE_MAX)) {
		return NULL;
	}
	if (!url_storage_get_read_sector(url, &sector)) {
		return NULL;
	}
	in_range_start = sizeof(Url_Storage_Herader_T) + ((uint32_t)URL_MAX_SIZE * type);
	in_range_end = sizeof(Url_Storage_Herader_T) + ((uint32_t)URL_MAX_SIZE * (type + 1));
	for (i = in_range_start; i < in_range_end; i++) {
		if (!(i % sizeof(data))) {
			if (spi_flash_read(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE) + i, (uint32_t*)data, sizeof(data)) != SPI_FLASH_RESULT_OK) {
				return 0;
			}
		}
		if (((data[i % sizeof(data)]) != 0) &&
			((data[i % sizeof(data)]) != 0xFF)) {
			len++;
		} else {
			break;
		}
	}
	if (!len) {
		return NULL;
	}
	if (!(res = calloc(1, len + 5))) {
		return NULL;
	}
	if (spi_flash_read(((url->base_sector + sector) * SPI_FLASH_SEC_SIZE) + in_range_start, (uint32_t*)res, len) != SPI_FLASH_RESULT_OK) {
		free(res);
		return NULL;
	}
	res[len] = '\0';
	return res;
}

void url_storage_erase_all(Url_Storage_T* storage) {
	if (!storage) {
		return;
	}
	spi_flash_erase_sector(storage->base_sector + 0);
	spi_flash_erase_sector(storage->base_sector + 1);
}
