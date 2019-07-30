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


/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/7/3, v1.0 create this file.
*******************************************************************************/

#include <string.h>
#include <osapi.h>
#include <user_interface.h>
#include <mem.h>
#include <espconn.h>
#include <ip_addr.h>
#include "debug.h"
#include "http.h"
#include "crc.h"
#include "utils.h"
#include "collect.h"
#include "peri.h"
#include "IQS333.h"
#include "store.h"
#include "ns.h"
#include "color.h"
#include "wifi.h"
#include "control.h"
#include "wps.h"
#include "rgb.h"
#include "payload.h"
#include "sleep.h"
#include "device.h"
#include "queue.h"
#include "timer.h"
#include "url_storage.h"
#include "version.h"

extern struct Store_T store;
Url_Storage_T url_storage;
Collect_T collect = NULL;
Ns_T ns = NULL;
char own_mac[13];
uint32_t timer_ticks = 0;
volatile uint32_t download_process = 0;
static Http_T http = NULL;
static Payload_T payload = NULL;
static Queue_T queue = NULL;

static struct Timer_T reconn_timer;
static struct Timer_T conn_timer;
static struct Timer_T action_timer;
static struct Timer_T dhcp_timer;
static struct Timer_T ap_blink;
static struct Timer_T reset_blink;
static struct Timer_T reset_timer;
static struct Timer_T service_timer;
static os_timer_t general_timer;
static uint8_t reboot_req = 0;
static uint8_t factory_restore = 0;
static uint8_t service_mode = 0;
static uint8_t periodic_wakeup = 0;
static uint8_t cold_start = 0;
static uint8_t conn_limit = TRY_CONN_LIMIT;
static uint8_t press = 0;
static uint8_t hold = 0;
#ifdef IQS
static uint8_t iqs_inhibit = 0;
static uint8_t iqs_wheel_inhibit = 0;
#endif

static void ICACHE_FLASH_ATTR timer_run_task(os_timer_t* timer, void (*func)(void*), void* owner, uint32_t time);
static void ICACHE_FLASH_ATTR user_reboot_cb(void);
static void ICACHE_FLASH_ATTR user_timers_deinit(void);

/*call only when deep sleep awake*/
static void ICACHE_FLASH_ATTR user_wakeup(uint8_t timer, uint8_t status) {
	debug_printf(COLOR_MAGENTA "wakeup timer: %x, status: %x\n" COLOR_END, (int)timer, (int)status);
	if (peri_hold_on_start()) {
		status = 1;
	} else {
		periodic_wakeup = sleep_wc_event(timer);
	}
	if (!status && !periodic_wakeup) {
		/*after FW upgrade activate button on 30s*/
		if (peri_rst_reason() != REASON_DEEP_SLEEP_AWAKE) {
			periodic_wakeup = 1;
		}
	}
	if (status || periodic_wakeup) {
		if (!status) {
			/*force disable LED*/
			peri_set_white(0);
			/*erase rtc*/
			wifi_rtc_ip_erase();
			sleep_inhibit(PERIODIC_WAKEUP_TIME);
			sleep_reset();
		} else {
			if (!peri_paired()) {
				wifi_rtc_ip_erase();
				sleep_inhibit(SHORT_INHIBIT_TIME);
				sleep_reset();
				cold_start = 1;
				debug_describe_P(COLOR_YELLOW "NOT PAIRED" COLOR_END);
			}
		}
		sleep_lock(SLEEP_WIFI);
		timer_restart(&action_timer);
		timer_notify(&reconn_timer);
	} else {
		/*to any release required*/
		sleep_set_time_to_sleep(0);
		sleep_unlock(SLEEP_WIFI);
	}
}

