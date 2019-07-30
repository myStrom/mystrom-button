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
#include <osapi.h>
#include <user_interface.h>
#include "sleep.h"
#include "debug.h"
#include "color.h"
#include "peri.h"
#include "wifi.h"
#include "rtc.h"
#include "user_config.h"
#ifdef IQS
#include "IQS333.h"
#endif

#define SLEEP_US_BASE ((uint32_t)1000000)
#define MIN_SLEEP_TIME 10
#define MAX_SLEEP_PERIOD 3600

static RWC_T wc = {0, 0, 0, 0};
static os_timer_t sleep_timer;
static os_timer_t work_timer;
static uint32_t sleep_cause = 0;
static uint32_t last_sleep_cause = 0;
static uint32_t last_timestamp = 0;
static uint32_t working_time = 0;
static uint8_t (*sleep_final_fn)(uint8_t final);
static uint32_t sleep_time_to = 0;
static uint32_t inhibit_time = 0;
static uint8_t inhibit_count = 0;
static uint8_t restart_req = 0;

static void ICACHE_FLASH_ATTR sleep_inc_worktime(void* owner) {
	working_time++;
}

/*return sleep period*/
static uint32_t ICACHE_FLASH_ATTR sleep_wc_save(void) {
	uint32_t time_from_begin;
	uint32_t time_to_wakeup;

	wc.sleep_timestamp = sleep_get_current_timestamp();
	if (!wc.begin_timestamp) {
		wc.begin_timestamp = wc.sleep_timestamp;
	} else {
		if (wc.begin_timestamp > wc.sleep_timestamp) {
			debug_enter_here_P();
			wc.begin_timestamp = wc.sleep_timestamp;
		}
	}
	time_from_begin = wc.sleep_timestamp - wc.begin_timestamp;
	debug_value(time_from_begin);
	if (time_from_begin >= WAKEUP_PERIOD_S) {
		debug_enter_here_P();
		wc.sleep_period = MIN_SLEEP_TIME;
	} else {
		time_to_wakeup = (uint32_t)WAKEUP_PERIOD_S - time_from_begin;
		debug_value(time_to_wakeup);
		if (time_to_wakeup >= MAX_SLEEP_PERIOD) {
			wc.sleep_period = MAX_SLEEP_PERIOD;
		} else {
			if (time_to_wakeup < MIN_SLEEP_TIME) {
				wc.sleep_period = MIN_SLEEP_TIME;
			} else {
				wc.sleep_period = time_to_wakeup;
			}
		}
	}
	if (rtc_write(RTC_WC_OFFSET, &wc, sizeof(wc))) {
		debug_describe_P("WC save");
	}
	debug_value(wc.sleep_period);
	return wc.sleep_period;
}

#ifdef IQS
static void ICACHE_FLASH_ATTR sleep_iqs_power_done(I2C_Order_T order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		debug_describe_P("IQS: powmod error");
		iqs_power_mode(1, sleep_iqs_power_done);
		return;
	}
	debug_describe_P("IQS sleep OK!");
	if (sleep_final_fn) {
		sleep_final_fn(1);
	}
	system_deep_sleep_set_option(2);
	if (wifi_is_save()) {
		//system_deep_sleep(SLEEP_US_BASE * 60);
		system_deep_sleep(SLEEP_US_BASE * sleep_wc_save());
	} else {
		system_deep_sleep(0);
	}
}

static void ICACHE_FLASH_ATTR sleep_iqs_led_off_done(I2C_Order_T order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		debug_describe_P("IQS: led error");
		iqs_led_off_order(sleep_iqs_led_off_done);
		return;
	}
	iqs_power_mode(1, sleep_iqs_power_done);
}

static void ICACHE_FLASH_ATTR sleep_iqs_reset_done(I2C_Order_T order, I2C_Result_T result) {
	if (result == I2C_RES_OK) {
		debug_describe_P(COLOR_GREEN "IQS RESET OK" COLOR_END);
		peri_set_event_mode(0);
	}
	if (sleep_final_fn) {
		sleep_final_fn(1);
	}
	system_restart();
}

static void ICACHE_FLASH_ATTR sleep_task(void* owner) {
	iqs_timer_stop();
	if (!restart_req) {
		debug_describe_P("Go to sleep, IQS finish tasks");
		iqs_led_off_order(sleep_iqs_led_off_done);
	} else {
		iqs_soft_reset(sleep_iqs_reset_done);
	}
}
#else
static void ICACHE_FLASH_ATTR sleep_task(void* owner) {
	debug_describe_P("Go to sleep final tasks");
	if (sleep_final_fn) {
		sleep_final_fn(1);
	}
	if (!restart_req) {
		system_deep_sleep_set_option(2);
		if (wifi_is_save()) {
			system_deep_sleep(SLEEP_US_BASE * sleep_wc_save());
		} else {
			system_deep_sleep(0);
		}
	} else {
		system_restart();
	}
}
#endif

