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
#include "collect.h"
#include "debug.h"
#include "crc.h"
#include "store.h"
#include "array_size.h"
#include "utils.h"
#include "color.h"
#include "peri.h"
#include "url_storage.h"
#include "version.h"

#define COLLECT_CRC 0x741B8CD7
#define COLLECT_PORT 7979

typedef Collect_Item_T Collect_Array_T;

struct Collect_T {
	struct espconn conn;
	esp_udp udp_conn;
	os_timer_t send_timer;
	os_timer_t write_timer;
	uint8_t last_updated : 1;
	uint8_t it : 6;
	Collect_Item_T items[1];
};

extern struct Store_T store;
extern char own_mac[13];
extern Url_Storage_T url_storage;

static const Collect_Item_T collect_null_item = {
	.mac = {0, 0, 0, 0, 0, 0},
	.type = 0,
	.child = 0
};

static uint8_t ICACHE_FLASH_ATTR collect_find_empty_mac(Collect_T collect, uint8_t* index) {
	uint16_t i;
	if (!collect || !index) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(collect->items); i++) {
		if (!memcmp(&collect->items[i], &collect_null_item, sizeof(collect_null_item))) {
			*index = i;
			return 1;
		}
	}
	return 0;
}

static uint8_t ICACHE_FLASH_ATTR collect_is_mac_exist(Collect_T collect, uint8_t* mac, uint8_t* index) {
	uint16_t i;
	if (!collect) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(collect->items); i++) {
		if (!memcmp(collect->items[i].mac, mac, 6)) {
			if (index) {
				*index = i;
			}
			return 1;
		}
	}
	return 0;
}

static uint8_t ICACHE_FLASH_ATTR collect_is_mac_exist_by_last(Collect_T collect, uint8_t* mac, uint8_t* index) {
	uint16_t i;
	if (!collect) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(collect->items); i++) {
		if (!memcmp(collect->items[i].mac + 3, mac + 3, 3)) {
			if (index) {
				*index = i;
			}
			return 1;
		}
	}
	return 0;
}

static void ICACHE_FLASH_ATTR collect_send_task(void* owner) {
	Collect_T collect = owner;
	uint8_t index;
	if (!collect) {
		return;
	}
	/*update item of child info*/
	index = ARRAY_SIZE(collect->items);
	collect_find_empty_mac(collect, &index);
	collect->conn.proto.udp->remote_port = COLLECT_PORT;
	collect->conn.proto.udp->remote_ip[0] = 255;
	collect->conn.proto.udp->remote_ip[1] = 255;
	collect->conn.proto.udp->remote_ip[2] = 255;
	collect->conn.proto.udp->remote_ip[3] = 255;
	if (espconn_sendto(&collect->conn, (uint8_t*)collect->items, sizeof(Collect_Item_T) * index)) {
		debug_describe_P("Unable send broadcast data");
		os_timer_disarm(&collect->send_timer);
		os_timer_setfn(&collect->send_timer, collect_send_task, collect);
		os_timer_arm(&collect->send_timer, 5000, 0);
		return;
	}
}

static void ICACHE_FLASH_ATTR collect_send(void* arg) {
	Collect_T collect;
	struct espconn* conn = (struct espconn*)arg;
	if (!conn) {
		return;
	}
	collect = conn->reverse;
	if (!collect) {
		return;
	}
	os_timer_disarm(&collect->send_timer);
	os_timer_setfn(&collect->send_timer, collect_send_task, collect);
	os_timer_arm(&collect->send_timer, 5000, 0);
}

static void ICACHE_FLASH_ATTR collect_recv(void *arg, char* data, unsigned short len) {

}

uint8_t ICACHE_FLASH_ATTR collect_size(Collect_T collect) {
	uint8_t index = 0;
	if (!collect) {
		return 0;
	}
	collect_find_empty_mac(collect, &index);
	return index;
}

