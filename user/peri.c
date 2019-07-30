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
#include <gpio.h>
#include <ets_sys.h>
#include <c_types.h>
#include "peri.h"
#include "queue.h"
#include "pin.h"
#include "debug.h"
#include "hw_timer.h"
#include "array_size.h"
#include "sleep.h"
#include "color.h"
#include "rtc.h"
#include "url_storage.h"
#include "rgb.h"
#ifdef IQS
#include "i2c.h"
#include "IQS333.h"
#endif

#define SPEED_BASE 128
#define MAX_SPEED 8
#define MAX_REPEAT 10

#ifdef IQS
	#define ADC_R1 330
	#define ADC_R2 82
	#define BATTERY_MAX 4500
	#define BATTERY_MIN 3000
#else
#define ADC_R1 2700
#define ADC_R2 820
#define BATTERY_MAX 4300
#define BATTERY_MIN 3700
#endif

#define BTN_PERIOD_DEBOUNCE (1)
#define BTN_PERIOD_SHORT (400)
#define BTN_PERIOD_LONG (1000)
#define BTN_PERIOD_SERVICE (5000)
#define BTN_PERIOD_RESET (10000)

enum {
	LED_R = 0,
	LED_W = 1,
	LED_G = 2,
	__LED_MAX
};

#ifdef IQS
static void ICACHE_FLASH_ATTR peri_i2c_clk(void* owner);
#endif
static void ICACHE_FLASH_ATTR peri_led_clk(void* owner);

extern uint32_t timer_ticks;
extern Url_Storage_T url_storage;
static Queue_T led_queue = NULL;
static os_timer_t rdy_timer;
static uint8_t event_mode = 0;
static void (*peri_btn_event_fn)(Btn_Action_T action);
static void (*peri_charge_event_fn)(uint8_t is_charge);
static struct Led_T rgb_leds[PERI_LED_COUNT];
static uint8_t peri_charge = 0;
static uint8_t peri_btn_press_on_start = 0;
static uint8_t peri_white_enabled = 0;
static uint8_t peri_white_locked = 0;
static uint16_t peri_btn_was_pressed = 0;
#ifdef IQS
static struct I2C_T i2c;
static Queue_T queue = NULL;
static uint8_t (*peri_rdy_event_fn)(I2C_Order_T order);
#else
static uint16_t peri_pwm_duty[PERI_LED_COUNT] = {0, 0, 0};
static uint8_t peri_pwm_lock = 0;
#endif

static const Led_Patterns_T rgb_led_patterns[] = {
	[RGB_WPS_SUCCESS] = {
		.speed = 512,
		.repeat = 3,
		.bypass = {1, 1, 0, 1}
	},
	[RGB_RESTORE] = {
		.speed = 683,
		.repeat = 16,
		.bypass = {1, 0, 1, 1}
	},
	[RGB_WPS_FAIL] = {
		.speed = 512,
		.repeat = 3,
		.bypass = {0, 1, 1, 1}
	},
	[RGB_SERVICE_MODE] = {
		.speed = 1024,
		.repeat = 1,
		.bypass = {0, 1, 0, 1}
	},
	[RGB_SOFT_RESET] = {
		.speed = 256,
		.repeat = 3,
		.bypass = {0, 1, 0, 1}
	},
	[RGB_WPS_START] = {
		.speed = 512,
		.repeat = 1,
		.bypass = {1, 0, 1, 1}
	},
	[RGB_SC_START] = {
		.speed = 512,
		.repeat = 2,
		.bypass = {0, 0, 0, 1}
	},
	[RGB_CONN_FAIL] = {
		.speed = 512,
		.repeat = 2,
		.bypass = {0, 1, 1, 1}
	},
};

