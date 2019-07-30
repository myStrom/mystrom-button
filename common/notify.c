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
#include "notify.h"
#include "debug.h"
#include "slash.h"
#include "buffer.h"
#include "rule.h"
#include "store.h"
#include "base64.h"
#include "rule.h"

struct Notify_T {
	char* path;
	char* host;
	char* auth;
	struct espconn dns;
	uint16_t port;
	ip_addr_t server_ip;
	ip_addr_t ip;
	os_timer_t timer;
	struct espconn conn;
	esp_tcp tcp;
	struct espconn* current_conn;
	void (*done_cb)(Notify_T notify, uint8_t error);
	uint8_t (*recv_cb)(Notify_T notify, uint8_t* data, uint32_t len);
	void* owner;
	char* post_data;
	char* headers;
	uint32_t timeout;
	Ns_Method_T method;
	uint8_t ssl : 1;
};

static void ICACHE_FLASH_ATTR notify_response_timeout_cb(void* arg);

static void ICACHE_FLASH_ATTR notify_done(Notify_T notify, uint8_t error) {
	if (!notify) {
		return;
	}
	debug_describe_P("Notify done");
	os_timer_disarm(&notify->timer);
	if (notify->done_cb) {
		notify->done_cb(notify, error);
	}
}

static void ICACHE_FLASH_ATTR notify_send(void* arg) {
	struct espconn* conn = (struct espconn*)arg;
	Notify_T notify;
	if (!conn || !(notify = conn->reverse)) {
		return;
	}
	debug_describe_P("Notify sended");
	notify->current_conn = conn;
	if (!notify->recv_cb) {
		if (notify->ssl) {
			espconn_secure_disconnect(notify->current_conn);
		} else {
			espconn_disconnect(notify->current_conn);
		}
	}
}

static void ICACHE_FLASH_ATTR notify_recv(void *arg, char* data, unsigned short len) {
	struct espconn* conn = (struct espconn*)arg;
	Notify_T notify;
	if (!conn || !(notify = conn->reverse) || !data) {
		return;
	}
	debug_describe_P("Notify recv");
	notify->current_conn = conn;
	if (notify->recv_cb && notify->recv_cb(notify, (uint8_t*)data, len)) {
		return;
	}
	if (notify->ssl) {
		espconn_secure_disconnect(notify->current_conn);
	} else {
		espconn_disconnect(notify->current_conn);
	}
}

static void ICACHE_FLASH_ATTR notify_close(void* arg) {
	struct espconn* conn = (struct espconn*)arg;
	Notify_T notify;
	if (!conn || !(notify = conn->reverse)) {
		return;
	}
	debug_describe_P("Notify close");
	if (notify->ssl) {
		espconn_secure_delete(conn);
	} else {
		espconn_delete(conn);
	}
	notify->current_conn = NULL;
	notify_done(notify, 0);
}