#ifdef IQS
static void ICACHE_FLASH_ATTR iqs_release(uint8_t busy) {
	if (busy && !iqs_wheel_inhibit) {
		peri_set_white(0);
	}
	if (!iqs_inhibit && !iqs_wheel_inhibit && !hold && !press && !busy) {
		debug_describe_P(COLOR_GREEN "@@@TOUCH" COLOR_END);
		if (!factory_restore) { 
			payload_action(payload, own_mac, BTN_ACTION_TOUCH, 0);
		}
	}
	if (iqs_wheel_inhibit) {
		debug_describe_P(COLOR_GREEN "@@@WHEEL" COLOR_END);
		if (!factory_restore) {
			payload_action(payload, own_mac, BTN_ACTION_WHEEL_FINAL, 0);
		} else {
			peri_set_white(0);
		}
	}
	iqs_wheel_inhibit = 0;
	iqs_inhibit = 0;
	hold = 0;
	sleep_unlock(SLEEP_TOUCH);
}

static void ICACHE_FLASH_ATTR iqs_press(void) {
	sleep_lock(SLEEP_TOUCH);
	peri_set_white(1);
}

static uint8_t ICACHE_FLASH_ATTR iqs_wheel(uint16_t count, int16_t diff) {
	if (!iqs_inhibit && !hold && !press && !payload_size(payload)) {
		uint16_t result;
		count /= 16;
		if (count > 255) {
			count = 255;
		}
		diff *= -1;
		diff /= 16;
		if (diff > 127) {
			diff = 127;
		} else {
			if (diff < -127) {
				diff = -127;
			}
		}
		result = (count << 8) | (diff & 0xFF);
		if (!factory_restore) {
			payload_action(payload, own_mac, BTN_ACTION_WHEEL, result);
		}
		iqs_wheel_inhibit = 1;
		return 1;
	}
	return 0;
}
#endif

static void ICACHE_FLASH_ATTR user_http_run(void) {
	if (http) {
		return;
	}
	if ((http = http_new())) {
		debug_describe_P(COLOR_GREEN "Http instance created" COLOR_END);
		wifi_http(http);
		control_http(http);
		device_http(http);
		boot_http(http);
	}
}

static void ICACHE_FLASH_ATTR user_factory_reset(void) {
	factory_restore = 1;
	timer_stop(&ap_blink);
	service_mode = 0;
	timer_stop(&service_timer);
	rgb_set_pattern(RGB_RESTORE, 0);
	peri_set_led(0, 0, 0, 0);
	payload_connect(payload, 0);
	collect_stop(collect);
	wifi_inhibit();
	sleep_lock(SLEEP_RESET);
	rtc_erase_all();
	store_erase();
	url_storage_erase_all(&url_storage);
	system_restore();
	sleep_restart();
	sleep_unlock(SLEEP_CHARGE);
	sleep_unlock(SLEEP_SERVICE);
	debug_describe_P(COLOR_GREEN "+++RESET DONE" COLOR_END);
	sleep_unlock(SLEEP_RESET);
	sleep_inhibit(1);
	sleep_immediately();
}


static void btn_action(Btn_Action_T action) {
	if (!queue) {
		return;
	}
	queue_write(queue, &action);
}

static void ICACHE_FLASH_ATTR user_reset_timeout(void* owner) {
	timer_stop(&reset_blink);
	if (!timer_is_start(&ap_blink)) {
		peri_lock_white(0);
		peri_set_white(0);
	}
	debug_describe_P(COLOR_RED "+++RESET TIMEOUT" COLOR_END);
	sleep_unlock(SLEEP_RESET);
}

