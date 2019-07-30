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


#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED 1

#include <inttypes.h>
#include "value.h"
#include "buffer.h"
#include "list.h"
#include "rule.h"

typedef struct Json_T* Json_T;

Json_T json_new(void);
void json_set_format(Json_T json, uint8_t array, uint8_t unformatted);
uint8_t json_add(Json_T json, Value_T value);
uint16_t json_print(Json_T json, Buffer_T buffer, uint8_t level);
void json_print_rewind(Json_T json);
uint16_t json_print_part(Json_T json, Buffer_T buffer, uint8_t level);
void json_clear(Json_T json);
void json_delete(Json_T json);
uint8_t json_add_obj(Json_T json, const char* name, Json_T obj);
uint8_t json_add_array(Json_T json, const char* name, Json_T obj);
uint8_t json_add_string(Json_T json, const char* name, const char* value);
uint8_t json_add_ip(Json_T json, const char* name, uint32_t addr);
uint8_t json_add_int(Json_T json, const char* name, int32_t value);
uint8_t json_add_real(Json_T json, const char* name, float value);
uint8_t json_add_bool(Json_T json, const char* name, uint8_t value);
uint8_t json_add_null(Json_T json, const char* name);
Json_T json_parse(char* string, const Rule_T* rule, uint8_t size);
List_T json_items(Json_T json);
uint8_t json_get(Json_T json, const char* name, Value_T value);
uint8_t json_to_values(Json_T json, Value_T values);
uint8_t json_set_callback(Json_T json, const char* name);
uint16_t json_size(Json_T json);
Buffer_T json_to_buffer(Json_T json);
Buffer_T json_to_buffer_size(Json_T json, uint16_t additional_size);

#endif