static void ICACHE_FLASH_ATTR notify_connected(void* arg) {
	struct Buffer_T request;
	uint8_t* storage = NULL;
	struct espconn* conn = (struct espconn*)arg;
	Notify_T notify;
	size_t req_len = 0;
	int8_t result;
	if (!conn || !(notify = conn->reverse)) {
		return;
	}
	debug_describe_P("Notify connected");
	notify->current_conn = conn;
	espconn_regist_disconcb(conn, notify_close);
	espconn_regist_recvcb(conn, notify_recv);
	espconn_regist_sentcb(conn, notify_send);

	req_len += 7; /*method*/
	req_len += strlen(notify->path) + 11; /*path*/
	req_len += strlen(notify->host) + 7 + 6 + 2;
	req_len += 17 + 2;
	if (notify->headers) {
		req_len += strlen(notify->headers);
	}
	if (notify->auth) {
		req_len += strlen(notify->auth) + 21 + 2;
	}
	if (notify->post_data) {
		req_len += strlen(notify->post_data) + 16 + 5 + 2;
	}
	req_len += 2 + 32;
	if (!(storage = calloc(1, req_len))) {
		result = 1;
		goto done;
	}
	buffer_init(&request, req_len, storage);
	buffer_clear(&request);

	if (notify->method == NS_METHOD_DELETE) {
		buffer_puts(&request, "DELETE ");
	} else {
		if ((notify->method == NS_METHOD_POST) ||
			(notify->method == NS_METHOD_PUT) ||
			notify->post_data) {
			if (notify->method == NS_METHOD_POST) {
				buffer_puts(&request, "POST ");
			} else {
				buffer_puts(&request, "PUT ");
			}
		} else {
			buffer_puts(&request, "GET ");
		}
	}
	buffer_puts(&request, notify->path);
	buffer_puts(&request, " HTTP/1.1" CRLF);
	free(notify->path);
	notify->path = NULL;

	buffer_puts(&request, "Host: ");
	buffer_puts(&request, notify->host);
	free(notify->host);
	notify->host = NULL;

	buffer_puts(&request, ":");
	buffer_dec(&request, notify->port);
	buffer_puts(&request, CRLF);

	buffer_puts(&request, "Connection: close" CRLF);
	if (notify->headers) {
		buffer_puts(&request, notify->headers);
		if (!buffer_equal_from_end_str(&request, CRLF)) {
			buffer_puts(&request, CRLF);
		}
		free(notify->headers);
		notify->headers = NULL;
	}
	if (notify->auth) {
		buffer_puts(&request, "Authorization: Basic ");
		buffer_puts(&request, notify->auth);
		buffer_puts(&request, CRLF);
		free(notify->auth);
		notify->auth = NULL;
	}
	if (notify->post_data) {
		buffer_puts(&request, "Content-Length: ");
		buffer_dec(&request, strlen(notify->post_data));
		buffer_puts(&request, CRLF);
	}
	buffer_puts(&request, CRLF);

	if (notify->post_data) {
		buffer_puts(&request, notify->post_data);
		free(notify->post_data);
		notify->post_data = NULL;
	}
	if (notify->ssl) {
		result = espconn_secure_send(conn, buffer_data(&request, 0), buffer_size(&request));
	} else {
		result = espconn_send(conn, buffer_data(&request, 0), buffer_size(&request));
	}
done:
	if (result) {
		debug_describe_P("Http request send error");
		if (notify->ssl) {
			espconn_secure_disconnect(conn);
		} else {
			espconn_disconnect(conn);
		}
		notify_done(notify, 1);
		free(storage);
		return;
	}
	debug_describe_P("SEND:");
	debug_describe(buffer_string(&request));
	debug_describe_P("");
	free(storage);
	if (notify->timeout) {
		os_timer_disarm(&notify->timer);
		os_timer_setfn(&notify->timer, notify_response_timeout_cb, notify);
		os_timer_arm(&notify->timer, notify->timeout, 0);
	}
}

static void ICACHE_FLASH_ATTR notify_error(void* arg, sint8 errType) {
	struct espconn* conn = (struct espconn*)arg;
	Notify_T notify;
	if (!conn || !(notify = conn->reverse)) {
		return;
	}
	if (notify->ssl) {
		espconn_secure_delete(conn);
	} else {
		espconn_delete(conn);
	}
	notify->current_conn = NULL;
	debug_describe_P("Unable connect to notify server");
	notify_done(notify, 1);
}

static uint8_t ICACHE_FLASH_ATTR notify_connect_by_ip(Notify_T notify) {
	if (!notify) {
		return 0;
	}
	debug_describe_P("Notify connect by IP");
	memset(&notify->conn, 0, sizeof(notify->conn));
	memset(&notify->tcp, 0, sizeof(notify->tcp));
	notify->conn.reverse = notify;
	notify->conn.type = ESPCONN_TCP;
	notify->conn.state = ESPCONN_NONE;
	notify->conn.proto.tcp = &notify->tcp;
	notify->conn.proto.tcp->local_port = espconn_port();
	notify->conn.proto.tcp->remote_port = notify->port;
	memcpy(&notify->conn.proto.tcp->remote_ip, &notify->ip, 4);
	espconn_set_opt(&notify->conn, ESPCONN_REUSEADDR | ESPCONN_NODELAY);
	espconn_regist_connectcb(&notify->conn, notify_connected);
	espconn_regist_reconcb(&notify->conn, notify_error);
	if (notify->ssl) {
		if (espconn_secure_connect(&notify->conn)) {
			debug_describe_P("Unable to connect to notify server by SSL");
			return 0;
		}
	} else {
		if (espconn_connect(&notify->conn)) {
			debug_describe_P("Unable to connect to notify server");
			return 0;
		}
	}
	debug_describe_P("Notify connecting...");
	return 1;
}

