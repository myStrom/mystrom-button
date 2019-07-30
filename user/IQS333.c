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


#ifdef IQS
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>
#include <ets_sys.h>
#include <c_types.h>
#include "IQS333.h"
#include "peri.h"
#include "debug.h"
#include "array_size.h"
#include "color.h"
#include "json.h"
#include "buffer.h"
#include "list.h"
#include "rule.h"
#include "wifi.h"
#include "store.h"

typedef struct {
	uint8_t command;
	uint8_t len;
	uint8_t* data;
} IQS_Settings_T;

extern struct Store_T store;

static uint8_t wheel_cancel = 0;

static void ICACHE_FLASH_ATTR iqs_version_done(I2C_Order_T order, I2C_Result_T result);
static void ICACHE_FLASH_ATTR iqs_generic_done(I2C_Order_T order, I2C_Result_T result);
static void ICACHE_FLASH_ATTR iqs_mode_done(I2C_Order_T order, I2C_Result_T result);
static void ICACHE_FLASH_ATTR iqs_status_done(I2C_Order_T order, I2C_Result_T result);
static void ICACHE_FLASH_ATTR iqs_timer_start(void);
static void ICACHE_FLASH_ATTR iqs_exit_event_mode_done(I2C_Order_T order, I2C_Result_T result);

#ifdef DEBUG_PRINTF
static const char* iqs_flags_desc[] = {
	"Zoon",
	"Noise detected",
	"ATI Busy",
	"LP Active",
	"Is Ch0",
	"Proj Mode",
	"Filter Halted",
	"Show reset"
};
#endif

static const IQS_Settings_T iqs_settings[] = {
	{PROXSETTINGS, 6, (uint8_t[]){0x06, 0x00, 0x00, 0x05, 0x07, 0x7F}},
	{ACTIVE_CHANNELS, 2, (uint8_t[]){0x8F, 0x00}},
	{MULTIPLIERS, 10, (uint8_t[]){0x3A, 0x28, 0x29, 0x28, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00}},
	{THRESHOLDS, 10, (uint8_t[]){0x07, 0x06, 0x06, 0x06, 0x0F, 0x0F, 0x0F, 0x06, 0x0F, 0x0F}},
	{TIMINGS, 5, (uint8_t[]){0x14, 0x00, 0x10, 0x02, 0x02}},
	{ATI_TARGETS, 2, (uint8_t[]){0x40, 0x40}},
};

static const uint8_t iqs_thresholds_default[] = 
	(uint8_t[]){0x07, 0x06, 0x06, 0x06, 0x0F, 0x0F, 0x0F, 0x0B, 0x0F, 0x0F};

static const IQS_Settings_T iqs_status[] = {
	{TOUCH_BYTES, 1, NULL},
	{COUNTS, 2, NULL},
	{LTA, 2, NULL},
	//{MULTIPLIERS, 1, NULL},
	//{COMPENSATION, 1, NULL},
};

#define IQS_VERSION_ORDER (struct I2C_Order_T) {\
	.address = IQS333_ADDR,\
	.dir = I2C_DIR_READ,\
	.command = VERSION_INFO,\
	.len = 2,\
	.timeout = 10,\
	.owner = NULL,\
	.done_cb = iqs_version_done\
}

#define IQS_MODE_ORDER (struct I2C_Order_T) {\
	.address = IQS333_ADDR,\
	.dir = I2C_DIR_WRITE,\
	.command = PROXSETTINGS,\
	.len = 3,\
	.timeout = 10,\
	.data = {0x06, 0x00, 0x04},\
	.owner = NULL,\
	.done_cb = iqs_mode_done\
}

#define IQS_EXIT_EVENT_MODE_ORDER (struct I2C_Order_T) { \
	.address = IQS333_ADDR,\
	.dir = I2C_DIR_WRITE,\
	.command = PROXSETTINGS,\
	.len = 3,\
	.timeout = 10,\
	.data = {0x06, 0x00, 0x00},\
	.owner = NULL,\
	.done_cb = iqs_exit_event_mode_done\
}

static void (*iqs_press_cb)(void) = NULL;
static void (*iqs_release_cb)(uint8_t) = NULL;
static void (*iqs_wakeup_cb)(uint8_t, uint8_t) = NULL;
static uint8_t (*iqs_wheel_cb)(uint16_t, int16_t) = NULL;
static os_timer_t iqs_timer;
static uint8_t redo_ati = 0;