static uint8_t ICACHE_FLASH_ATTR btn_action_flash(Btn_Action_T action) {
	switch (action) {
		case BTN_ACTION_SHORT:
			debug_describe_P(COLOR_GREEN "+++SHORT" COLOR_END);
		break;

		case BTN_ACTION_LONG:
			debug_describe_P(COLOR_GREEN "+++LONG" COLOR_END);
		break;

		case BTN_ACTION_DOUBLE:
			debug_describe_P(COLOR_GREEN "+++DOUBLE" COLOR_END);
		break;

		case BTN_ACTION_RESET:
			debug_describe_P(COLOR_YELLOW "+++RESET BEGIN" COLOR_END);
		break;

		case BTN_ACTION_PRESS:
			debug_describe_P(COLOR_GREEN ">>>PRESS" COLOR_END);
		break;

		case BTN_ACTION_RELEASE:
			debug_describe_P(COLOR_GREEN "<<<RELEASE" COLOR_END);
		break;

		case BTN_ACTION_SERVICE:
			debug_describe_P(COLOR_GREEN "!SERVICE!" COLOR_END);
		break;
	};

	switch (action) {
		case BTN_ACTION_RESET:
			if (factory_restore && !timer_is_start(&ap_blink)) {
				debug_describe_P("Can not restore - no settings save");
				break;
			}
			sleep_lock(SLEEP_RESET);
			peri_set_white(1);
			peri_lock_white(1);
			timer_notify(&reset_blink);
			timer_restart(&reset_blink);
		break;

		case BTN_ACTION_PRESS: {
			sleep_lock(SLEEP_PRESS);
#ifdef IQS
			iqs_inhibit = 1;
#endif
			press = 1;
			if (timer_is_start(&reset_blink)) {
				timer_stop(&reset_blink);
				timer_stop(&reset_timer);
				user_factory_reset();
			} else {
				peri_set_white(1);
			}
#ifdef IQS
			if (iqs_wheel_inhibit) {
				iqs_wheel_cancel();
				if (!factory_restore) {
					payload_action(payload, own_mac, BTN_ACTION_WHEEL_FINAL, 0);
				}
				iqs_wheel_inhibit = 0;
			}
#endif
		}
		break;

		case BTN_ACTION_RELEASE: {
			press = 0;
			sleep_unlock(SLEEP_PRESS);
			if (timer_is_start(&reset_blink)) {
				timer_restart(&reset_timer);
			}
		}
		break;

		case BTN_ACTION_SHORT:
		case BTN_ACTION_LONG:
		case BTN_ACTION_DOUBLE:
			if (service_mode) {
				sleep_reset();
			}
			if (!factory_restore) {
				return payload_action(payload, own_mac, action, 0);
			} else {
				if (action == BTN_ACTION_LONG) {
					return wps_abort();
				}
			}
		break;

		case BTN_ACTION_SERVICE:
			if (factory_restore) {
				break;
			}
			if (!service_mode) {
				service_mode = 1;
				sleep_inhibit(600000);
				sleep_reset();
				if (payload_connected(payload)) {
					collect_start(collect);
					user_http_run();
				}
				if (!timer_is_start(&service_timer)) {
					rgb_set_pattern(RGB_SERVICE_MODE, 0);
					timer_start(&service_timer);
				}
			} else {
				service_mode = 0;
				timer_stop(&service_timer);
				sleep_immediately();
			}
			break;

		default:
		break;
	}
	return 0;
}

static void ICACHE_FLASH_ATTR charge_action(uint8_t is_charge) {
	if (is_charge) {
		debug_describe_P("Charging");
	} else {
		sleep_unlock(SLEEP_CHARGE);
		debug_describe_P("Charger dissconect");
	}
}

static void ICACHE_FLASH_ATTR timer_run_task(os_timer_t* timer, void (*func)(void*), void* owner, uint32_t time) {
	if (!timer || !func) {
		return;
	}
	os_timer_disarm(timer);
	os_timer_setfn(timer, (os_timer_func_t *)func, owner);
	os_timer_arm(timer, time, 0);
}

static void ICACHE_FLASH_ATTR user_wifi_disconn(void) {
	debug_describe_P("!!!!DISCONN");
	sleep_lock(SLEEP_WIFI);
	sleep_unlock(SLEEP_DHCP);
	sleep_unlock(SLEEP_DELAY);
	timer_stop(&dhcp_timer);
	timer_stop(&conn_timer);
	payload_connect(payload, 0);
	collect_stop(collect);
	wifi_rtc_ip_erase();
}

