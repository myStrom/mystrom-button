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


#ifndef PIN_H_INCLUDED
#define PIN_H_INCLUDED 1

#include <inttypes.h>

typedef enum {
	PIN_MODE_IN,
	PIN_MODE_OUT,
} Pin_Mode_T;

typedef enum {
	PIN_OTYPE_PP,
	PIN_OTYPE_OD
} Pin_OType_T;

typedef enum {
	PIN_PUPD_NOPULL,
	PIN_PUPD_UP,
	PIN_PUPD_DOWN
} Pin_PuPd_T;

struct Pin_T {
	uint32_t id;
	Pin_Mode_T mode;
	Pin_OType_T otype;
	Pin_PuPd_T pupd;
};

uint8_t pin_init(uint8_t id, Pin_Mode_T mode, Pin_OType_T otype, Pin_PuPd_T pupd, uint8_t state);
uint8_t pin_set(uint8_t id, uint8_t value);
uint8_t pin_get(uint8_t id);
uint8_t pin_deinit(uint8_t id);
uint8_t pin_check(uint8_t id);

#endif
