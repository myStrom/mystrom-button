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
#include <c_types.h>
#include <osapi.h>
#include "json.h"
#include "buffer.h"
#include "slash.h"
#include "rule.h"
#include "debug.h"

struct Json_T {
	List_T list;
	const Rule_T* rule;
	char* callback;
	uint16_t it;
	uint8_t rule_amount;
	uint8_t array : 1;
	uint8_t unformatted : 1;
	uint8_t rewind : 1;
};

static void ICACHE_FLASH_ATTR json_free_value(Value_T value);
static void ICACHE_FLASH_ATTR json_print_value(Value_T value, Buffer_T buffer, uint8_t level);

static char* ICACHE_FLASH_ATTR json_string_escape(const char* value) {
	struct Buffer_T buffer;
	uint8_t* escaped;
	if (!value) {
		return NULL;
	}
	if (!(escaped = malloc((strlen(value) * 6) + 1))) {
		return NULL;
	}
	memset(escaped, 0, (strlen(value) * 6) + 1);
	buffer_init(&buffer, strlen(value) * 6, escaped);
	buffer_puts_escape(&buffer, value);
	return (char*)escaped;
}

Json_T ICACHE_FLASH_ATTR json_new(void) {
	Json_T json;
	if (!(json = (Json_T)malloc(sizeof(struct Json_T)))) {
		return NULL;
	}
	memset(json, 0, sizeof(struct Json_T));
	if (!(json->list = list_new(sizeof(struct Value_T)))) {
		goto error0;
	}
	return json;
error0:
	free(json);
	return NULL;
}

void ICACHE_FLASH_ATTR json_set_format(Json_T json, uint8_t array, uint8_t unformatted) {
	if (!json) {
		return;
	}
	json->array = array ? 1 : 0;
	json->unformatted = unformatted ? 1 : 0;
}

