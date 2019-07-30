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
#include <stdio.h>
#include <stdlib.h>
#include <user_interface.h>
#include <osapi.h>
#include <ctype.h>
#include <time.h>

#include "rule.h"
#include "slash.h"
#include "debug.h"

extern char own_mac[13];

static const char* rule_mac_chars = "0123456789abcdefABCDEF";

static uint8_t ICACHE_FLASH_ATTR rule_month_length(uint16_t year, uint8_t month);

static uint8_t ICACHE_FLASH_ATTR rule_check_day(uint16_t year, uint8_t month, uint8_t day) {
	return ((day) && (day <= rule_month_length(year, month)));
}

static uint8_t ICACHE_FLASH_ATTR rule_check_month(uint8_t month) {
	return ((month) && (month < 13));
}

static uint8_t ICACHE_FLASH_ATTR rule_check_year(uint16_t year) {
	return ((year >= 1970) && (year <= 2034));
}

static uint8_t ICACHE_FLASH_ATTR rule_is_leap_year(uint16_t year) {
	return ((!(year % 4) && (year % 100)) || (!(year % 400)));
}

static uint16_t ICACHE_FLASH_ATTR rule_year_days(uint16_t year) {
	if (rule_is_leap_year(year)) {
		return 366;
	}
	return 365;
}

static uint8_t ICACHE_FLASH_ATTR rule_month_length(uint16_t year, uint8_t month) {
	if (!rule_check_month(month)) {
		return 0;
	}
	switch (month) {
		case 4:
		case 6:
		case 9:
		case 11: {
			return 30;
		}
		case 2: {
			if (rule_is_leap_year(year)) {
				return 29;
			}
			return 28;
		}
		default: {
			return 31;
		}
	}
}

static uint16_t ICACHE_FLASH_ATTR rule_leave_days_from_begin_year(uint16_t year, uint8_t month, uint8_t day) {
	uint16_t days_sum = 0;
	uint8_t i;
	for (i = 1; i < month; i++) {
		days_sum += rule_month_length(year, i);
	}
	days_sum += day;
	return days_sum;
}

static uint32_t ICACHE_FLASH_ATTR rule_days_from_1970(uint16_t year, uint8_t month, uint8_t day) {
	uint32_t days = 0;
	uint16_t y;
	for (y = 1970; y < year; y++) {
		days += rule_year_days(y);
	}
	days += rule_leave_days_from_begin_year(year, month, day);
	return days;
}

static uint8_t ICACHE_FLASH_ATTR rule_check_y_m_d(uint16_t year, uint8_t month, uint8_t day) {
	return (rule_check_day(year, month, day) && rule_check_month(month) && rule_check_year(year));
}

uint8_t ICACHE_FLASH_ATTR rule_char_in(char data, const char* set) {
	uint16_t i;
	if (!set || !strlen(set)) {
		return 0;
	}
	for (i = 0; i < strlen(set); i++) {
		if (data == set[i]) {
			return 1;
		}
	}
	return 0;
}

static uint8_t ICACHE_FLASH_ATTR rule_has_only_set(const char* value, const char* set) {
	uint16_t i;
	if (!value || !set) {
		return 0;
	}
	for (i = 0; i < strlen(value); i++) {
		if (!rule_char_in(value[i], set)) {
			return 0;
		}
	}
	return 1;
}

uint16_t ICACHE_FLASH_ATTR rule_count_char(const char* str, char value) {
	uint16_t i;
	uint16_t sum = 0;
	if (!str) {
		return 0;
	}
	for (i = 0; i < strlen(str); i++) {
		if (str[i] == value) {
			sum++;
		}
	}
	return sum;
}

