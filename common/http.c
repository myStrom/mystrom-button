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
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include <user_interface.h>
#include <ip_addr.h>
#include <espconn.h>
#include <c_types.h>
#include "http.h"
#include "debug.h"
#include "parser.h"
#include "list.h"
#include "slash.h"
#include "array_size.h"
#include "ns.h"
#include "color.h"
#include "store.h"

#define HTTP_PART_SIZE ((uint32_t)1400)

extern Ns_T ns;
extern struct Store_T store;

struct Http_T {
	struct espconn conn;
	esp_tcp tcp;
	List_T list;
	Parser_State_T state;
};

struct Http_Request_T {
	char args_cpy[512];
	char query_cpy[512];
	Item_T args_items[5];
	struct Value_T query_items[10];
	Http_T http;
	Parser_T parser;
	Buffer_T send;
	Buffer_T content;
	const struct Http_Page_T* page;
	struct espconn* conn;
	void* data;
	uint32_t send_bytes;
	uint32_t content_length;
	uint16_t part;
	uint16_t length;
	Item_T* args;
	uint8_t args_amount;
	uint8_t chunked_ref;
	uint8_t ready : 1;
	uint8_t close : 1;
	uint8_t header : 1;
	uint8_t wait_discon : 1;
};

static Http_T http_global = NULL;

static void ICACHE_FLASH_ATTR  http_error_response(Http_Request_T request) {
	static const char bad_request_400_html[] =
		"HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
		"<html>"
		  "<head><title>Error</title></head>"
		  "<body><h1>400 Bad Request</h1></body>"
		"</html>";
	static const char not_found_404_html[] =
		"HTTP/1.1 404 Not Found\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
		"<html>"
		  "<head><title>Error</title></head>"
		  "<body><h1>404 Not Found</h1></body>"
		"</html>";
	static const char not_allowed_405_html[] =
		"HTTP/1.1 405 Method Not Allowed\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
		"<html>"
		  "<head><title>Error</title></head>"
		  "<body><h1>405 Method Not Allowed</h1></body>"
		"</html>";
	static const char internal_server_error_500_html[] =
		"HTTP/1.1 500 Internal Server Error\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
		"<html>"
		  "<head><title>Error</title></head>"
		  "<body><h1>500 Internal Server Error</h1></body>"
		"</html>";
	static const char version_not_supported_505_html[] =
		"HTTP/1.1 505 HTTP Version Not Supported\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
		"<html>"
		  "<head><title>Error</title></head>"
		  "<body><h1>505 HTTP Version Not Supported</h1></body>"
		"</html>";
	const char* content = NULL;
	uint32_t len = 0;
	if (!request) {
		return;
	}
	switch(request->http->state) {
		case Parser_State_Bad_Request_400:
			content = bad_request_400_html;
			break;
		case Parser_State_Not_Found_404:
			content = not_found_404_html;
			break;
		case Parser_State_Method_Not_Allowed_405:
			content = not_allowed_405_html;
			break;
		case Parser_State_HTTP_Version_Not_Supported_505:
			content = version_not_supported_505_html;
			break;
		default:
			debug_describe_P("Default response");
			content = internal_server_error_500_html;
			break;
	}
	len = strlen(content);
	if (espconn_send(request->conn, (uint8_t*)content, len)) {
		os_printf("%s", COLOR_RED "Http unable to send error" COLOR_END);
		request->wait_discon = 1;
		espconn_disconnect(request->conn);
	} else {
		request->close = 1;
	}
}

static uint8_t ICACHE_FLASH_ATTR http_fill_header(Buffer_T buffer, const char* type, int32_t len) {
	if (!buffer) {
		return 0;
	}
	buffer_puts(buffer, "HTTP/1.1 200 OK" CRLF);
	buffer_puts(buffer, "Pragma: no-cache" CRLF);
	buffer_puts(buffer, "Cache-Control: no-store, no-cache" CRLF);
	buffer_puts(buffer, "Access-Control-Allow-Origin: *" CRLF);
	buffer_puts(buffer, "Content-Type: ");
	if (type && strlen(type)) {
		buffer_puts(buffer, type);
	} else {
		buffer_puts(buffer, "text/html");
	}
	buffer_puts(buffer, CRLF);
	if (len >= 0) {
		buffer_puts(buffer, "Content-Length: ");
		buffer_dec(buffer, len);
		buffer_puts(buffer, CRLF);
		buffer_puts(buffer, "Connection: close" CRLF);
	} else {
		buffer_puts(buffer, "Transfer-Encoding: chunked" CRLF);
	}
	buffer_puts(buffer, CRLF);
	return 1;
}