#ifdef IQS
static u8 ICACHE_FLASH_ATTR i2c_inh_init(void* owner) {
	pin_init(I2C_SDA_GPIO, PIN_MODE_OUT, PIN_OTYPE_OD, PIN_PUPD_NOPULL, 1);
	pin_init(I2C_SCL_GPIO, PIN_MODE_OUT, PIN_OTYPE_OD, PIN_PUPD_NOPULL, 1);
	return 1;
}

static void ICACHE_FLASH_ATTR i2c_inh_deinit(void* owner) {
	
}

static void ICACHE_FLASH_ATTR i2c_inh_write(void* owner, u8 sda) {
	pin_set(I2C_SDA_GPIO, sda);
}

static u8 ICACHE_FLASH_ATTR i2c_inh_read(void* owner) {
	return pin_get(I2C_SDA_GPIO);
}

static u8 ICACHE_FLASH_ATTR i2c_inh_busy(void* owner) {
	if (!pin_get(I2C_SCL_GPIO)) {
		return 1;
	}
	return 0;
}

static void ICACHE_FLASH_ATTR i2c_inh_clk(void* owner, u8 clk) {
	pin_set(I2C_SCL_GPIO, clk);
}

static void ICACHE_FLASH_ATTR ICACHE_FLASH_ATTR i2c_inh_wait(void* owner) {
	os_delay_us(10);
}

static const I2C_Inh_T i2c_inh = {
	.init = i2c_inh_init,
	.write = i2c_inh_write,
	.read = i2c_inh_read,
	.clk = i2c_inh_clk,
	.wait = i2c_inh_wait,
	.deinit = i2c_inh_deinit,
	.busy = i2c_inh_busy,
};
#endif

static void peri_btn_action(Btn_Action_T action) {
	if (peri_btn_event_fn) {
		peri_btn_event_fn(action);
	}
}

static uint8_t peri_btn_pin_get(void) {
	static uint8_t reset = 0;
	static uint8_t ret_val = 0;
	if (peri_btn_was_pressed) {
		peri_btn_was_pressed--;
		return 0;
	}
	if (!reset) {
		ret_val = pin_get(BTN_GPIO);
	}
	pin_set(BTN_GPIO, reset);
	reset = 1 - reset;
	return ret_val;
}

static void peri_check_button(void) {
	static uint32_t press_period = 0;
	static uint32_t release_period = 0;
	static uint32_t debounce_cunter = 0;
	static uint32_t last = 1;
	static uint32_t wait_next = 0;
	uint32_t cur;

	cur = peri_btn_pin_get();
	if (cur != last) {
		debounce_cunter = 0;
	}
	last = cur;
	if (debounce_cunter < (BTN_PERIOD_DEBOUNCE - 1)) {
		debounce_cunter++;
		if (debounce_cunter < (BTN_PERIOD_DEBOUNCE - 1)) {
			return;
		}
	}
	if (cur) {
		/*release*/
		if (press_period) {
			peri_btn_action(BTN_ACTION_RELEASE);
			if (press_period < BTN_PERIOD_LONG) {
				/*short press wait for next press*/
				if (wait_next) {
					/*double short press*/
					peri_btn_action(BTN_ACTION_DOUBLE);
					wait_next = 0;
				} else {
					wait_next = 1;
				}
			} else {
				wait_next = 0;
			}
			if ((press_period >= BTN_PERIOD_SERVICE) &&
				(press_period <= (BTN_PERIOD_SERVICE + 2500))) {
				peri_btn_action(BTN_ACTION_SERVICE);
			}
		}
		if (wait_next) {
			if (release_period >= BTN_PERIOD_SHORT) {
				/*single short press*/
				peri_btn_action(BTN_ACTION_SHORT);
				wait_next = 0;
			}
		}
		if (release_period < 0xFFFFFFFF) {
			release_period++;
		}
		press_period = 0;
	} else {
		/*press*/
		if (!press_period) {
			peri_btn_action(BTN_ACTION_PRESS);
		} else {
			if (press_period == BTN_PERIOD_LONG) {
				peri_btn_action(BTN_ACTION_LONG);
			}
			if (press_period == BTN_PERIOD_RESET) {
				peri_btn_action(BTN_ACTION_RESET);
			}
		}
		if (wait_next) {
			if (press_period >= BTN_PERIOD_LONG) {
				peri_btn_action(BTN_ACTION_SHORT);
				wait_next = 0;
			}
		}
		if (press_period < 0xFFFFFFFF) {
			press_period++;
		}
		release_period = 0;
	}
}

