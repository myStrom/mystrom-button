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


#ifndef RULE_H_INCLUDED
#define RULE_H_INCLUDED 1

#include <inttypes.h>
#include <time.h>
#include <c_types.h>
#include "item.h"
#include "buffer.h"
#include "value.h"

typedef enum {
	RULE_UNSIGNED_INT,
	RULE_SIGNED_INT,
	RULE_BOOLEAN,
	RULE_ENUM,
	RULE_ALNUM,
	RULE_ALPHA,
	RULE_CHAR,
	RULE_DIGITS,
	RULE_NONE,
	RULE_SINGLE_CHAR,
	RULE_MAC,
	RULE_HEX,
	RULE_IP,
	RULE_LONG_HEX,
	RULE_REAL,
	RULE_EQUAL,
	RULE_SET,
	RULE_MAC_OR_SELF,
	__RULE_MAX,
} __attribute__((aligned(4))) Rule_Type_T;

typedef enum {
	RULE_DOMAIN_NONE,
	RULE_DOMAIN_UINT,
	RULE_DOMAIN_INT,
	RULE_DOMAIN_STRING,
	RULE_DOMAIN_CHAR,
	RULE_DOMAIN_REAL
} Rule_Domain_T;

typedef struct {
	Rule_Type_T type __attribute__((aligned(4)));
	const char* name;
	uint32_t min_len;
	uint32_t max_len;
	uint32_t required : 1;
	union {
		void* dummy;
		struct {
			uint32_t min_val;
			uint32_t max_val;
		} unsigned_int __attribute__((aligned(4)));

		struct {
			int32_t min_val;
			int32_t max_val;
		} signed_int __attribute__((aligned(4)));

		struct {
			uint32_t amount : 16;
			const char** values;
		} enumerate __attribute__((aligned(4)));

		const char* equal;
		const char* set;
	} detail __attribute__((aligned(4)));
} __attribute__((aligned(4))) Rule_T;

#define rule_set_with_cut(min_val, max_val, value) (((value) < (min_val))?(min_val):(((value) > max_val)?(max_val):(value)))

#define RULE_DEF_BOOLEAN {\
	.type = RULE_UNSIGNED_INT,\
	.to_lower = 0,\
	.min_len = 1,\
	.max_len = 1,\
	{\
		.unsigned_int = {\
			.min_val = 0,\
			.max_val = 1\
		}\
	}\
}

uint8_t rule_check(const Rule_T* rule, char* data, Item_T* valid_data);
uint8_t rule_check_to_value(const Rule_T* rule, char* data, Value_T value);
uint8_t rule_check_mac(const char* mac);
uint8_t rule_check_hex(const char* value, uint64_t* ret);
uint8_t rule_hex_to_int(char value);
uint8_t rule_has_only_digit(const char* input_str);
uint8_t rule_check_utc_time(const char* value, uint64_t* ret_time);
uint8_t rule_check_bool(const char* value, uint8_t* ret);
uint8_t rule_check_path(char* str, const Rule_T* params, Item_T* values, uint8_t count, char cut_char);
uint8_t rule_check_digit(const char* str, int64_t min_val, int64_t max_val, int32_t* ret_val);
uint8_t rule_check_ip(const char* ip, uint32_t* ret);
uint16_t rule_count_char(const char* str, char value);
uint8_t rule_check_allowed_cheracters(const char* data, const char* characters);
uint16_t rule_count_chars(const char* toCheck, char specyfied);
const char* rule_name(const Rule_T* rule);
Rule_Type_T rule_get_type(const Rule_T* rule) ;
uint8_t rule_required(const Rule_T* rule);
uint8_t rule_describe_type(const Rule_T* rule, Buffer_T buffer);
uint8_t rule_describe_range(const Rule_T* rule, Buffer_T buffer);
uint8_t rule_char_in(char data, const char* set);

#endif