uint8_t ICACHE_FLASH_ATTR iqs_read(uint8_t cmd, uint8_t len, I2C_Done_Cb done_cb) {
	I2C_Order_T order = NULL;
	if (!done_cb || !len) {
		return 0;
	}
	if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
		return 0;
	}
	order->address = IQS333_ADDR,
	order->dir = I2C_DIR_READ,
	order->command = cmd,
	order->len = len,
	order->timeout = 10,
	order->owner = NULL,
	order->done_cb = done_cb;
	if (!peri_order(&order)) {
		free(order);
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR iqs_write(uint8_t cmd, uint8_t* data, uint8_t len, I2C_Done_Cb done_cb) {
	I2C_Order_T order = NULL;
	if (!len || !data || (len > sizeof(order->data))) {
		return 0;
	}
	if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
		return 0;
	}
	order->address = IQS333_ADDR,
	order->dir = I2C_DIR_WRITE,
	order->command = cmd,
	order->len = len,
	memcpy(order->data, data, len);
	order->timeout = 10,
	order->owner = NULL,
	order->done_cb = done_cb;
	if (!peri_order(&order)) {
		free(order);
		return 0;
	}
	return 1;
}

static void ICACHE_FLASH_ATTR iqs_generic_done(I2C_Order_T order, I2C_Result_T result) {
	if (!order) {
		return;
	}
	debug_printf("Result for %u, %s\n", (uint32_t)order->command, (order->dir == I2C_DIR_READ) ? "read" : "write");
	switch (result) {
		case I2C_RES_ERR:
			debug_describe_P("ERROR");
			break;
		case I2C_RES_OK:
			debug_describe_P("OK");
			break;
		case I2C_RES_TIME:
			debug_describe_P("TIMEOUT");
			break;
	}
	if (result != I2C_RES_OK) {
		return;
	}
	if (order->dir == I2C_DIR_READ) {
		uint16_t i;
		for (i = 0; i < order->len; i++) {
			debug_printf("0x%02x, ", order->data[i]);
		}
		debug_describe_P("");
	}
}

static void ICACHE_FLASH_ATTR iqs_error_done(I2C_Order_T order, I2C_Result_T result) {
	I2C_Order_T mode = NULL;
	if (!order) {
		return;
	}
	iqs_generic_done(order, result);
	if (order->data[1] & 0x02) {
		debug_describe_P("ATI error");
	} else {
		debug_describe_P("ATI OK");
	}
	if (!(mode = calloc(1, sizeof(struct I2C_Order_T)))) {
		return;
	}
	*mode = IQS_MODE_ORDER;
	if (!peri_order(&mode)) {
		free(mode);
	}
}

static void ICACHE_FLASH_ATTR iqs_ati_done(I2C_Order_T order, I2C_Result_T result) {
	if (!order) {
		return;
	}
	iqs_generic_done(order, result);
	if (result != I2C_RES_OK) {
		iqs_read(FLAGS, 1, iqs_ati_done);
		return;
	}
	if (order->data[0] & 0x04) {
		debug_describe_P("ATI busy");
		iqs_read(FLAGS, 1, iqs_ati_done);
		return;
	}
	debug_describe_P("ATI done");
	iqs_read(PROXSETTINGS, 2, iqs_error_done);
}

static void ICACHE_FLASH_ATTR iqs_version_done(I2C_Order_T ret_order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		I2C_Order_T version = NULL;
		debug_describe_P(COLOR_RED "Can not read IQS version" COLOR_END);
		if (!(version = calloc(1, sizeof(struct I2C_Order_T)))) {
			return;
		}
		*version = IQS_VERSION_ORDER;
		if (!peri_order(&version)) {
			free(version);
		}
		return;
	}
	if (!peri_is_event_mode()) {
		/*iqs set settings*/
		iqs_write(iqs_settings[0].command, iqs_settings[0].data, iqs_settings[0].len, iqs_generic_done);
		iqs_write(iqs_settings[1].command, iqs_settings[1].data, iqs_settings[1].len, iqs_generic_done);
		iqs_write(iqs_settings[2].command, iqs_settings[2].data, iqs_settings[2].len, iqs_generic_done);
		iqs_write(iqs_settings[3].command, store.thresholds, ARRAY_SIZE(store.thresholds), iqs_generic_done);
		iqs_write(iqs_settings[4].command, iqs_settings[4].data, iqs_settings[4].len, iqs_generic_done);
		iqs_write(iqs_settings[5].command, iqs_settings[5].data, iqs_settings[5].len, iqs_generic_done);
		if (redo_ati) {
			iqs_write(PROXSETTINGS, (uint8_t[]){0x16}, 1, NULL);
			redo_ati = 0;
		}
		iqs_read(FLAGS, 1, iqs_ati_done);
	}
}

