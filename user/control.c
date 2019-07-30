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
#include <inttypes.h>
#include <c_types.h>
#include <osapi.h>
#include "control.h"
#include "buffer.h"
#include "slash.h"
#include "rule.h"
#include "item.h"
#include "debug.h"
#include "json.h"
#include "value.h"
#include "store.h"
#include "array_size.h"
#include "sleep.h"

extern struct Store_T store;

static const Rule_T control_panel_rule[] = {
	{
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.type = RULE_ENUM,
		.required = 1,
		.detail = {
			.enumerate = {
				.amount = 2,
				.values = (const char*[]){"panel", "settings"},
			}
		}
	}
};

static const Rule_T control_set_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "panel",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
	{
		.name = "rest",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
	{
		.name = "token",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = sizeof(store.token) - 1,
	},
	{
		.name = "bssid",
		.type = RULE_BOOLEAN,
		.required = 0,
	},
};

static const Rule_T control_keep_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "keep"
		}
	},
};

static const Rule_T control_name_page_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "v1"
		}
	},
	{
		.name = "",
		.type = RULE_EQUAL,
		.required = 1,
		.detail = {
			.equal = "name"
		}
	},
};

static Parser_State_T ICACHE_FLASH_ATTR control_panel_get_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	Json_T json;
	if (!(json = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_bool(json, "panel", store.panel_dis ? 0 : 1);
	json_add_bool(json, "rest", store.rest_dis ? 0 : 1);
	json_add_bool(json, "token", strnlen(store.token, sizeof(store.token)) ? 1 : 0);
	json_add_bool(json, "bssid", store.bssid_enable);
	if ((*buffer = json_to_buffer(json))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T control_panel_get_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = control_panel_get_exec,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = control_panel_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR control_panel_set_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	struct Value_T query[ARRAY_SIZE(control_set_rule)];
	Json_T json;
	memset(&query, 0, sizeof(query));
	if (!content ||
		!buffer_size(content) ||
		!(json = json_parse(buffer_string(content), control_set_rule, ARRAY_SIZE(control_set_rule)))) {
		return Parser_State_Bad_Request_400;
	}
	if (!json_to_values(json, query)) {
		goto error;
	}
	if (!query[0].present && !query[1].present && !query[2].present && !query[3].present) {
		goto error;
	}
	if (query[0].present) {
		store.panel_dis = query[0].bool_value ? 0 : 1;
	}
	if (query[1].present) {
		store.rest_dis = query[1].bool_value ? 0 : 1;
	}
	if (query[2].present) {
		memset(store.token, 0, sizeof(store.token));
		strncpy(store.token, query[2].string_value, sizeof(store.token));
	}
	if (query[3].present) {
		store.bssid_enable = query[3].bool_value;
	}
	json_delete(json);
	store_save();
	return Parser_State_OK_200;
error:
	json_delete(json);
	return Parser_State_Bad_Request_400;
}

static const struct Http_Page_T control_panel_set_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = control_panel_set_exec,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = control_panel_rule,
	.path_rules_amount = 2,
	.panel = 1
};

static Parser_State_T ICACHE_FLASH_ATTR control_keep_exec(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	sleep_inhibit(GENERAL_INHIBIT_TIME);
	sleep_reset();
	return Parser_State_OK_200;
}

static const struct Http_Page_T control_panel_keep_page ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "text/html",
	.exec = control_keep_exec,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = control_keep_rule,
	.path_rules_amount = 2,
	.rest = 1
};

static const Rule_T srv_password_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "password",
		.required = 1,
		.type = RULE_CHAR,
		.min_len = 0,
		.max_len = 255,
	},
	{
		.name = "not_final",
		.required = 0,
		.type = RULE_BOOLEAN,
	},
};

static Parser_State_T set_name_cb(Buffer_T* out, Item_T* args, Value_T path_query, Buffer_T content) {
	if (!content || (buffer_size(content) >= sizeof(store.name))) {
		return Parser_State_Bad_Request_400;
	}
	strncpy(store.name, buffer_string(content), sizeof(store.name) - 1);
	store_save();
	return Parser_State_OK_200;
}

static const struct Http_Page_T page_set_name ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "text/plain",
	.exec = set_name_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = control_name_page_rule,
	.path_rules_amount = 2,
	.rest = 1,
};

static Parser_State_T get_name_cb(Buffer_T* buffer, Item_T* args, Value_T path_query, Buffer_T content) {
	Json_T root;
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_string(root, "name", store.name);
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T page_get_name ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = get_name_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = control_name_page_rule,
	.path_rules_amount = 2,
	.rest = 1,
};

uint8_t ICACHE_FLASH_ATTR control_http(Http_T http) {
	if (!http) {
		return 0;
	}
	http_add_page(http, &control_panel_get_page);
	http_add_page(http, &control_panel_set_page);
	http_add_page(http, &control_panel_keep_page);
	http_add_page(http, &page_get_name);
	http_add_page(http, &page_set_name);
	return 1;
}