static void ICACHE_FLASH_ATTR user_wifi_reconn(uint32_t time_ms) {
	static uint8_t try_again = 1;
	uint8_t is_charge = peri_is_charge();
	debug_value(system_get_free_heap_size());
	debug_value(time_ms);
	debug_describe_P("!!!!RECONN");
	timer_stop(&reconn_timer);
	if (reboot_req) {
		return;
	}
	if (periodic_wakeup) {
		is_charge = 0;
	}
	/*limit try connect*/
	if (!factory_restore &&
		!periodic_wakeup &&
		!cold_start &&
		!is_charge &&
		!service_mode) {
			if (!try_again) {
				sleep_unlock(SLEEP_WIFI);
				rgb_set_pattern(RGB_WPS_FAIL, 0);
				sleep_immediately();
				return;
			} else {
				try_again--;
			}
	} else {
		if (!timer_is_start(&ap_blink)) {
			if (!is_charge) {
				if (conn_limit) {
					conn_limit--;
				} else {
					sleep_unlock(SLEEP_WIFI);
					sleep_immediately();
					return;
				}
			}
		} else {
			sleep_unlock(SLEEP_WIFI);
			return;
		}
	}
	if (!time_ms) {
		timer_notify(&reconn_timer);
		return;
	}
	timer_set(&reconn_timer, time_ms / 10);
	timer_restart(&reconn_timer);
}

static void ICACHE_FLASH_ATTR user_on_conn(void* owner) {
	uint8_t is_charge = peri_is_charge();
	if (timer_is_start(&ap_blink)) {
		timer_stop(&ap_blink);
		peri_lock_white(0);
		peri_set_white(0);
	}
	if (periodic_wakeup) {
		is_charge = 0;
	}
	if (is_charge) {
		sleep_lock(SLEEP_CHARGE);
	}
	payload_connect(payload, 1);
	if (factory_restore || cold_start || is_charge || service_mode) {
		sleep_reset();
		user_http_run();
	}
	if (periodic_wakeup || factory_restore || cold_start || is_charge || service_mode) {
		payload_action(payload, own_mac, BTN_ACTION_BATTERY, peri_battery_raw());
		collect_start(collect);
		sleep_reset();
	}
	/*unlock factory mode after first connect*/
	factory_restore = 0;
	sleep_unlock(SLEEP_DELAY);
}

static void ICACHE_FLASH_ATTR user_wifi_dhcp(void) {
	struct ip_info info;
	debug_describe_P("!!!!DHCP SUCCESS");
	timer_stop(&dhcp_timer);
	memset(&info, 0, sizeof(info));
	if (wifi_get_ip_info(STATION_IF, &info)) {
		RTC_IP_T rtc_ip;
		debug_describe_P("Get AP info OK");
		memset(&rtc_ip, 0, sizeof(rtc_ip));
		rtc_ip.info = info;
		rtc_ip.dns1 = espconn_dns_getserver(0);
		rtc_ip.dns2 = espconn_dns_getserver(1);
		if (wifi_rtc_ip_write(&rtc_ip)) {
			debug_describe_P("RTC IP save");
		}
	}
		sleep_lock(SLEEP_DELAY);
		if (peri_rst_reason() == REASON_DEEP_SLEEP_AWAKE) {
			timer_restart(&conn_timer);
		} else {
			timer_notify(&conn_timer);
		}
	sleep_unlock(SLEEP_DHCP);
}

static void ICACHE_FLASH_ATTR user_dhcp_timeout(void* owner) {
	debug_describe_P("!!!!DHCP TIMEOUT");
	sleep_unlock(SLEEP_DHCP);
	sleep_immediately();
}

static void ICACHE_FLASH_ATTR user_wifi_conn(void) {
	debug_describe_P("!!!!CONNECTED");
	sleep_lock(SLEEP_DHCP);
	sleep_unlock(SLEEP_WIFI);
	timer_restart(&dhcp_timer);
}

static void ICACHE_FLASH_ATTR user_wps_status(WiFi_Wps_Status_T status) {
	sleep_reset();
	if (timer_is_start(&ap_blink)) {
		timer_stop(&ap_blink);
		peri_lock_white(0);
		peri_set_white(0);
	}
	switch (status) {
		case WIFI_WPS_STARTED:
			sleep_lock(SLEEP_WPS);
		break;

		case WIFI_WPS_SHORTEN:
			sleep_unlock(SLEEP_WIFI);
			sleep_unlock(SLEEP_WPS);
		break;

		case WIFI_WPS_FAIL:
			sleep_unlock(SLEEP_WIFI);
			sleep_unlock(SLEEP_WPS);
			sleep_immediately();
		break;

		case WIFI_WPS_SUCCESS:
			sleep_unlock(SLEEP_WPS);
		break;;
	}
}