static Parser_State_T ICACHE_FLASH_ATTR http_200(Http_Request_T req) {
	uint8_t storage[512];
	struct Buffer_T header;
	Parser_State_T result;
	if (!req || !req->page) {
		return 0;
	}
	buffer_init(&header, sizeof(storage), storage);
	if (req->page->exec || req->page->exec_req) {
		if (req->page->exec) {
			result = req->page->exec(&req->send, req->args_items, req->query_items, req->content);
		} else {
			result = req->page->exec_req(req);
		}
		if (result != Parser_State_OK_200) {
			return result;
		}
		if (!req->send && req->page->len) {
			if (!(req->send = buffer_new(req->page->len))) {
				return Parser_State_Internal_Server_Error_500;
			}
		}
		if (req->send) {
			req->content_length = buffer_size(req->send);
		} else {
			req->content_length = 0;
		}
	} else {
		if (req->page->len && !req->page->content) {
			return Parser_State_Internal_Server_Error_500;
		}
		req->content_length = req->page->len;
	}
	http_fill_header(&header,
					req->page->type,
					req->page->chunked ? -1 : req->content_length);
	req->send_bytes = 0;
	if (espconn_send(req->conn, buffer_data(&header, 0), buffer_size(&header))) {
		os_printf("%s", COLOR_RED "Http unable to send 200" COLOR_END);
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
	} else {
		req->header = 1;
		if (!req->content_length && !req->page->chunked) {
			req->close = 1;
		}
	}
	return Parser_State_OK_200;
}

static Parser_State_T http_200_options(Http_Request_T req) {
	uint8_t storage[512];
	struct Buffer_T header;

	if (!req) {
		return 0;
	}
	buffer_init(&header, sizeof(storage), storage);

	buffer_puts(&header, "HTTP/1.1 200 OK" CRLF);
	buffer_puts(&header, "Pragma: no-cache" CRLF);
	buffer_puts(&header, "Cache-Control: no-store, no-cache" CRLF);
	buffer_puts(&header, "Content-Type: text/plain" CRLF);
	buffer_puts(&header, "Content-Length: 0" CRLF);
	buffer_puts(&header, "Connection: close" CRLF);
	buffer_puts(&header, "Access-Control-Allow-Origin: *" CRLF);
	buffer_puts(&header, "Access-Control-Allow-Methods: GET, POST, OPTIONS" CRLF);
	buffer_puts(&header, CRLF);

	req->send_bytes = 0;
	if (espconn_send(req->conn, buffer_data(&header, 0), buffer_size(&header))) {
		os_printf("%s", COLOR_RED "Http unable to send 200 OPTIONS" COLOR_END);
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
	} else {
		req->header = 1;
		req->close = 1;
	}
	return Parser_State_OK_200;
}