static void ICACHE_FLASH_ATTR sleep_unlock_action(void) {
	if (inhibit_count) {
		return;
	}
	if (sleep_cause) {
		return;
	}
	if (sleep_final_fn) {
		sleep_final_fn(0);
	}
	os_timer_disarm(&sleep_timer);
	os_timer_setfn(&sleep_timer, sleep_task, NULL);
	os_timer_arm(&sleep_timer, sleep_time_to, 0);
}

static void ICACHE_FLASH_ATTR sleep_inhibit_task(void* owner) {
	debug_describe_P("End of sleep inhibit");
	inhibit_count = 0;
	sleep_unlock_action();
}

void ICACHE_FLASH_ATTR sleep_init(uint32_t time_to_sleep, uint8_t (*final_fn)(uint8_t final)) {
	sleep_final_fn = final_fn;
	sleep_time_to = time_to_sleep;
	if (!rtc_read(RTC_WC_OFFSET, &wc, sizeof(wc)) ||
		(peri_rst_reason() != REASON_DEEP_SLEEP_AWAKE)) {
			debug_describe_P("Wakeup counter init");
			memset(&wc, 0, sizeof(wc));
			if (rtc_write(RTC_WC_OFFSET, &wc, sizeof(wc))) {
				debug_describe_P("Wakeup counter reset");
			}
	}
	sleep_update_timestamp(wc.sleep_timestamp);
	os_timer_disarm(&work_timer);
	os_timer_setfn(&work_timer, sleep_inc_worktime, NULL);
	os_timer_arm(&work_timer, 1000, 1);
}

void ICACHE_FLASH_ATTR sleep_deinit(void) {
	os_timer_disarm(&work_timer);
}

void ICACHE_FLASH_ATTR sleep_set_time_to_sleep(uint32_t time_to_sleep) {
	sleep_time_to = time_to_sleep;
}

uint8_t ICACHE_FLASH_ATTR sleep_wc_event(uint8_t timer) {
	uint8_t ret_val = 0;
	if (timer) {
		sleep_update_timestamp(sleep_get_current_timestamp() + wc.sleep_period);
		if (sleep_get_current_timestamp() >= wc.begin_timestamp) {
			if ((sleep_get_current_timestamp() - wc.begin_timestamp) >= WAKEUP_PERIOD_S) {
				wc.begin_timestamp = sleep_get_current_timestamp();
				ret_val = 1;
			}
		}
	}
	return ret_val;
}

void ICACHE_FLASH_ATTR sleep_lock(Sleep_Lock_T lock) {
	sleep_cause |= lock;
	if (sleep_cause) {
		if (!inhibit_count) {
			os_timer_disarm(&sleep_timer);
		}
	}
	if (sleep_cause != last_sleep_cause) {
		debug_printf("Sleep lock: %x\n", sleep_cause);
	}
	last_sleep_cause = sleep_cause;
}

void ICACHE_FLASH_ATTR sleep_unlock(Sleep_Lock_T lock) {
	sleep_cause &= ~((uint32_t)lock);
	sleep_unlock_action();
	if (sleep_cause != last_sleep_cause) {
		debug_printf("Sleep unlock: %x\n", sleep_cause);
	}
	last_sleep_cause = sleep_cause;
}

uint32_t ICACHE_FLASH_ATTR sleep_state(void) {
	return sleep_cause;
}

void ICACHE_FLASH_ATTR sleep_inhibit(uint32_t time) {
	inhibit_time = time;
}

uint8_t ICACHE_FLASH_ATTR sleep_reset(void) {
	if (!inhibit_time) {
		debug_describe_P(COLOR_YELLOW "No inhibit time" COLOR_END);
		return 0;
	}
	os_timer_disarm(&sleep_timer);
	os_timer_setfn(&sleep_timer, sleep_inhibit_task, NULL);
	inhibit_count = 1;
	debug_printf(COLOR_CYAN "Inhibit time: %u\n" COLOR_END, inhibit_time);
	os_timer_arm(&sleep_timer, inhibit_time, 0);
	return 1;
}

void ICACHE_FLASH_ATTR sleep_done(void) {
	if (!inhibit_time) {
		return;
	}
	os_timer_disarm(&sleep_timer);
	sleep_inhibit_task(NULL);
}

void ICACHE_FLASH_ATTR sleep_immediately(void) {
	if (!inhibit_count) {
		return;
	}
	os_timer_disarm(&sleep_timer);
	sleep_inhibit_task(NULL);
}

void ICACHE_FLASH_ATTR sleep_restart(void) {
	restart_req = 1;
}

void ICACHE_FLASH_ATTR sleep_update_timestamp(uint32_t timestamp) {
	last_timestamp = timestamp;
	working_time = 0;
	if (!wc.begin_timestamp) {
		wc.begin_timestamp = last_timestamp;
	} else {
		if (wc.begin_timestamp > last_timestamp) {
			wc.begin_timestamp = last_timestamp;
		}
	}
	debug_printf(COLOR_YELLOW "update_timestamp: %d\n" COLOR_END, last_timestamp);
}

uint32_t ICACHE_FLASH_ATTR sleep_get_current_timestamp(void) {
	return last_timestamp + working_time;
}