uint8_t ICACHE_FLASH_ATTR rule_check_ip(const char* ip, uint32_t* ret) {
	Slash_T slash;
	char copy[16];
	char* part;
	if (!ip || !ret || (strlen(ip) > 15) ||
		(strlen(ip) < 7) || !rule_has_only_set(ip, "0123456789.") ||
		(rule_count_char(ip, '.') != 3)) {
		return 0;
	}
	*ret = 0;
	memset(copy, 0, sizeof(copy));
	strcpy(copy, ip);
	slash_init(&slash, copy, '.');
	if (!(part = slash_next(&slash))) {
		return 0;
	}
	*ret |= atoi(part) & 0xFF;
	if (!(part = slash_next(&slash))) {
		return 0;
	}
	*ret |= (atoi(part) & 0xFF) << 8;
	if (!(part = slash_next(&slash))) {
		return 0;
	}
	*ret |= (atoi(part) & 0xFF) << 16;
	if (!(part = slash_next(&slash))) {
		return 0;
	}
	*ret |= (atoi(part) & 0xFF) << 24;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_check_mac(const char* mac) {
	uint16_t i;
	if (!mac || (strlen(mac) != 12)) {
		return 0;
	}
	for (i = 0; i < strlen(mac); i++) {
		if (!rule_char_in(mac[i], rule_mac_chars)) {
			return 0;
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_hex_to_int(char value) {
	if ((value >= '0') && (value <= '9')) {
		return value - '0';
	}
	if ((value >= 'a') && (value <= 'f')) {
		return value - 'a' + 10;
	}
	return value - 'A' + 10;
}

uint8_t ICACHE_FLASH_ATTR rule_check_hex(const char* value, uint64_t* ret) {
	uint16_t i;
	if (!value || !strlen(value)) {
		return 0;
	}
	if (!rule_has_only_set(value, rule_mac_chars)) {
		return 0;
	}
	if (!ret) {
		return 1;
	}
	*ret = 0;
	for (i = 0; i < strlen(value); i++) {
		*ret <<= 4;
		*ret |= rule_hex_to_int(value[i]) & 0x0F;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_has_only_digit(const char* input_str) {
	if (!input_str || !(*input_str)) {
	}
	while (*input_str) {
		if (!isdigit((int)*input_str)) {
			return 0;
		}
		input_str++;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_check_digit(const char* str, int64_t min_val, int64_t max_val, int32_t* ret_val) {
	int64_t val;
	if (!str) {
		return 0;
	}
	if (!strlen(str)) {
		return 0;
	}
	if (str[0] == '-') {
		if (!rule_has_only_digit(str + 1)) {
			return 0;
		}
	} else {
		if (!rule_has_only_digit(str)) {
			return 0;
		}
	}
	val = atoi(str);
	if ((val >= min_val) && (val <= max_val)) {
		if (ret_val) {
			*ret_val = val;
		}
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR rule_check_utc_time(const char* value, uint64_t* ret_time) {
	char cpy[21];
	struct tm tm = {0};
	Slash_T tok;
	if (!value || (strlen(value)) != 20) {
		return 0;
	}
	memset(cpy, 0, sizeof(cpy));
	strncpy(cpy, value, sizeof(cpy) - 1);
	slash_init(&tok, cpy, '-');
	if (!rule_check_digit(slash_next(&tok), 1970, 2034, &tm.tm_year)) {
		goto error;
	}
	//tm.tm_year -= 1900;
	slash_set_delimeter(&tok, '-');
	if (!rule_check_digit(slash_next(&tok), 1, 12, &tm.tm_mon)) {
		goto error;
	}
	//tm.tm_mon--;
	slash_set_delimeter(&tok, 'T');
	if (!rule_check_digit(slash_next(&tok), 1, 31, &tm.tm_mday)) {
		goto error;
	}
	slash_set_delimeter(&tok, ':');
	if (!rule_check_digit(slash_next(&tok), 0, 59, &tm.tm_hour)) {
		goto error;
	}
	slash_set_delimeter(&tok, ':');
	if (!rule_check_digit(slash_next(&tok), 0, 59, &tm.tm_min)) {
		goto error;
	}
	slash_set_delimeter(&tok, 'Z');
	if (!rule_check_digit(slash_next(&tok), 0, 59, &tm.tm_sec)) {
		goto error;
	}
	if (!rule_check_y_m_d(tm.tm_year, tm.tm_mon, tm.tm_mday)) {
		goto error;
	}
	if (ret_time) {
		*ret_time = (rule_days_from_1970(tm.tm_year, tm.tm_mon, tm.tm_mday) - 1) * 86400;
		*ret_time += (uint32_t)tm.tm_hour * 3600;
		*ret_time += (uint32_t)tm.tm_min * 60;
		*ret_time += tm.tm_sec;
	}
	return 1;

error:
	return 0;
}

uint8_t ICACHE_FLASH_ATTR rule_check_bool(const char* value, uint8_t* ret) {
	if (!value || !strlen(value)) {
		return 0;
	}
	if (strcmp(value, "true") && strcmp(value, "false")) {
		return 0;
	}
	if (ret) {
		if (!strcmp(value, "true")) {
			*ret = 1;
		} else {
			*ret = 0;
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_check_allowed_cheracters(const char* data, const char* characters) {
	uint16_t i;
	if (!data || !characters) {
		return 0;
	}
	for (i = 0; i < strlen(data); i++) {
		if (!strchr(characters, data[i])) {
			return 0;
		}
	}
	return 1;
}
uint16_t ICACHE_FLASH_ATTR rule_count_chars(const char* toCheck, char specyfied) {
	uint16_t sum = 0;
	uint16_t i;
	if (!toCheck || !specyfied || !strlen(toCheck)) {
		return 0;
	}
	for (i = 0; i < strlen(toCheck); i++) {
		if (toCheck[i] == specyfied) {
			sum++;
		}
	}
	return sum;
}

uint8_t ICACHE_FLASH_ATTR rule_check_float(const char* str, float* retVal) {
	uint16_t dots;
	uint16_t minus;
	if (!str || !strlen(str)) {
		return 0;
	}
	if (!rule_check_allowed_cheracters(str, "-.0123456789")) {
		return 0;
	}
	dots = rule_count_chars(str, '.');
	minus = rule_count_chars(str, '-');
	if (minus > 0) {
		if (strlen(str) == 1) {
			return 0;
		}
		if (str[0] != '-') {
			return 0;
		}
		if (minus > 1) {
			return 0;
		}
	}
	if (dots > 0) {
		if (strlen(str) < 3) {
			return 0;
		}
		if (str[0] == '.') {
			return 0;
		}
		if (dots > 1) {
			return 0;
		}
	}
	if (dots && minus) {
		if (strlen(str) < 4) {
			return 0;
		}
		if (str[1] == '.') {
			return 0;
		}
	}
	if (dots) {
		if (str[strlen(str) - 1] == '.') {
			return 0;
		}
	}
	if (retVal) {
		//*retVal = atof(str);
		//no space in iram
		*retVal = 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_check_to_value(const Rule_T* rule, char* data, Value_T value) {
	Item_T item;
	if (!rule || !data || !value) {
		return 0;
	}
	if (!rule_check(rule, data, &item)) {
		return 0;
	}
	memset(value, 0, sizeof(struct Value_T));
	value->name = rule->name;
	value->present = 1;
	switch (rule->type) {
		case RULE_REAL:
			value->type = VALUE_TYPE_REAL;
			value->real_value = item.data_float;
			break;
		case RULE_BOOLEAN:
			value->type = VALUE_TYPE_BOOL;
			value->bool_value = item.data_uint8;
			break;
		case RULE_UNSIGNED_INT:
			value->type = VALUE_TYPE_UINT;
			value->uint_value = item.data_uint32;
			break;
		case RULE_SIGNED_INT:
			value->type = VALUE_TYPE_INT;
			value->int_value = item.data_int32;
			break;
		case RULE_IP:
			value->type = VALUE_TYPE_UINT;
			value->uint_value = item.data_uint32;
			break;
		case RULE_HEX:
			value->type = VALUE_TYPE_UINT;
			value->uint_value = item.data_uint32;
			break;
		case RULE_LONG_HEX:
			value->type = VALUE_TYPE_STRING;
			value->string_value = item.data_char;
			break;
		case RULE_ENUM:
			value->type = VALUE_TYPE_UINT;
			value->uint_value = item.data_uint32;
			break;
		case RULE_MAC:
			value->type = VALUE_TYPE_STRING;
			value->string_value = item.data_char;
			break;
		case RULE_MAC_OR_SELF:
			value->type = VALUE_TYPE_CONST_STRING;
			value->const_string_value = item.data_const_char;
			break;
		case RULE_CHAR:
		case RULE_ALNUM:
		case RULE_ALPHA:
		case RULE_EQUAL:
			value->type = VALUE_TYPE_STRING;
			value->string_value = item.data_char;
			break;
		case RULE_DIGITS:
			value->type = VALUE_TYPE_UINT;
			value->uint_value = item.data_uint32;
			break;
		default:
			value->type = VALUE_TYPE_UNDEFINE;
			value->u64_value = 0;
			break;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR rule_check(const Rule_T* rule, char* data, Item_T* valid_data) {
	uint32_t data_len;
	if (!rule || !data) {
		return 0;
	}
	data_len = strlen(data);
	if (rule->type != RULE_SINGLE_CHAR) {
		switch (rule->type) {
			case RULE_ALNUM:
			case RULE_ALPHA:
			case RULE_DIGITS:
			case RULE_CHAR:
			case RULE_HEX:
			case RULE_LONG_HEX:
			case RULE_SET:
			if ((data_len < rule->min_len) || (data_len > rule->max_len)) {
				return 0;
			}
		}
		switch (rule->type) {
			case RULE_EQUAL: {
				if (!rule->detail.equal) {
					return 0;
				}
				if (strcmp(rule->detail.equal, data)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_REAL: {
				float ret_val = 0;
				if (!rule_check_float(data, &ret_val)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_float = ret_val;
				}
				return 1;
			}
			break;
			case RULE_BOOLEAN: {
				uint8_t ret_val;
				if (!rule_check_bool(data, &ret_val)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_uint8 = ret_val;
				}
				return 1;
			}
			break;
			case RULE_UNSIGNED_INT: {
				int32_t ret_val = 0;
				if (rule_check_digit(data, rule->detail.unsigned_int.min_val, rule->detail.unsigned_int.max_val, &ret_val)) {
					if (valid_data) {
						valid_data->data_uint32 = ret_val;
					}
					return 1;
				}
			}
			break;
			case RULE_SIGNED_INT: {
				int32_t ret_val = 0;
				if (!rule_check_digit(data, rule->detail.signed_int.min_val, rule->detail.signed_int.max_val, &ret_val)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_int32 = ret_val;
				}
				return 1;
			}
			break;
			case RULE_IP: {
				uint32_t ret_val;
				if (!rule_check_ip(data, &ret_val)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_uint32 = ret_val;
				}
				return 1;
			}
			break;
			case RULE_HEX: {
				uint64_t ret;
				if (!rule_check_hex(data, &ret)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_uint32 = ret;
				}
				return 1;
			}
			break;
			case RULE_LONG_HEX: {
				if (!rule_has_only_set(data, "0123456789abcdefABCDEF")) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_SET: {
				if (!rule->detail.set || !strlen(rule->detail.set)) {
					return 0;
				}
				if (!rule_has_only_set(data, rule->detail.set)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_char = data;
				}
			}
			break;
			case RULE_MAC: {
				if (!rule_check_mac(data)) {
					return 0;
				}
				if (valid_data)
				{
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_MAC_OR_SELF: {
				if (!strcmp(data, "self")) {
					if (valid_data) {
						valid_data->data_const_char = own_mac;
					}
					return 1;
				}
				if (!rule_check_mac(data)) {
					return 0;
				}
				if (valid_data) {
					valid_data->data_const_char = data;
				}
				return 1;
			}
			break;
			case RULE_ENUM: {
				uint16_t i;
				for (i = 0; i < rule->detail.enumerate.amount; i++) {
					if (!strcmp(rule->detail.enumerate.values[i], data)) {
						if (valid_data) {
							valid_data->data_uint32 = i;
						}
						return 1;
					}
				}
			}
			break;
			case RULE_ALNUM: {
				uint16_t i;
				for (i = 0; i < data_len; i++) {
					if (!(isalnum((int)*(data + i)) || isspace((int)*(data + i)))) {
						return 0;
					}
				}
				if (valid_data) {
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_ALPHA: {
				uint16_t i;
				for (i = 0; i < data_len; i++) {
					if (!isalpha((int)*(data + i))) {
						return 0;
					}
				}
				if (valid_data) {
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_DIGITS: {
				uint16_t i;
				for (i = 0; i < data_len; i++) {
					if (!isdigit((int)*(data + i))) {
						return 0;
					}
				}
				if (valid_data) {
					if (*data) {
						valid_data->data_uint32 = atoi(data);
					} else {
						valid_data->data_uint32 = 0;
					}
				}
				return 1;
			}
			break;
			case RULE_CHAR: {
				if (valid_data) {
					valid_data->data_char = data;
				}
				return 1;
			}
			break;
			case RULE_NONE: {
				return 1;
			}
			break;
			default: {

			}
			break;
		}
	} else {
		if (data_len == 1) {
			if (valid_data) {
				valid_data->data_single_char = *data;
			}
			return 1;
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR rule_check_path(char* str, const Rule_T* params, Item_T* values, uint8_t count, char cut_char) {
	Slash_T slash;
	char* value;
	uint16_t i = 0;
	if (!str || !strlen(str) || !params || !values || !count || !cut_char) {
		return 0;
	}
	slash_init(&slash, str, cut_char);
	for (i = 0; i < count; i++) {
		if (!(value = slash_next(&slash))) {
			return 0;
		}
		if (!rule_check(&params[i], value, &values[i])) {
			return 0;
		}
	}
	if (slash_have_next(&slash)) {
		return 0;
	}
	return 1;
}

const char* ICACHE_FLASH_ATTR rule_name(const Rule_T* rule) {
	if (!rule) {
		return NULL;
	}
	return rule->name;
}

Rule_Type_T ICACHE_FLASH_ATTR rule_get_type(const Rule_T* rule) {
	if (!rule) {
		return 0;
	}
	return rule->type;
}

uint8_t ICACHE_FLASH_ATTR rule_required(const Rule_T* rule) {
	if (!rule) {
		return 0;
	}
	return rule->required;
}

uint8_t ICACHE_FLASH_ATTR rule_describe_type(const Rule_T* rule, Buffer_T buffer) {
	static const char* types[__RULE_MAX] = (const char*[]){
		"UINT",  "SINT",  "BOOL", "ENUM",
		"ALNUM", "ALPHA", "CHRS", "DIGS",
		"NONE",  "SCHAR", "MAC",  "HEX",
		"IP",    "LHEX",  "REAL", "EQUAL",
		"SET",  "SELF"};
	if (!rule || !buffer) {
		return 0;
	}
	if (rule->type >= __RULE_MAX) {
		return 0;
	}
	return buffer_puts(buffer, types[rule->type]);
}

uint8_t ICACHE_FLASH_ATTR rule_describe_range(const Rule_T* rule, Buffer_T buffer) {
	char storage[64];
	if (!rule || !buffer) {
		return 0;
	}
	if (rule->type == RULE_ENUM) {
		uint16_t i;
		for (i = 0; i < rule->detail.enumerate.amount; i++) {
			if (i) {
				buffer_write(buffer, '|');
			}
			if (!buffer_puts(buffer, rule->detail.enumerate.values[i])) {
				return 0;
			}
		}
		return 1;
	}
	if (rule->type == RULE_BOOLEAN) {
		return buffer_puts(buffer, "false|true");
	}
	if (rule->type == RULE_EQUAL) {
		return buffer_puts(buffer, rule->detail.equal);
	}
	storage[0] = '\0';
	switch (rule->type) {
		case RULE_UNSIGNED_INT:
			snprintf(storage, sizeof(storage), "%u..%u", rule->detail.unsigned_int.min_val, rule->detail.unsigned_int.max_val);
			break;
		case RULE_SIGNED_INT:
			snprintf(storage, sizeof(storage), "%d..%d", rule->detail.signed_int.min_val, rule->detail.signed_int.max_val);
			break;
			case RULE_ALNUM:
			case RULE_ALPHA:
			case RULE_DIGITS:
			case RULE_CHAR:
			case RULE_HEX:
			case RULE_LONG_HEX:
			case RULE_SET:
			snprintf(storage, sizeof(storage), "%u..%u", rule->min_len, rule->max_len);
			break;
		default:
			break;
	}
	return buffer_puts(buffer, storage);
}
