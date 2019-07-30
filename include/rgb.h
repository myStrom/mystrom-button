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


#ifndef RGB_H_INCLUDED
#define RGB_H_INCLUDED 1

#include <inttypes.h>
#include "peri.h"

#define RGB_WPS_SUCCESS 0
#define RGB_RESTORE 1
#define RGB_WPS_FAIL 2
#define RGB_SERVICE_MODE 3
#define RGB_SOFT_RESET 4
#define RGB_WPS_START 5
#define RGB_SC_START 6
#define RGB_CONN_FAIL 7

#define RGB_POWER_R 300
#define RGB_POWER_G 750
#define RGB_POWER_B 850
#define RGB_POWER_W 1200
#define RGB_POWER_S 500

typedef union {
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t w;
	};
	uint8_t data[4];
} RGB_T;

typedef enum {
	RGB_SET = 0,
	RGB_ON = 1,
	RGB_OFF = 2,
	RGB_TOGGLE = 3,
	__RGB_ACTION_MAX = 4
} RGB_Action_T;

#define rgb_set_pattern(index, repeat) peri_set_pattern(index, repeat)

#endif
