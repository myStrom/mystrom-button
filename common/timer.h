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


#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED 1

#include <inttypes.h>

typedef struct Timer_T* Timer_T;

struct Timer_T {
	void* owner;
	void (*lapse)(void* owner);
	uint16_t time;
	uint16_t counter;
	union {
		uint8_t flags;
		struct {
			uint8_t repeat : 1;
			uint8_t start : 1;
			uint8_t event : 1;
			uint8_t now : 1;
		};
	};
};

Timer_T timer_new(uint16_t time, void* owner, void (*lapse)(void* owner));
Timer_T timer_init(Timer_T timer, uint16_t time, void* owner, void (*lapse)(void* owner));
void timer_free(Timer_T timer);
void timer_repeat(Timer_T timer, uint8_t repeat);
void timer_start(Timer_T timer);
void timer_stop(Timer_T timer);
void timer_restart(Timer_T timer);
void timer_clk(Timer_T timer);
uint8_t timer_event(Timer_T timer);
void timer_notify(Timer_T timer);
void timer_call(Timer_T timer);
void timer_set(Timer_T timer, uint16_t time);
void timer_reset(Timer_T timer);
uint16_t timer_counter(Timer_T timer);
uint16_t timer_time(Timer_T timer);
void timer_set_counter(Timer_T timer, uint16_t counter);
uint8_t timer_is_start(Timer_T timer);
void timer_now(Timer_T timer);

#endif
