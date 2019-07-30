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


#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED 1

#include <inttypes.h>
#include "buffer.h"
#include "item.h"
#include "parser.h"
#include "rule.h"
#include "value.h"
#include "json.h"

typedef struct Http_T* Http_T;
typedef struct Http_Page_T* Http_Page_T;
typedef struct Http_Request_T* Http_Request_T;

struct Http_Page_T {
	const char* path;
	const uint8_t* content;
	const char* type;
	Parser_State_T (*exec)(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content);
	Parser_State_T (*exec_req)(Http_Request_T req);
	void (*close_cb)(Http_Request_T req);
	void (*send_cb)(Http_Request_T req);
	void (*header_cb)(Http_Request_T req);
	const struct Http_Page_T* alias;
	Parser_Method_T method __attribute__ ((aligned(4)));
	const Rule_T* path_rules;
	const Rule_T* query_rules;
	uint32_t path_rules_amount : 4;
	uint32_t query_rules_amount : 4;
	uint32_t len : 16;
	uint32_t dynamic : 1;
	uint32_t chunked : 1;
	uint32_t event : 1;
	uint32_t rest : 1;
	uint32_t panel : 1;
	uint32_t multipart : 1;
} __attribute__ ((aligned(4)));

Http_T http_new(void);
void http_delete(Http_T http);
uint8_t http_add_page(Http_T http, const struct Http_Page_T* page);
uint8_t http_req_send_chunked_json(Http_Request_T req, Json_T json);
uint8_t http_req_send_chunked(Http_Request_T req);
Buffer_T http_req_send_buffer(Http_Request_T req) ;
void http_req_close(Http_Request_T req);
void* http_req_data(Http_Request_T req);
void http_req_set_data(Http_Request_T req, void* data);
Parser_T http_req_parser(Http_Request_T req);
Http_T http_req_http(Http_Request_T req);
uint8_t http_help(Http_T http, Http_Request_T req, uint32_t it);

#endif
