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
#include <osapi.h>
#include <user_interface.h>
#include <espconn.h>
#include "wps.h"
#include "debug.h"
#include "color.h"
#include "rgb.h"
#include "user_config.h"

static os_timer_t wps_timer;
static uint8_t wps_enabled = 0;
static uint8_t wps_try_counter = 0;
static uint8_t wps_success = 0;
static uint8_t wps_shorten = 0;
static uint8_t wps_error = 0;
static void (*wps_fail_cb)(uint8_t) = NULL;

static void ICACHE_FLASH_ATTR wps_run(void* owner);

static void ICACHE_FLASH_ATTR wps_fail(void* owner) {
	peri_lock_white(0);
	if (wps_error) {
		rgb_set_pattern(RGB_WPS_FAIL, 0);
	}
	wps_enabled = 0;
	if (wps_fail_cb) {
		wps_fail_cb(wps_shorten);
	}
	wps_shorten = 0;
}

static void ICACHE_FLASH_ATTR wps_status_cb(int status) {
	debug_printf("WPS status: %d\n", status);
	wifi_wps_disable();
	switch (status) {
		case WPS_CB_ST_SUCCESS:
			wps_success = 1;
			wps_enabled = 0;
			peri_lock_white(0);
			return;
		case WPS_CB_ST_FAILED:
			debug_describe_P("Error: WPS failed");
			wps_try_counter = 0;
			wps_error = 1;
			break;
		case WPS_CB_ST_TIMEOUT:
			debug_describe_P("Error: WPS timeout");
			wps_try_counter = 0;
			wps_error = 1;
			break;
		case WPS_CB_ST_WEP:
			debug_describe_P("Error: WPS WEP???");
			wps_try_counter = 0;
			wps_error = 1;
			break;
	}
	if (wps_try_counter) {
		wps_try_counter--;
		os_timer_disarm(&wps_timer);
		os_timer_setfn(&wps_timer, wps_run, NULL);
		os_timer_arm(&wps_timer, 3000, 0);
		return;
	}
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_fail, NULL);
	os_timer_arm(&wps_timer, 1000, 0);
}

static void ICACHE_FLASH_ATTR wps_run(void* owner) {
	if (!wps_try_counter) {
		goto error0;
	}
	if (!wifi_wps_enable(WPS_TYPE_PBC)) {
		goto error0;
	}
	if (!wifi_set_wps_cb(wps_status_cb)) {
		goto error1;
	}
	if (!wifi_wps_start()) {
		goto error1;
	}
	debug_describe_P(COLOR_YELLOW "WPS started..." COLOR_END);
	rgb_set_pattern(RGB_WPS_START, 0);
	return;

error1:
	wifi_wps_disable();
error0:
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_fail, NULL);
	os_timer_arm(&wps_timer, 1000, 0);
}

static void ICACHE_FLASH_ATTR wps_enable_dhcp(void* owner) {
	if (!wps_try_counter) {
		goto error;
	}
	wifi_station_disconnect();
	if (wifi_station_dhcpc_status() != DHCP_STARTED) {
		wifi_station_dhcpc_start();
	}
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_run, NULL);
	os_timer_arm(&wps_timer, 3000, 0);
	return;
error:
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_fail, NULL);
	os_timer_arm(&wps_timer, 1000, 0);
}

static void ICACHE_FLASH_ATTR wps_to_sta(void* owner) {
	if (!wps_try_counter) {
		goto error;
	}
	if (!wifi_station_set_reconnect_policy(0)) {
		goto error;
	}
	if (wifi_get_opmode() != STATION_MODE) {
		if (!wifi_set_opmode_current(STATION_MODE)) {
			goto error;
		}
	}
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_enable_dhcp, NULL);
	os_timer_arm(&wps_timer, 3000, 0);
	return;
error:
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_fail, NULL);
	os_timer_arm(&wps_timer, 1000, 0);
}

uint8_t ICACHE_FLASH_ATTR wps_start(void) {
	if (wps_enabled) {
		return 0;
	}
	peri_set_white(1);
	peri_lock_white(1);
	wps_try_counter = WPS_TRY;
	wps_success = 0;
	wps_shorten = 0;
	wps_error = 0;
	wps_enabled = 1;
	os_timer_disarm(&wps_timer);
	os_timer_setfn(&wps_timer, wps_to_sta, NULL);
	os_timer_arm(&wps_timer, 3000, 0);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR wps_is_enabled(void) {
	return wps_enabled;
}

uint8_t ICACHE_FLASH_ATTR wps_success_is_event(void) {
	if (wps_success) {
		wps_success = 0;
		return 1;
	}
	return 0;
}

void ICACHE_FLASH_ATTR wps_fail_fn(void (*cb)(uint8_t shorten)) {
	wps_fail_cb = cb;
}

uint8_t ICACHE_FLASH_ATTR wps_stop(void) {
	if (!wps_enabled) {
		return 0;
	}
	os_timer_disarm(&wps_timer);
	wifi_wps_disable();
	return 1;
}

uint8_t ICACHE_FLASH_ATTR wps_abort(void) {
	if (!wps_enabled) {
		return 0;
	}
	if (wps_try_counter) {
		wps_try_counter = 0;
		wps_shorten = 1;
		return 1;
	}
	return 0;
}