static void ICACHE_FLASH_ATTR iqs_mode_done(I2C_Order_T order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		I2C_Order_T mode = NULL;
		debug_describe_P(COLOR_RED "Can not switch to event mode" COLOR_END);
		if (!(mode = calloc(1, sizeof(struct I2C_Order_T)))) {
			return;
		}
		*mode = IQS_MODE_ORDER;
		if (!peri_order(&mode)) {
			free(mode);
		}
		return;
	}
	peri_set_event_mode(1);
	peri_set_rdy_cb(iqs_status_order);
	debug_describe_P(COLOR_GREEN "IQS enter event mode" COLOR_END);
	iqs_timer_start();
}

static void ICACHE_FLASH_ATTR iqs_exit_event_mode_done(I2C_Order_T order, I2C_Result_T result) {
	I2C_Order_T version = NULL;
	if (result != I2C_RES_OK) {
		I2C_Order_T event = NULL;
		debug_describe_P(COLOR_RED "Can not exit from event mode" COLOR_END);
		if (!(event = calloc(1, sizeof(struct I2C_Order_T)))) {
			return;
		}
		*event = IQS_EXIT_EVENT_MODE_ORDER;
		if (!peri_order(&event)) {
			free(event);
		}
		return;
	}
	debug_describe_P(COLOR_GREEN "IQS exit event mode" COLOR_END);
	peri_set_event_mode(0);
	if (!(version = calloc(1, sizeof(struct I2C_Order_T)))) {
		return;
	}
	*version = IQS_VERSION_ORDER;
	if (!peri_order(&version)) {
		free(version);
	}
}

static void ICACHE_FLASH_ATTR iqs_pwm_order_done(I2C_Order_T order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		debug_describe_P(COLOR_RED "PWM ERR" COLOR_END);
	}
#ifdef IQS
	pin_set(WHITE_LED_GPIO, 0);
#endif
}

static uint16_t ICACHE_FLASH_ATTR iqs_read_position(uint8_t* data, uint8_t offset) {
	if (!data) {
		return 0;
	}
	return ((uint16_t)data[offset + 1] << 8) | data[offset];
}

static uint8_t ICACHE_FLASH_ATTR iqs_is_center_pad(uint8_t* data, uint8_t offset) {
	uint32_t ch = 0;
	if (!data) {
		return 0;
	}
	ch = ((uint32_t)data[offset + 1] << 8) | data[offset];
	if (ch & 0x80) {
		return 1;
	}
	return 0;
}

static uint16_t ICACHE_FLASH_ATTR iqs_touch_channels(uint8_t* data, uint8_t offset) {
	if (!data) {
		return 0;
	}
	return ((uint16_t)data[offset + 1] << 8) | data[offset];
}

