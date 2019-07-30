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


#ifndef SLEEP_H_INCLUDED
#define SLEEP_H_INCLUDED 1

#include <inttypes.h>

typedef enum {
	SLEEP_WPS = 0x0001,
	SLEEP_AP = 0x0004,
	SLEEP_TOUCH = 0x0008,
	SLEEP_PRESS = 0x0010,
	SLEEP_RESET = 0x0020,
	SLEEP_WIFI = 0x0040,
	SLEEP_CHARGE = 0x0080,
	SLEEP_UPGRADE = 0x0100,
	SLEEP_PWM = 0x0200,
	SLEEP_NS = 0x0400,
	SLEEP_REBOOT = 0x0800,
	SLEEP_DHCP = 0x1000,
	SLEEP_DELAY = 0x2000,
	SLEEP_SERVICE = 0x4000,
} Sleep_Lock_T;

void sleep_init(uint32_t time_to_sleep, uint8_t (*final_fn)(uint8_t final));
void sleep_deinit(void);
void sleep_set_time_to_sleep(uint32_t time_to_sleep);
void sleep_lock(Sleep_Lock_T lock);
void sleep_unlock(Sleep_Lock_T lock);
uint32_t sleep_state(void);
void sleep_inhibit(uint32_t time);
uint8_t sleep_reset(void);
void sleep_done(void);
void sleep_immediately(void);
void sleep_restart(void);
uint8_t sleep_wc_event(uint8_t timer);
void sleep_update_timestamp(uint32_t timestamp);
uint32_t sleep_get_current_timestamp(void);

#endif