static void ICACHE_FLASH_ATTR peri_btn_event(uint32_t period) {
	debug_printf("BTN press, period %u\n", period);
}

static void peri_tick_clk(void) {
	#ifndef IQS
	static uint32_t counter = 0;
	static uint32_t div = 0;
	if (!peri_pwm_lock) {
		pin_set(LED_R_GPIO, (counter >= peri_pwm_duty[PWM_R_CH]) ? 1 : 0);
		pin_set(LED_W_GPIO, (counter >= peri_pwm_duty[PWM_W_CH]) ? 1 : 0);
		pin_set(LED_G_ACU_GPIO, (counter >= peri_pwm_duty[PWM_G_CH]) ? 1 : 0);
		if (!peri_pwm_duty[2]) {
			/*pin set to logic one*/
			/*check pin status*/
			if (!pin_get(LED_G_ACU_GPIO)) {
				peri_charge = 1;
			} else {
				peri_charge = 0;
			}
		}
		counter++;
		counter %= PWM_LEVELS;
	}
	if (!div) {
		timer_ticks++;
		peri_check_button();
	}
	div++;
	div %= 20;
	#else
	timer_ticks++;
	#endif
}

static void ICACHE_FLASH_ATTR peri_clk(void* owner) {
	static uint16_t div = 0;
	if (!div) {
		peri_led_clk(owner);
	}
	div++;
	div %= 25;
	#ifdef IQS
	peri_i2c_clk(owner);
	#endif
	os_timer_disarm(&rdy_timer);
	os_timer_setfn(&rdy_timer, peri_clk, NULL);
	os_timer_arm(&rdy_timer, 1, 0);
}

static void ICACHE_FLASH_ATTR peri_led_clk(void* owner) {
	static uint8_t last_any;
	#ifndef IQS
	static uint8_t charge = 0;
	#endif
	uint16_t set_val[3];
	uint8_t i;
	uint8_t set_any = 0;
	for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
		set_any |= led_clk(&rgb_leds[i], &set_val[i]);
		set_val[i] /= 128;
	}
	if (set_any) {
		/*os_printf_plus("%u,%u,%u\n", (uint32_t)set_val[0], (uint32_t)set_val[1], (uint32_t)set_val[2]);*/
		#ifdef IQS
			iqs_led_pwm_order(set_val);
		#else
		peri_pwm_lock = 1;
		memcpy(peri_pwm_duty, set_val, sizeof(peri_pwm_duty));
		peri_pwm_lock = 0;
		#endif
	}
	if (set_any > last_any) {
		sleep_lock(SLEEP_PWM);
	}
	if ((set_any < last_any) && !queue_size(led_queue)) {
		sleep_unlock(SLEEP_PWM);
	}
	last_any = set_any;
	if (!set_any) {
		if (queue_size(led_queue)) {
			uint8_t all_ready = 1;
			for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
				all_ready &= led_ready(&rgb_leds[i]);
			}
			if (all_ready) {
				Peri_Led_Command_T command;
				if (queue_read(led_queue, &command)) {
					switch (command.action) {
						case LED_ACTION_TRANSITION:
							for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
								if (!command.transition[i].bypass) {
									led_set(&rgb_leds[i],
										command.transition[i].value,
										command.transition[i].speed);
								}
							}
						break;

						case LED_ACTION_BLINK:
							for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
								if ((i == LED_W) &&
									command.blink[i].bypass &&
									!peri_white_locked &&
									peri_white_enabled) {
										led_set(&rgb_leds[i],
											0,
											command.blink[i].speed);
										peri_white_enabled = 0;
								} else {
									led_start(&rgb_leds[i],
										command.blink[i].repeat,
										command.blink[i].speed,
										command.blink[i].bypass);
								}
							}
						break;
					}
				}
			}
		}
	}
	#ifndef IQS
	if (peri_charge > charge) {
		if (peri_charge_event_fn) {
			peri_charge_event_fn(1);
		}
	} else {
		if (peri_charge < charge) {
			if (peri_charge_event_fn) {
				peri_charge_event_fn(0);
			}
		}
	}
	charge = peri_charge;
	#endif
}