static void ICACHE_FLASH_ATTR http_recv(void *arg, char* data, unsigned short len) {
	struct espconn* conn = arg;
	Http_Request_T request;
	const struct Http_Page_T* page;
	Slash_T slash;
	char* index;
	char* path;
	char* path_copy = NULL;
	char* args = NULL;
	uint16_t i;
	uint8_t ok_200 = 0;
	if (!conn || !data) {
		return;
	}
	request = (Http_Request_T)conn->reverse;
	if (!request || request->wait_discon) {
		return;
	}
	request->conn = conn;
	for (i = 0; i < len; i++) {
		request->http->state = parser_do(request->parser, data[i], len);
	}
	if ((request->http->state == Parser_State_Continue_100) &&
		(request->page) &&
		request->page->multipart) {
		request->http->state = http_200(request);
		if (request->http->state == Parser_State_OK_200) {
			goto done;
		}
		if (request->http->state == Parser_State_Continue_100) {
			goto done;
		}
		goto error;
	}
	if (request->http->state == Parser_State_OK_200) {
		ok_200 = 1;
	}
	if (ok_200 || parser_header_done(request->parser)) {
		const char* referer;
		uint8_t ref_ok = 1;
		while ((referer = parser_referer(request->parser))) {
			struct ip_info info;
			struct Buffer_T ip_buff;
			uint8_t ip_stor[32];
			const char* pos;
			memset(&info, 0, sizeof(info));
			if (!wifi_get_ip_info(store.connect.save ? STATION_IF : SOFTAP_IF, &info)) {
				ref_ok = 0;
				break;
			}
			memset(ip_stor, 0, sizeof(ip_stor));
			buffer_init(&ip_buff, sizeof(ip_stor), ip_stor);
			buffer_puts_ip(&ip_buff, info.ip.addr);
			pos = strstr(referer, buffer_string(&ip_buff));
			if (!pos) {
				ref_ok = 0;
				break;
			}
			if ((pos - referer) > strlen("http://")) {
				ref_ok = 0;
			}
			break;
		}
		if (!ref_ok) {
			request->http->state = Parser_State_Not_Found_404;
			goto error;
		}
		if (parser_method(request->parser) == Parser_Method_OPTIONS) {
			http_200_options(request);
			goto done;
		}
		if (!(path_copy = strdup(parser_path(request->parser)))) {
			goto error;
		}
		path = path_copy;
		debug_printf("path: %s\n", path);
		while (*path == '/') {
			path++;
		}
		if (!slash_init(&slash, path, '?')) {
			goto error;
		}
		index = slash_next(&slash);
		if (index) {
			debug_printf("index: %s\n", index);
		}
		request->http->state = Parser_State_Not_Found_404;
		list_rewind(request->http->list);
		if (index && (args = strchr(index, '/'))) {
			*args = 0;
			args++;
		}
		if (args) {
			debug_printf("args: %s\n", args);
		}
		while(list_next(request->http->list, &page)) {
			if (index && strcmp(page->path, index)) {
				continue;
			}
			if (!index && strlen(page->path)) {
				continue;
			}
			if (parser_method(request->parser) != page->method) {
				continue;
			}
			while (page->alias) {
				page = page->alias;
			}
			request->page = page;
			if (request->page->panel && store.panel_dis) {
				continue;
			}
			if (request->page->rest) {
				if (store.rest_dis) {
					continue;
				}
				if (strnlen(store.token, sizeof(store.token))) {
					const char* token;
					if (!(token = parser_token(request->parser))) {
						continue;
					}
					if (strncmp(store.token, token, sizeof(store.token))) {
						continue;
					}
				}
			}
			if (args && (!page->path_rules || !page->path_rules_amount)) {
				continue;
			}
			if ((!args || !strlen(args)) && page->path_rules_amount) {
				continue;
			}
			if (slash_have_next(&slash) && (!page->query_rules || !page->query_rules_amount)) {
				continue;
			}
			if (page->query_rules) {
				uint16_t required = 0;
				uint16_t i;
				for (i = 0; i < page->query_rules_amount; i++) {
					if (page->query_rules[i].required) {
						required++;
					}
				}
				if (required && !slash_have_next(&slash)) {
					continue;
				}
			}
			memset(request->args_items, 0, sizeof(request->args_items));
			if (args) {
				memset(request->args_cpy, 0, sizeof(request->args_cpy));
				strncpy(request->args_cpy, args, sizeof(request->args_cpy) - 1);
				if (!rule_check_path(request->args_cpy,
					page->path_rules,
					request->args_items,
					(page->path_rules_amount > ARRAY_SIZE(request->args_items)) ? ARRAY_SIZE(request->args_items) : page->path_rules_amount,
					'/')) {
					continue;
				}
			}
			memset(request->query_items, 0, sizeof(request->query_items));
			if (slash_have_next(&slash)) {
				Slash_T amper;
				memset(request->query_cpy, 0, sizeof(request->query_cpy));
				strncpy(request->query_cpy, slash_current(&slash), sizeof(request->query_cpy) - 1);
				slash_init(&amper, request->query_cpy, '&');
				if (!slash_parse(&amper,
					page->query_rules,
					request->query_items,
					(page->query_rules_amount > ARRAY_SIZE(request->query_items)) ? ARRAY_SIZE(request->query_items) : page->query_rules_amount,
					'=')) {
					continue;
				}
			}
			if (!ok_200 && !request->page->multipart) {
				debug_describe_P("No multipart");
				goto done;
			}
			if (parser_method(request->parser) == Parser_Method_POST) {
				request->content = parser_content(request->parser);
			} else {
				request->content = NULL;
			}
			request->http->state = http_200(request);
			break;
		}
		if (request->http->state == Parser_State_OK_200) {
			goto done;
		}
	}
	if (request->http->state == Parser_State_Continue_100) {
		goto done;
	}
error:
	http_error_response(request);
	request->page = NULL;
done:
	if (path_copy) {
		free(path_copy);
	}
}

