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


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include "led.h"
#include "debug.h"

Led_T ICACHE_FLASH_ATTR led_init(Led_T led, uint8_t index) {
	if (!led) {
		return NULL;
	}
	memset(led, 0, sizeof(struct Led_T));
	led->index = index;
	return led;
}

void ICACHE_FLASH_ATTR led_start(Led_T led, uint8_t repeat, uint16_t speed, uint8_t bypass) {
	if (!led || !speed) {
		return;
	}
	if (led->state == LED_STATE_TRANSITION) {
		led->shadow += led->diff;
		led->state = LED_STATE_REWRITE;
	}
	if (led->state == LED_STATE_REWRITE) {
		led->repeat = repeat;
		led->speed = speed;
		led->value = 4095;
		led->state = LED_STATE_BEGIN;
		led->bypass = bypass;
	}
}

void ICACHE_FLASH_ATTR led_set(Led_T led, uint8_t value, uint16_t speed) {
	uint16_t mul_value;
	if (!led) {
		return;
	}
	if (!speed) {
		speed = 4095;
	}
	mul_value = (uint16_t)value * 16;
	if (led->state == LED_STATE_REWRITE) {
		led->speed = speed;
		led->value = 0;
		led->diff = (int16_t)mul_value - led->shadow;
		led->state = LED_STATE_TRANSITION;
		led->set_value = mul_value;
		return;
	}
	if (led->state == LED_STATE_TRANSITION) {
		led->shadow += ((int32_t)led->diff * led->value) / 4095;
		led->speed = speed;
		led->diff = (int16_t)mul_value - led->shadow;
		led->value = 0;
		led->set_value = mul_value;
	}
}

uint8_t ICACHE_FLASH_ATTR led_get(Led_T led) {
	if (!led) {
		return 0;
	}
	return led->set_value / 16;
}

uint8_t ICACHE_FLASH_ATTR led_get_current(Led_T led) {
	if (!led) {
		return 0;
	}
	return led->last_value / 16;
}

/*
static uint8_t led_sinus(Led_T led) {
	int16_t value;
	value = ((int16_t)(127 - led->value) * led->speed) / 255;
	if (value < 0) {
		value *= -1;
	}
	return value + 1;
}
*/

uint8_t ICACHE_FLASH_ATTR led_ready(Led_T led) {
	if (!led) {
		return 0;
	}
	if (led->state != LED_STATE_REWRITE) {
		return 0;
	}
	if (led->shadow != led->value) {
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR led_stop_transition(Led_T led) {
	if (!led) {
		return 0;
	}
	if (led->state != LED_STATE_TRANSITION) {
		return 0;
	}
	led->stop = 1;
	return 1;
}

uint8_t led_clk(Led_T led, uint16_t* set_val) {
	uint8_t mk_set = 1;
	uint8_t ret_val = 0;
	if (!led || !set_val) {
		return 0;
	}
	switch (led->state) {
		case LED_STATE_REWRITE:
			if (led->shadow != led->value) {
				led->value = led->shadow;
			} else {
				mk_set = 0;
			}
			break;
		case LED_STATE_TRANSITION:
		case LED_STATE_END:
		case LED_STATE_UP:
			if (led->bypass && (led->state == LED_STATE_UP)) {
				mk_set = 0;
			}
			if (((int16_t)led->value + led->speed) < 4095) {
				led->value += led->speed;
			} else {
				led->value = 4095;
				if (led->state == LED_STATE_TRANSITION) {
					led->shadow += led->diff;
				}
				if ((led->state == LED_STATE_END) || (led->state == LED_STATE_TRANSITION)) {
					led->value = led->shadow;
					led->state = LED_STATE_REWRITE;
				} else {
					led->state = LED_STATE_DOWN;
				}
			}
			break;
		case LED_STATE_BEGIN:
		case LED_STATE_DOWN:
			if (led->bypass && (led->state == LED_STATE_DOWN)) {
				mk_set = 0;
			}
			if (((int16_t)led->value - led->speed) > 0) {
				led->value -= led->speed;
			} else {
				led->value = 0;
				if (led->state == LED_STATE_DOWN) {
					if (led->repeat) {
						if (led->repeat != 255) {
							led->repeat--;
						}
					}
				}
				if (led->repeat) {
					led->state = LED_STATE_UP;
				} else {
					led->state = LED_STATE_END;
				}
			}
			break;
	}
	if (mk_set) {
		uint16_t value;
		if ((led->state == LED_STATE_BEGIN) || (led->state == LED_STATE_END)) {
			value = ((uint32_t)led->value * led->shadow) / 4095;
		} else {
			if (led->state == LED_STATE_TRANSITION) {
				value = led->shadow + (((int32_t)led->diff * led->value) / 4095);
				if (led->stop) {
					led->value = value;
					led->shadow = value;
					led->stop = 0;
					led->state = LED_STATE_REWRITE;
				}
			} else {
				value = led->value;
			}
		}
		if (value != led->last_value) {
			led->last_value = value;
			ret_val = 1;
		}
	}
	*set_val = led->last_value;
	return ret_val;
}
