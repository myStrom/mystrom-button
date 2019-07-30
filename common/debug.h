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


#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED 1

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <c_types.h>
#include <osapi.h>
#include <user_interface.h>
#include <mem.h>

extern uint32_t timer_ticks;

#define DEBUG_COLOR_BLACK 0
#define DEBUG_COLOR_RED 1
#define DEBUG_COLOR_GREEN 2
#define DEBUG_COLOR_YELLOW 3
#define DEBUG_COLOR_BLUE 4
#define DEBUG_COLOR_MAGENTA 5
#define DEBUG_COLOR_CYAN 6
#define DEBUG_COLOR_WHITE 7

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef DEBUG_PRINTF
#define debug_printf(...) os_printf(__VA_ARGS__)
#define debug_enter_here() debug_printf("Enter here, file: " __FILE__ ", line: "TOSTRING(__LINE__) "\n")
#define debug_value(value) debug_printf("[%u] %s: %d\n", timer_ticks, #value, (int)(value))
#define debug_value_long(value) debug_printf("[%u] %s: %ld\n", timer_ticks, #value, value)
#define debug_hex(value) debug_printf("[%u] %s: 0x%08x\n", timer_ticks, #value, (uint32_t)value)
#define debug_describe(str) debug_printf("[%u] %s\n", timer_ticks, str)
#define debug_print(str) os_printf("[%u] %s\n", timer_ticks, str)
#define debug_put(sign) debug_printf("%c", (char)sign)
#define debug_puts(str) debug_printf("[%u] %s\n", timer_ticks, str)
#define debug_timestamp() debug_printf("[%u] ", timer_ticks)

#define debug_describe_P(s)											\
	do {															\
		static const char __c[] ICACHE_RODATA_ATTR = (s);			\
		char __buf[sizeof(__c)];									\
		memcpy(__buf, __c, sizeof(__c));							\
		debug_describe(__buf);										\
	} while (0)

#define debug_enter_here_P(s)																									\
	do {																														\
		static const char __c[] ICACHE_RODATA_ATTR = "Enter here, file: " __FILE__ ", line: "TOSTRING(__LINE__) "\n";			\
		char __buf[sizeof(__c)];																								\
		memcpy(__buf, __c, sizeof(__c));																						\
		debug_describe(__buf);																									\
	} while (0)
#else
#define debug_printf(...)
#define debug_logf(...) do {char __trans_buffer[48]; snprintf(__trans_buffer, sizeof(__trans_buffer), __VA_ARGS__); log_puts(__trans_buffer);} while (0)
#define debug_enter_here()
#define debug_value(value)
#define debug_value_long(value)
#define debug_hex(value)
#define debug_describe(str)
#define debug_print(str)
#define debug_put(sign)
#define debug_puts(str)
#define debug_timestamp()

#define debug_describe_P(s)
#define debug_enter_here_P(s)
#endif

#endif