#ifdef IQS
/*place immediate order*/
static uint8_t ICACHE_FLASH_ATTR peri_rdy_event(I2C_Order_T order) {
	if (!peri_rdy_event_fn) {
		return 0;
	}
	return peri_rdy_event_fn(order);
}

static uint8_t ICACHE_FLASH_ATTR peri_check_rdy(void) {
	static uint8_t prev = 0;
	uint8_t cur;
	uint8_t ret_val = 0;
	cur = pin_get(I2C_RDY_GPIO);
	if (!event_mode) {
		if (cur < prev) {
			ret_val = 1;
		}
	} else {
		if (!cur) {
			ret_val = 1;
		}
	}
	prev = cur;
	return ret_val;
}

static void ICACHE_FLASH_ATTR peri_i2c_clk(void* owner) {
	static struct I2C_Order_T order;
	static struct I2C_Order_T back;
	static uint16_t timeout = 0;
	static uint16_t force = 0;
	static uint16_t wait_window = 0;
	static uint8_t process = 0;
	static uint8_t repeat = 0;
	uint8_t ret_val;
	uint8_t window;
	uint8_t rdy;
	
	peri_check_button();
	window = peri_check_rdy();
	if (!process) {
		if (!event_mode || !window || repeat || !(rdy = peri_rdy_event(&order))) {
			/*no immediate request, check queue*/
			I2C_Order_T item = NULL;
			if (!repeat) {
				if (!queue_head(queue, &item)) {
					/*no order*/
					goto exit;
				}
				order = *item;
			} else {
				order = back;
			}
			if (order.address == IQS333_ADDR) {
				/*require special handle*/
				if (!event_mode) {
					if (!window) {
						goto exit;
					}
				} else {
					if (!window) {
						/*force window, RDY line down*/
						pin_set(I2C_RDY_GPIO, 0);
						force = 10;
					}
				}
			}
			if (queue_read(queue, &item)) {
				free(item);
			}
		}
		process = 1;
		if (!repeat) {
			back = order;
			repeat = order.repeat;
		}
		timeout = order.timeout;
	}
	if (force) {
		force--;
		if (force) {
			goto exit;
		}
		pin_set(I2C_RDY_GPIO, 1);
		wait_window = 3;
		goto exit;
	}
	if (wait_window) {
		if (!window) {
			if (timeout) {
				timeout--;
				if (!timeout) {
					if (repeat) {
						if (repeat != 255) {
							repeat--;
						}
					}
					if (!repeat) {
						if (order.done_cb) {
							order.done_cb(&order, I2C_RES_WINDOW);
						}
					}
					wait_window = 0;
					process = 0;
				}
			}
			goto exit;
		}
		wait_window--;
		goto exit;
	}
	if (order.dir == I2C_DIR_READ) {
		ret_val = i2c_read_burst(&i2c, order.address, order.command, order.len, order.data);
	} else {
		if (order.dir == I2C_DIR_WRITE) {
			ret_val = i2c_write_burst(&i2c, order.address, order.command, order.len, order.data);
		} else {
			ret_val = i2c_read_serial(&i2c, order.address, order.command, order.serial_len, order.data);
		}
	}
	if (!ret_val) {
		if (repeat) {
			if (repeat != 255) {
				repeat--;
			}
		}
		if (!repeat) {
			if (order.done_cb) {
				order.done_cb(&order, I2C_RES_ERR);
			}
		}
	} else {
		repeat = 0;
		if (order.done_cb) {
			order.done_cb(&order, I2C_RES_OK);
		}
	}
	process = 0;
	
exit:
	return;
}
#endif

