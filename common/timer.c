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


#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <c_types.h>
#include "timer.h"

Timer_T ICACHE_FLASH_ATTR timer_new(uint16_t time, void* owner, void (*lapse)(void* owner)) {
	Timer_T timer;
	if (!(timer = (void*)malloc(sizeof(struct Timer_T)))) {
		return NULL;
	}
	memset(timer, 0, sizeof(struct Timer_T));
	timer->time = time;
	timer->counter = time;
	timer->owner = owner;
	timer->lapse = lapse;
	return timer;
}

Timer_T ICACHE_FLASH_ATTR timer_init(Timer_T timer, uint16_t time, void* owner, void (*lapse)(void* owner)) {
	if (!timer) {
		return NULL;
	}
	memset(timer, 0, sizeof(struct Timer_T));
	timer->time = time;
	timer->counter = time;
	timer->owner = owner;
	timer->lapse = lapse;
	return timer;
}

void ICACHE_FLASH_ATTR timer_set(Timer_T timer, uint16_t time) {
	if (!timer) {
		return;
	}
	timer->time = time;
	timer->counter = timer->time;
}

void ICACHE_FLASH_ATTR timer_free(Timer_T timer) {
	if (!timer) {
		return;
	}
	free(timer);
}

void ICACHE_FLASH_ATTR timer_repeat(Timer_T timer, uint8_t repeat) {
	if (!timer) {
		return;
	}
	timer->repeat = repeat ? 1 : 0;
}

void ICACHE_FLASH_ATTR timer_start(Timer_T timer) {
	if (!timer) {
		return;
	}
	timer->start = 1;
}

void ICACHE_FLASH_ATTR timer_stop(Timer_T timer) {
	if (!timer) {
		return;
	}
	timer->start = 0;
	timer->now = 0;
	timer->event = 0;
}

void ICACHE_FLASH_ATTR timer_restart(Timer_T timer) {
	if (!timer) {
		return;
	}
	timer->counter = timer->time;
	timer->start = 1;
}

void ICACHE_FLASH_ATTR timer_reset(Timer_T timer) {
	if (!timer) {
		return;
	}
	timer->counter = timer->time;
}

void ICACHE_FLASH_ATTR timer_clk(Timer_T timer) {
	if (!timer) {
		return;
	}
	if (!timer->start) {
		return;
	}
	if (timer->now) {
		timer->now = 0;
		if (timer->lapse) {
			timer->lapse(timer->owner);
		}
	}
	if (timer->counter) {
		timer->counter--;
		if (!timer->counter) {
			if (timer->repeat) {
				timer->counter = timer->time;
			} else {
				timer->start = 0;
			}
			timer->event = 1;
			if (timer->lapse) {
				timer->lapse(timer->owner);
			}
		}
	}
}

void ICACHE_FLASH_ATTR timer_call(Timer_T timer) {
	if (!timer) {
		return;
	}
	if (!timer->lapse) {
		return;
	}
	timer->lapse(timer->owner);
}

void ICACHE_FLASH_ATTR timer_now(Timer_T timer) {
	if (!timer) {
		return;
	}
	if (timer->start) {
		timer->now = 1;
	}
}

uint8_t ICACHE_FLASH_ATTR timer_event(Timer_T timer) {
	if (!timer) {
		return 0;
	}
	if (timer->event) {
		timer->event = 0;
		return 1;
	}
	return 0;
}

void ICACHE_FLASH_ATTR timer_notify(Timer_T timer) {
	if (!timer) {
		return;
	}
	timer->event = 1;
}

uint16_t ICACHE_FLASH_ATTR timer_counter(Timer_T timer) {
	uint16_t ret_val;
	if (!timer) {
		return 0;
	}
	ret_val = timer->counter;
	return ret_val;
}

uint16_t ICACHE_FLASH_ATTR timer_time(Timer_T timer) {
	uint16_t ret_val;
	if (!timer) {
		return 0;
	}
	ret_val = timer->time;
	return ret_val;
}

void ICACHE_FLASH_ATTR timer_set_counter(Timer_T timer, uint16_t counter) {
	if (!timer) {
		return;
	}
	timer->counter = counter;
}

uint8_t ICACHE_FLASH_ATTR timer_is_start(Timer_T timer) {
	uint8_t ret_val;
	if (!timer) {
		return 0;
	}
	ret_val = timer->start;
	return ret_val;
}