static void ICACHE_FLASH_ATTR user_ap_start(void) {
	sleep_inhibit(AP_TIMEOUT);
	sleep_reset();
	user_http_run();
	peri_set_white(1);
	peri_lock_white(1);
	timer_notify(&ap_blink);
	timer_restart(&ap_blink);
	sleep_unlock(SLEEP_WIFI);
}

static void ICACHE_FLASH_ATTR user_new_conn(void) {
	if (timer_is_start(&ap_blink)) {
		timer_stop(&ap_blink);
		peri_lock_white(0);
		peri_set_white(0);
	}
}

static WiFi_Inh_T user_wifi_inh = {
	.on_disconn = user_wifi_disconn,
	.on_dhcp = user_wifi_dhcp,
	.on_conn = user_wifi_conn,
	.on_reconn = user_wifi_reconn,
	.wps_status = user_wps_status,
	.ap_start = user_ap_start,
	.new_conn = user_new_conn
};

static void ICACHE_FLASH_ATTR user_reboot_cb(void) {
	reboot_req = 1;
	collect_stop(collect);
	wifi_inhibit();
	//wifi_set_event_handler_cb(NULL);
	timer_stop(&reconn_timer);
	timer_stop(&conn_timer);
	//wifi_station_disconnect();
	//wifi_set_opmode(NULL_MODE);
}

static uint8_t ICACHE_FLASH_ATTR user_sleep_final(uint8_t final) {
	debug_printf(COLOR_MAGENTA "Sleep final %d\n" COLOR_END, (int)final);
	if (final) {
		user_reboot_cb();
		wifi_inhibit();
		user_timers_deinit();
		sleep_deinit();
		peri_deinit();
	} else {
		if (timer_is_start(&ap_blink)) {
			timer_stop(&ap_blink);
			peri_lock_white(0);
			peri_set_white(0);
		}
		collect_stop(collect);
	}
	return 1;
}