void ICACHE_FLASH_ATTR peri_set_event_mode(uint8_t value) {
	uint32_t magic;
	if (value) {
		event_mode = 1;
		magic = RTC_MAGIC;
	} else {
		event_mode = 0;
		magic = 0xFFFFFFFF;
	}
	system_rtc_mem_write(RTC_MODE_OFFSET, &magic, sizeof(magic));
}

uint8_t ICACHE_FLASH_ATTR peri_is_event_mode(void) {
	return event_mode ? 1 : 0;
}

#ifdef IQS
void ICACHE_FLASH_ATTR peri_set_rdy_cb(uint8_t (*rdy_event)(I2C_Order_T order)) {
	peri_rdy_event_fn = rdy_event;
}

uint8_t ICACHE_FLASH_ATTR peri_order(I2C_Order_T* order) {
	if (!order) {
		return 0;
	}
	return queue_write(queue, order);
}

uint8_t ICACHE_FLASH_ATTR peri_order_low_priority(I2C_Order_T* order) {
	if (!order) {
		return 0;
	}
	if (queue_size(queue)) {
		return 0;
	}
	return queue_write(queue, order);
}
#endif

void ICACHE_FLASH_ATTR peri_set_btn_cb(void (*btn_event)(Btn_Action_T action)) {
	peri_btn_event_fn = btn_event;
}

void ICACHE_FLASH_ATTR peri_set_is_charge_cb(void (*charge_event)(uint8_t is_charge)) {
	peri_charge_event_fn = charge_event;
}

void ICACHE_FLASH_ATTR peri_pre_init(void) {
	RTC_GPIO_T rtc_gpio;
	//pin_init(BTN_GPIO, PIN_MODE_IN, PIN_OTYPE_PP, PIN_PUPD_NOPULL, 0);
	pin_init(BTN_GPIO, PIN_MODE_OUT, PIN_OTYPE_OD, PIN_PUPD_NOPULL, 1);
	if (!pin_get(BTN_GPIO)) {
		peri_btn_press_on_start = 1;
	}
	if (rtc_read(RTC_GPIO_OFFSET, &rtc_gpio, sizeof(rtc_gpio))) {
		if (peri_rst_reason() == REASON_DEEP_SLEEP_AWAKE) {
			if (!(rtc_gpio.value & ((uint32_t)1 << BTN_GPIO))) {
				peri_btn_press_on_start = 1;
			}
			rtc_erase(RTC_GPIO_OFFSET, &rtc_gpio, sizeof(rtc_gpio));
		}
	}
}

uint8_t ICACHE_FLASH_ATTR peri_init(void) {
	uint32_t magic;
	uint8_t i;
	peri_pre_init();
	peri_btn_was_pressed = peri_btn_press_on_start ? BTN_PERIOD_DEBOUNCE : 0;
	for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
		led_init(&rgb_leds[i], i);
	}
	#ifdef IQS
	if (!queue) {
		queue = queue_new(sizeof(I2C_Order_T), 20);
	}
	#endif
	if (!led_queue) {
		led_queue = queue_new(sizeof(Peri_Led_Command_T), 10);
	}
	system_rtc_mem_read(RTC_MODE_OFFSET, &magic, sizeof(magic));
	if (magic == RTC_MAGIC) {
		event_mode = 1;
	}
	hw_timer_init(NMI_SOURCE, 1);
	hw_timer_set_func(peri_tick_clk);

#ifdef IQS
	hw_timer_arm(1000);
#else
	hw_timer_arm(50);
#endif