static Http_Request_T ICACHE_FLASH_ATTR http_request_new(Http_T http, struct espconn* conn) {
	Http_Request_T request;
	if (!http || !conn) {
		return NULL;
	}
	if (!(request = (void*)malloc(sizeof(struct Http_Request_T)))) {
		return NULL;
	}
	memset(request, 0, sizeof(struct Http_Request_T));
	if (!(request->parser = parser_new())) {
		free(request);
		return NULL;
	}
	request->http = http;
	request->conn = conn;
	request->page = NULL;
	return request;
}

static void ICACHE_FLASH_ATTR http_request_delete(Http_Request_T request) {
	if (!request) {
		return;
	}
	if (request->chunked_ref) {
		ns_unregister(ns, request->chunked_ref - 1);
	}
	if (request->page && request->page->close_cb) {
		request->page->close_cb(request);
	}
	if (request->send) {
		free(request->send);
	}
	parser_free(request->parser);
	free(request);
	debug_printf("Free heap: %u\n", system_get_free_heap_size());
}

static void ICACHE_FLASH_ATTR http_recon(void *arg, sint8 err) {
	struct espconn* conn = arg;
	if (!conn) {
		return;
	}
	if (conn->reverse) {
		http_request_delete((Http_Request_T)conn->reverse);
		conn->reverse = NULL;
	}
}

static void ICACHE_FLASH_ATTR http_discon(void *arg) {
	struct espconn* conn = arg;
	if (!conn) {
		return;
	}
	if (conn->reverse) {
		http_request_delete((Http_Request_T)conn->reverse);
		conn->reverse = NULL;
	}
}

Buffer_T ICACHE_FLASH_ATTR http_req_send_buffer(Http_Request_T req) {
	if (!req) {
		return NULL;
	}
	if (!req->ready) {
		return NULL;
	}
	return req->send;
}

uint8_t ICACHE_FLASH_ATTR http_req_send_chunked_json(Http_Request_T req, Json_T json) {
	if (!req) {
		return 0;
	}
	if (!req->ready) {
		debug_describe_P("Error: http busy");
		return 0;
	}
	if (req->send) {
		buffer_delete(req->send);
		req->send = NULL;
	}
	if (json) {
		req->send = buffer_new(json_size(json) + 16);
		if (req->send) {
			json_print(json, req->send, 0);
		}
	}
	return http_req_send_chunked(req);
}

void ICACHE_FLASH_ATTR http_req_close(Http_Request_T req) {
	if (!req) {
		return;
	}
	os_printf("%s", "HTTP close req");
	if (!req->wait_discon) {
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
	}
}