uint8_t ICACHE_FLASH_ATTR json_set_callback(Json_T json, const char* name) {
	if (!json) {
		return 0;
	}
	if (json->callback) {
		free(json->callback);
		json->callback = NULL;
	}
	if (!name || !strlen(name)) {
		return 1;
	}
	if (!(json->callback = strdup(name))) {
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR json_extract_value(char* data, const char* name, Json_T json) {
	char* end;
	int32_t ret_val;
	uint16_t i;
	uint8_t found = 0;
	struct Value_T value;
	if (!data || !strlen(data) || !name || !strlen(name) || !json) {
		return 0;
	}
	if (!strlen(data)) {
		return 0;
	}
	if (json->rule) {
		for (i = 0; i < json->rule_amount; i++) {
			if (!strcmp(json->rule[i].name, name)) {
				found = 1;
				break;
			}
		}
	}
	if (data[0] == '"') {
		/*string*/
		if (!(end = strrchr(data + 1, '"'))) {
			return 0;
		}
		*end = 0;
		if (!found) {
			return json_add_string(json, name, slash_replace(data + 1));
		}
		if (rule_check_to_value(&json->rule[i], slash_replace(data + 1), &value)) {
			return json_add(json, &value);
		}
		return 0;
	}
	if (data[0] == '{') {
		Json_T obj;
		if (!(obj = json_parse(data, json->rule, json->rule_amount))) {
			return 0;
		}
		/*object*/
		return json_add_obj(json, name, obj);
	}
	if (strchr("tn", data[0])) {
		if (!found) {
			if (!strcmp(data, "true")) {
				return json_add_bool(json, name, 1);
			} else {
				if (!strcmp(data, "false")) {
					return json_add_bool(json, name, 0);
				}
				return 0;
			}
		}
		if (rule_check_to_value(&json->rule[i], data, &value)) {
			return json_add(json, &value);
		}
		return 0;
	}
	if (!found) {
		if (rule_check_digit(data, -2147483647, 2147483647, &ret_val)) {
			return json_add_int(json, name, ret_val);
		}
		return 0;
	}
	if (rule_check_to_value(&json->rule[i], data, &value)) {
		return json_add(json, &value);
	}
	return 0;
}

static uint8_t ICACHE_FLASH_ATTR json_parse_item(char* data, Json_T json) {
	Slash_T slash;
	char* name;
	char* value;
	if (!data || !strlen(data) || !json) {
		debug_enter_here_P();
		return 0;
	}
	slash_init(&slash, data, ':');
	slash_set_exclude(&slash, '{', '}');
	slash_set_str(&slash, '"', '\\');
	name = slash_replace(slash_trim(slash_next_trim(&slash), "\""));
	if (!name) {
		debug_enter_here_P();
		return 0;
	}
	value = slash_next_trim(&slash);
	if (!value) {
		debug_enter_here_P();
		return 0;
	}
	if (!json_extract_value(value, name, json)) {
		debug_enter_here_P();
		return 0;
	}
	return 1;
}

Json_T ICACHE_FLASH_ATTR json_parse(char* string, const Rule_T* rule, uint8_t size) {
	Slash_T slash;
	char* item;
	char* content;
	Json_T json;
	if (!string || !strlen(string)) {
		return NULL;
	}
	if (size && !rule) {
		return NULL;
	}
	content = slash_trim_unprint(string);
	if (!content || (strlen(content) < 2)) {
		debug_enter_here_P();
		return NULL;
	}
	if (content[0] != '{') {
		debug_enter_here_P();
		return NULL;
	}
	if (content[strlen(content) - 1] != '}') {
		debug_enter_here_P();
		return NULL;
	}
	content[strlen(content) - 1] = 0;
	if (!strlen(content + 1)) {
		debug_enter_here_P();
		return NULL;
	}
	if (!(json = json_new())) {
		return NULL;
	}
	json->rule = rule;
	json->rule_amount = size;
	slash_init(&slash, content + 1, ',');
	slash_set_exclude(&slash, '{', '}');
	slash_set_str(&slash, '"', '\\');
	while ((item = slash_next(&slash))) {
		if (!json_parse_item(item, json)) {
			debug_describe_P("Not valid JSON");
			json_delete(json);
			return NULL;
		}
	}
	return json;
}

uint8_t ICACHE_FLASH_ATTR json_add_bool(Json_T json, const char* name, uint8_t value) {
	if (!json) {
		return 0;
	}
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_BOOL, {.bool_value = value ? 1 : 0}});
}

uint8_t ICACHE_FLASH_ATTR json_add_obj(Json_T json, const char* name, Json_T obj) {
	if (!json || !obj) {
		return 0;
	}
	if (json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_OBJ, {.void_data = obj}})) {
		obj->array = 0;
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR json_add_array(Json_T json, const char* name, Json_T obj) {
	if (!json || !obj) {
		return 0;
	}
	if (json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_ARRAY, {.void_data = obj}})) {
		obj->array = 1;
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR json_add_string(Json_T json, const char* name, const char* value) {
	if (!json || !value) {
		return 0;
	}
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_STRING, {.string_value = (char*)value}});
}

uint8_t ICACHE_FLASH_ATTR json_add_ip(Json_T json, const char* name, uint32_t addr) {
	struct Buffer_T buffer;
	uint8_t storage[16];
	if (!json) {
		return 0;
	}
	buffer_init(&buffer, sizeof(storage), storage);
	buffer_puts_ip(&buffer, addr);
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_STRING, {.string_value = buffer_string(&buffer)}});
}

uint8_t ICACHE_FLASH_ATTR json_add_int(Json_T json, const char* name, int32_t value) {
	if (!json) {
		return 0;
	}
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_INT, {.int_value = value}});
}

uint8_t ICACHE_FLASH_ATTR json_add_real(Json_T json, const char* name, float value) {
	if (!json) {
		return 0;
	}
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_REAL, {.real_value = value}});
}