static void ICACHE_FLASH_ATTR notify_dns_cb(const char* name, ip_addr_t* ipaddr, void* arg) {
	Notify_T notify;
	struct espconn* conn = (struct espconn*)arg;
	if (!conn || !(notify = conn->reverse)) {
		debug_describe_P("Notify unexpected error");
		return;
	}
	debug_printf("dns cb notify: %p\n", notify);
	os_timer_disarm(&notify->timer);
	if (!name || !ipaddr) {
		debug_describe_P("Unable to resolve notify server");
		notify_done(notify, 1);
		return;
	}
	debug_describe_P("Notify address resolved");
	notify->ip = *ipaddr;
	if (!notify_connect_by_ip(notify)) {
		notify_done(notify, 1);
	}
}

static void ICACHE_FLASH_ATTR notify_dns_timeout_cb(void* arg) {
	Notify_T notify = arg;
	debug_describe_P("Error: Notify DNS resolve timeout");
	if (!notify) {
		return;
	}
	notify_done(notify, 1);
}

static void ICACHE_FLASH_ATTR notify_response_timeout_cb(void* arg) {
	Notify_T notify = arg;
	debug_describe_P("Error: Notify response timeout");
	if (!notify) {
		return;
	}
	if (notify->ssl) {
		espconn_secure_disconnect(notify->current_conn);
	} else {
		espconn_disconnect(notify->current_conn);
	}
}

