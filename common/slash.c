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
#include <osapi.h>
#include <c_types.h>
#include "slash.h"
#include "buffer.h"

Slash_T* ICACHE_FLASH_ATTR slash_init(Slash_T* slash, char* buffer, uint8_t cut_char) {
	if (!slash || !buffer || !cut_char) {
		return NULL;
	}
	memset(slash, 0, sizeof(Slash_T));
	slash->current = buffer;
	slash->cut_char = cut_char;
	slash->have_cut_char = 1;
	slash->end_find = 0;
	slash->delimeters = 0;
	return slash;
}

Slash_T* ICACHE_FLASH_ATTR slash_init_delimeters(Slash_T* slash, char* buffer, const char* delimeters) {
	if (!slash || !buffer || !delimeters) {
		return NULL;
	}
	memset(slash, 0, sizeof(Slash_T));
	slash->current = buffer;
	slash->cut_char = 0;
	slash->have_cut_char = 0;
	slash->end_find = 0;
	slash->delimeters = delimeters;
	return slash;
}

void ICACHE_FLASH_ATTR slash_set_exclude(Slash_T* slash, char begin, char end) {
	if (!slash || !begin || !end) {
		return;
	}
	slash->begin = begin;
	slash->end = end;
}

void ICACHE_FLASH_ATTR slash_set_str(Slash_T* slash, char delim, char esc) {
	if (!slash || !delim || !esc) {
		return;
	}
	slash->str_delim = delim;
	slash->esc_seq = esc;
}

void ICACHE_FLASH_ATTR slash_restore(Slash_T* slash, char* buffer) {
	if (!slash || !buffer) {
		return;
	}
	slash->current = buffer;
	slash->end_find = 0;
}

static uint8_t ICACHE_FLASH_ATTR slash_is_delimeter(Slash_T* slash, uint8_t check_char, uint8_t found) {
	if (!slash) {
		return 0;
	}
	if (found) {
		goto check_delim;
	}
	if (slash->str_delim) {
		if (slash->in_str) {
			if ((check_char == slash->str_delim) &&
				(slash->prev != slash->esc_seq)) {
					slash->in_str = 0;
			} else {
				slash->prev = check_char;
				return 0;
			}
		} else {
			if (check_char == slash->str_delim) {
				slash->in_str = 1;
				slash->prev = check_char;
			}
		}
	}
	if (check_char) {
		if (slash->begin == check_char) {
			slash->level++;
		}
		if (slash->end == check_char) {
			if (slash->level) {
				slash->level--;
			}
		}
	}
	if (slash->level) {
		return 0;
	}
check_delim:
	if (slash->have_cut_char) {
		if (check_char == slash->cut_char) {
			return 1;
		}
	} else {
		if (strchr(slash->delimeters, check_char)) {
			return 1;
		}
	}
	return 0;
}

char* ICACHE_FLASH_ATTR slash_next(Slash_T* slash) {
	char* prev;
	uint8_t delim = 0;
	if (!slash) {
		return NULL;
	}
	if (!slash_have_next(slash)) {
		return NULL;
	}
	prev = slash->current;
	while (*slash->current) {
		if (slash_is_delimeter(slash, *slash->current, delim)) {
			delim = 1;
			*slash->current = 0;
			slash->current++;
			if (!(*slash->current)) {
				slash->end_find = 1;
			}
			continue;
		}
		if (delim) {
			break;
		}
		slash->current++;
	}

	if (!(*prev)) {
		if (slash->end_find) {
			slash->end_find = 0;
			return prev;
		}
		return NULL;
	}
	return prev;
}

char* ICACHE_FLASH_ATTR slash_next_trim(Slash_T* slash) {
	char* part;
	if (!(part = slash_next(slash))) {
		return NULL;
	}
	return slash_trim_unprint(part);
}

uint8_t ICACHE_FLASH_ATTR slash_have_next(Slash_T* slash) {
	if (!slash || !slash->current ||
		!(*slash->current) || slash->end_find) {
		return 0;
	}
	return 1;
}

char* ICACHE_FLASH_ATTR slash_current(Slash_T* slash) {
	if (!slash) {
		return NULL;
	}
	return slash->current;
}

uint16_t ICACHE_FLASH_ATTR slash_escape(char** in, const char* escape_str) {
	uint16_t counter = 0;
	if (!in || !(*in) || !escape_str) {
		return 0;
	}
	while (**in && strchr(escape_str, **in)) {
		counter++;
		(*in)++;
	}
	return counter;
}

char* ICACHE_FLASH_ATTR slash_escape_unprint(char* in) {
	if (!in) {
		return NULL;
	}
	while (*in && ((*in) <= 32)) {
		in++;
	}
	return in;
}

uint8_t ICACHE_FLASH_ATTR slash_strchr_escaped(const char* in, char c, uint16_t* index) {
	uint32_t i;
	if (!in) {
		return 0;
	}
	for (i = 0; i < strlen(in); i++) {
		if (in[i] == '\\') {
			i++;
			if (!in[i]) {
				return 0;
			}
			continue;
		}
		if (in[i] == c) {
			if (index) {
				*index = i;
			}
			return 1;
		}
	}
	return 0;
}

char* ICACHE_FLASH_ATTR slash_value(char* part, const char* name, const char* separators) {
	Slash_T slash;
	char* sub_part;
	if (!part || !name || !separators) {
		return NULL;
	}
	if (!slash_init_delimeters(&slash, part, separators)) {
		return NULL;
	}
	if (!slash_have_next(&slash) || !(sub_part = slash_next(&slash))) {
		return NULL;
	}
	if (strcmp(sub_part, name)) {
		return NULL;
	}
	if (!slash_have_next(&slash) || !(sub_part = slash_next(&slash))) {
		return NULL;
	}
	return sub_part;
}