uint8_t ICACHE_FLASH_ATTR http_req_send_chunked(Http_Request_T req) {
	uint8_t storage[16];
	struct Buffer_T size;
	if (!req || !req->send) {
		debug_describe_P("Http no ref");
		return 0;
	}
	if (!req->ready) {
		debug_describe_P("Error: http busy");
		return 0;
	}
	req->ready = 0;
	buffer_init(&size, sizeof(storage), storage);
	if (!buffer_size(req->send)) {
		debug_describe_P("Zero chunk");
		req->close = 1;
	}
	buffer_hex_no_zeros(&size, buffer_size(req->send));
	buffer_puts(&size, CRLF);
	buffer_shift_right(req->send, buffer_size(&size), 0);
	buffer_overwrite(req->send, buffer_data(&size, 0), buffer_size(&size), 0);
	buffer_puts(req->send, CRLF);
	if (espconn_send(req->conn, buffer_data(req->send, 0), buffer_size(req->send))) {
		debug_describe_P("Http unable to send chunked data");
		debug_printf("Buf size: %d" CRLF, (int)buffer_size(req->send));
		debug_printf("HS: %d" CRLF, system_get_free_heap_size());
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
		return 0;
	}
	debug_describe_P("Chunk send");
	return 1;
}

static void ICACHE_FLASH_ATTR http_sendcon(void *arg) {
	struct espconn* conn = arg;
	Http_Request_T req;
	uint32_t send_len = 0;
	uint8_t* send_data = NULL;
	if (!conn) {
		return;
	}
	req = (Http_Request_T)conn->reverse;
	if (!req || req->wait_discon) {
		os_printf("%s", "Http sendcon while wait discon");
		return;
	}
	req->conn = conn;
	if (req->close) {
		req->close = 0;
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
		return;
	}
	if (!req->page) {
		return;
	}
	if (req->page->chunked && (req->send_bytes >= req->content_length)) {
		uint8_t index;
		if (req->send) {
			buffer_clear(req->send);
		}
		req->ready = 1;
		if (!req->page->event) {
			if (req->header) {
				req->header = 0;
				if (req->page->header_cb) {
					req->page->header_cb(req);
					return;
				}
			}
			if (req->page->send_cb) {
				req->page->send_cb(req);
			}
			return;
		}
		if (req->chunked_ref) {
			return;
		}
		if (ns_register(ns, req, &index)) {
			req->chunked_ref = index + 1;
			return;
		}
		if (!req->send || !buffer_size(req->send)) {
			return;
		}
		req->content_length = buffer_size(req->send);
		req->send_bytes = 0;
	}
	send_len = req->content_length - req->send_bytes;
	if (send_len > HTTP_PART_SIZE) {
		send_len = HTTP_PART_SIZE;
	}
	if (req->send) {
		send_data = buffer_data(req->send, req->send_bytes);
	} else {
		send_data = (uint8_t*)req->page->content + req->send_bytes;
	}
	if (!send_len || !send_data) {
		os_printf("%s", COLOR_RED "HTTP unexpected error" COLOR_END);
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
		return;
	}
	if (espconn_send(req->conn, send_data, send_len)) {
		os_printf("%s", COLOR_RED "Http unable to send data" COLOR_END);
		req->wait_discon = 1;
		espconn_disconnect(req->conn);
		return;
	}
	req->send_bytes += send_len;
	if (req->send_bytes >= req->content_length) {
		if (!req->page->chunked) {
			req->close = 1;
		}
	}
}

static void ICACHE_FLASH_ATTR http_listen(void* arg) {
	struct espconn* conn = arg;
	if (!conn) {
		return;
	}
	espconn_regist_recvcb(conn, http_recv);
	espconn_regist_reconcb(conn, http_recon);
	espconn_regist_disconcb(conn, http_discon);
	espconn_regist_sentcb(conn, http_sendcon);
	if (!(conn->reverse = (void*)http_request_new(http_global, conn))) {
		debug_describe_P(COLOR_RED "Unable to create parser" COLOR_END);
	}
}

Http_T ICACHE_FLASH_ATTR http_new(void) {
	Http_T http;
	if (!(http = (void*)malloc(sizeof(struct Http_T)))) {
		return NULL;
	}
	memset(http, 0, sizeof(struct Http_T));
	if (!(http->list = list_new(sizeof(Http_Page_T)))) {
		free(http);
		return NULL;
	}
	http->conn.type = ESPCONN_TCP;
	http->conn.state = ESPCONN_NONE;
	http->conn.proto.tcp = &http->tcp;
	http->conn.proto.tcp->local_port = 80;
	http->conn.reverse = http;
	http_global = http;
	espconn_regist_connectcb(&http->conn, http_listen);
	espconn_accept(&http->conn);
	espconn_regist_time(&http->conn, 7200, 0);
	espconn_tcp_set_max_con_allow(&http->conn, 5);
	return http;
}

