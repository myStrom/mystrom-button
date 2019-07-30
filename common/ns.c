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
#include <stdio.h>
#include <stdlib.h>
#include <user_interface.h>
#include <osapi.h>
#include <ctype.h>
#include <espconn.h>
#include "ns.h"
#include "notify.h"
#include "debug.h"
#include "color.h"
#include "list.h"
#include "array_size.h"
#include "slash.h"
#include "rule.h"
#ifdef BUTTON
#include "sleep.h"
#endif

typedef struct {
	char* url;
	const char* body;
	char* args;
	char* headers;
	Json_T json;
	void* owner;
	Ns_Recv_Cb recv_cb;
	Ns_Done_Cb done_cb;
	Ns_Method_T method;
} Ns_Item_T;

struct Ns_T {
	Http_Request_T event[3];
	List_T list;
	Notify_T notify;
	void* owner;
	Ns_Recv_Cb recv_cb;
	Ns_Done_Cb done_cb;
	uint32_t timeout;
	uint8_t connected : 1;
	uint8_t done : 1;
};

static void ICACHE_FLASH_ATTR ns_send_done_cb(Notify_T notify, uint8_t error);
static uint8_t ICACHE_FLASH_ATTR ns_recv_cb(Notify_T notify, uint8_t* data, uint32_t len);
static uint8_t ICACHE_FLASH_ATTR ns_send(Ns_T ns);

Ns_T ICACHE_FLASH_ATTR ns_new(void) {
	Ns_T ns;
	if (!(ns = malloc(sizeof(struct Ns_T)))) {
		return NULL;
	}
	memset(ns, 0, sizeof(struct Ns_T));
	ns->timeout = 10000;
	if (!(ns->list = list_new(sizeof(Ns_Item_T)))) {
		free(ns);
		return NULL;
	}
	return ns;
}

void ICACHE_FLASH_ATTR ns_delete(Ns_T ns) {
	if (!ns) {
		return;
	}
	list_delete(ns->list);
	free(ns);
}

void ICACHE_FLASH_ATTR ns_timeout(Ns_T ns, uint32_t time_ms) {
	if (!ns) {
		return;
	}
	ns->timeout = time_ms;
}

static void ICACHE_FLASH_ATTR ns_done(Ns_T ns, uint8_t error) {
	if (!ns) {
		return;
	}
	if (ns->done || error) {
		debug_describe_P("NS done");
		Ns_Done_Cb done_cb = ns->done_cb;
		void* owner = ns->owner;
		ns->done = 0;
		ns->recv_cb = NULL;
		ns->done_cb = NULL;
		ns->owner = NULL;
		if (ns->notify) {
			notify_delete(ns->notify);
			ns->notify = NULL;
		}
		if (done_cb) {
			debug_describe_P("NS make done cb");
			done_cb(owner, error);
		} else {
			debug_describe_P("NS no done cb");
		}
		#ifdef BUTTON
		sleep_unlock(SLEEP_NS);
		#endif
		debug_value(system_get_free_heap_size());
		ns_send(ns);
	} else {
		debug_describe_P("NS doing");
		ns->done = 1;
	}
}

static uint8_t ICACHE_FLASH_ATTR ns_send(Ns_T ns) {
	Ns_Item_T item;
	uint8_t ret_val = 0;
	if (!ns) {
		return 0;
	}
	if (!ns->connected) {
		return 1;
	}
	if (ns->notify) {
		return 1;
	}
	if (!list_read(ns->list, 0, &item)) {
		return 0;
	}
	list_remove(ns->list, 0);
	ns->recv_cb = item.recv_cb;
	ns->done_cb = item.done_cb;
	ns->owner = item.owner;
	ns->done = 0;
	if (item.json) {
		struct Buffer_T test;
		uint8_t storage[8];
		Buffer_T buffer;
		uint16_t json_len;
		buffer_init(&test, sizeof(storage), storage);
		json_print(item.json, &test, 0);
		json_len = buffer_overflow(&test) + buffer_capacity(&test);
		debug_value(json_len);
		if ((buffer = buffer_new(json_len + 1))) {
			json_print(item.json, buffer, 0);
			json_delete(item.json);
			ns->notify = notify_new(item.url,
									item.headers,
									buffer_string(buffer),
									NULL,
									ns_send_done_cb,
									ns->recv_cb ? ns_recv_cb : NULL,
									ns,
									ns->timeout,
									item.method);
			buffer_delete(buffer);
		} else {
			debug_describe_P("!!!No space left for NS buffer");
			json_delete(item.json);
		}
	} else {
		if (item.body) {
			ns->notify = notify_new(item.url,
									item.headers,
									item.body,
									item.args,
									ns_send_done_cb,
									ns->recv_cb ? ns_recv_cb : NULL,
									ns,
									ns->timeout,
									item.method);
		} else {
			ns->notify = notify_new(item.url,
									item.headers,
									NULL,
									item.args,
									ns_send_done_cb,
									ns->recv_cb ? ns_recv_cb : NULL,
									ns,
									ns->timeout,
									item.method);
		}
	}
	free(item.headers);
	free(item.url);
	free(item.args);
	#ifdef BUTTON
		if (ns->notify) {
			sleep_lock(SLEEP_NS);
		}
	#endif
	if (ns->notify) {
		ret_val = 1;
	}
	ns_done(ns, ret_val ? 0 : 1);
	return ret_val;
}

