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


#ifndef CRC_H_INCLUDED
#define CRC_H_INCLUDED 1

#include <inttypes.h>

typedef struct {
	uint64_t poly;
	uint64_t last_value;
	uint64_t init_value;
	uint8_t power;
} Crc_T;

Crc_T* crc_init(Crc_T* crc, uint64_t poly, uint64_t init_value);
uint64_t crc_calculate(Crc_T* crc, uint64_t value);
void crc_reset(Crc_T* crc);
uint64_t crc_last(Crc_T* crc);
uint64_t crc_check(Crc_T* crc, const uint8_t* buffer, uint16_t length);

#endif