static void ICACHE_FLASH_ATTR iqs_status_read(uint8_t* data) {
	static uint32_t total_count = 0;
	static uint16_t prev_position = 0;
	static int16_t position_diff = 0;
	static uint8_t first = 0;
	static uint8_t press = 0;
	static uint8_t ati_busy = 0;
	static uint8_t wheel = 0;
	uint16_t position = 0;
	uint8_t flags = 0;
	int8_t i;
	if (!data) {
		return;
	}
	flags = data[0];
	position = iqs_read_position(data, 1);
	if (!flags && !first) {
		debug_describe_P(COLOR_MAGENTA "TIMER" COLOR_END);
	}
	if (wheel_cancel) {
		wheel = 0;
		prev_position = position;
		total_count = 0;
		position_diff = 0;
		wheel_cancel = 0;
	}
	if (!first && iqs_wakeup_cb && wifi_is_save()) {
		if ((peri_rst_reason() != REASON_DEFAULT_RST) &&
			(peri_rst_reason() != REASON_EXT_SYS_RST)) {
			if (peri_rst_reason() == REASON_DEEP_SLEEP_AWAKE) {
				uint8_t wakeup;
				switch (store.veryfication) {
					case Veryfication_Double:
						wakeup = (flags == 0x41) && 
								(iqs_touch_channels(data, 1 + 4) & 0x8E);
						break;
					case Veryfication_Single:
						wakeup = ((flags & 0x41) == flags) && flags;
						break;
					default:
						wakeup = 0;
						break;
				}
				iqs_wakeup_cb(!flags, wakeup);
				if (wakeup) {
					debug_describe_P("IQS WAKE PRESS");
					press = 1;
					prev_position = position;
					if (iqs_press_cb) {
						iqs_press_cb();
					}
				}
			} else {
				iqs_wakeup_cb(0, 0);
			}
		}
	}
	if (flags) {
		for (i = 7; i >= 0; i--) {
			if (flags & (1 << i)) {
				debug_printf(" %s, ", iqs_flags_desc[i]);
			}
		}
		debug_describe_P("");/*new line*/
		/*check if press*/
		if (flags & 0x80) {
			if (peri_is_event_mode()) {
				I2C_Order_T event = NULL;
				debug_describe_P(COLOR_RED "RELOAD IQS CONFIG" COLOR_END);
				if (!(event = calloc(1, sizeof(struct I2C_Order_T)))) {
					return;
				}
				*event = IQS_EXIT_EVENT_MODE_ORDER;
				if (!(peri_order(&event))) {
					free(event);
				}
			}
		} else {
			if (flags & 0x04) {
				if (press) {
					ati_busy = 1;
				}
			} else {
				if ((flags & 0x40) && (iqs_touch_channels(data, 1 + 4) & 0x8E)) {
					if (!press) {
						debug_describe_P("IQS PRESS");
						press = 1;
						prev_position = position;
						total_count = 0;
						position_diff = 0;
						if (iqs_press_cb) {
							iqs_press_cb();
						}
					}
				} else {
					if ((flags & 0x01) == flags) {
						if (!wheel) {
							if (press) {
								debug_describe_P("IQS RELEASE");
								press = 0;
								if (iqs_release_cb) {
									iqs_release_cb(ati_busy);
								}
								ati_busy = 0;
							}
						}
					}
				}
			}
		}
	} else {
		if (press) {
			debug_describe_P("IQS ALT RELEASE");
			press = 0;
			wheel = 0;
			if (iqs_release_cb) {
				iqs_release_cb(ati_busy);
			}
			ati_busy = 0;
		}
	}
	if (press) {
		if (!ati_busy) {
			uint16_t channels = iqs_touch_channels(data, 1 + 4);
			if (channels && ((channels & 0x0E) == (channels & 0x8E))) {
				int16_t diff = position - prev_position;
				if (diff > 1023) {
					diff -= 2048;
				}
				if (diff < -1023) {
					diff += 2048;
				}
				if (abs(diff) > 4) {
					position_diff += diff;
				}
				debug_printf("Wheel: %d, diff: %d\n", (int)position, (int)diff);
				prev_position = position;
				total_count++;
				if ((total_count >= 8) && (abs(position_diff) >= (128 - ((uint32_t)wheel * 112)))) {
					if (iqs_wheel_cb) {
						debug_printf("Count: %d, diff: %d\n", (int)total_count, (int)position_diff);
						if (iqs_wheel_cb(total_count, position_diff)) {
							total_count = 0;
							position_diff = 0;
							wheel = 1;
						}
					}
				}
			}
		} else {
			prev_position = position;
			total_count = 0;
			position_diff = 0;
			wheel = 0;
		}
	}
	first = 1;
}

static void ICACHE_FLASH_ATTR iqs_touch_read(uint8_t* data) {
	uint32_t touch = 0;
	int32_t i;
	if (!data) {
		return;
	}
	touch = ((uint32_t)data[1] << 8) | data[0];
	if (touch) {
		for (i = 9; i >= 0; i--) {
			if (touch & (1 << i)) {
				debug_printf("CH: %d, ", (int)i);
			}
		}
		debug_describe_P("");/*new line*/
	}
}

