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


#ifndef LED_H_INCLUDED
#define LED_H_INCLUDED 1

#include <inttypes.h>

typedef struct Led_T* Led_T;

typedef enum {
	LED_STATE_REWRITE = 0,
	LED_STATE_BEGIN = 1,
	LED_STATE_UP = 2,
	LED_STATE_DOWN = 3,
	LED_STATE_END = 4,
	LED_STATE_TRANSITION = 5
} Led_State_T;

typedef enum {
	LED_ACTION_NONE,
	LED_ACTION_TRANSITION,
	LED_ACTION_BLINK,
	__LED_ACTION_MAX
} Led_Action_T;

typedef struct {
	uint16_t speed;
	uint8_t value;
	uint8_t bypass;
} Led_Transition_T;

typedef struct {
	uint16_t speed;
	uint8_t repeat;
	uint8_t bypass;
} Led_Blink_T;

struct Led_T {
	Led_State_T state;
	int16_t diff;
	uint16_t speed;
	uint16_t value;
	uint16_t last_value;
	uint16_t set_value;
	uint16_t shadow;
	uint8_t repeat;
	uint8_t index;
	uint8_t bypass : 1;
	uint8_t stop : 1;
};

typedef struct {
	uint16_t speed;
	uint8_t repeat;
	uint8_t bypass[4];
} Led_Patterns_T;

Led_T led_init(Led_T led, uint8_t index);
void led_start(Led_T led, uint8_t repeat, uint16_t speed, uint8_t bypass);
void led_set(Led_T led, uint8_t value, uint16_t speed);
uint8_t led_get(Led_T led);
void led_div(Led_T led, uint16_t div);
uint8_t led_ready(Led_T led);
uint8_t led_clk(Led_T led, uint16_t* set_val);
uint8_t led_stop_transition(Led_T led);

#endif
