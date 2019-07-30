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
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include <user_interface.h>
#include <ip_addr.h>
#include <espconn.h>
#include <inttypes.h>
#include <c_types.h>

#include "payload.h"
#include "debug.h"
#include "color.h"
#include "queue.h"
#include "ns.h"
#include "rtc.h"
#include "store.h"
#include "buffer.h"
#include "slash.h"
#include "rule.h"
#include "json.h"
#include "peri.h"
#include "list.h"
#include "url_storage.h"

#define PAYLOAD_BUFFER_SIZE 3072
#define PAYLOAD_EVENT_PORT 7980

typedef struct Payload_Action_T* Payload_Action_T;

struct Payload_T {
	Ns_T ns;
	List_T udp_events;
	List_T ns_queue;
	uint8_t connected : 1;
};

typedef struct {
	uint8_t mac[6];
	uint8_t type;
	uint16_t value;
	union {
		uint8_t flags;
	};
} Udp_Event_Item_T;

typedef struct {
	Url_Storage_Type_T type;
	char* args;
	Btn_Action_T action;
	uint8_t local : 1;
} Payload_Ns_Event_T;

typedef struct Udp_Event_T* Udp_Event_T;

struct Udp_Event_T {
	struct espconn conn;
	esp_udp udp_conn;
	uint8_t type;
	uint16_t value;
	Udp_Event_Item_T item;
	os_timer_t timeout;
};

struct Payload_Action_T {
	Payload_T payload;
	Buffer_T buffer;
	Btn_Action_T action;
	uint32_t len;
	uint32_t read;
	uint16_t code;
	uint16_t items;
	uint8_t chunked;
	uint8_t local : 1;
	uint8_t handled : 1;
	uint8_t body : 1;
	uint8_t headers : 1;
};

extern struct Store_T store;
extern Url_Storage_T url_storage;

enum {
	RESP_REPEAT = 0,
	RESP_SPEED = 1,
	RESP_R = 2,
	RESP_W = 3,
	RESP_G = 4,
	RESP_TIME = 5,
	__RESP_MAX
};