void ICACHE_FLASH_ATTR http_delete(Http_T http) {
	if (!http) {
		return;
	}
	list_delete(http->list);
	free(http);
	http_global = NULL;
}

uint8_t ICACHE_FLASH_ATTR http_add_page(Http_T http, const struct Http_Page_T* page) {
	if (!http || !page) {
		return 0;
	}
	return list_add(http->list, &page);
}

void* ICACHE_FLASH_ATTR http_req_data(Http_Request_T req) {
	if (!req) {
		return NULL;
	}
	return req->data;
}

void ICACHE_FLASH_ATTR http_req_set_data(Http_Request_T req, void* data) {
	if (!req) {
		return;
	}
	req->data = data;
}

Parser_T ICACHE_FLASH_ATTR http_req_parser(Http_Request_T req) {
	if (!req) {
		return NULL;
	}
	return req->parser;
}

Http_T ICACHE_FLASH_ATTR http_req_http(Http_Request_T req) {
	if (!req) {
		return NULL;
	}
	return req->http;
}

static uint8_t ICACHE_FLASH_ATTR http_page_describe_rule(Http_Page_T page, const Rule_T* rule, Buffer_T buffer, uint8_t name_none) {
	uint8_t store_storage[128];
	struct Buffer_T store;
	if (!rule || !buffer) {
		return 0;
	}
	buffer_init(&store, sizeof(store_storage), store_storage);
	if (!name_none &&
		rule_name(rule) &&
		strlen(rule_name(rule))) {
		buffer_puts(buffer, rule_name(rule));
		buffer_write(buffer, '=');
	}
	buffer_write(buffer, '<');
	rule_describe_type(rule, buffer);
	rule_describe_range(rule, &store);
	if (buffer_size(&store)) {
		buffer_write(buffer, ' ');
		buffer_append_buffer(buffer, &store, 0, buffer_size(&store));
	}
	buffer_write(buffer, '>');
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR http_page_describe(Http_Page_T page, Buffer_T buffer) {
	const Rule_T* rule;
	uint32_t i;
	if (!page || !buffer) {
		return 0;
	}
	switch (page->method) {
		case Parser_Method_GET:
			buffer_puts(buffer, "GET");
			break;
		case Parser_Method_POST:
			buffer_puts(buffer, "POST");
			break;
		default:
			buffer_puts(buffer, "UNDEF");
			break;
	}
	buffer_write(buffer, ' ');
	buffer_puts(buffer, page->path);
	if (page->path_rules_amount) {
		for (i = 0; i < page->path_rules_amount; i++) {
			rule = &page->path_rules[i];
			if (!rule_required(rule)) {
				buffer_write(buffer, '[');
			}
			buffer_write(buffer, '/');
			if (rule_get_type(rule) != RULE_EQUAL) {
				http_page_describe_rule(page, rule, buffer, 1);
			} else {
				rule_describe_range(rule, buffer);
			}
			if (!rule_required(rule)) {
				buffer_write(buffer, ']');
			}
		}
	}
	if (page->query_rules && page->query_rules_amount) {
		for (i = 0; i < page->query_rules_amount; i++) {
			rule = &page->query_rules[i];
			if (!rule_required(rule)) {
				buffer_write(buffer, '[');
			}
			if (i) {
				buffer_write(buffer, '&');
			} else {
				buffer_write(buffer, '?');
			}
			http_page_describe_rule(page, rule, buffer, 0);
			if (!rule_required(rule)) {
				buffer_write(buffer, ']');
			}
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR http_help(Http_T http, Http_Request_T req, uint32_t it) {
	Http_Page_T page;
	if (!http || !req) {
		return 0;
	}
	if (it >= list_size(http->list)) {
		goto done;
	}
	if (!list_read(http->list, it, &page)) {
		goto done;
	}
	buffer_clear(req->send);
	http_page_describe(page, req->send);
	buffer_puts(req->send, CRLF);
done:
	return http_req_send_chunked(req);
}