static void ICACHE_FLASH_ATTR ns_send_done_cb(Notify_T notify, uint8_t error) {
	Ns_T ns;
	if (!notify || !(ns = notify_owner(notify))) {
		return;
	}
	debug_describe_P("Notify send done cb");
	ns_done(ns, error);
}

static uint8_t ICACHE_FLASH_ATTR ns_recv_cb(Notify_T notify, uint8_t* data, uint32_t len) {
	Ns_T ns;
	if (!notify || !(ns = notify_owner(notify))) {
		return 0;
	}
	debug_describe_P("NS recv");
	if (ns->recv_cb) {
		return ns->recv_cb(ns->owner, data, len);
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR ns_post(Ns_T ns,
	const char* url, Json_T json,
	void* owner,
	Ns_Recv_Cb recv_cb,
	Ns_Done_Cb done_cb)
{
	Ns_Item_T item;
	uint8_t i;
	if (!ns || !json) {
		return 0;
	}
	debug_describe_P(COLOR_YELLOW "Post event" COLOR_END);
	for (i = 0; i < ARRAY_SIZE(ns->event); i++) {
		if (!ns->event[i]) {
			continue;
		}
		debug_describe_P("Send event over http");
		http_req_send_chunked_json(ns->event[i], json);
	}
	if (!url || !strlen(url)) {
		json_delete(json);
		return 1;
	}
	if (list_size(ns->list) >= 5) {
		debug_describe_P("Error: NS queue full");
		return 0;
	}
	memset(&item, 0, sizeof(Ns_Item_T));
	item.json = json;
	item.owner = owner;
	item.recv_cb = recv_cb;
	item.done_cb  = done_cb;
	item.method = NS_METHOD_POST;
	if (!(item.url = strdup(url))) {
		return 0;
	}
	if (!(item.headers = strdup("Content-Type: application/json"))) {
		free(item.url);
		return 0;
	}
	if (!(list_add(ns->list, &item))) {
		free(item.url);
		return 0;
	}
	ns_send(ns);
	return 1;
}

uint16_t ICACHE_FLASH_ATTR ns_multi_query(Ns_T ns, char* url, const char* args,
		void* owner, Ns_Recv_Cb recv_cb, Ns_Done_Cb done_cb) {
	char* begin;
	char* part;
	uint16_t count = 0;
	if (!ns || !url || !strlen(url)) {
		return 0;
	}
	begin = url;
	while (begin && *begin) {
		if ((part = strstr(begin, "||"))) {
			while ((*part) == '|') {
				*part = '\0';
				part++;
			}
		}
		if (!ns_query(ns, begin, args, owner, recv_cb, done_cb)) {
			goto done;
		}
		begin = part;
		count++;
	}
done:
	return count;
}

static uint8_t ICACHE_FLASH_ATTR ns_post_data_is_query_string(const char* data) {
	Slash_T slash_a;
	Slash_T slash_e;
	char* copy = NULL;
	char* part = NULL;
	char* key;
	uint32_t i;
	if (!data || (strlen(data) < 2)) {
		return 0;
	}
	if (!strchr(data, (int)'=')) {
		goto no;
	}
	if (data[0] == '=') {
		goto no;
	}
	if (data[0] == '&') {
		goto no;
	}
	if (strstr(data, "==")) {
		goto no;
	}
	if (strstr(data, "&&")) {
		goto no;
	}
	i = strlen(data);
	while (i) {
		i--;
		if (strchr("{}[]", (int)data[i])) {
			goto no;
		}
	}
	if (!(copy = strdup(data))) {
		goto no;
	}
	slash_init(&slash_a, copy, '&');
	while ((part = slash_next(&slash_a))) {
		if (strlen(part) < 2) {
			goto no;
		}
		if (part[0] == '=') {
			goto no;
		}
		if (!strchr(part, (int)'=')) {
			goto no;
		}
		if (rule_count_char(part, '=') != 1) {
			goto no;
		}
		slash_init(&slash_e, part, '=');
		key = slash_next(&slash_e);
		if (!key || !strlen(key)) {
			goto no;
		}
		i = strlen(key);
		while (i) {
			i--;
			if (!isalnum((int)key[i]) && (key[i] != '%')) {
				goto no;
			}
		}
	}
	free(copy);
	return 1;
no:
	free(copy);
		return 0;
	}

static uint8_t ICACHE_FLASH_ATTR ns_post_data_is_json(const char* data) {
	if (!data || !strlen(data)) {
		return 0;
	}
	if (!rule_char_in(data[0], "{[")) {
		return 0;
	}
	if (data[0] == '{') {
		if (data[strlen(data) - 1] != '}') {
			return 0;
		}
	} else {
		if (data[strlen(data) - 1] != ']') {
			return 0;
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR ns_query(Ns_T ns, const char* url, const char* args,
		void* owner, Ns_Recv_Cb recv_cb, Ns_Done_Cb done_cb) {
	struct Buffer_T buffer;
	char* url_dup = NULL;
	char* post_data = NULL;
	Ns_Item_T item;
	const char* get = "get://";
	const char* gets = "gets://";
	const char* post = "post://";
	const char* posts = "posts://";
	const char* delete = "delete://";
	const char* deletes = "deletes://";
	const char* put = "put://";
	const char* puts = "puts://";
	const char* soap = "soap://";
	const char* http = "http://";
	const char* https = "https://";
	union {
		uint16_t flags;
		struct {
			uint16_t get : 1;
			uint16_t post : 1;
			uint16_t delete : 1;
			uint16_t put : 1;
			uint16_t gets : 1;
			uint16_t posts : 1;
			uint16_t deletes : 1;
			uint16_t puts : 1;
			uint16_t soap : 1;
		};
	} is;
	if (!ns || !url || !strlen(url)) {
		return 0;
	}
	if (!(url_dup = calloc(1, strlen(url) + 10))) {
		return 0;
	}
	strcpy(url_dup, url);
	buffer_init(&buffer, strlen(url_dup) + 10, (uint8_t*)url_dup);
	memset(&item, 0, sizeof(Ns_Item_T));
	is.flags = 0;
	buffer_set_writer(&buffer, strlen(url_dup));
	is.get = buffer_equal_str(&buffer, get);
	is.post = buffer_equal_str(&buffer, post);
	is.gets = buffer_equal_str(&buffer, gets);
	is.posts = buffer_equal_str(&buffer, posts);
	is.delete = buffer_equal_str(&buffer, delete);
	is.deletes = buffer_equal_str(&buffer, deletes);
	is.put = buffer_equal_str(&buffer, put);
	is.puts = buffer_equal_str(&buffer, puts);
	is.soap = buffer_equal_str(&buffer, soap);
	if (!is.flags) {
		debug_describe_P("NS: bad protocol");
		free(url_dup);
		return 0;
	}
	if (is.get || is.gets || is.put || is.puts) {
		/*+ one char*/
		buffer_shift_right(&buffer, 1, 0);
	}
	if (is.delete || is.deletes) {
		/*- two posiotion*/
		buffer_shift_left(&buffer, 2);
	}
	/*replace proto*/
	if (is.get || is.post || is.delete || is.put || is.soap) {
		buffer_overwrite(&buffer, (uint8_t*)http, strlen(http), 0);
	} else {
		buffer_overwrite(&buffer, (uint8_t*)https, strlen(http), 0);
	}
	buffer_close(&buffer);
	if (is.post || is.posts || is.put || is.puts || is.soap) {
		uint16_t index;
		if (buffer_search(&buffer, 0, '?', &index)) {
			buffer_write_index(&buffer, 0, index);
			if (buffer_size_offset(&buffer, index + 1)) {
				post_data = (char*)buffer_data(&buffer, index + 1);
			}
			buffer_set_writer(&buffer, index);
		}
		item.method = NS_METHOD_POST;
		if (is.post || is.posts || is.put || is.puts) {
			if (is.put || is.puts) {
				item.method = NS_METHOD_PUT;
			}
			if (args || ns_post_data_is_query_string(post_data)) {
				if (!(item.headers = strdup("Content-Type: application/x-www-form-urlencoded"))) {
					goto error;
				}
			} else {
				if (ns_post_data_is_json(post_data)) {
					if (!(item.headers = strdup("Content-Type: application/json"))) {
						goto error;
					}
				} else {
				if (!(item.headers = strdup("Content-Type: text/plain"))) {
					goto error;
					}
				}
			}
		} else {
			const char* soap_format =
				"<?xml version=\"1.0\"?>"
				"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
				"<s:Body>"
				"<u:%s xmlns:u=\"%s\">"
					"%s"
				"</u:%s>"
				"</s:Body>"
				"</s:Envelope>";
			const char* header_format =
				"Content-Type: application/soap+xml; charset=utf-8" CRLF
				"SOAPAction: %s#%s";
			const char* urn = NULL;
			const char* cmd = NULL;
			const char* body = NULL;
			char* part;
			char* tok_r;
			char* tok_e;
			char* key = NULL;
			char* value = NULL;
			char* url = NULL;
			uint32_t msg_len = 0;
			if (!post_data) {
				goto error;
			}
			for (part = strtok_r(post_data, "&", &tok_r); part; part = strtok_r(NULL, "&", &tok_r)) {
				key = strtok_r(part, "=", &tok_e);
				if (key) {
					value = strtok_r(NULL, "=", &tok_e);
				}
				if (!key || !value || !strlen(key) || !strlen(value)) {
					continue;
				}
				if (!strcmp(key, "urn")) {
					urn = value;
					continue;
				}
				if (!strcmp(key, "cmd")) {
					cmd = value;
					continue;
				}
				if (!strcmp(key, "body")) {
					body = value;
				}
			}
			if (!urn || !cmd || !body) {
				goto error;
			}
			if (!strlen(urn) || !strlen(cmd) || !strlen(body)) {
				goto error;
			}
			msg_len += strlen(url_dup);
			msg_len += strlen(soap_format);
			msg_len += strlen(urn);
			msg_len += strlen(cmd) * 2;
			msg_len += strlen(body);
			if (!(url = calloc(1, msg_len + 1))) {
				goto error;
			}
			msg_len = strlen(header_format);
			msg_len += strlen(urn);
			msg_len += strlen(cmd);
			if (!(item.headers = calloc(1, msg_len + 1))) {
				free(url);
				goto error;
			}
			strcpy(url, url_dup);
			url[strlen(url_dup)] = '\0';
			post_data = url + strlen(url_dup) + 1;
			sprintf(post_data, soap_format, cmd, urn, body, cmd);
			sprintf(item.headers, header_format, urn, cmd);
			free(url_dup);
			url_dup = url;
		}
	} else {
		if (is.delete || is.deletes) {
			item.method = NS_METHOD_DELETE;
		} else {
			item.method = NS_METHOD_GET;
		}
	}
	item.url = url_dup;
	item.body = post_data;
	if (args) {
		item.args = strdup(args);
	}
	item.owner = owner;
	item.recv_cb = recv_cb;
	item.done_cb = done_cb;
	if (!(list_add(ns->list, &item))) {
		goto error;
	}
	ns_send(ns);
	return 1;

error:
	free(item.args);
	free(item.headers);
	if (url_dup) {
		free(url_dup);
	}
	return 0;
}

uint16_t ICACHE_FLASH_ATTR ns_size(Ns_T ns) {
	if (!ns) {
		return 0;
	}
	return list_size(ns->list);
}

uint8_t ICACHE_FLASH_ATTR ns_register(Ns_T ns, Http_Request_T req, uint8_t* index) {
	uint8_t i;
	if (!ns || !req || !index) {
		return 0;
	}
	debug_describe_P(COLOR_YELLOW "Ns register" COLOR_END);
	for (i = 0; i < ARRAY_SIZE(ns->event); i++) {
		if (ns->event[i]) {
			http_req_close(ns->event[i]);
		}
	}
	for (i = 0; i < ARRAY_SIZE(ns->event); i++) {
		if (!ns->event[i]) {
			ns->event[i] = req;
			*index = i;
			debug_printf(COLOR_GREEN "Registered: %u\n" COLOR_END, (uint32_t)i);
			return 1;
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR ns_unregister(Ns_T ns, uint8_t index) {
	if (!ns || (index >= ARRAY_SIZE(ns->event))) {
		return 0;
	}
	debug_printf(COLOR_YELLOW "Ns unregister: %u\n" COLOR_END, (uint32_t)index);
	if (ns->event[index]) {
		ns->event[index] = NULL;
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR ns_connect(Ns_T ns, uint8_t yes) {
	if (!ns) {
		return 0;
	}
	ns->connected = yes ? 1 : 0;
	if (ns->connected) {
		return ns_send(ns);
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR ns_connected(Ns_T ns) {
	if (!ns) {
		return 0;
	}
	return ns->connected;
}
