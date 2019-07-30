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


#ifndef PERI_H_INCLUDED
#define PERI_H_INCLUDED 1

#include <inttypes.h>
#include <ip_addr.h>
#include "wifi.h"
#include "rtc.h"
#include "led.h"

#define PERI_LED_COUNT 3

#ifdef IQS
	#define PWM_R_CH 2
	#define PWM_W_CH 1
	#define PWM_G_CH 0
#else
	#define PWM_R_CH 0
	#define PWM_W_CH 1
	#define PWM_G_CH 2
#endif

#ifdef IQS
	#define I2C_SDA_GPIO 2
	#define I2C_SCL_GPIO 14
	#define I2C_RDY_GPIO 13
	#define BTN_GPIO 4
	#define INHIBIT_GPIO 12
	#define WHITE_LED_GPIO 5
#else
	#define BTN_GPIO 4
	#define LED_R_GPIO 14
	#define LED_W_GPIO 5
	#define LED_G_ACU_GPIO 13
	#define INHIBIT_GPIO 12
	#define PWM_LEVELS 32
#endif

typedef struct I2C_Order_T* I2C_Order_T;

typedef enum {
	BTN_ACTION_NONE = 0,
	BTN_ACTION_SHORT,
	BTN_ACTION_DOUBLE,
	BTN_ACTION_LONG,
	BTN_ACTION_TOUCH,
	BTN_ACTION_WHEEL,
	BTN_ACTION_BATTERY,
	BTN_ACTION_RESET,
	BTN_ACTION_PRESS,
	BTN_ACTION_RELEASE,
	BTN_ACTION_HOLD,
	BTN_ACTION_WHEEL_FINAL,
	BTN_ACTION_SERVICE,
	__BTN_ACTION_MAX
} Btn_Action_T;

typedef enum {
	I2C_RES_OK,
	I2C_RES_ERR,
	I2C_RES_TIME,
	I2C_RES_WINDOW
} I2C_Result_T;

typedef enum {
	I2C_DIR_WRITE,
	I2C_DIR_READ,
	I2C_DIR_READ_SERIAL,
} I2C_Dir_T;

typedef void (*I2C_Done_Cb)(I2C_Order_T order, I2C_Result_T result);

struct I2C_Order_T {
	I2C_Dir_T dir;
	uint16_t timeout;
	uint8_t repeat;
	uint8_t address;
	uint8_t command;
	uint8_t len;
	void* owner;
	I2C_Done_Cb done_cb;
	uint8_t data[10];
	uint8_t serial_len[4];
} __packed;

typedef struct {
	Led_Action_T action;
	union {
		Led_Transition_T transition[PERI_LED_COUNT];
		Led_Blink_T blink[PERI_LED_COUNT];
	};
} Peri_Led_Command_T;

void peri_pre_init(void);
uint8_t peri_init();
void peri_set_event_mode(uint8_t value);
uint8_t peri_is_event_mode(void);
void peri_deinit(void);
uint8_t peri_set_pattern(uint8_t index, uint8_t repeat);
void peri_set_btn_cb(void (*btn_event)(Btn_Action_T action));
void peri_set_is_charge_cb(void (*charge_event)(uint8_t is_charge));
uint8_t peri_is_charge(void);
uint8_t peri_hold_on_start(void);
uint32_t peri_battery(void);
uint8_t peri_battery_percentage(uint32_t voltage);
uint32_t peri_battery_voltage(uint16_t adc_value);
uint16_t peri_battery_raw(void);
uint32_t peri_rst_reason(void);
void peri_lock_white(uint8_t lock);
uint8_t peri_set_white(uint8_t on);
uint8_t peri_set_led(uint8_t r, uint8_t w, uint8_t g, uint16_t ramp);
uint8_t peri_led_test(uint8_t action, uint8_t index);
uint8_t peri_blink(uint8_t r, uint8_t w, uint8_t g, uint8_t speed, uint8_t repeat);
void peri_set_led_no_inhibit_sleep(uint8_t state);
uint8_t peri_paired(void);
#ifdef IQS
uint8_t peri_order(I2C_Order_T* order);
uint8_t peri_order_low_priority(I2C_Order_T* order);
void peri_set_rdy_cb(uint8_t (*rdy_event)(I2C_Order_T order));
#endif

#endif
