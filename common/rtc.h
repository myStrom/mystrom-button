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


#ifndef RTC_H_INCLUDED
#define RTC_H_INCLUDED 1

#include <inttypes.h>
#include <ip_addr.h>

typedef struct {
	uint32_t magic;
	struct ip_info info;
	ip_addr_t dns1;
	ip_addr_t dns2;
} RTC_IP_T;

typedef struct {
	uint32_t magic;
	uint32_t sleep_timestamp;
	uint32_t sleep_period;
	uint32_t begin_timestamp;
} RWC_T;//Wakeup Counter

typedef struct {
	uint32_t magic;
	uint32_t value;
} RTC_GPIO_T;

#define RTC_MAGIC ((uint32_t)0x55AAAA55)
#define RTC_MODE_OFFSET (64)
#define RTC_IP_OFFSET (65)
#define RTC_WC_OFFSET ((sizeof(RTC_IP_T) / 4) + RTC_IP_OFFSET)
#define RTC_NEXT_OFFSET ((sizeof(RWC_T) / 4) + RTC_WC_OFFSET)

#define RTC_GPIO_OFFSET (190)

uint8_t rtc_write(uint8_t offset, void* data, uint16_t len);
uint8_t rtc_read(uint8_t offset, void* data, uint16_t len);
uint8_t rtc_erase(uint8_t offset, void* data, uint16_t len);
uint8_t rtc_erase_all(void);

#endif