Notify_T ICACHE_FLASH_ATTR notify_new(
	const char* url,
	const char* headers,
	const char* post_data,
	const char* args,
	void (*done_cb)(Notify_T notify, uint8_t error),
	uint8_t (*recv_cb)(Notify_T notify, uint8_t* data, uint32_t len),
	void* owner,
	uint32_t timeout_ms,
	Ns_Method_T method)
{
	uint8_t* storage = NULL;
	struct Buffer_T buffer;
	size_t len;
	size_t len_plus_spaces;
	size_t path_len;
	size_t i;
	Notify_T notify;
	Slash_T slash;
	char* server;
	char* port;
	char* at;
	char* proto;
	err_t err;
	uint8_t require_question = 0;
	if (!url) {
		debug_describe_P("No URL");
		return NULL;
	}
	if (!(len = strlen(url))) {
		debug_describe_P("URL zero len");
		return NULL;
	}
	if (!(notify = malloc(sizeof(struct Notify_T)))) {
		debug_describe_P("Can't alloc notify");
		return NULL;
	}
	debug_describe_P("New notify");
	memset(notify, 0, sizeof(struct Notify_T));
	memset(&notify->dns, 0, sizeof(notify->dns));
	if (headers && strlen(headers)) {
		if (!(notify->headers = strdup(headers))) {
			goto error;
		}
	}
	debug_value(method);
	if ((post_data && strlen(post_data)) ||
		(args && strlen(args) && ((method == NS_METHOD_POST) || (method == NS_METHOD_PUT)))) {
		size_t body_len = 1;
		if (post_data) {
			body_len += strlen(post_data);
		}
		if (args) {
			body_len += strlen(args);
		}
		if (!(notify->post_data = calloc(1, body_len + 1))) {
			goto error;
		}
		if (post_data && strlen(post_data)) {
			strcat(notify->post_data, post_data);
		}
		if (args && strlen(args)) {
			if (post_data && strlen(post_data)) {
				strcat(notify->post_data, "&");
			}
			strcat(notify->post_data, args);
		}
	}
	notify->done_cb = done_cb;
	notify->recv_cb = recv_cb;
	notify->owner = owner;
	notify->dns.reverse = notify;
	notify->timeout = timeout_ms;
	notify->method = method;
	len_plus_spaces = len + (rule_count_char(url, ' ') * 2);
	if (!(storage = calloc(1, len_plus_spaces + 1))) {
		goto error;
	}
	buffer_init(&buffer, len_plus_spaces + 1, storage);
	for (i = 0; i < len; i++) {
		if (url[i] == ' ') {
			buffer_puts(&buffer, "%20");
			continue;
		}
		buffer_write(&buffer, url[i]);
	}
	slash_init(&slash, buffer_string(&buffer), '/');
	if (!(proto = slash_next(&slash))) {
		debug_describe_P("Notify no protocol");
		goto error;
	}
	if (!strcmp(proto, "https:")) {
		notify->ssl = 1;
	} else {
		if (strcmp(proto, "http:")) {
			debug_describe_P("Notify error unknow protocol");
			goto error;
		}
	}
	if (slash_current(&slash)) {
		if (!strstr(slash_current(&slash), "/") || strstr(slash_current(&slash), "/?")) {
			require_question = 1;
		}
	}
	if (!slash_init_delimeters(&slash, slash_current(&slash), "/?#") ||
		!(server = slash_next(&slash))) {
		debug_describe_P("Bad notify server");
		goto error;
	}
	if ((at = strchr(server, '@'))) {
		uint32_t len;
		*at = 0;
		len = Base64encode_len(strlen(server));
		if (!(notify->auth = malloc(len + 1))) {
			debug_describe_P("Can not alloc auth");
			goto error;
		}
		memset(notify->auth, 0, len + 1);
		Base64encode(notify->auth, server, strlen(server));
		server = at + 1;
	}
	/*serach for port*/
	if ((port = strchr(server, ':'))) {
		/*addres with port*/
		notify->port = atoi(port + 1);
		port[0] = 0;
	} else {
		if (notify->ssl) {
			notify->port = 443;
		} else {
			notify->port = 80;
		}
	}
	if (!(notify->host = strdup(server))) {
		goto error;
	}
	path_len = 1;
	path_len += require_question;
	if (slash_have_next(&slash)) {
		path_len += strlen(slash_current(&slash));
	}
	if (((method == NS_METHOD_GET) || (method == NS_METHOD_DELETE)) && !post_data && args && strlen(args)) {
		path_len += strlen(args) + 1;
	}
	if (!(notify->path = calloc(1, path_len + 1))) {
		goto error;
	}
	strcpy(notify->path, "/");
	if (slash_have_next(&slash)) {
		if (require_question) {
			strcpy(notify->path + 1, "?");
		}
		strncpy(notify->path + 1 + require_question, slash_current(&slash), path_len - 1 - require_question);
	}
	if (((method == NS_METHOD_GET) || (method == NS_METHOD_DELETE)) && !post_data && args && strlen(args)) {
		if (strrchr(notify->path, (int)'?')) {
			strcat(notify->path, "&");
		} else {
			strcat(notify->path, "?");
		}
		strcat(notify->path, args);
	}
	debug_printf("Notify server: %s:%d, path: %s\n", notify->host, (int)notify->port, notify->path);
	if (rule_check_ip(notify->host, &notify->server_ip.addr)) {
		debug_printf("Notify immediate IP:" IPSTR "\n", IP2STR(&notify->server_ip));
		notify->ip = notify->server_ip;
		if (!notify_connect_by_ip(notify)) {
			goto error;
		}
		free(storage);
		return notify;
	}
	notify->server_ip.addr = 0;
	err = espconn_gethostbyname(&notify->dns, notify->host, &notify->server_ip, notify_dns_cb);
	switch (err) {
		case ESPCONN_OK:
			debug_printf("Notify server IP:" IPSTR "\n", IP2STR(&notify->server_ip));
			notify->ip = notify->server_ip;
			if (!notify_connect_by_ip(notify)) {
				goto error;
			}
			free(storage);
			return notify;
		case ESPCONN_INPROGRESS:
			os_timer_disarm(&notify->timer);
			os_timer_setfn(&notify->timer, notify_dns_timeout_cb, notify);
			os_timer_arm(&notify->timer, 10000, 0);
			free(storage);
			return notify;
		case ESPCONN_ARG:
			break;
	}
error:
	free(storage);
	notify_delete(notify);
	return NULL;
}

void ICACHE_FLASH_ATTR notify_delete(Notify_T notify) {
	if (!notify) {
		return;
	}
	debug_describe_P("Notify delete");
	if (notify->host) {
		free(notify->host);
	}
	if (notify->path) {
		free(notify->path);
	}
	if (notify->auth) {
		free(notify->auth);
	}
	if (notify->headers) {
		free(notify->headers);
	}
	if (notify->post_data) {
		free(notify->post_data);
	}
	free(notify);
}

void* ICACHE_FLASH_ATTR notify_owner(Notify_T notify) {
	if (!notify) {
		return NULL;
	}
	return notify->owner;
}

void ICACHE_FLASH_ATTR notify_timeout(Notify_T notify, uint32_t time) {
	if (!notify) {
		return;
	}
	notify->timeout = time;
}