static void ICACHE_FLASH_ATTR iqs_wheel_read(uint8_t* data) {
	static int32_t last_wheel = 0;
	int32_t wheel = 0;
	if (!data) {
		return;
	}
	wheel = ((uint32_t)data[1] << 8) | data[0];
	if (wheel != last_wheel) {
		debug_printf("Wheel: %d, diff: %d\n", wheel, (int)(wheel - last_wheel));
	}
	last_wheel = wheel;
}

static void ICACHE_FLASH_ATTR iqs_status_done(I2C_Order_T order, I2C_Result_T result) {
	if (!order) {
		return;
	}
	if (result != I2C_RES_OK) {
		debug_printf(COLOR_RED "IQS serial read error: %u\n" COLOR_END, (uint32_t)result);
		goto exit;
	}
	iqs_status_read(order->data);
	//iqs_wheel_read(order->data + 1);
	iqs_touch_read(order->data + 1 + 4);
exit:
	return;
}

void static ICACHE_FLASH_ATTR iqs_power_done(I2C_Order_T order, I2C_Result_T result) {
	if (result != I2C_RES_OK) {
		debug_describe_P(COLOR_RED "Can not switch power mode" COLOR_END);
		iqs_power_mode(0, iqs_power_done);
		return;
	}
	debug_describe_P(COLOR_GREEN "IQS exit power save" COLOR_END);
	if ((peri_rst_reason() == REASON_SOFT_RESTART) && peri_is_event_mode()) {
		I2C_Order_T event = NULL;
		debug_describe_P(COLOR_YELLOW "RELOAD IQS CONFIG" COLOR_END);
		redo_ati = 1;
		if (!(event = calloc(1, sizeof(struct I2C_Order_T)))) {
			return;
		}
		*event = IQS_EXIT_EVENT_MODE_ORDER;
		if (!peri_order(&event)) {
			free(event);
		}
	} else {
		I2C_Order_T status = NULL;
		peri_set_rdy_cb(iqs_status_order);
		if (!(status = calloc(1, sizeof(struct I2C_Order_T)))) {
			return;
		}
		iqs_status_order(status);
		status->timeout = 100;
		if (!peri_order(&status)) {
			free(status);
		}
		iqs_timer_start();
	}
}

static uint8_t ICACHE_FLASH_ATTR iqs_values_order(void) {
	I2C_Order_T order = NULL;
	uint16_t i;
	for (i = 0; i < ARRAY_SIZE(iqs_status); i++) {
		if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
			return 0;
		}
		order->address = IQS333_ADDR;
		order->command = iqs_status[i].command;
		order->len = iqs_status[i].len;
		order->dir = I2C_DIR_READ;
		order->timeout = 10;
		order->owner = NULL;
		order->done_cb = iqs_generic_done;
		if (!peri_order(&order)) {
			free(order);
			return 0;
		}
	}
	return 1;
}

static void ICACHE_FLASH_ATTR iqs_timer_clk(void* owner) {
	I2C_Order_T order = calloc(1, sizeof(struct I2C_Order_T));
	if (!order) {
		return;
	}
	iqs_status_order(order);
	if (!peri_order_low_priority(&order)) {
		free(order);
	}
}

static void ICACHE_FLASH_ATTR iqs_timer_start(void) {
	os_timer_disarm(&iqs_timer);
	os_timer_setfn(&iqs_timer, iqs_timer_clk, NULL);
	os_timer_arm(&iqs_timer, 1000, 1);
}

void ICACHE_FLASH_ATTR iqs_timer_stop(void) {
	os_timer_disarm(&iqs_timer);
}

void ICACHE_FLASH_ATTR iqs_init(void) {
	if (!store.veryfication) {
		store.veryfication = Veryfication_Single;
		memcpy(store.thresholds, iqs_thresholds_default, ARRAY_SIZE(store.thresholds));
		store_save();
	}
	os_timer_disarm(&iqs_timer);
	if (!peri_is_event_mode()) {
		I2C_Order_T version = calloc(1, sizeof(struct I2C_Order_T));
		if (!version) {
			return;
		}
		*version = IQS_VERSION_ORDER;
		if (!peri_order(&version)) {
			free(version);
		}
	} else {
		iqs_power_mode(0, iqs_power_done);
	}
}

