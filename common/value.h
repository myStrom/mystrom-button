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


#ifndef VALUE_H_INCLUDED
#define VALUE_H_INCLUDED 1

#include <inttypes.h>
#include <time.h>

typedef struct Value_T* Value_T;

enum Value_Type_T {
	VALUE_TYPE_UNDEFINE,
	VALUE_TYPE_STRING,
	VALUE_TYPE_INT,
	VALUE_TYPE_REAL,
	VALUE_TYPE_HEX,
	VALUE_TYPE_TIME,
	VALUE_TYPE_OBJ,
	VALUE_TYPE_CONST_STRING,
	VALUE_TYPE_BOOL,
	VALUE_TYPE_UINT,
	VALUE_TYPE_ARRAY,
	VALUE_TYPE_NULL,
};

struct Value_T {
	const char* name;
	enum Value_Type_T type;
	union {
		const char* const_string_value;
		char* string_value;
		int32_t int_value;
		uint32_t uint_value;
		void* void_data;
		float real_value;
		uint64_t u64_value;
		time_t time_value;
		uint8_t bool_value;
	};
	uint8_t present : 1;
};

uint8_t value_int_in(int32_t value, int32_t* values, uint8_t size);

#endif
