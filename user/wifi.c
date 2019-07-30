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


#include <string.h>
#include <osapi.h>
#include <user_interface.h>
#include <espconn.h>
#include <c_types.h>
#include <ip_addr.h>
#include "wifi.h"
#include "debug.h"
#include "store.h"
#include "color.h"
#include "json.h"
#include "rule.h"
#include "wps.h"
#include "collect.h"
#include "array_size.h"
#include "sleep.h"
#include "version.h"
#include "rgb.h"
#include "utils.h"

extern struct Store_T store;
extern char own_mac[13];
extern Collect_T collect;

static void ICACHE_FLASH_ATTR wifi_handle_event_cb(System_Event_t* evt);
static void ICACHE_FLASH_ATTR wifi_scan_done(void* arg, STATUS status);
static uint8_t ICACHE_FLASH_ATTR wifi_configure(WiFi_Config_T* config);
static uint8_t ICACHE_FLASH_ATTR wifi_conn(WiFi_Config_T* config);
static uint8_t ICACHE_FLASH_ATTR wifi_enable_ap(void);
static uint8_t ICACHE_FLASH_ATTR wifi_load_config(WiFi_Config_T* config);
static uint8_t ICACHE_FLASH_ATTR wifi_load_config_no_change(WiFi_Config_T* config);

static Http_Request_T scan_req = NULL;
static uint8_t manual_wps = 0;
static uint8_t manual_connect = 0;
static uint8_t config_set = 0;
static uint8_t new_station = 0;
static uint8_t immediate_connect = 0;
static char scan_ssid[33];
static char jsonp_callback[64];
static uint8_t last_bssid[6];
static Json_T scan_json = NULL;
static WiFi_Inh_T wifi_inh = {NULL, NULL, NULL, NULL};
static WiFi_Config_T temp_config;
static uint8_t try_counter = 0;
struct {
	WiFi_Config_T config;
	char ssid[33];
	char rssi;
	uint8_t bssid[6];
} strong;

static const Rule_T wifi_scan_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "arg1",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "arg2",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "scan"
		}
	},
};

static const Rule_T wifi_scan_json_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "ssid",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 1,
		.max_len = 32,
	},
	{
		.name = "callback",
		.type = RULE_CHAR,
		.required = 1,
		.min_len = 1,
		.max_len = 63,
	},
};

static const Rule_T wifi_connect_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "arg1",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "arg2",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "connect"
		}
	},
};

static const Rule_T device_network_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "ssid",
		.type = RULE_CHAR,
		.required = 1,
		.min_len = 1,
		.max_len = 32,
	},
	{
		.name = "passwd",
		.type = RULE_CHAR,
		.required = 1,
		.min_len = 1,
		.max_len = 63
	},
	{
		.name = "ip",
		.type = RULE_IP,
		.required = 0,
	},
	{
		.name = "mask",
		.type = RULE_IP,
		.required = 0,
	},
	{
		.name = "gw",
		.type = RULE_IP,
		.required = 0,
	},
	{
		.name = "dns",
		.type = RULE_IP,
		.required = 0,
	},
};

static const Rule_T wifi_info_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "arg1",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "arg2",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "info"
		}
	},
};

static const Rule_T wifi_ip_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "arg1",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "arg2",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "ip"
		}
	},
};

static const Rule_T wifi_wps_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "arg1",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "arg2",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "wps"
		}
	},
};

static void ICACHE_FLASH_ATTR wifi_scan_finish(void* arg, STATUS status) {
	//static struct station_config station;
	//static struct scan_config scan_conf;
	struct bss_info* bss_link = (struct bss_info*)arg;
	char ssid_str[33];
	Buffer_T send_buf;
	if (!scan_req) {
		debug_describe_P("No scan req");
		return;
	}
	if (scan_json) {
		debug_describe_P("Lost scan json");
		json_delete(scan_json);
		scan_json = NULL;
	}
	if (status != OK) {
		debug_describe_P("Scan result NOK");
		goto error;
	}
	if (!(scan_json = json_new())) {
		debug_describe_P("Scan no mem");
		goto error;
	}
	if (strlen(jsonp_callback)) {
		json_set_callback(scan_json, jsonp_callback);
	}
	json_set_format(scan_json, 1, 1);
	debug_printf("HSJ: %d" CRLF, system_get_free_heap_size());
	while (bss_link) {
		if ((bss_link->authmode == AUTH_WPA_PSK) ||
			(bss_link->authmode == AUTH_WPA2_PSK) ||
			(bss_link->authmode == AUTH_WPA_WPA2_PSK)) {
				memset(ssid_str, 0, sizeof(ssid_str));
				strncpy(ssid_str, (char*)bss_link->ssid, bss_link->ssid_len);
				json_add_string(scan_json, NULL, ssid_str);
				json_add_int(scan_json, NULL, bss_link->rssi);
		}
		bss_link = bss_link->next.stqe_next;
	}
	debug_printf("HSF: %d" CRLF, system_get_free_heap_size());
	json_print_rewind(scan_json);
	if ((send_buf = http_req_send_buffer(scan_req))) {
		buffer_clear(send_buf);
		json_print_part(scan_json, send_buf, 0);
		if (!http_req_send_chunked(scan_req)) {
			debug_describe_P("Can not send scan part");
		}
	} else {
		debug_describe_P("Can not get send buf for scan");
		http_req_close(scan_req);
	}
	debug_printf("HSS: %d" CRLF, system_get_free_heap_size());
	return;
error:
	debug_describe_P("Scan general error");
	http_req_close(scan_req);
}