static const Rule_T payload_resp_rule[] ICACHE_RODATA_ATTR = {
	[RESP_REPEAT] = {
		.name = "repeat",
		.type = RULE_UNSIGNED_INT,
		.required = 1,
		.detail = {
			.unsigned_int = {
				.min_val = 0,
				.max_val = 10
			}
		}
	},
	[RESP_SPEED] = {
		.name = "speed",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 8
			}
		}
	},
	[RESP_R] = {
		.name = "r",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
	[RESP_W] = {
		.name = "w",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
	[RESP_G] = {
		.name = "g",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
	[RESP_TIME] = {
		.name = "time",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 20,
		.max_len = 20
	},
};

static const Rule_T payload_rule_hex ICACHE_RODATA_ATTR = {
	.type = RULE_HEX,
	.min_len = 1,
	.max_len = 8
};

static const Rule_T payload_rule_len ICACHE_RODATA_ATTR = {
	.type = RULE_UNSIGNED_INT,
	.detail = {
			.unsigned_int = {
					.min_val = 0,
					.max_val = PAYLOAD_BUFFER_SIZE - 1
			}
	}
};

static uint8_t ICACHE_FLASH_ATTR payload_udp_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value);
static uint8_t ICACHE_FLASH_ATTR payload_send_ns_event(Payload_T p);

static Payload_Action_T ICACHE_FLASH_ATTR payload_action_new(Payload_T payload, uint8_t local, Btn_Action_T action) {
	Payload_Action_T payload_action;
	if (!payload) {
		return NULL;
	}
	if (!(payload_action = malloc(sizeof(struct Payload_Action_T)))) {
		return NULL;
	}
	memset(payload_action, 0, sizeof(struct Payload_Action_T));
	payload_action->payload = payload;
	payload_action->local = local ? 1 : 0;
	payload_action->action = action;
	return payload_action;
}

static void ICACHE_FLASH_ATTR payload_action_set_items_items_count(Payload_Action_T pa, uint16_t count) {
	if (!pa) {
		return;
	}
	pa->items = count;
}

static uint8_t ICACHE_FLASH_ATTR payload_action_is_last_item(Payload_Action_T pa) {
	if (!pa) {
		return 0;
	}
	if (pa->items) {
		pa->items--;
	}
	if (!pa->items) {
		return 1;
	}
	return 0;
}

static void ICACHE_FLASH_ATTR payload_action_delete(Payload_Action_T payload_action) {
	if (!payload_action) {
		return;
	}
	if (payload_action->buffer) {
		buffer_delete(payload_action->buffer);
	}
	free(payload_action);
}

static void ICACHE_FLASH_ATTR payload_action_peri_blink(Payload_Action_T pa, uint8_t r, uint8_t w, uint8_t g, uint8_t speed, uint8_t repeat) {
	if (!pa) {
		return;
	}
	switch (pa->action) {
		case BTN_ACTION_SHORT:
		case BTN_ACTION_DOUBLE:
		case BTN_ACTION_LONG:
		case BTN_ACTION_TOUCH:
		case BTN_ACTION_WHEEL_FINAL:
			break;
		default:
			return;
	}
	peri_blink(r, w, g, speed, repeat);
}

static uint8_t ICACHE_FLASH_ATTR payload_action_blink(Payload_Action_T payload_action, uint8_t error) {
	if (!payload_action) {
		return 0;
	}
	if (payload_action->handled) {
		return 0;
	}
	if (!payload_action->local) {
		return 0;
	}
	if (error) {
		payload_action_peri_blink(payload_action, 1, 0, 0, 4, 1);
	} else {
		payload_action_peri_blink(payload_action, 0, 0, 1, 4, 1);
	}
	payload_action->handled = 1;
	return 1;
}

static Buffer_T ICACHE_FLASH_ATTR payload_action_buffer(Payload_Action_T payload_action) {
	if (!payload_action) {
		return NULL;
	}
	if (!payload_action->buffer) {
		debug_describe_P("PA new buffer");
		payload_action->buffer = buffer_new(PAYLOAD_BUFFER_SIZE);
	}
	return payload_action->buffer;
}

static void ICACHE_FLASH_ATTR payload_done(void* owner, uint8_t error) {
	Payload_Action_T pa;
	Payload_T p;
	uint8_t local;
	if (!(pa = owner) || !(p = pa->payload)) {
		return;
	}
	debug_describe_P("---Payload done");
	local = pa->local;
	payload_action_blink(pa, error);
	if (payload_action_is_last_item(pa)) {
		payload_action_delete(pa);
		list_remove(p->ns_queue, 0);
		payload_send_ns_event(p);
	}
}

static uint8_t ICACHE_FLASH_ATTR payload_parse_status(Buffer_T buffer, uint16_t* ret_code) {
	Slash_T slash;
	char* version;
	char* code;
	char* describe;
	int32_t ret_val;
	if (!buffer) {
		return 0;
	}
	if (!buffer_size(buffer)) {
		debug_describe_P("Bad status size");
		return 0;
	}
	slash_init(&slash, buffer_string(buffer), ' ');
	if (!(version = slash_next(&slash)) ||
		!(code = slash_next(&slash)) ||
		!(describe = slash_current(&slash))) {
			debug_describe_P("Bad status format");
			return 0;
	}
	debug_printf("Version: %s\n", version);
	debug_printf("Code: %s\n", code);
	debug_printf("Describe: %s\n", describe);
	if (!rule_check_digit(code, 100, 599, &ret_val)) {
		debug_describe_P("Bad code");
		return 0;
	}
	if (code) {
		*ret_code = ret_val;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR payload_check_header(Buffer_T buffer, uint8_t* chunked, uint32_t* len) {
	Slash_T slash;
	char* name;
	char* value;
	if (!buffer || !buffer_size(buffer)) {
		return 0;
	}
	slash_init_delimeters(&slash, buffer_string(buffer), ": ");
	if (!(name = slash_next(&slash)) ||
		!(value = slash_current(&slash))) {
			debug_describe_P("Bad header format");
			return 0;
	}
	if (chunked && !strcasecmp(name, "Transfer-Encoding")) {
		if (!strcmp(value, "chunked")) {
			*chunked = 1;
		}
		return 1;
	}
	if (len && !strcasecmp(name, "Content-Length")) {
		Item_T item;
		if (!rule_check(&payload_rule_len, value, &item)) {
			debug_describe_P("Rule lenght check error");
			return 0;
		}
		*len = item.data_uint32;
		return 1;
	}
	return 1;
}

/*
static const char* test_json =	"{"
								"\"repeat\": 3,"
								"\"speed\": 2,"
								"\"r\": false,"
								"\"w\": false,"
								"\"g\": true,"
								"}";
*/

static uint8_t ICACHE_FLASH_ATTR payload_recv(void* owner, uint8_t* data, uint32_t len) {
	struct Value_T values[__RESP_MAX];
	Buffer_T buffer;
	Payload_Action_T pa;
	Payload_T p;
	Json_T json = NULL;
	uint64_t utc_time;
	uint32_t i;
	uint8_t final = 0;
	if (!(pa = owner) || !(p = pa->payload)) {
		return 0;
	}
	debug_printf("Payload recv: %uB\n", len);
	if (!(buffer = payload_action_buffer(pa))) {
		debug_describe_P("Can not create PA buffer");
		peri_set_white(0);
		return 0;
	}
	for (i = 0; i < len; i++) {
		if (!buffer_write(buffer, data[i])) {
			final = 1;
		}
		if (pa->body) {
			if (pa->chunked) {
				if (pa->read) {
					pa->read--;
				}
				if (!pa->read) {
					if (buffer_equal_from_end_str(buffer, CRLF)) {
						Item_T item;
						if (buffer_size(buffer) == (pa->len + 2)) {
							buffer_set_writer(buffer, buffer_size(buffer) - 2);
							continue;
						}
						buffer_set_writer(buffer, buffer_size(buffer) - 2);
						buffer_close(buffer);
						if (!rule_check(&payload_rule_hex, (char*)buffer_data(buffer, pa->len), &item)) {
							debug_describe_P("Rule chunk len check error");
							debug_describe((char*)buffer_data(buffer, pa->len));
							goto error;
						}
						buffer_set_writer(buffer, pa->len);
						pa->read = item.data_uint32;
						pa->len += pa->read;
						if (!pa->read) {
							debug_describe_P("End chunk");
							final = 1;
						}
					}
				}
				continue;
			}
			if (buffer_size(buffer) >= pa->len) {
				final = 1;
			}
			continue;
		}
		if (buffer_equal_from_end_str(buffer, CRLF)) {
			if (buffer_size(buffer) == 2) {
				if (pa->chunked && pa->len) {
					goto error;
				}
				if (!pa->chunked && !pa->len) {
					final = 1;
				}
				pa->body = 1;
				debug_describe_P("Body");
			} else {
				buffer_set_writer(buffer, buffer_size(buffer) - 2);
				if (!pa->headers) {
					if (!payload_parse_status(buffer, &pa->code)) {
						goto error;
					}
					pa->headers = 1;
				} else {
					debug_printf("Header: %s\n", buffer_string(buffer));
					if (!payload_check_header(buffer, &pa->chunked, &pa->len)) {
						debug_describe_P("Bad header");
						goto error;
					}
				}
			}
			buffer_clear(buffer);
		}
	}
	if (!pa->body || !final) {
		return 1;
	}
	//buffer_set_writer(buffer, pa->len);
	debug_describe(buffer_string(buffer));
	if (payload_action_blink(pa, pa->code != 200)) {
		debug_describe_P("Local resp");
		return 0;
	}
	if (pa->code != 200) {
		debug_describe_P("Bad code");
		goto error1;
	}
	if (!buffer_size(buffer)) {
		debug_describe_P("Empty resp");
		goto error1;
	}
	if (!(json = json_parse(buffer_string(buffer), payload_resp_rule, __RESP_MAX))) {
		debug_describe_P("Bad resp json");
		goto error1;
	}
	if (!json_to_values(json, values)) {
		debug_describe_P("Invalid json");
		goto error1;
	}
	if (rule_check_utc_time(values[RESP_TIME].string_value, &utc_time)) {
		sleep_update_timestamp(utc_time);
	}
	payload_action_peri_blink(pa,
		values[RESP_R].bool_value,
		values[RESP_W].bool_value,
		values[RESP_G].bool_value,
		values[RESP_SPEED].uint_value,
		values[RESP_REPEAT].uint_value);
	json_delete(json);
	return 0;
error1:
	if (pa->code != 200) {
		payload_action_peri_blink(pa, 1, 0, 0, 4, 1);
	} else {
		payload_action_peri_blink(pa, 0, 0, 1, 4, 1);
	}
error:
	debug_describe_P("PA error response from server");
	if (json) {
		json_delete(json);
	}
	return 0;
}

Payload_T ICACHE_FLASH_ATTR payload_new(void) {
	Payload_T p;
	if (!(p = malloc(sizeof(struct Payload_T)))) {
		return NULL;
	}
	memset(p, 0, sizeof(struct Payload_T));
	if (!(p->ns = ns_new())) {
		free(p);
		return NULL;
	}
	if (!(p->udp_events = list_new(sizeof(Udp_Event_T)))) {
		ns_delete(p->ns);
		free(p);
		return NULL;
	}
	if (!(p->ns_queue = list_new(sizeof(Payload_Ns_Event_T)))) {
		list_delete(p->udp_events);
		ns_delete(p->ns);
		free(p);
		return NULL;
	}
	ns_timeout(p->ns, 7500);
	return p;
}

static void ICACHE_FLASH_ATTR payload_event_delete(Udp_Event_T event) {
	if (!event) {
		return;
	}
	debug_describe_P("UDP event delete");
	os_timer_disarm(&event->timeout);
	espconn_delete(&event->conn);
	free(event);
}

static void ICACHE_FLASH_ATTR payload_udp_timeout(void* arg) {
	Udp_Event_T event = (struct Udp_Event_T*)arg;
	if (!event) {
		return;
	}
	debug_describe_P("UDP event response timeout");
	payload_event_delete(event);
}

static void ICACHE_FLASH_ATTR payload_udp_send(void* arg) {
	Udp_Event_T event = (struct Udp_Event_T*)arg;
	if (!event) {
		return;
	}
	debug_describe_P("UDP event send");
	os_timer_disarm(&event->timeout);
	os_timer_setfn(&event->timeout, payload_udp_timeout, event);
	os_timer_arm(&event->timeout, 1000, 1);
}

static void ICACHE_FLASH_ATTR payload_udp_recv(void *arg, char* data, unsigned short len) {
	Udp_Event_Item_T item;
	Udp_Event_T event = (struct Udp_Event_T*)arg;
	if (!event) {
		return;
	}
	debug_describe_P("UDP event read");
	os_timer_disarm(&event->timeout);
	if (!len || !data) {
		goto error0;
	}
	if (len != sizeof(Udp_Event_Item_T)) {
		goto error0;
	}
	memcpy(&item, data, sizeof(Udp_Event_Item_T));
	if (memcmp(item.mac, event->item.mac, 6)) {
		goto error0;
	}
	if (item.type != 255) {
		goto error0;
	}
	peri_blink(0, 0, 1, 8, 1);
	debug_describe_P("UDP event OK");
	payload_event_delete(event);
	return;
error0:
	debug_describe_P("Bad UDP event response");
	payload_event_delete(event);
}

static Udp_Event_T ICACHE_FLASH_ATTR payload_event_new(uint8_t type, uint16_t value) {
	Udp_Event_T event;
	int8_t ret;
	if (!(event = (void*)malloc(sizeof(struct Udp_Event_T)))) {
		return NULL;
	}
	memset(event, 0, sizeof(struct Udp_Event_T));
	event->conn.type = ESPCONN_UDP;
	event->conn.state = ESPCONN_NONE;
	event->conn.proto.udp = &event->udp_conn;
	event->conn.proto.udp->remote_ip[0] = 255;
	event->conn.proto.udp->remote_ip[1] = 255;
	event->conn.proto.udp->remote_ip[2] = 255;
	event->conn.proto.udp->remote_ip[3] = 255;
	event->conn.proto.udp->local_port = PAYLOAD_EVENT_PORT;
	event->conn.proto.udp->remote_port = PAYLOAD_EVENT_PORT;
	event->conn.reverse = event;
	espconn_regist_recvcb(&event->conn, payload_udp_recv);
	espconn_regist_sentcb(&event->conn, payload_udp_send);
	wifi_get_macaddr(STATION_IF, event->item.mac);
	event->item.type = type;
	event->item.value = value;
	if ((ret = espconn_create(&event->conn))) {
		if (ret == ESPCONN_ISCONN) {
			debug_describe_P("UDP EVENT aleady connected");
			return event;
		}
		debug_describe_P("Unable to create UDP EVENT connection");
		free(event);
		return NULL;
	}
	return event;
}

static uint8_t ICACHE_FLASH_ATTR payload_event_send(Udp_Event_T event) {
	int8_t ret;
	if (!event) {
		return 0;
	}
	if ((ret = espconn_sendto(&event->conn, (uint8_t*)&event->item, sizeof(event->item)))) {
		if (ret == ESPCONN_ISCONN) {
			debug_describe_P("Send broadcast event data but already connected");
			return 1;
		}
		debug_describe_P("Unable send broadcast event data");
		payload_event_delete(event);
		return 0;
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR payload_udp_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value) {
	Udp_Event_T event;
	uint8_t type = 0;
	if (!p) {
		return 0;
	}
	switch (action) {
		case BTN_ACTION_SHORT:
			type = 0;
			break;
		case BTN_ACTION_DOUBLE:
			type = 1;
			break;
		case BTN_ACTION_LONG:
			type = 2;
			break;
		default:
			return 0;
	}
	if (!(event = payload_event_new(type, value))) {
		return 0;
	}
	if (p->connected) {
		return payload_event_send(event);
	}
	if (!list_add(p->udp_events, &event)) {
		payload_event_delete(event);
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR payload_send_ns_event(Payload_T p) {
	Payload_Ns_Event_T event;
	Payload_Action_T pa = NULL;
	char* url = NULL;
	uint16_t count = 0;
	if (!p) {
		return 0;
	}
	if (!list_read(p->ns_queue, 0, &event)) {
		return 0;
	}
	payload_udp_action(p, NULL, event.action, 0);
	if (!(url = url_storage_read(&url_storage, event.type))) {
		goto error;
	}
	if (!(pa = payload_action_new(p, event.local, event.action))) {
		debug_describe_P("Can not create PA");
		goto error;
	}
	count = ns_multi_query(p->ns, url, event.args, pa, payload_recv, payload_done);
	if (!count) {
		goto error;
	}
	payload_action_set_items_items_count(pa, count);
	free(url);
	free(event.args);
	return 1;
error:
	free(event.args);
	free(url);
	payload_action_delete(pa);
	list_remove(p->ns_queue, 0);
	return payload_send_ns_event(p);
}

static uint8_t ICACHE_FLASH_ATTR payload_add_ns_event(Payload_T p, Url_Storage_Type_T type,
		const char* args, Btn_Action_T action, uint8_t local) {
	Payload_Ns_Event_T event;
	if (!p || (type >= __URL_TYPE_MAX)) {
		return 0;
	}
	if (list_size(p->ns_queue) >= 5) {
		debug_describe_P("Exceed queue limit");
		return 0;
	}
	memset(&event, 0, sizeof(event));
	event.type = type;
	if (args) {
		event.args = strdup(args);
	}
	event.action = action;
	event.local = local;
	if (!list_add(p->ns_queue, &event)) {
		free(event.args);
		return 0;
	}
	if (list_size(p->ns_queue) == 1) {
		return payload_send_ns_event(p);
	}
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR payload_local_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value) {
	char args[32];
	Url_Storage_Type_T type;
	uint8_t ret_val = 0;
	if (!p) {
		return 0;
	}
	switch (action) {
		case BTN_ACTION_SHORT:
		case BTN_ACTION_DOUBLE:
		case BTN_ACTION_LONG:
		case BTN_ACTION_TOUCH:
		case BTN_ACTION_WHEEL:
		case BTN_ACTION_WHEEL_FINAL:
			break;
		default:
			return 0;
	}
	memset(args, 0, sizeof(args));
	switch (action) {
		case BTN_ACTION_SHORT:
			type = URL_TYPE_SINGLE;
			break;

		case BTN_ACTION_DOUBLE:
			type = URL_TYPE_DOUBLE;
			break;

		case BTN_ACTION_LONG:
			type = URL_TYPE_LONG;
			break;

		case BTN_ACTION_TOUCH:
			type = URL_TYPE_TOUCH;
			break;
			
		case BTN_ACTION_WHEEL:
		case BTN_ACTION_WHEEL_FINAL: {
			struct Buffer_T buffer;
			type = URL_TYPE_TOUCH;
			buffer_init(&buffer, sizeof(args), (uint8_t*)args);
			buffer_puts(&buffer, "wheel=");
			buffer_dec(&buffer, (char)(value & 0xFF));
			buffer_close(&buffer);
		}
		break;

		default:
			ret_val = 0;
			break;
	}
	return payload_add_ns_event(p, type, strlen(args) ? args : NULL, action, 1);
}

static uint8_t ICACHE_FLASH_ATTR payload_general_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value) {
	uint8_t storage[96];
	struct Buffer_T buffer;
	if (!p) {
		return 0;
	}
	switch (action) {
		case BTN_ACTION_SHORT:
		case BTN_ACTION_DOUBLE:
		case BTN_ACTION_LONG:
		case BTN_ACTION_TOUCH:
		case BTN_ACTION_WHEEL:
		case BTN_ACTION_WHEEL_FINAL:
		case BTN_ACTION_BATTERY:
			break;
		default:
			return 0;
	}
	buffer_init(&buffer, sizeof(storage), storage);
	buffer_puts(&buffer, "mac=");
	buffer_puts(&buffer, mac);
	buffer_puts(&buffer, "&action=");
	buffer_dec(&buffer, action);
	if (action == BTN_ACTION_WHEEL) {
		buffer_puts(&buffer, "&wheel=");
		buffer_dec(&buffer, (char)(value & 0xFF));
	}
	buffer_puts(&buffer, "&battery=");
	if (action == BTN_ACTION_BATTERY) {
		buffer_dec(&buffer, peri_battery_percentage(peri_battery_voltage(value)));
	} else {
		buffer_dec(&buffer, peri_battery_percentage(peri_battery()));
	}
	if (store.bssid_enable) {
		struct station_config sta_config;
		memset(&sta_config, 0, sizeof(sta_config));
		if (wifi_station_get_config(&sta_config)) {
			buffer_puts(&buffer, "&bssid=");
			buffer_puts_mac(&buffer, sta_config.bssid);
		}
	}
	return payload_add_ns_event(p, URL_TYPE_GENERIC, buffer_string(&buffer), action, 1);
}

uint8_t ICACHE_FLASH_ATTR payload_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value) {
	uint8_t ret_local;
	uint8_t ret_general;
	uint8_t ret_udp = 0;
	ret_local = payload_local_action(p, mac, action, value);
	//ret_udp = payload_udp_action(p, mac, action, value);
	ret_general = payload_general_action(p, mac, action, value);
	if (!ret_local && !ret_udp && !ret_general) {
		switch (action) {
			case BTN_ACTION_SHORT:
			case BTN_ACTION_DOUBLE:
			case BTN_ACTION_LONG:
			case BTN_ACTION_TOUCH:
			case BTN_ACTION_WHEEL_FINAL:
				peri_blink(1, 0, 0, 4, 1);
				break;

			default:
				break;
		}
	}
	return ret_local;
}

uint8_t ICACHE_FLASH_ATTR payload_connect(Payload_T p, uint8_t yes) {
	Udp_Event_T event;
	if (!p) {
		return 0;
	}
	p->connected = yes;
	if (p->connected) {
		list_rewind(p->udp_events);
		while (list_next(p->udp_events, &event)) {
			payload_event_send(event);
		}
		list_clear(p->udp_events);
	}
	return ns_connect(p->ns, yes);
}

uint16_t ICACHE_FLASH_ATTR payload_size(Payload_T p) {
	if (!p) {
		return 0;
	}
	return ns_size(p->ns);
}

uint8_t ICACHE_FLASH_ATTR payload_connected(Payload_T p) {
	if (!p) {
		return 0;
	}
	return ns_connected(p->ns);
}
