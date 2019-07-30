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


#ifndef WIFI_H_INCLUDED
#define WIFI_H_INCLUDED 1

#include <inttypes.h>
#include <user_interface.h>
#include <ip_addr.h>
#include "http.h"
#include "rtc.h"

#define WIFI_DEVICE_SSID "my-button"
#define WIFI_DEVICE_DHCP_NAME "myStrom-Button"

typedef enum {
	WIFI_WPS_STARTED,
	WIFI_WPS_SUCCESS,
	WIFI_WPS_FAIL,
	WIFI_WPS_SHORTEN,
} WiFi_Wps_Status_T;

typedef struct {
	struct station_config station;
	uint8_t static_ip;
	ip_addr_t ip;
	ip_addr_t mask;
	ip_addr_t gw;
	ip_addr_t dns1;
	ip_addr_t dns2;
} WiFi_Config_T;

typedef struct {
	void (*on_disconn)(void);
	void (*on_dhcp)(void);
	void (*on_conn)(void);
	void (*on_reconn)(uint32_t time_ms);
	void (*wps_status)(WiFi_Wps_Status_T status);
	void (*ap_start)(void);
	void (*new_conn)(void);
} WiFi_Inh_T;

uint8_t wifi_init(WiFi_Inh_T* inh);
uint8_t wifi_http(Http_T http);
void wifi_connect(void* arg);
uint8_t wifi_rtc_ip_read(RTC_IP_T* info);
uint8_t wifi_rtc_ip_write(RTC_IP_T* info);
uint8_t wifi_rtc_ip_erase(void);
void wifi_inhibit(void);
uint8_t wifi_is_save(void);
uint8_t wifi_start_wps(void);
uint8_t wifi_dbm_to_percentage(int8_t rssi);
void wifi_set_immediate_connect(uint8_t val);

#endif
