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


#ifndef ITEM_H_INCLUDED
#define ITEM_H_INCLUDED 1

#include <inttypes.h>

typedef union {
	void* data_void;
	const void* data_const_void;
	uint32_t data_uint32;
	int32_t data_int32;
	uint16_t data_uint16;
	char* data_char;
	const char* data_const_char;
	uint8_t data_uint8;
	uint8_t* data_uint8_ptr;
	const char* const* data_const_char_list;
	char data_single_char;
	float data_float;
} Item_T;

#endif