static Parser_State_T ICACHE_FLASH_ATTR wifi_scan_exec(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	debug_describe_P("Scan exec");
	if (scan_req) {
		return Parser_State_Internal_Server_Error_500;
	}
	memset(scan_ssid, 0, sizeof(scan_ssid));
	memset(jsonp_callback, 0, sizeof(jsonp_callback));
	if (query && query[0].present) {
		strncpy(scan_ssid, query[0].string_value, sizeof(scan_ssid) - 1);
		debug_printf("Scan for: %s" CRLF, scan_ssid);
	} else {
		debug_describe_P("SSID not present");
	}
	if (query && query[1].present) {
		strncpy(jsonp_callback, query[1].string_value, sizeof(jsonp_callback) - 1);
		debug_printf("JPCB: %s" CRLF, jsonp_callback);
	} else {
		debug_describe_P("JSONP callback undefine");
	}
	return Parser_State_OK_200;
}

static void ICACHE_FLASH_ATTR wifi_scan_run(Http_Request_T req) {
	static struct scan_config scan_conf;
	if (scan_req) {
		debug_enter_here_P();
		return;
	}
	debug_describe_P("Scan start");
	scan_req = req;
	memset(&scan_conf, 0, sizeof(scan_conf));
	if (strlen(scan_ssid)) {
		scan_conf.ssid = (uint8_t*)scan_ssid;
		scan_conf.show_hidden = 1;
	}
	if (!wifi_station_scan(&scan_conf, wifi_scan_finish)) {
		debug_enter_here_P();
		http_req_close(scan_req);
	}
	debug_describe_P("Scan run");
}

static void ICACHE_FLASH_ATTR wifi_scan_send(Http_Request_T req) {
	Buffer_T send_buf;
	if (!scan_req) {
		debug_describe_P("Unexpected scan handle");
		return;
	}
	if ((send_buf = http_req_send_buffer(scan_req))) {
		buffer_clear(send_buf);
		json_print_part(scan_json, send_buf, 0);
		http_req_send_chunked(scan_req);
	} else {
		http_req_close(scan_req);
	}
}

static void ICACHE_FLASH_ATTR wifi_scan_close(Http_Request_T req) {
	debug_printf("Scan close: %u" CRLF, system_get_free_heap_size());
	memset(scan_ssid, 0, sizeof(scan_ssid));
	scan_req = NULL;
	if (scan_json) {
		json_delete(scan_json);
		scan_json = NULL;
	}
	debug_printf("Scan finish: %u" CRLF, system_get_free_heap_size());
}

static const struct Http_Page_T wifi_scan_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = wifi_scan_exec,
	.header_cb = wifi_scan_run,
	.close_cb = wifi_scan_close,
	.send_cb = wifi_scan_send,
	.len = 256,
	.dynamic = 1,
	.chunked = 1,
	.method = Parser_Method_GET,
	.query_rules = wifi_scan_json_rule,
	.query_rules_amount = 1,
	.path_rules = wifi_scan_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static const struct Http_Page_T wifi_scanp_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/javascript",
	.exec = wifi_scan_exec,
	.header_cb = wifi_scan_run,
	.close_cb = wifi_scan_close,
	.send_cb = wifi_scan_send,
	.len = 0,
	.dynamic = 1,
	.chunked = 1,
	.method = Parser_Method_GET,
	.query_rules = wifi_scan_json_rule,
	.query_rules_amount = 2,
	.path_rules = wifi_scan_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR wifi_connect_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	struct Value_T query[ARRAY_SIZE(device_network_rule)];
	Json_T json = NULL;
	config_set = 0;
	if (manual_connect || new_station) {
		goto error;
	}
	if (!content || !buffer_size(content)) {
		goto error;
	}
	if (!(json = json_parse(buffer_string(content), device_network_rule, ARRAY_SIZE(device_network_rule)))) {
		goto error;
	}
	if (!json_to_values(json, query)) {
		goto error;
	}
	if (!query[0].present || !query[1].present) {
		goto error;
	}
	memset(&temp_config, 0, sizeof(WiFi_Config_T));
	strncpy((char*)temp_config.station.ssid, query[0].string_value, sizeof(temp_config.station.ssid));
	strncpy((char*)temp_config.station.password, query[1].string_value, sizeof(temp_config.station.password));
	if (query[2].present || query[3].present || query[4].present || query[5].present) {
		if (query[2].present && query[3].present && query[4].present && query[5].present) {
			debug_describe_P("Connect over static IP");
			temp_config.ip.addr = query[2].uint_value;
			temp_config.mask.addr = query[3].uint_value;
			temp_config.gw.addr = query[4].uint_value;
			temp_config.dns1.addr = query[5].uint_value;
			temp_config.dns2.addr = query[5].uint_value;
			temp_config.static_ip = 1;
		} else {
			goto error;
		}
	}
	json_delete(json);
	config_set = 1;
	return Parser_State_OK_200;