#ifdef IQS
	pin_init(I2C_RDY_GPIO, PIN_MODE_OUT, PIN_OTYPE_OD, PIN_PUPD_NOPULL, 1);
	pin_init(WHITE_LED_GPIO, PIN_MODE_OUT, PIN_OTYPE_PP, PIN_PUPD_NOPULL, peri_btn_press_on_start);
	i2c_init(&i2c, &i2c_inh, NULL);
#else
	pin_init(LED_R_GPIO, PIN_MODE_OUT, PIN_OTYPE_PP, PIN_PUPD_NOPULL, 1);
	pin_init(LED_W_GPIO, PIN_MODE_OUT, PIN_OTYPE_PP, PIN_PUPD_NOPULL, 1);
	pin_init(LED_G_ACU_GPIO, PIN_MODE_OUT, PIN_OTYPE_OD, PIN_PUPD_NOPULL, 1);
#endif

	os_timer_disarm(&rdy_timer);
	os_timer_setfn(&rdy_timer, peri_clk, NULL);
	os_timer_arm(&rdy_timer, 1, 0);
#ifdef IQS
	iqs_init();
#endif

	return 1;
}

static void ICACHE_FLASH_ATTR hw_timer_deinit(void) {
	TM1_EDGE_INT_DISABLE();
	ETS_FRC1_INTR_DISABLE();
}

void ICACHE_FLASH_ATTR peri_deinit(void) {
	hw_timer_deinit();
	os_timer_disarm(&rdy_timer);
	#ifdef IQS
	if (queue) {
		I2C_Order_T item;
		while (queue_read(queue, &item)) {
			free(item);
		}
		queue_delete(queue);
		queue = NULL;
	}
	i2c_deinit(&i2c);
	pin_set(WHITE_LED_GPIO, 0);
	pin_set(I2C_RDY_GPIO, 1);
	//pin_set(BTN_GPIO, 1);
	#else
	//pin_deinit(BTN_GPIO);
	pin_deinit(LED_R_GPIO);
	pin_deinit(LED_W_GPIO);
	pin_deinit(LED_G_ACU_GPIO);
	#endif
	pin_set(BTN_GPIO, 1);
}

uint8_t ICACHE_FLASH_ATTR peri_set_pattern(uint8_t index, uint8_t repeat) {
	Peri_Led_Command_T command;
	uint8_t i;
	if (index >= ARRAY_SIZE(rgb_led_patterns)) {
		return 0;
	}
	memset(&command, 0, sizeof(command));
	for (i = 0; i < ARRAY_SIZE(rgb_leds); i++) {
		command.blink[i].repeat = repeat ? repeat : rgb_led_patterns[index].repeat;
		command.blink[i].speed = rgb_led_patterns[index].speed;
		command.blink[i].bypass = rgb_led_patterns[index].bypass[i];
	}
	command.action = LED_ACTION_BLINK;
	return queue_write(led_queue, &command);
}

uint8_t ICACHE_FLASH_ATTR peri_blink(uint8_t r, uint8_t w, uint8_t g, uint8_t speed, uint8_t repeat) {
	Peri_Led_Command_T command;
	uint16_t real_speed;
	uint8_t bypass[PERI_LED_COUNT];
	uint8_t i;
	if (!r && !w && !g) {
		return 0;
	}
	if (!speed) {
		return 0;
	}
	if (speed > MAX_SPEED) {
		return 0;
	}
	if (repeat > MAX_REPEAT) {
		return 0;
	}
	memset(&command, 0, sizeof(command));
	memset(bypass, 0, sizeof(bypass));
	real_speed = (uint16_t)SPEED_BASE * speed;
	bypass[0] = r ? 0 : 1;
	bypass[1] = w ? 0 : 1;
	bypass[2] = g ? 0 : 1;
	for (i = 0; i < PERI_LED_COUNT; i++) {
		command.blink[i].repeat = repeat;
		command.blink[i].speed = real_speed;
		command.blink[i].bypass = bypass[i];
	}
	command.action = LED_ACTION_BLINK;
	return queue_write(led_queue, &command);
}