Collect_T ICACHE_FLASH_ATTR collect_new(void) {
	Collect_T collect;
	if (!(collect = (void*)malloc(sizeof(struct Collect_T)))) {
		return NULL;
	}
	memset(collect, 0, sizeof(struct Collect_T));
	collect->conn.type = ESPCONN_UDP;
	collect->conn.state = ESPCONN_NONE;
	collect->conn.proto.udp = &collect->udp_conn;
	collect->conn.proto.udp->remote_ip[0] = 255;
	collect->conn.proto.udp->remote_ip[1] = 255;
	collect->conn.proto.udp->remote_ip[2] = 255;
	collect->conn.proto.udp->remote_ip[3] = 255;
	collect->conn.proto.udp->local_port = COLLECT_PORT;
	collect->conn.proto.udp->remote_port = COLLECT_PORT;
	collect->conn.reverse = collect;
	espconn_regist_recvcb(&collect->conn, collect_recv);
	espconn_regist_sentcb(&collect->conn, collect_send);
	os_timer_disarm(&collect->send_timer);
	os_timer_setfn(&collect->send_timer, collect_send_task, collect);
	wifi_get_macaddr(STATION_IF, collect->items[0].mac);
	if (espconn_create(&collect->conn)) {
		debug_describe_P("Unable to create COLLECT connection");
		free(collect);
		return NULL;
	}
	return collect;
}

void ICACHE_FLASH_ATTR collect_stop(Collect_T collect) {
	if (!collect) {
		return;
	}
	os_timer_disarm(&collect->send_timer);
}

void ICACHE_FLASH_ATTR collect_start(Collect_T collect) {
	if (!collect) {
		return;
	}
	os_timer_disarm(&collect->send_timer);
	os_timer_setfn(&collect->send_timer, collect_send_task, collect);
	os_timer_arm(&collect->send_timer, 5000, 0);
}

void ICACHE_FLASH_ATTR collect_delete(Collect_T collect) {
	if (!collect) {
		return;
	}
	os_timer_disarm(&collect->send_timer);
	espconn_delete(&collect->conn);
	free(collect);
}

Json_T ICACHE_FLASH_ATTR collect_get_device(Collect_T collect, const char* mac) {
	uint8_t storage[64];
	struct Buffer_T buffer;
	Collect_Item_T* item;
	Json_T obj = NULL;
	char* url = NULL;
	uint8_t root;
	if (!collect || !mac) {
		return NULL;
	}
	if (!(item = collect_get(collect, mac))) {
		return NULL;
	}
	root = !strcasecmp(own_mac, mac) ? 1 : 0;
	if (!(obj = json_new())) {
		return NULL;
	}
	json_set_format(obj, 0, 1);
	buffer_init(&buffer, sizeof(storage), storage);
	json_add_string(obj, "type", "button");
	if (!root) {
		return obj;
	}
	json_add_bool(obj, "battery", 1);
	json_add_bool(obj, "reachable", 1);
	json_add_bool(obj, "charge", peri_is_charge());
	json_add_real(obj, "voltage", peri_battery());
	json_add_string(obj, "fw_version", APP_VERSION);

	url = url_storage_read(&url_storage, URL_TYPE_SINGLE);
	json_add_string(obj, "single", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_DOUBLE);
	json_add_string(obj, "double", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_LONG);
	json_add_string(obj, "long", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_GENERIC);
	json_add_string(obj, "generic", url ? url : "");
	free(url);

	json_add_string(obj, "name", store.name);
	return obj;
}

Json_T ICACHE_FLASH_ATTR collect_get_status(Collect_T collect, const char* mac) {
	uint8_t storage[64];
	struct Buffer_T buffer;
	Collect_Item_T* item;
	Json_T obj;
	uint8_t root;
	if (!collect || !mac) {
		return NULL;
	}
	if (!(item = collect_get(collect, mac))) {
		return NULL;
	}
	if (!(obj = json_new())) {
		return NULL;
	}
	buffer_init(&buffer, sizeof(storage), storage);
	root = !strcasecmp(own_mac, mac) ? 1 : 0;
	if (root) {
		return obj;
	}
	json_delete(obj);
	return NULL;
}

Json_T ICACHE_FLASH_ATTR collect_get_devices(Collect_T collect) {
	Json_T obj;
	Json_T item;
	char mac_str[13];
	uint8_t index;
	uint8_t i;
	if (!collect) {
		return NULL;
	}
	if (!(obj = json_new())) {
		return NULL;
	}
	index = ARRAY_SIZE(collect->items);
	collect_find_empty_mac(collect, &index);
	for (i = 0; i < index; i++) {
		memset(mac_str, 0, sizeof(mac_str));
		utils_mac_bytes_to_string((uint8_t*)mac_str, collect->items[i].mac);
		if (!(item = collect_get_device(collect, mac_str))) {
			continue;
		}
		json_add_obj(obj, mac_str, item);
	}
	return obj;
}