error:
	config_set = 0;
	if (json) {
		json_delete(json);
	}
	return Parser_State_Bad_Request_400;
}

static void ICACHE_FLASH_ATTR wifi_connect_close(Http_Request_T req) {
	debug_describe_P("Connect close");
	if (config_set) {
		if (wifi_inh.new_conn) {
			wifi_inh.new_conn();
		}
		config_set = 0;
		try_counter = 5;
		manual_connect = 1;
		if (store.connect.save) {
			wifi_station_disconnect();
		} else {
			if (!wifi_conn(&temp_config)) {
				if (wifi_inh.on_reconn) {
					wifi_inh.on_reconn(5000);
				}
			}
		}
	}
}

static const struct Http_Page_T wifi_connect_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = wifi_connect_exec,
	.close_cb = wifi_connect_close,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = wifi_connect_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR wifi_info_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	WiFi_Config_T config;
	char ssid[33];
	Json_T resp;
	if (!(resp = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_string(resp, "version", APP_VERSION);
	json_add_string(resp, "mac", own_mac);
	if (wifi_load_config_no_change(&config)) {
		struct ip_info info;
		memset(&info, 0, sizeof(info));
		memset(ssid, 0, sizeof(ssid));
		strncpy(ssid, (char*)config.station.ssid, sizeof(config.station.ssid));
		json_add_string(resp, "ssid", ssid);
		if (!config.static_ip) {
			if (!wifi_get_ip_info(STATION_IF, &info)) {
				json_delete(resp);
				return Parser_State_Internal_Server_Error_500;
			}
			json_add_ip(resp, "ip", info.ip.addr);
			json_add_ip(resp, "mask", info.netmask.addr);
			json_add_ip(resp, "gw", info.gw.addr);
			json_add_ip(resp, "dns", info.gw.addr);
		} else {
			json_add_ip(resp, "ip", config.ip.addr);
			json_add_ip(resp, "mask", config.mask.addr);
			json_add_ip(resp, "gw", config.gw.addr);
			json_add_ip(resp, "dns", config.dns1.addr);
		}
		json_add_bool(resp, "static", config.static_ip ? 1 : 0);
		json_add_bool(resp, "connected", 1);
	} else {
		json_add_bool(resp, "connected", 0);
	}
	json_add_int(resp, "signal", wifi_dbm_to_percentage(wifi_station_get_rssi()));
	if ((*buffer = json_to_buffer(resp))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T wifi_info_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = wifi_info_exec,
	.close_cb = NULL,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = wifi_info_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR wifi_ip_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	struct ip_info info;
	Json_T resp;
	if (!(resp = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	memset(&info, 0, sizeof(info));
	if (!wifi_get_ip_info(store.connect.save ? STATION_IF : SOFTAP_IF, &info)) {
		json_delete(resp);
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_ip(resp, "ip", info.ip.addr);
	if ((*buffer = json_to_buffer(resp))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T wifi_ip_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = wifi_ip_exec,
	.close_cb = NULL,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = wifi_ip_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR wifi_wps_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	debug_describe_P("Manual WPS");
	return Parser_State_OK_200;
}

static void ICACHE_FLASH_ATTR wifi_wps_close(Http_Request_T req) {
	debug_describe_P("On WPS req close");
	if (store.connect.save) {
		manual_wps = 1;
		wifi_station_disconnect();
	} else {
		if (wps_start()) {
			if (wifi_inh.wps_status) {
				wifi_inh.wps_status(WIFI_WPS_STARTED);
			}
		}
	}
}

static const struct Http_Page_T wifi_wps_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = wifi_wps_exec,
	.close_cb = wifi_wps_close,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = wifi_wps_rule,
	.path_rules_amount = 2,
	.panel = 1
};

uint8_t ICACHE_FLASH_ATTR wifi_http(Http_T http) {
	if (!http) {
		return 0;
	}
	http_add_page(http, &wifi_scanp_page);
	http_add_page(http, &wifi_scan_page);
	http_add_page(http, &wifi_connect_page);
	http_add_page(http, &wifi_info_page);
	http_add_page(http, &wifi_ip_page);
	http_add_page(http, &wifi_wps_page);
	return 1;
}

static void ICACHE_FLASH_ATTR wifi_wps_fail(uint8_t shorten) {
	if (wifi_inh.wps_status) {
		wifi_inh.wps_status(shorten ? WIFI_WPS_SHORTEN : WIFI_WPS_FAIL);
	}
	if (store.connect.save) {
		WiFi_Config_T config;
		if (wifi_load_config(&config)) {
			if (!wifi_conn(&config)) {
				if (wifi_inh.on_reconn) {
					wifi_inh.on_reconn(5000);
				}
			}
			return;
		}
	}
	if (shorten) {
		if (!wifi_enable_ap()) {
			debug_describe_P(COLOR_RED "Critical: can not enable AP" COLOR_END);
		} else {
			debug_describe_P(COLOR_GREEN "AP enabled" COLOR_END);
			if (wifi_inh.ap_start) {
				wifi_inh.ap_start();
			}
		}
	}
}

uint8_t ICACHE_FLASH_ATTR wifi_init(WiFi_Inh_T* inh) {
	wifi_set_event_handler_cb(wifi_handle_event_cb);
	wifi_station_disconnect();
	wifi_station_set_reconnect_policy(0);
	if (wifi_station_get_auto_connect()) {
		wifi_station_set_auto_connect(0);
	}
	wifi_set_opmode_current(NULL_MODE);
	if (inh) {
		wifi_inh = *inh;
	}
	wps_fail_fn(wifi_wps_fail);
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_to_sta(void) {
	if (wifi_get_opmode() != STATION_MODE) {
		debug_describe_P("Set station mode...");
		if (wifi_set_opmode_current(STATION_MODE)) {
			debug_describe_P(COLOR_GREEN "Mode set success" COLOR_END);
		} else {
			debug_describe_P(COLOR_RED "Unable to set station mode" COLOR_END);
			return 0;
		}
	}
	if (!wifi_station_set_reconnect_policy(0)) {
		debug_describe_P("Can not set reconnect policy");
		return 0;
	}
	if (wifi_station_get_auto_connect()) {
		debug_describe_P("Disable auto connect");
		wifi_station_set_auto_connect(0);
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_configure(WiFi_Config_T* config) {
	char name[33];
	if (!config) {
		return 0;
	}
	if (!wifi_to_sta()) {
		debug_describe_P("can not switch to STA mode");
		return 0;
	}
	if (config->static_ip) {
		struct ip_info info;
		if (wifi_station_dhcpc_status() != DHCP_STOPPED) {
			if (!wifi_station_dhcpc_stop()) {
				debug_describe_P("Can not stop DHCP server");
				return 0;
			}
		}
		memset(&info, 0, sizeof(info));
		info.ip = config->ip;
		info.gw = config->gw;
		info.netmask = config->mask;

		debug_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR CRLF,
			IP2STR(&info.ip),
			IP2STR(&info.netmask),
			IP2STR(&info.gw));
		debug_printf("dns1:" IPSTR ",dns2:" IPSTR CRLF,
			IP2STR(&config->dns1),
			IP2STR(&config->dns2));

		if (!wifi_set_ip_info(STATION_IF, &info)) {
			debug_describe_P("Can not set station ip info");
			return 0;
		}
		espconn_dns_setserver(0, &config->dns1);
		espconn_dns_setserver(1, &config->dns2);
	} else {
		if (wifi_station_dhcpc_status() != DHCP_STARTED) {
			if (!wifi_station_dhcpc_start()) {
				debug_describe_P("Can not start DHCP server");
				return 0;
			}
		}
		memset(name, 0, sizeof(name));
		snprintf(name, sizeof(name), WIFI_DEVICE_DHCP_NAME "-%s", own_mac + 6);
		if (!wifi_station_set_hostname(name)) {
			debug_describe_P("Can not set station hostname");
			return 0;
		}
	}
	memset(name, 0, sizeof(name));
	strncpy(name, (char*)config->station.ssid, sizeof(config->station.ssid));
	debug_printf("Connect to: %s\n", name);
	#ifdef PRINT_SAVE_NET
	debug_printf("SSID: %s\n", config->station.ssid);
	debug_printf("PWD: %s\n", config->station.password);
	debug_printf("BSSID: " MACSTR "\n", MAC2STR(config->station.bssid));
	debug_printf("SET: %d\n", (int)config->station.bssid_set);
	#endif
	debug_printf("STATIC: %d\n", (int)config->static_ip);
	if (!wifi_station_set_config(&config->station)) {
		debug_describe_P(COLOR_RED "Unable to set station config" COLOR_END);
		return 0;
	}
	if (!wifi_station_connect()) {
		debug_describe_P(COLOR_RED "Can not connect" COLOR_END);
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_enable_ap(void) {
	struct softap_config ap;
	struct ip_info info;
	uint8_t mac[6];
	char mac_string[13];
	memset(&ap, 0, sizeof(ap));
	memset(mac_string, 0, sizeof(mac_string));
	wifi_get_macaddr(STATION_IF, mac);
	utils_mac_bytes_to_string((uint8_t*)mac_string, mac);
	sprintf((char*)ap.ssid, WIFI_DEVICE_SSID "-%s", mac_string + 6);
	ap.ssid_len = strlen((char*)ap.ssid);
	ap.channel = 1;
	ap.max_connection = 4;
	ap.beacon_interval = 100;
	ap.authmode = AUTH_OPEN;
	debug_printf("Set AP channel to: %d\n", (int)ap.channel);
	if (wifi_get_opmode() != STATIONAP_MODE) {
		debug_describe_P("Try to set station STATIONAP mode...");
		if (!wifi_set_opmode_current(STATIONAP_MODE)) {
			debug_describe_P(COLOR_RED "Unable to set AP" COLOR_END);
			return 0;
		} else {
			debug_describe_P("Mode set success");
		}
	}
	if (!wifi_station_set_reconnect_policy(0)) {
		debug_describe_P("Can not set reconnect policy");
		return 0;
	}
	if (wifi_station_disconnect()) {
		debug_describe_P("Disconnect OK");
	}
	if (wifi_softap_dhcps_status() == DHCP_STARTED) {
		debug_describe_P("AP DHCP running, try to stop it...");
		if (!wifi_softap_dhcps_stop()) {
			/*need to handle this better*/
			debug_describe_P("Critical: Can not stop AP DHCP! UNEXPECTED FAILURE. STOP");
			return 0;
		} else {
			debug_describe_P("AP DHCP stop success");
		}
	}
	memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 254, 1);
	IP4_ADDR(&info.gw, 192, 168, 254, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	if (!wifi_set_ip_info(SOFTAP_IF, &info)) {
		debug_describe_P("Critical: can not set IP for AP");
		return 0;
	}
	debug_describe_P("AP IP set ok");
	if (!wifi_softap_dhcps_start()) {
		debug_describe_P("Critical: AP DHCP start failure");
		return 0;
	}
	debug_describe_P("AP DHCP start ok");
	if (!wifi_softap_set_config_current(&ap)) {
		debug_describe_P("Critical: can not set AP config");
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_check_config(void) {
	struct station_config station;
	if (!wifi_station_get_config(&station)) {
		return 0;
	}
	if (!strnlen((char*)station.password, sizeof(station.password))) {
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_save(ip_addr_t* dns) {
	struct station_config station;
	if (store.connect.save) {
		return 0;
	}
	/*get current config*/
	if (!wifi_station_get_config(&station)) {
		debug_describe_P("Can not get station config");
		return 0;
	}
	if (wifi_station_dhcpc_status() == DHCP_STOPPED) {
		struct ip_info info;
		if (!dns) {
			return 0;
		}
		debug_describe_P(COLOR_YELLOW "STATIC CONNECT" COLOR_END);
		memset(&info, 0, sizeof(info));
		if (!wifi_get_ip_info(STATION_IF, &info)) {
			debug_describe_P("Can not get IP info");
			return 0;
		}
		store.ip = info.ip;
		store.mask = info.netmask;
		store.gw = info.gw;
		store.dns = *dns;
		store.static_ip = 1;
	} else {
		debug_describe_P(COLOR_GREEN "DYNAMIC CONNECT" COLOR_END);
		memset(&store.ip, 0, sizeof(store.ip));
		memset(&store.mask, 0, sizeof(store.mask));
		memset(&store.gw, 0, sizeof(store.gw));
		memset(&store.dns, 0, sizeof(store.dns));
		store.static_ip = 0;
	}
	memcpy(&store.connect.station, &station, sizeof(store.connect.station));
	memset(&store.connect.station.bssid, 0, sizeof(store.connect.station.bssid));
	store.connect.station.bssid_set = 0;
	store.connect.save = 1;
	return store_save();
}

static uint8_t ICACHE_FLASH_ATTR wifi_load_config(WiFi_Config_T* config) {
	if (!config) {
		return 0;
	}
	if (!store.connect.save) {
		return 0;
	}
	memset(config, 0, sizeof(WiFi_Config_T));
	memcpy(&config->station, &store.connect.station, sizeof(struct station_config));
	config->station.bssid_set = 0;
	memset(config->station.bssid, 0, sizeof(config->station.bssid));
	config->static_ip = store.static_ip;
	if (config->static_ip) {
		config->ip = store.ip;
		config->mask = store.mask;
		config->gw = store.gw;
		config->dns1 = store.dns;
		config->dns2 = store.dns;
	}
	if (!config->static_ip) {
		RTC_IP_T rtc_ip;
		memset(&rtc_ip, 0, sizeof(rtc_ip));
		if (wifi_rtc_ip_read(&rtc_ip)) {
			debug_describe_P("CONNECT BY STATIC RTC IP");
			config->ip = rtc_ip.info.ip;
			config->mask = rtc_ip.info.netmask;
			config->gw = rtc_ip.info.gw;
			if ((rtc_ip.dns1.addr == 0) || (rtc_ip.dns1.addr == 0xFFFFFFFF)) {
				config->dns1 = rtc_ip.info.gw;
			} else {
				config->dns1 = rtc_ip.dns1;
			}
			if ((rtc_ip.dns2.addr == 0) || (rtc_ip.dns2.addr == 0xFFFFFFFF)) {
				config->dns2 = rtc_ip.info.gw;
			} else {
				config->dns2 = rtc_ip.dns2;
			}
			config->static_ip = 1;
		} else {
			debug_describe_P("DHCP CONNECT");
		}
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR wifi_load_config_no_change(WiFi_Config_T* config) {
	if (!config) {
		return 0;
	}
	if (!store.connect.save) {
		return 0;
	}
	memset(config, 0, sizeof(WiFi_Config_T));
	memcpy(&config->station, &store.connect.station, sizeof(struct station_config));
	config->station.bssid_set = 0;
	memset(config->station.bssid, 0, sizeof(config->station.bssid));
	config->static_ip = store.static_ip;
	if (config->static_ip) {
		config->ip = store.ip;
		config->mask = store.mask;
		config->gw = store.gw;
		config->dns1 = store.dns;
		config->dns2 = store.dns;
	}
	return 1;
}

static void ICACHE_FLASH_ATTR wifi_handle_event_cb(System_Event_t* evt) {
	ip_addr_t dns;
	if (!evt) {
		return;
	}
	switch (evt->event) {
		case EVENT_STAMODE_CONNECTED:
			debug_printf("Conn to %s;%d;" MACSTR CRLF,
						evt->event_info.connected.ssid,
						(int)evt->event_info.connected.channel,
						MAC2STR(evt->event_info.connected.bssid));
			wifi_station_set_reconnect_policy(0);
			if (wifi_station_get_auto_connect()) {
				wifi_station_set_auto_connect(0);
			}
			if (wifi_inh.on_conn) {
				wifi_inh.on_conn();
			}
		break;
		case EVENT_STAMODE_DISCONNECTED:
			debug_printf("Disconn from %s;%d" CRLF, evt->event_info.disconnected.ssid, (int)evt->event_info.disconnected.reason);
			wifi_station_set_reconnect_policy(0);
			if (wifi_station_get_auto_connect()) {
				wifi_station_set_auto_connect(0);
			}
			wifi_station_disconnect();
			if (wifi_inh.on_disconn) {
				wifi_inh.on_disconn();
			}
			if (manual_connect) {
				debug_describe_P("Manual connect");
				manual_connect = 0;
				if (store.connect.save) {
					new_station = 1;
					if (wifi_conn(&temp_config)) {
						debug_describe_P(COLOR_YELLOW "Scan req success" COLOR_END);
						break;
					}
					if (wifi_inh.on_reconn) {
						wifi_inh.on_reconn(5000);
					}
				} else {
					if (try_counter &&
						(evt->event_info.disconnected.reason == REASON_NO_AP_FOUND)) {
							if (wifi_conn(&temp_config)) {
								debug_describe_P(COLOR_YELLOW "Conn success" COLOR_END);
								manual_connect = 1;
								new_station = 0;
								try_counter--;
								break;
							} else {
								debug_describe_P(COLOR_RED "Conn fail" COLOR_END);
							}
					}
					try_counter = 0;
					rgb_set_pattern(RGB_WPS_FAIL, 0);
					if (wifi_enable_ap()) {
						if (wifi_inh.ap_start) {
							wifi_inh.ap_start();
						}
					}
					break;
				}
			}
			if (new_station) {
				new_station = 0;
				if (try_counter &&
					(evt->event_info.disconnected.reason == REASON_NO_AP_FOUND)) {
						if (wifi_conn(&temp_config)) {
							debug_describe_P(COLOR_YELLOW "Conn success" COLOR_END);
							manual_connect = 0;
							new_station = 1;
							try_counter--;
							break;
						} else {
							debug_describe_P(COLOR_RED "Conn fail" COLOR_END);
						}
				}
				try_counter = 0;
				rgb_set_pattern(RGB_WPS_FAIL, 0);
			}
			if (manual_wps) {
				manual_wps = 0;
				if (wps_start()) {
					if (wifi_inh.wps_status) {
						wifi_inh.wps_status(WIFI_WPS_STARTED);
					}
				}
				break;
			}
			if (wps_success_is_event()) {
				if (wifi_check_config()) {
					if (!store.connect.save) {
						immediate_connect = 1;
					}
					store.connect.save = 0;
					if (wifi_station_dhcpc_status() != DHCP_STARTED) {
						wifi_station_dhcpc_start();
					}
					if (wifi_save(NULL)) {
						debug_describe_P("WPS SUCCESS");
						collect_erase(collect);
						rgb_set_pattern(RGB_WPS_SUCCESS, 0);
						if (wifi_inh.wps_status) {
							wifi_inh.wps_status(WIFI_WPS_SUCCESS);
						}
					}
				} else {
					rgb_set_pattern(RGB_WPS_FAIL, 0);
					if (wifi_inh.wps_status) {
						wifi_inh.wps_status(WIFI_WPS_FAIL);
					}
				}
			}
			if (!wps_is_enabled()) {
				if (wifi_inh.on_reconn) {
					debug_describe_P(COLOR_MAGENTA "CALL CONNECT" COLOR_END);
					wifi_inh.on_reconn(2500);
				}
			} else {
				debug_describe_P("Warning: WPS enabled");
			}
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE:
			debug_printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
			break;
		case EVENT_STAMODE_GOT_IP:
			debug_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR CRLF,
				IP2STR(&evt->event_info.got_ip.ip),
				IP2STR(&evt->event_info.got_ip.mask),
				IP2STR(&evt->event_info.got_ip.gw));
			dns = evt->event_info.got_ip.gw;
			if (new_station || manual_connect || wps_success_is_event()) {
				if ((new_station || manual_connect) && temp_config.static_ip && temp_config.dns1.addr) {
					dns = temp_config.dns1;
				}
				manual_connect = 0;
				new_station = 0;
				try_counter = 0;
				store.connect.save = 0;
			}
			if (wifi_save(&dns)) {
				collect_erase(collect);
				rgb_set_pattern(RGB_WPS_SUCCESS, 0);
			}
			if (wifi_inh.on_dhcp) {
				wifi_inh.on_dhcp();
			}
			break;
		case EVENT_SOFTAPMODE_STACONNECTED:
			debug_printf("station: " MACSTR " join, AID = %d" CRLF, MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED:
			debug_printf("station: " MACSTR " leave, AID = %d" CRLF, MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
			break;
		case EVENT_SOFTAPMODE_PROBEREQRECVED:
			break;
		default:
			break;
	}
}

static void ICACHE_FLASH_ATTR wifi_scan_done(void* arg, STATUS status) {
	char ssid[33];
	struct bss_info* bss_link = (struct bss_info*)arg;
	char mac_str[13];
	if (status != OK) {
		debug_describe_P(COLOR_RED "SCAN ERROR" COLOR_END);
		if (wifi_inh.on_reconn) {
			wifi_inh.on_reconn(5000);
		}
		return;
	}
	while (bss_link) {
		if ((bss_link->authmode == AUTH_WPA_PSK) ||
			(bss_link->authmode == AUTH_WPA2_PSK) ||
			(bss_link->authmode == AUTH_WPA_WPA2_PSK)) {
			memset(mac_str, 0, sizeof(mac_str));
			memset(ssid, 0, sizeof(ssid));
			strncpy(ssid, (char*)bss_link->ssid, bss_link->ssid_len);
			utils_mac_bytes_to_string((uint8_t*)mac_str, bss_link->bssid);
			debug_printf("Found: %s;%d;%d;%s" CRLF,
							ssid,
							(int)bss_link->rssi,
							(int)bss_link->channel,
							mac_str);
			if (memcmp(last_bssid, bss_link->bssid, sizeof(last_bssid))) {
				if (bss_link->rssi > strong.rssi) {
					debug_describe_P("Stronger");
					memset(strong.ssid, 0, sizeof(strong.ssid));
					strncpy(strong.ssid, (char*)bss_link->ssid, bss_link->ssid_len);
					memcpy(strong.bssid, bss_link->bssid, sizeof(strong.bssid));
					strong.rssi = bss_link->rssi;
				}
			} else {
				debug_describe_P("Skip");
			}
		}
		bss_link = bss_link->next.stqe_next;
	}
	/*scan next*/

	debug_describe_P("All scanned");
	if (strong.rssi == -127) {
		debug_describe_P("No AP found");
		memset(last_bssid, 0, sizeof(last_bssid));
		if (manual_connect || new_station) {
			manual_connect = 0;
			new_station = 0;
			try_counter = 0;
			rgb_set_pattern(RGB_WPS_FAIL, 0);
		}
		if (wifi_inh.on_reconn) {
			wifi_inh.on_reconn(0);
		}
		return;
	}
	/*connect to stronger*/
	debug_printf("Target to: %s: %d" CRLF, strong.ssid, (int)strong.rssi);
	debug_describe_P("STA->GW");
	memcpy(strong.config.station.bssid, strong.bssid, sizeof(strong.config.station.bssid));
	strong.config.station.bssid_set = 1;
	memset(last_bssid, 0, sizeof(last_bssid));

	if (!wifi_configure(&strong.config)) {
		debug_describe_P(COLOR_RED "Can not configure station" COLOR_END);
		if (wifi_inh.on_reconn) {
			wifi_inh.on_reconn(5000);
		}
		return;
	}
	debug_describe_P("Connected");
}

static uint8_t ICACHE_FLASH_ATTR wifi_conn(WiFi_Config_T* config) {
	struct scan_config scan_conf;
	if (!config) {
		return 0;
	}
	debug_describe_P("Connect task");

	if (immediate_connect) {
		debug_describe_P("Info: immediate connect");
		immediate_connect = 0;
		if (!wifi_configure(config)) {
			debug_describe_P(COLOR_RED "Can not configure station" COLOR_END);
			return 0;
		}
		debug_describe_P("Station connect...");
		return 1;
	}

	if (!wifi_to_sta()) {
		debug_describe_P("Can not switch to STA mode");
		return 0;
	}
	/*search strong signal*/
	memset(&scan_conf, 0, sizeof(scan_conf));
	scan_conf.ssid = config->station.ssid;
	scan_conf.show_hidden = 1;
	strong.rssi = -127;
	strong.config = *config;
	memset(strong.bssid, 0, sizeof(strong.bssid));
	if (!wifi_station_scan(&scan_conf, wifi_scan_done)) {
		debug_describe_P(COLOR_RED "WiFi Scan failed" COLOR_END);
		return 0;
	}
	/*wait for scan results*/
	debug_describe_P("Wait for scan results");
	return 1;
}

void ICACHE_FLASH_ATTR wifi_connect(void* arg) {
	WiFi_Config_T config;
	if (!wifi_load_config(&config)) {
		debug_describe_P(COLOR_RED "No saved wifi settings, enable WPS" COLOR_END);
		if (wps_start()) {
			if (wifi_inh.wps_status) {
				wifi_inh.wps_status(WIFI_WPS_STARTED);
			}
		}
		return;
	}
	if (!wifi_conn(&config)) {
		if (wifi_inh.on_reconn) {
			wifi_inh.on_reconn(5000);
		}
	}
}

uint8_t ICACHE_FLASH_ATTR wifi_rtc_ip_read(RTC_IP_T* rtc_ip) {
	RTC_IP_T ip;
	if (!rtc_ip) {
		return 0;
	}
	if (!system_rtc_mem_read(RTC_IP_OFFSET, &ip, sizeof(ip))) {
		return 0;
	}
	if (ip.magic != RTC_MAGIC) {
		return 0;
	}
	*rtc_ip = ip;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR wifi_rtc_ip_write(RTC_IP_T* rtc_ip) {
	RTC_IP_T ip;
	if (!rtc_ip) {
		return 0;
	}
	ip = *rtc_ip;
	ip.magic = RTC_MAGIC;
	if (!system_rtc_mem_write(RTC_IP_OFFSET, &ip, sizeof(ip))) {
		return 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR wifi_rtc_ip_erase(void) {
	RTC_IP_T ip;
	memset(&ip, 0, sizeof(ip));
	if (!system_rtc_mem_write(RTC_IP_OFFSET, &ip, sizeof(ip))) {
		return 0;
	}
	return 1;
}

void ICACHE_FLASH_ATTR wifi_inhibit(void) {
	memset(&wifi_inh, 0, sizeof(wifi_inh));
	wps_stop();
}

uint8_t ICACHE_FLASH_ATTR wifi_is_save(void) {
	if (store.connect.save) {
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR wifi_start_wps(void) {
	if (manual_wps) {
		return 0;
	}
	debug_describe_P("Manula WPS start");
	if (store.connect.save) {
		manual_wps = 1;
		wifi_station_disconnect();
		return 1;
	}
	if (wps_start()) {
		if (wifi_inh.wps_status) {
			wifi_inh.wps_status(WIFI_WPS_STARTED);
		}
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR wifi_dbm_to_percentage(int8_t rssi) {
	if (rssi <= -100) {
		return 0;
	}
	if (rssi >= -50) {
		return 100;
	}
	return 2 * (rssi + 100);
}

void ICACHE_FLASH_ATTR wifi_set_immediate_connect(uint8_t val) {
	immediate_connect = val;
}

