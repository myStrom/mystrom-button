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


#ifndef IQS333_H
#define	IQS333_H
#ifdef IQS

//i2c Slave Address Default
#define IQS333_ADDR 0x64

// Communication Command / Address Structure on IQS333 - ie. Memory Map
#define VERSION_INFO		0x00	// Product number can be read      : 2 bytes
#define FLAGS 				0x01	// System flags and events         : 1 byte
#define WHEEL_COORDS		0x02	// Wheel coordinates - 2 wheels    : 4 bytes
#define TOUCH_BYTES			0x03	// Touch channels                  : 2 bytes
#define COUNTS				0x04	// Count Values                    :20 bytes
#define LTA					0x05	// LTA Values                      :20 bytes
#define MULTIPLIERS			0x06	// Multipliers Values              :10 bytes
#define COMPENSATION		0x07	// Compensation Values (PCC)       :10 bytes
#define PROXSETTINGS		0x08	// Prox Settings - Various         : 6 bytes
#define THRESHOLDS			0x09	// Threshold Values                :10 bytes
#define TIMINGS				0x0A    // Timings                         : 5 bytes
#define	ATI_TARGETS			0x0B	// Targets for ATI                 : 2 bytes
#define PWM 				0x0C	// PWM Settings                    : 4 bytes
#define PWM_LIMIT			0x0D	// PWM Limits and speed            : 2 bytes
#define ACTIVE_CHANNELS		0x0E	// Active channels                 : 2 bytes
#define BUZZER				0x0F	// Buzzer Output                   : 1 byte

#include <inttypes.h>
#include "peri.h"

void iqs_init(void);
uint8_t iqs_reload(void);
void iqs_set_press_cb(void (*cb)(void));
void iqs_set_release_cb(void (*cb)(uint8_t busy));
void iqs_set_wakeup_cb(void (*cb)(uint8_t timer, uint8_t status));
void iqs_set_wheel_cb(uint8_t (*cb)(uint16_t count, int16_t diff));
void iqs_wheel_cancel(void);
uint8_t iqs_led_off_order(I2C_Done_Cb done_cb);
uint8_t iqs_led_pwm_order(uint16_t* values);
uint8_t iqs_power_mode(uint8_t save, I2C_Done_Cb done_cb);
uint8_t iqs_status_order(I2C_Order_T order);
void iqs_timer_stop(void);
uint8_t iqs_soft_reset(I2C_Done_Cb done_cb);

#endif
#endif
