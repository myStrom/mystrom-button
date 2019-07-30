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


#ifndef STORE_H_INCLUDED
#define STORE_H_INCLUDED 1

#include <inttypes.h>
#include <user_interface.h>
#include <ip_addr.h>

#define STORE_VERSION ((uint32_t)0)
#define STORE_PASSWORD_LENGTH 256//Must be aligned to 4

typedef struct {
	uint8_t mac[6];
	uint8_t type;
	union {
		uint8_t flags;
		struct {
			uint8_t child : 1;
			uint8_t registered : 1;
			uint8_t : 6;
		};
	};
} Collect_Item_T;

typedef struct {
	struct station_config station;
	struct {
		char ssid[33];
		char rssi;
	} strong;
	uint8_t save : 1;
} Store_Connect_T;

typedef enum {
	Veryfication_Undefine,
	Veryfication_Single,
	Veryfication_Double
} Touch_Veryfication_T;

struct Store_T {
	uint32_t crc;
	uint32_t counter : 20;
	uint32_t len : 12;
	Store_Connect_T connect;
	uint8_t service_mode;
	uint16_t version;
	uint8_t static_ip;
	ip_addr_t ip;
	ip_addr_t mask;
	ip_addr_t gw;
	ip_addr_t dns;
	char token[64];
	uint8_t panel_dis : 1;
	uint8_t rest_dis : 1;
	uint8_t debug;
	uint64_t upgrade_utc_timestamp;
	uint8_t thresholds[10];
	uint8_t veryfication;
	uint8_t bssid_enable : 1;
	char name[51];
};

void store_init(void);
uint8_t store_save(void);
void store_erase(void);
#endif
