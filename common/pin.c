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


#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>
#include <ets_sys.h>
#include <c_types.h>
#include <eagle_soc.h>
#include "pin.h"

#define GPIO_MAX_INDEX 16
#define UNDEFINED 0

static uint32_t pin_mux[] = {
	PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_U0TXD_U, PERIPHS_IO_MUX_GPIO2_U, PERIPHS_IO_MUX_U0RXD_U,
	PERIPHS_IO_MUX_GPIO4_U, PERIPHS_IO_MUX_GPIO5_U, UNDEFINED, UNDEFINED, UNDEFINED, PERIPHS_IO_MUX_SD_DATA2_U,
	PERIPHS_IO_MUX_SD_DATA3_U, UNDEFINED, PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U,
	PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U, PAD_XPD_DCDC_CONF
};

static uint8_t pin_func[] = {
	FUNC_GPIO0, FUNC_GPIO1, FUNC_GPIO2, FUNC_GPIO3,
	FUNC_GPIO4, FUNC_GPIO5, UNDEFINED, UNDEFINED, UNDEFINED, FUNC_GPIO9,
	FUNC_GPIO10, UNDEFINED, FUNC_GPIO12, FUNC_GPIO13,
	FUNC_GPIO14, FUNC_GPIO15, UNDEFINED
};

static uint8_t pin_is_gpio_invalid(uint8_t gpio) {
	if (gpio > GPIO_MAX_INDEX) {
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR pin_init(uint8_t id, Pin_Mode_T mode, Pin_OType_T otype, Pin_PuPd_T pupd, uint8_t state) {
	if (pin_is_gpio_invalid(id)) {
		return 0;
	}
	switch (pupd) {
		case PIN_PUPD_NOPULL:
			PIN_PULLUP_DIS(pin_mux[id]);
		break;
		case PIN_PUPD_UP:
			PIN_PULLUP_EN(pin_mux[id]);
		break;
	}
	if (mode == PIN_MODE_IN) {
		GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, ((uint32_t)1 << id) & GPIO_OUT_W1TC_DATA_MASK);
	} else {
		if (mode == PIN_MODE_OUT) {
			pin_set(id, state);
			if (otype == PIN_OTYPE_OD) {
				GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(id)), GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE));
			} else {
				GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(id)), 0);
			}
			GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, ((uint32_t)1 << id) & GPIO_OUT_W1TC_DATA_MASK);
		} else {
			return 0;
		}
	}
	PIN_FUNC_SELECT(pin_mux[id], pin_func[id]);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR pin_deinit(uint8_t id) {
	if (pin_is_gpio_invalid(id)) {
		return 0;
	}
	PIN_FUNC_SELECT(pin_mux[id], 0);
	GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, ((uint32_t)1 << id) & GPIO_OUT_W1TC_DATA_MASK);
	PIN_PULLUP_EN(pin_mux[id]);
	return 1;
}

uint8_t pin_set(uint8_t id, uint8_t value) {
	if (pin_is_gpio_invalid(id)) {
		return 0;
	}
	if (value) {
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (uint16_t)1 << id);
	} else {
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (uint16_t)1 << id);
	}
	return 1;
}

uint8_t pin_get(uint8_t id) {
	if (pin_is_gpio_invalid(id)) {
		return 0;
	}
	if ((uint32_t)GPIO_REG_READ(GPIO_IN_ADDRESS) & ((uint32_t)1 << id)) {
		return 1;
	}
	return 0;
}

uint8_t pin_check(uint8_t id) {
	uint8_t ret_val = 0;
	if (pin_is_gpio_invalid(id)) {
		return 0;
	}
	GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, ((uint32_t)1 << id) & GPIO_OUT_W1TC_DATA_MASK);
	if ((uint32_t)GPIO_REG_READ(GPIO_IN_ADDRESS) & ((uint32_t)1 << id)) {
		ret_val = 1;
	}
	GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, ((uint32_t)1 << id) & GPIO_OUT_W1TC_DATA_MASK);
	return ret_val;
}