uint8_t ICACHE_FLASH_ATTR iqs_reload(void) {
	if (peri_is_event_mode()) {
		I2C_Order_T event = calloc(1, sizeof(struct I2C_Order_T));
		if (!event) {
			return 0;
		}
		debug_describe_P(COLOR_YELLOW "RELOAD IQS CONFIG" COLOR_END);
		redo_ati = 1;
		*event = IQS_EXIT_EVENT_MODE_ORDER;
		if (!peri_order(&event)) {
			free(event);
			return 0;
		}
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR iqs_status_order(I2C_Order_T order) {
	if (!order) {
		return 0;
	}
	memset(order, 0, sizeof(struct I2C_Order_T));
	order->address = IQS333_ADDR;
	order->command = FLAGS;
	order->len = 0;
	order->dir = I2C_DIR_READ_SERIAL;
	memset(order->serial_len, 0, sizeof(order->serial_len));
	order->serial_len[0] = 1;//FLAGS
	order->serial_len[1] = 4;//WHEEL_COORDS
	order->serial_len[2] = 2;//TOUCH_BYTES
	order->timeout = 10;
	order->repeat = 10;
	order->owner = NULL;
	order->done_cb = iqs_status_done;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR iqs_led_off_order(I2C_Done_Cb done_cb) {
	I2C_Order_T order;
	uint8_t i;
	if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
		return 0;
	}
	order->address = IQS333_ADDR;
	order->command = PWM;
	order->len = 3;
	for (i = 0; i < 3; i++) {
		order->data[i] = 0;
	}
	order->dir = I2C_DIR_WRITE;
	order->timeout = 10;
	order->repeat = 10;
	order->owner = NULL;
	order->done_cb = done_cb;
#ifdef IQS
	pin_set(WHITE_LED_GPIO, 0);
#endif
	if (!peri_order(&order)) {
		free(order);
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR iqs_led_pwm_order(uint16_t* values) {
	I2C_Order_T order;
	if (!values) {
		return 0;
	}
	if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
		return 0;
	}
	order->address = IQS333_ADDR;
	order->command = PWM;
	order->len = 3;
	order->data[PWM_R_CH] = (values[0] & 0x1F) | 0x20;
	order->data[PWM_W_CH] = (values[1] & 0x1F) | 0x20;
	order->data[PWM_G_CH] = (values[2] & 0x1F) | 0x20;
	order->dir = I2C_DIR_WRITE;
	order->timeout = 10;
	order->repeat = 10;
	order->owner = NULL;
	order->done_cb = iqs_pwm_order_done;
	if (!peri_order(&order)) {
		free(order);
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR iqs_power_mode(uint8_t save, I2C_Done_Cb done_cb) {
	I2C_Order_T order;
	if (!(order = calloc(1, sizeof(struct I2C_Order_T)))) {
		return 0;
	}
	order->address = IQS333_ADDR;
	order->command = TIMINGS;
	order->len = 5;
	order->data[0] = 0x14;
	order->data[1] = save ? 0x04 : 0x00;
	order->data[2] = 0x10;
	order->data[3] = 0x02;
	order->data[4] = 0x02;
	order->dir = I2C_DIR_WRITE;
	order->owner = NULL;
	order->done_cb = done_cb;
	order->timeout = 20;
	order->repeat = 255;
	if (!peri_order(&order)) {
		free(order);
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR iqs_soft_reset(I2C_Done_Cb done_cb) {
	return iqs_write(PROXSETTINGS, (uint8_t[]){0x06, 0x00, 0x80}, 3, done_cb);
}

void ICACHE_FLASH_ATTR iqs_set_press_cb(void (*cb)(void)) {
	iqs_press_cb = cb;
}

void ICACHE_FLASH_ATTR iqs_set_release_cb(void (*cb)(uint8_t busy)) {
	iqs_release_cb = cb;
}

void ICACHE_FLASH_ATTR iqs_set_wakeup_cb(void (*cb)(uint8_t timer, uint8_t status)) {
	iqs_wakeup_cb = cb;
}

void ICACHE_FLASH_ATTR iqs_set_wheel_cb(uint8_t (*cb)(uint16_t count, int16_t diff)) {
	iqs_wheel_cb = cb;
}

void ICACHE_FLASH_ATTR iqs_wheel_cancel(void) {
	wheel_cancel = 1;
}

#endif