uint8_t ICACHE_FLASH_ATTR json_add_null(Json_T json, const char* name) {
	if (!json) {
		return 0;
	}
	return json_add(json, &(struct Value_T){.name = name, .type = VALUE_TYPE_NULL, {.void_data = NULL}});
}

uint8_t ICACHE_FLASH_ATTR json_add(Json_T json, Value_T value) {
	struct Value_T item;
	char* name = NULL;
	if (!json || !value) {
		return 0;
	}
	memcpy(&item, value, sizeof(struct Value_T));
	if (value->name) {
		if (!(name = (char*)malloc(strlen(value->name) + 1))) {
			return 0;
		}
		strcpy(name, value->name);
		item.name = name;
	} else {
		item.name = NULL;
	}
	item.present = 1;
	if (value->type == VALUE_TYPE_STRING) {
		if (!value->string_value) {
			goto error0;
		}
		if (!(item.string_value = (char*)malloc(strlen(value->string_value) + 1))) {
			goto error0;
		}
		strcpy(item.string_value, value->string_value);
	}
#ifdef LIST_CHANGE_ADD_PROTO
	if (!list_add_(json->list, &item)) {
#else
	if (!list_add(json->list, &item)) {
#endif
		json_free_value(&item);
		return 0;
	}
	return 1;
error0:
	if (name) {
		free(name);
	}
	return 0;
}

static void ICACHE_FLASH_ATTR json_print_value(Value_T value, Buffer_T buffer, uint8_t level) {
	char* str;
	if (!value || !buffer) {
		return;
	}
	switch (value->type) {
		case VALUE_TYPE_STRING:
			buffer_write(buffer, '"');
			if ((str = json_string_escape(value->string_value))) {
				buffer_puts(buffer, str);
				free(str);
			}
			buffer_write(buffer, '"');
			break;
		case VALUE_TYPE_INT:
			buffer_dec(buffer, value->int_value);
			break;
		case VALUE_TYPE_REAL:
			buffer_real(buffer, value->real_value);
			break;
		case VALUE_TYPE_BOOL:
			if (value->bool_value) {
				buffer_puts(buffer, "true");
			} else {
				buffer_puts(buffer, "false");
			}
			break;
		case VALUE_TYPE_OBJ:
		case VALUE_TYPE_ARRAY:
			json_print((Json_T)value->void_data, buffer, level + 1);
			break;
		case VALUE_TYPE_NULL:
			buffer_puts(buffer, "null");
			break;
		default:
			break;
	}
}

uint16_t ICACHE_FLASH_ATTR json_print(Json_T json, Buffer_T buffer, uint8_t level) {
	if (!json || !buffer) {
		return 0;
	}
	json_print_rewind(json);
	while (1) {
		if (json_print_part(json, buffer, level)) {
			continue;
		}
		break;
	}
	return buffer_size(buffer);
}

void ICACHE_FLASH_ATTR json_print_rewind(Json_T json) {
	if (!json) {
		return;
	}
	json->it = 0;
	json->rewind = 1;
}

uint16_t ICACHE_FLASH_ATTR json_print_part(Json_T json, Buffer_T buffer, uint8_t level) {
	struct Value_T value;
	if (!json || !buffer) {
		return 0;
	}
	if ((json->it >= list_size(json->list)) &&
		!json->rewind) {
		return 0;
	}
	json->rewind = 0;
	if (json->it == 0) {
		if (!json->unformatted) {
			buffer_indent(buffer, '\t', level);
		}
		if (json->callback) {
			buffer_puts(buffer, json->callback);
			buffer_write(buffer, '(');
		}
		buffer_write(buffer, json->array ? '[' : '{');
	}
	if (list_size(json->list)) {
		if (!list_read(json->list, json->it, &value)) {
			return 0;
		}
		if (!json->unformatted) {
			buffer_crlf(buffer);
			buffer_indent(buffer, '\t', level + 1);
		}
		if (value.name) {
			char* name;
			if ((name = json_string_escape(value.name))) {
				buffer_write(buffer, '"');
				buffer_puts(buffer, name);
				free(name);
				buffer_puts(buffer, "\": ");
			}
		}
		json_print_value(&value, buffer, level);
		json->it++;
	}
	if (json->it < list_size(json->list)) {
		if (!json->unformatted) {
			buffer_write(buffer, ',');
		} else {
			buffer_puts(buffer, ", ");
		}
	} else {
		if (!json->unformatted) {
			buffer_crlf(buffer);
			buffer_indent(buffer, '\t', level);
		}
		buffer_write(buffer, json->array ? ']' : '}');
		if (json->callback) {
			buffer_puts(buffer, ");");
		}
	}
	return buffer_size(buffer);
}

static void ICACHE_FLASH_ATTR json_free_value(Value_T value) {
	if (!value) {
		return;
	}
	if (value->type == VALUE_TYPE_STRING) {
		if (value->string_value) {
			free(value->string_value);
		}
	}
	if ((value->type == VALUE_TYPE_OBJ) ||
		(value->type == VALUE_TYPE_ARRAY)) {
		if (value->void_data) {
			json_delete((Json_T)value->void_data);
		}
	}
	if (value->name) {
		free((void*)value->name);
	}
}

void ICACHE_FLASH_ATTR json_clear(Json_T json) {
	struct Value_T value;
	if (!json) {
		return;
	}
	list_rewind(json->list);
	while (list_next(json->list, &value)) {
		json_free_value(&value);
	}
	list_clear(json->list);
}

void ICACHE_FLASH_ATTR json_delete(Json_T json) {
	if (!json) {
		return;
	}
	json_clear(json);
	list_delete(json->list);
	free(json);
}

List_T ICACHE_FLASH_ATTR json_items(Json_T json) {
	if (!json) {
		return NULL;
	}
	return json->list;
}

uint8_t ICACHE_FLASH_ATTR json_get(Json_T json, const char* name, Value_T value) {
	struct Value_T item;
	if (!json || !name || !strlen(name)) {
		return 0;
	}
	list_rewind(json->list);
	while (list_next(json->list, &item)) {
		if (!strcmp(item.name, name)) {
			memcpy(value, &item, sizeof(struct Value_T));
			return 1;
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR json_to_values(Json_T json, Value_T values) {
	uint16_t required = 0;
	uint16_t given = 0;
	uint16_t i;
	if (!json || !values || !json->rule || !json->rule_amount) {
		return 0;
	}
	memset(values, 0, sizeof(struct Value_T) * json->rule_amount);
	for (i = 0; i < json->rule_amount; i++) {
		if (json->rule[i].required) {
			required++;
		}
		if (json_get(json, json->rule[i].name, values + i)) {
			if (json->rule[i].required) {
				given++;
			}
		}
	}
	if (given < required) {
		return 0;
	}
	return 1;
}

uint16_t ICACHE_FLASH_ATTR json_size(Json_T json) {
	struct Buffer_T test;
	uint8_t storage[8];
	if (!json) {
		return 0;
	}
	buffer_init(&test, sizeof(storage), storage);
	json_print(json, &test, 0);
	return buffer_overflow(&test) + buffer_capacity(&test);
}

Buffer_T ICACHE_FLASH_ATTR json_to_buffer(Json_T json) {
	return json_to_buffer_size(json, 0);
}

Buffer_T ICACHE_FLASH_ATTR json_to_buffer_size(Json_T json, uint16_t additional_size) {
	Buffer_T buffer;
	if (!json) {
		return NULL;
	}
	buffer = buffer_new(json_size(json) + additional_size);
	if (buffer) {
		json_print(json, buffer, 0);
	}
	json_delete(json);
	return buffer;
}