static void ICACHE_FLASH_ATTR handle_rst(void) {
	peri_pre_init();
	system_update_cpu_freq(SYS_CPU_160MHZ);
#ifdef DEBUG_PRINTF
		uart_div_modify(0, UART_CLK_FREQ / 460800);
#endif
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void) {
	system_phy_freq_trace_enable(0);
	peri_pre_init();
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

static void ICACHE_FLASH_ATTR user_print_version(const char* own_mac) {
	if (!own_mac) {
		return;
	}
#ifdef IQS
	os_printf(COLOR_CYAN "Button Plus: %s, MAC: %s\n" COLOR_END, APP_VERSION, own_mac);
#else
	os_printf(COLOR_CYAN "Button Simple: %s, MAC: %s\n" COLOR_END, APP_VERSION, own_mac);
#endif
}

static void ICACHE_FLASH_ATTR user_timers_clk(void* owner) {
#ifdef DEBUG_PRINTF
	static uint32_t div = 0;
	if (!div) {
		debug_value(system_get_free_heap_size());
	}
	div++;
	div %= 500;
#endif
	timer_clk(&reconn_timer);
	timer_clk(&conn_timer);
	timer_clk(&action_timer);
	timer_clk(&dhcp_timer);
	timer_clk(&ap_blink);
	timer_clk(&reset_blink);
	timer_clk(&reset_timer);
	timer_clk(&service_timer);

	if (timer_event(&reconn_timer)) {
		if (!periodic_wakeup && !store.bssid_enable) {
			wifi_set_immediate_connect(1);
		}
		wifi_connect(NULL);
	}
	if (timer_event(&conn_timer)) {
		user_on_conn(NULL);
	}
	if (timer_event(&action_timer)) {
		Btn_Action_T action;
		while (queue_read(queue, &action)) {
			btn_action_flash(action);
		}
	}
	if (timer_event(&dhcp_timer)) {
		user_dhcp_timeout(NULL);
	}
	if (timer_event(&ap_blink)) {
		if (!timer_is_start(&reset_blink)) {
			peri_blink(1, 0, 0, 2, 1);
		}
	}
	if (timer_event(&reset_blink)) {
		peri_blink(1, 0, 0, 8, 1);
	}
	if (timer_event(&reset_timer)) {
		user_reset_timeout(NULL);
	}
	if (timer_event(&service_timer)) {
		rgb_set_pattern(RGB_SERVICE_MODE, 0);
		timer_restart(&service_timer);
	}
}

static void ICACHE_FLASH_ATTR user_timers_init(void) {
	timer_init(&reconn_timer, 50, NULL, NULL);
	timer_init(&conn_timer, 51, NULL, NULL);
	timer_init(&action_timer, 1, NULL, NULL);
	timer_repeat(&action_timer, 1);
	timer_init(&dhcp_timer, DHCP_TIMEOUT / 10, NULL, NULL);
	timer_init(&ap_blink, AP_BLINK_PERIOD / 10, NULL, NULL);
	timer_repeat(&ap_blink, 1);
	timer_init(&reset_blink, 45, NULL, NULL);
	timer_repeat(&reset_blink, 1);
	timer_init(&reset_timer, 500, NULL, NULL);
	timer_init(&service_timer, 500, NULL, NULL);

	os_timer_disarm(&general_timer);
	os_timer_setfn(&general_timer, user_timers_clk, NULL);
	os_timer_arm(&general_timer, 10, 1);
}

static void ICACHE_FLASH_ATTR user_timers_deinit(void) {
	os_timer_disarm(&general_timer);
	timer_stop(&reconn_timer);
	timer_stop(&conn_timer);
	timer_stop(&action_timer);
	timer_stop(&dhcp_timer);
	timer_stop(&ap_blink);
	timer_stop(&reset_blink);
	timer_stop(&reset_timer);
	timer_stop(&service_timer);
}

void ICACHE_FLASH_ATTR user_init(void) {
	uint8_t mac[6];
	handle_rst();
	hold = peri_hold_on_start();
	url_storage_init(&url_storage, 0xF8);
	store_init();
	debug_describe_P("Program start...");
	debug_value(hold);
	debug_value(system_get_free_heap_size());
	queue = queue_new(sizeof(Btn_Action_T), 10);
	sleep_init(550, user_sleep_final);
	peri_set_btn_cb(btn_action);
	peri_set_is_charge_cb(charge_action);
#ifdef IQS
	iqs_set_wakeup_cb(user_wakeup);
	iqs_set_release_cb(iqs_release);
	iqs_set_press_cb(iqs_press);
	iqs_set_wheel_cb(iqs_wheel);
#endif
	peri_init();
	ns = ns_new();
	if (ns) {
		debug_describe_P("Notify service created");
	}
	memset(own_mac, 0, sizeof(own_mac));
	memset(mac, 0, sizeof(mac));
	wifi_get_macaddr(STATION_IF, mac);
	utils_mac_bytes_to_string((uint8_t*)own_mac, mac);
	boot_init();
	user_print_version(own_mac);
	os_printf(COLOR_CYAN "RST info: %u\n" COLOR_END, peri_rst_reason());
	if (reboot_req) {
		return;
	}
	wifi_init(&user_wifi_inh);
	if ((collect = collect_new())) {
		debug_describe_P(COLOR_GREEN "Collect service created" COLOR_END);
	}
	if ((payload = payload_new())) {
		debug_describe_P(COLOR_GREEN "Payload created" COLOR_END);
	}
	user_timers_init();
	if (!wifi_is_save()) {
		debug_describe_P("FACTORY DEFAULT");
		factory_restore = 1;
	}
	if ((peri_rst_reason() == REASON_DEFAULT_RST) ||
		(peri_rst_reason() == REASON_EXT_SYS_RST)) {
			debug_describe_P("COLD START");
			cold_start = 1;
	}
	if (cold_start || factory_restore) {
		rtc_erase_all();
		sleep_inhibit(GENERAL_INHIBIT_TIME);
		sleep_reset();
		sleep_lock(SLEEP_WIFI);
		timer_restart(&reconn_timer);
		timer_restart(&action_timer);
	} else {
#ifndef IQS
		user_wakeup(peri_rst_reason() == REASON_DEEP_SLEEP_AWAKE, 0);
#endif
	}
}