void ICACHE_FLASH_ATTR peri_lock_white(uint8_t lock) {
	peri_white_locked = lock ? 1 : 0;
}

uint8_t ICACHE_FLASH_ATTR peri_set_white(uint8_t on) {
	if (peri_white_locked) {
		return 0;
	}
	if (on) {
		if (peri_white_enabled) {
			return 1;
		}
		if (peri_set_led(0, 255, 0, 0)) {
			peri_white_enabled = 1;
			return 1;
		}
	} else {
		if (!peri_white_enabled) {
			return 1;
		}
		if (peri_set_led(0, 0, 0, 0)) {
			peri_white_enabled = 0;
			return 1;
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR peri_set_led(uint8_t r, uint8_t w, uint8_t g, uint16_t ramp) {
	Peri_Led_Command_T command;
	uint8_t i;
	debug_describe_P(COLOR_MAGENTA "LED ORDER" COLOR_END);
	memset(&command, 0, sizeof(command));
	for (i = 0; i < PERI_LED_COUNT; i++) {
		command.transition[i].bypass = 0;
		command.transition[i].speed = ramp;
	}
	command.transition[0].value = r;
	command.transition[1].value = w;
	command.transition[2].value = g;
	command.action = LED_ACTION_TRANSITION;
	return queue_write(led_queue, &command);
}

uint8_t ICACHE_FLASH_ATTR peri_led_test(uint8_t action, uint8_t index) {
	Peri_Led_Command_T command;
	uint8_t i;
	if (index >= ARRAY_SIZE(rgb_leds)) {
		return 0;
	}
	memset(&command, 0, sizeof(command));
	for (i = 0; i < PERI_LED_COUNT; i++) {
		command.transition[i].bypass = 1;
		command.transition[i].speed = 0;
	}
	command.transition[index].value = action ? 255 : 0;
	command.transition[index].bypass = 0;
	command.action = LED_ACTION_TRANSITION;
	return queue_write(led_queue, &command);
}

uint8_t ICACHE_FLASH_ATTR peri_is_charge(void) {
	if (peri_charge) {
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR peri_hold_on_start(void) {
	return peri_btn_press_on_start;
}

uint32_t ICACHE_FLASH_ATTR peri_battery(void) {
	uint16_t adc_value = system_adc_read();
	return ((uint64_t)adc_value * (ADC_R1 + ADC_R2) * 1000) / (1024 * ADC_R2);
}

uint32_t ICACHE_FLASH_ATTR peri_battery_voltage(uint16_t adc_value) {
	return ((uint64_t)adc_value * (ADC_R1 + ADC_R2) * 1000) / (1024 * ADC_R2);
}

uint8_t ICACHE_FLASH_ATTR peri_battery_percentage(uint32_t voltage) {
	uint32_t range = BATTERY_MAX - BATTERY_MIN;
	uint32_t percent;
	if (voltage >= BATTERY_MAX) {
		return 100;
	}
	if (voltage <= BATTERY_MIN) {
		return 0;
	}
	percent = (uint32_t)((voltage - BATTERY_MIN) * 100) / range;
	return percent;
}

uint16_t ICACHE_FLASH_ATTR peri_battery_raw(void) {
	return system_adc_read();
}

uint32_t ICACHE_FLASH_ATTR peri_rst_reason(void) {
	struct rst_info* reset_info = system_get_rst_info();
	if (!reset_info) {
		return 0;
	}
	return reset_info->reason;
}

uint8_t ICACHE_FLASH_ATTR peri_paired(void) {
	char* url = NULL;
	uint8_t i;
	for (i = 0; i < __URL_TYPE_MAX; i++) {
		url = url_storage_read(&url_storage, i);
		if (url) {
			free(url);
			return 1;
		}
	}
	return 0;
}
