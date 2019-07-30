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


#ifndef URL_H_INCLUDED
#define URL_H_INCLUDED 1

#include <inttypes.h>

#define URL_MAX_SIZE 816

typedef struct {
	uint32_t crc;
	uint32_t counter;
} Url_Storage_Herader_T;

typedef enum {
	URL_TYPE_SINGLE,
	URL_TYPE_DOUBLE,
	URL_TYPE_LONG,
	URL_TYPE_TOUCH,
	URL_TYPE_GENERIC,
	__URL_TYPE_MAX
} Url_Storage_Type_T;

typedef struct {
	uint32_t base_sector;
} Url_Storage_T;

Url_Storage_T* url_storage_init(Url_Storage_T* url, uint32_t base_sector);
uint8_t url_storage_write(Url_Storage_T* url, Url_Storage_Type_T type, const char* str);
char* url_storage_read(Url_Storage_T* url, Url_Storage_Type_T type);
void url_storage_erase_all(Url_Storage_T* storage);

#endif
