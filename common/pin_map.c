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


#include "pin_map.h"
#include "eagle_soc.h"

// better than 0 in case of unallowed gpio access
#define UNDEFINED PERIPHS_IO_MUX_GPIO5_U

uint32_t pin_name[GPIO_MAX_INDEX + 1] = {
	PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_U0TXD_U, PERIPHS_IO_MUX_GPIO2_U, PERIPHS_IO_MUX_U0RXD_U,
	PERIPHS_IO_MUX_GPIO4_U, PERIPHS_IO_MUX_GPIO5_U, UNDEFINED, UNDEFINED, UNDEFINED, PERIPHS_IO_MUX_SD_DATA2_U,
	PERIPHS_IO_MUX_SD_DATA3_U, UNDEFINED, PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U,
	PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U, PAD_XPD_DCDC_CONF
};

#undef UNDEFINED

// better than 0 in case of unallowed gpio access
#define UNDEFINED FUNC_GPIO5
uint8_t pin_func[GPIO_MAX_INDEX + 1] = {
	FUNC_GPIO0, FUNC_GPIO1, FUNC_GPIO2, FUNC_GPIO3,
	FUNC_GPIO4, FUNC_GPIO5, UNDEFINED, UNDEFINED, UNDEFINED, FUNC_GPIO9,
	FUNC_GPIO10, UNDEFINED, FUNC_GPIO12, FUNC_GPIO13,
	FUNC_GPIO14, FUNC_GPIO15, UNDEFINED
};
#undef UNDEFINED

GPIO_INT_TYPE pin_int_type[GPIO_MAX_INDEX + 1] = {
	GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE,
	GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE,
	GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE,
	GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_DISABLE,
	GPIO_PIN_INTR_DISABLE
};