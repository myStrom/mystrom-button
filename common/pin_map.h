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



#ifndef __PIN_MAP_H__
#define __PIN_MAP_H__

#include "c_types.h"
#include "user_config.h"
#include "gpio.h"

#define GPIO_PIN_NUM 13
#define GPIO_MAX_INDEX 16

extern uint8_t pin_func[GPIO_MAX_INDEX + 1];
extern uint32_t pin_name[GPIO_MAX_INDEX + 1];
#ifdef GPIO_INTERRUPT_ENABLE
extern GPIO_INT_TYPE pin_int_type[GPIO_MAX_INDEX + 1];
#endif

static inline int is_gpio_invalid(unsigned gpio)
{
    if (gpio > GPIO_MAX_INDEX) {
        return 1;
    }
    switch (gpio) {
    case 6:
    case 7:
    case 8:
    case 11:
        return 1;
    default:
        return 0;
    }

}
#endif // #ifndef __PIN_MAP_H__
