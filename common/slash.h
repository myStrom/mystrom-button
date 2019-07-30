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


#ifndef SLASH_H_INCLUDED
#define SLASH_H_INCLUDED 1

#include <inttypes.h>
#include "rule.h"
#include "value.h"

typedef struct {
	const char* delimeters;
	char* current;
	uint8_t cut_char;
	uint8_t level;
	uint8_t end_find : 1;
	uint8_t have_cut_char : 1;
	uint8_t in_str : 1;
	char begin;
	char end;
	char str_delim;
	char esc_seq;
	char prev;
} Slash_T;

Slash_T* slash_init(Slash_T* slash, char* buffer, uint8_t cut_char);
Slash_T* slash_init_delimeters(Slash_T* slash, char* buffer, const char* delimeters);
void slash_set_exclude(Slash_T* slash, char begin, char end);
void slash_set_str(Slash_T* slash, char delim, char esc);
char* slash_next(Slash_T* slash);
char* slash_next_trim(Slash_T* slash);
uint16_t slash_escape(char** in, const char* escape_str);
char* slash_escape_unprint(char* in);
uint8_t slash_strchr_escaped(const char* in, char c, uint16_t* index);
uint8_t slash_have_next(Slash_T* slash);
void slash_restore(Slash_T* slash, char* buffer);
char* slash_value(char* part, const char* name, const char* separators);
char* slash_trim(char* part, const char* space_char);
char* slash_trim_unprint(char* part);
char* slash_replace(char* part);
uint8_t slash_next_equal(Slash_T* slash, const char* data);
char* slash_current(Slash_T* slash);
uint16_t slash_to_array(Slash_T* slash, char** array, uint16_t size);
void slash_set_delimeter(Slash_T* slash, uint8_t cut_char);
char* slash_next_name_and_value(Slash_T* slash, char cut_char, char** name);
uint8_t slash_parse(Slash_T* slash, const Rule_T* params, Value_T values, uint8_t count, char cut_char);
uint8_t slash_start_with(const char* str, const char* begin);

#endif