void ICACHE_FLASH_ATTR collect_unregister_all(Collect_T collect) {
	uint8_t i;
	uint8_t index;
	if (!collect) {
		return;
	}
	index = ARRAY_SIZE(collect->items);
	collect_find_empty_mac(collect, &index);
	for (i = 0; i < index; i++) {
		collect->items[i].registered = 0;
	}
}

Collect_Item_T* ICACHE_FLASH_ATTR collect_get(Collect_T collect, const char* mac) {
	uint8_t mac_bytes[6];
	uint8_t index = 0;
	if (!collect || !mac || !strlen(mac)) {
		return NULL;
	}
	utils_mac_string_to_bytes(mac, mac_bytes);
	if (!collect_is_mac_exist(collect, mac_bytes, &index)) {
		return NULL;
	}
	return &collect->items[index];
}

uint8_t ICACHE_FLASH_ATTR collect_belongs(Collect_T collect, const char* mac) {
	uint8_t mac_bytes[6];
	uint8_t index = 0;
	if (!collect || !mac || !strlen(mac)) {
		return 0;
	}
	utils_mac_string_to_bytes(mac, mac_bytes);
	if (!collect_is_mac_exist_by_last(collect, mac_bytes, &index)) {
		return 0;
	}
	if (!index) {
		return 0;
	}
	return 1;
}

Collect_Item_T* ICACHE_FLASH_ATTR collect_get_by_index(Collect_T collect, uint8_t id) {
	uint8_t index;
	if (!collect) {
		return NULL;
	}
	index = ARRAY_SIZE(collect->items);
	collect_find_empty_mac(collect, &index);
	if (id >= index) {
		return NULL;
	}
	return &collect->items[id];
}

uint8_t ICACHE_FLASH_ATTR collect_get_info(Collect_T collect, Buffer_T buffer) {
	Collect_Item_T* item;
	char mac_str[13];
	uint8_t index;
	uint8_t i;
	if (!collect || !buffer) {
		return 0;
	}
	index = ARRAY_SIZE(collect->items);
	collect_find_empty_mac(collect, &index);
	for (i = 0; i < index; i++) {
		if (!(item = collect_get_by_index(collect, i))) {
			continue;
		}
		memset(mac_str, 0, sizeof(mac_str));
		utils_mac_bytes_to_string((uint8_t*)mac_str, item->mac);
		buffer_puts(buffer, mac_str);
		buffer_write(buffer, '=');
		buffer_dec(buffer, item->type);
		buffer_write(buffer, ':');
		if (!i) {
			buffer_puts(buffer, "THIS;");
		} else {
			if (item->child) {
				buffer_puts(buffer, "SLAVE;");
			} else {
				buffer_puts(buffer, "OTHER;");
			}
		}
		if (item->registered) {
			buffer_puts(buffer, "REG;");
		} else {
			buffer_puts(buffer, "UNREG;");
		}
		buffer_puts(buffer, CRLF);
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR collect_erase(Collect_T collect) {
	return 1;
}

uint8_t ICACHE_FLASH_ATTR collect_item_get_type(Collect_Item_T* item) {
	if (!item) {
		return 0;
	}
	return item->type;
}

uint8_t ICACHE_FLASH_ATTR collect_item_is_child(Collect_Item_T* item) {
	if (!item) {
		return 0;
	}
	return item->child;
}

void ICACHE_FLASH_ATTR collect_item_register(Collect_Item_T* item) {
	if (!item) {
		return;
	}
	item->registered = 1;
}

void ICACHE_FLASH_ATTR collect_item_unregister(Collect_Item_T* item) {
	if (!item) {
		return;
	}
	item->registered = 0;
}

uint8_t ICACHE_FLASH_ATTR collect_item_registered(Collect_Item_T* item) {
	if (!item) {
		return 0;
	}
	return item->registered;
}