char* ICACHE_FLASH_ATTR slash_next_name_and_value(Slash_T* slash, char cut_char, char** name) {
	Slash_T part;
	char* split;
	char* value;
	if (!slash || !name || !cut_char) {
		return NULL;
	}
	if (!slash_have_next(slash)) {
		return NULL;
	}
	if (!(split = slash_next(slash))) {
		return NULL;
	}
	slash_init(&part, split, cut_char);
	if (!slash_have_next(&part)) {
		return NULL;
	}
	*name = slash_next(&part);
	if (!(value = slash_next(&part))) {
		value = (*name) + strlen(*name);
	}
	return value;
}

uint8_t ICACHE_FLASH_ATTR slash_parse(Slash_T* slash, const Rule_T* params, Value_T values, uint8_t count, char cut_char) {
	char* value;
	char* name;
	uint16_t i;
	uint16_t valid_params = 0;
	uint16_t req_params = 0;
	if (!slash || !params || !count || !cut_char || !values) {
		return 0;
	}
	for (i = 0; i < count; i++) {
		memset(&values[i], 0, sizeof(struct Value_T));
		values[i].name = params[i].name;
		if (!params[i].required) {
			continue;
		}
		req_params++;
	}
	while ((value = slash_next_name_and_value(slash, cut_char, &name))) {
		for (i = 0; i < count; i++) {
			if (!params[i].name) {
				continue;
			}
			if (strcmp(name, params[i].name)) {
				continue;
			}
			if (!rule_check_to_value(&params[i], value, &values[i])) {
				return 0;
			}
			if (params[i].required) {
				valid_params++;
			}
		}
	}
	if (valid_params < req_params) {
		return 0;
	}
	return 1;
}

char* ICACHE_FLASH_ATTR slash_trim(char* part, const char* space_char) {
	char* begin;
	uint32_t len;
	if (!part || !space_char || !strlen(space_char)) {
		return NULL;
	}
	begin = part;
	while (strchr(space_char, *begin)) {
		begin++;
	}
	while ((len = strlen(begin)) && strrchr(space_char, begin[len - 1])) {
		begin[len - 1] = 0;
	}
	return begin;
}

static uint8_t ICACHE_FLASH_ATTR slash_hex_to_bin(uint8_t hex, uint8_t* value) {
	if ((hex >= '0') && (hex <= '9')) {
		if (value) {
			*value = hex - '0';
		}
		return 1;
	}
	if ((hex >= 'a') && (hex <= 'f')) {
		if (value) {
			*value = hex - 'a' + 10;
		}
		return 1;
	}
	if ((hex >= 'A') && (hex <= 'F')) {
		if (value) {
			*value = hex - 'A' + 10;
		}
		return 1;
	}
	return 0;
}

char* ICACHE_FLASH_ATTR slash_replace(char* part) {
	uint32_t it = 0;
	uint32_t len;
	uint32_t i;
	uint8_t j;
	uint8_t digit;
	uint16_t value;
	if (!part) {
		return NULL;
	}
	len = strlen(part);
	for (i = 0; i < len; i++) {
		if (part[i] == '\\') {
			i++;
			switch (part[i]) {
				case 0:
					return NULL;
				case 'b':
					part[it] = '\b';
					break;
				case 'f':
					part[it] = '\f';
					break;
				case 'n':
					part[it] = '\n';
					break;
				case 't':
					part[it] = '\t';
					break;
				case 'r':
					part[it] = '\r';
					break;
				case 'u':
					i++;
					if (!part[i]) {
						return NULL;
					}
					value = 0;
					for (j = 0; j < 4; j++, i++) {
						value <<= 4;
						if (!slash_hex_to_bin(part[i], &digit)) {
							return NULL;
						}
						value |= digit & 0x0F;
					}
					part[it] = value;
					break;
				default:
					part[it] = part[i];
					break;
			}
		} else {
			part[it] = part[i];
		}
		it++;
	}
	part[it] = 0;
	return part;
}

char* ICACHE_FLASH_ATTR slash_trim_unprint(char* part) {
	char* begin;
	uint32_t len;
	if (!part) {
		return NULL;
	}
	begin = part;
	while ((*begin) <= 32) {
		begin++;
	}
	while ((len = strlen(begin)) && (begin[len - 1] <= 32)) {
		begin[len - 1] = 0;
	}
	return begin;
}

uint8_t ICACHE_FLASH_ATTR slash_next_equal(Slash_T* slash, const char* data) {
	char* part;
	if (!slash || !data) {
		return 0;
	}
	if (!slash_have_next(slash) || !(part = slash_next(slash))) {
		return 0;
	}
	if (strcmp(part, data)) {
		return 0;
	}
	return 1;
}

uint16_t ICACHE_FLASH_ATTR slash_to_array(Slash_T* slash, char** array, uint16_t size) {
	uint16_t items = 0;
	if (!slash || !array || !size) {
		return 0;
	}
	while (size && slash_have_next(slash)) {
		array[items] = slash_next(slash);
		items++;
		size--;
	}
	return items;
}

void ICACHE_FLASH_ATTR slash_set_delimeter(Slash_T* slash, uint8_t cut_char) {
	if (!slash) {
		return;
	}
	slash->cut_char = cut_char;
	slash->have_cut_char = 1;
}

uint8_t ICACHE_FLASH_ATTR slash_start_with(const char* str, const char* begin) {
	if (!str || !begin) {
		return 0;
	}
	if (strncmp(str, begin, strlen(begin))  == 0) {
		return 1;
	}
	return 0;
}
