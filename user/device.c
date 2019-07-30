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
#include <pwm.h>
#include "device.h"
#include "buffer.h"
#include "slash.h"
#include "rule.h"
#include "item.h"
#include "debug.h"
#include "rgb.h"
#include "collect.h"
#include "json.h"
#include "value.h"
#include "array_size.h"
#include "parser.h"
#include "sleep.h"
#include "url_storage.h"

extern Collect_T collect;
extern struct Store_T store;
extern char own_mac[13];
extern Url_Storage_T url_storage;

static Parser_State_T ICACHE_FLASH_ATTR device_service(Buffer_T buffer, uint8_t enable);

static const Rule_T device_args_rule[] ICACHE_RODATA_ATTR = {
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
			.equal = "device"
		}
	},
	{
		.name = "mac",
		.type = RULE_MAC_OR_SELF,
		.required = 1
	},
};

static const Rule_T device_thresholds_args_rule[] ICACHE_RODATA_ATTR = {
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
			.equal = "thresholds"
		}
	}
};

static const Rule_T device_veryfication_args_rule[] ICACHE_RODATA_ATTR = {
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
			.equal = "verification"
		}
	}
};

static const Rule_T device_sleep_args_rule[] ICACHE_RODATA_ATTR = {
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
			.equal = "sleep"
		}
	}
};

static const Rule_T device_query_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "single",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = URL_MAX_SIZE - 1
	},
	{
		.name = "double",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = URL_MAX_SIZE - 1
	},
	{
		.name = "long",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = URL_MAX_SIZE - 1
	},
	{
		.name = "touch",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = URL_MAX_SIZE - 1
	},
	{
		.name = "generic",
		.type = RULE_CHAR,
		.required = 0,
		.min_len = 0,
		.max_len = URL_MAX_SIZE - 1
	},
};

static const Rule_T device_thresholds_rule[] ICACHE_RODATA_ATTR = {
	{
		.name = "ch0",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 63
			}
		}
	},
	{
		.name = "ch1",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 63
			}
		}
	},
	{
		.name = "ch2",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 63
			}
		}
	},
	{
		.name = "ch3",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 63
			}
		}
	},
	{
		.name = "ch7",
		.type = RULE_UNSIGNED_INT,
		.required = 0,
		.detail = {
			.unsigned_int = {
				.min_val = 1,
				.max_val = 63
			}
		}
	}
};

static const Rule_T device_veryfication_rule[] ICACHE_RODATA_ATTR = {
	{
		.type = RULE_BOOLEAN,
		.required = 1,
		.name = "enabled"
	}
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_1(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	Json_T root;
	if (!(root = collect_get_devices(collect))) {
		return Parser_State_Internal_Server_Error_500;
	}
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_1 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_1,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = device_args_rule,
	.path_rules_amount = 2,
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_2(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	Json_T root;
	Json_T obj;
	debug_enter_here_P();
	if (!(obj = collect_get_device(collect, args[2].data_char))) {
		return Parser_State_Not_Found_404;
	}
	if (!(root = json_new())) {
		json_delete(obj);
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_obj(root, args[2].data_char, obj);
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_2 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_2,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = device_args_rule,
	.path_rules_amount = 3,
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_3(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	struct Value_T query[5];
	Slash_T amper;
	Json_T root;
	uint8_t i;
	char* value;
	char* url = NULL;
	if (content && buffer_size(content)) {
		slash_init(&amper, buffer_string(content), '&');
		if (!slash_parse(&amper,
			device_query_rule,
			query,
			5,
			'=')) {
			return Parser_State_Bad_Request_400;
		}
		for (i = 0; i < ARRAY_SIZE(query); i++) {
			if (query[i].present) {
				value = NULL;
				if (query[i].string_value && strlen(query[i].string_value)) {
					value = parser_decode_url(query[i].string_value);
				}
				if (!value || !strlen(value)) {
					url_storage_write(&url_storage, i, "");
				} else {
					url_storage_write(&url_storage, i, value);
				}
			}
		}
	}
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}

	url = url_storage_read(&url_storage, URL_TYPE_SINGLE);
	json_add_string(root, "single", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_DOUBLE);
	json_add_string(root, "double", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_LONG);
	json_add_string(root, "long", url ? url : "");
	free(url);

	url = url_storage_read(&url_storage, URL_TYPE_TOUCH);
	json_add_string(root, "touch", url ? url : "");
	free(url);
	
	url = url_storage_read(&url_storage, URL_TYPE_GENERIC);
	json_add_string(root, "generic", url ? url : "");
	free(url);

	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_3 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_3,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = device_args_rule,
	.path_rules_amount = 3,
	.rest = 1
};

#ifdef IQS
static Parser_State_T ICACHE_FLASH_ATTR device_exec_4(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	struct Value_T query[ARRAY_SIZE(device_thresholds_rule)];
	Slash_T amper;
	Json_T root;
	uint8_t i;
	uint8_t any = 0;
	if (content && buffer_size(content)) {
		slash_init(&amper, buffer_string(content), '&');
		if (!slash_parse(&amper, 
			device_thresholds_rule, 
			query, 
			ARRAY_SIZE(device_thresholds_rule), 
			'=')) {
			return Parser_State_Bad_Request_400;
		}
		for (i = 0; i < ARRAY_SIZE(query); i++) {
			if (query[i].present) {
				switch (i) {
					case 0:
					case 1:
					case 2:
					case 3:
						store.thresholds[i] = query[i].uint_value;
						break;
					case 4:
						store.thresholds[7] = query[i].uint_value;
						break;
				};
				any = 1;
			}
		}
		if (any) {
			store_save();
			if (iqs_reload()) {
				if (!(root = json_new())) {
					return Parser_State_Internal_Server_Error_500;
				}
				json_add_int(root, "ch0", store.thresholds[0]);
				json_add_int(root, "ch1", store.thresholds[1]);
				json_add_int(root, "ch2", store.thresholds[2]);
				json_add_int(root, "ch3", store.thresholds[3]);
				json_add_int(root, "ch7", store.thresholds[7]);
				if ((*buffer = json_to_buffer(root))) {
					return Parser_State_OK_200;
				}
				return Parser_State_Internal_Server_Error_500;
			}
			return Parser_State_Internal_Server_Error_500;
		}
	}
	return Parser_State_Bad_Request_400;
}

static const struct Http_Page_T device_page_4 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_4,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = device_thresholds_args_rule,
	.path_rules_amount = ARRAY_SIZE(device_thresholds_args_rule),
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_5(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	Json_T root;
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_int(root, "ch0", store.thresholds[0]);
	json_add_int(root, "ch1", store.thresholds[1]);
	json_add_int(root, "ch2", store.thresholds[2]);
	json_add_int(root, "ch3", store.thresholds[3]);
	json_add_int(root, "ch7", store.thresholds[7]);
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_5 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_5,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = device_thresholds_args_rule,
	.path_rules_amount = ARRAY_SIZE(device_thresholds_args_rule),
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_6(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	Json_T root;
	if (!args) {
		return Parser_State_Internal_Server_Error_500;
	}
	if (query_path[0].bool_value) {
		store.veryfication = Veryfication_Double;
	} else {
		store.veryfication = Veryfication_Single;
	}
	store_save();
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_bool(root, "enabled", (store.veryfication == Veryfication_Double) ? 1 : 0);
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_6 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_6,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = device_veryfication_args_rule,
	.path_rules_amount = ARRAY_SIZE(device_veryfication_args_rule),
	.query_rules = device_veryfication_rule,
	.query_rules_amount = ARRAY_SIZE(device_veryfication_rule),
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR device_exec_7(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	Json_T root;
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	json_add_bool(root, "enabled", (store.veryfication == Veryfication_Double) ? 1 : 0);
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T device_page_7 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = device_exec_7,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = device_veryfication_args_rule,
	.path_rules_amount = ARRAY_SIZE(device_veryfication_args_rule),
	.rest = 1
};
#endif

static Parser_State_T ICACHE_FLASH_ATTR device_exec_8(Buffer_T* buffer, Item_T* args, Value_T query_path, Buffer_T content) {
	sleep_immediately();
	return Parser_State_OK_200;
}

static const struct Http_Page_T device_page_8 ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "text/plain",
	.exec = device_exec_8,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = device_sleep_args_rule,
	.path_rules_amount = ARRAY_SIZE(device_sleep_args_rule),
	.rest = 1
};

static const Rule_T action_args_rule[] = {
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
			.equal = "action"
		}
	},
	{
		.name = "action",
		.type = RULE_ENUM,
		.required = 1,
		.detail = {
			.enumerate = {
					.values = (const char*[]){"single", "double", "long", "touch", "generic"},
					.amount = 5
			}
		}
	},
};

static Parser_State_T ICACHE_FLASH_ATTR action_set_request_cb(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	uint8_t action = args[2].data_uint32;
	char* value;
	if (!content) {
		value = "";
	} else {
		if (buffer_size(content) >= URL_MAX_SIZE) {
			return Parser_State_Bad_Request_400;
		}
		value = buffer_string(content);
	}
	if (!url_storage_write(&url_storage, action, value)) {
		return Parser_State_Internal_Server_Error_500;
	}
	return Parser_State_OK_200;
}

static const struct Http_Page_T page_action_set ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = action_set_request_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.path_rules = action_args_rule,
	.path_rules_amount = 3,
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR action_get_request_cb(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	Json_T root = NULL;
	uint8_t action = args[2].data_uint32;
	char* url;
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	url = url_storage_read(&url_storage, action);
	if (url) {
		json_add_string(root, "url", url);
		free(url);
	} else {
		json_add_string(root, "url", "");
	}
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T page_action_get ICACHE_RODATA_ATTR STORE_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = action_get_request_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = action_args_rule,
	.path_rules_amount = 3,
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR action_get_all_request_cb(Buffer_T* buffer, Item_T* args, Value_T query, Buffer_T content) {
	const char** name = (const char*[]){"single", "double", "long", "touch", "generic"};
	Json_T root;
	char* value;
	uint8_t i;
	if (!(root = json_new())) {
		return Parser_State_Internal_Server_Error_500;
	}
	for (i = 0; i < __URL_TYPE_MAX; i++) {
		value = url_storage_read(&url_storage, i);
		json_add_string(root, name[i], value ? value : "");
		free(value);
	}
	if ((*buffer = json_to_buffer(root))) {
		return Parser_State_OK_200;
	}
	return Parser_State_Internal_Server_Error_500;
}

static const struct Http_Page_T page_action_all_get ICACHE_RODATA_ATTR = {
	.path = "api",
	.content = NULL,
	.type = "application/json",
	.exec = action_get_all_request_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.path_rules = action_args_rule,
	.path_rules_amount = 2,
	.rest = 1
};

static Parser_State_T ICACHE_FLASH_ATTR help_exec_cb(Http_Request_T req) {
	Http_T http;
	if (!req || !(http = http_req_http(req))) {
		return Parser_State_Internal_Server_Error_500;
	}
	return Parser_State_OK_200;
}

static void ICACHE_FLASH_ATTR help_send_cb(Http_Request_T req) {
	Http_T http;
	uint32_t* it;
	if (!req ||
		!(http = http_req_http(req)) ||
		!(it = http_req_data(req))) {
			debug_enter_here_P();
		return;
	}
	http_help(http, req, *it);
	(*it)++;
}

static void ICACHE_FLASH_ATTR help_header_cb(Http_Request_T req) {
	uint32_t* it;
	if (!req) {
		return;
	}
	if (!(it = calloc(sizeof(uint32_t), 1))) {
		return;
	}
	*it = 0;
	http_req_set_data(req, it);
	help_send_cb(req);
}

static void ICACHE_FLASH_ATTR help_close_cb(Http_Request_T req) {
	uint32_t* it;
	if (!req) {
		return;
	}
	if (!(it = http_req_data(req))) {
		return;
	}
	free(it);
	debug_describe_P("Free it");
}

static const struct Http_Page_T page_help ICACHE_RODATA_ATTR = {
	.path = "help",
	.content = NULL,
	.type = "text/plain",
	.exec_req = help_exec_cb,
	.header_cb = help_header_cb,
	.send_cb = help_send_cb,
	.close_cb = help_close_cb,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_GET,
	.panel = 1,
	.chunked = 1,
	.len = 512,
};

uint8_t ICACHE_FLASH_ATTR device_http(Http_T http) {
	if (!http) {
		return 0;
	}
	http_add_page(http, &device_page_1);
	http_add_page(http, &device_page_2);
	http_add_page(http, &device_page_3);
#ifdef IQS
	http_add_page(http, &device_page_4);
	http_add_page(http, &device_page_5);
	http_add_page(http, &device_page_6);
	http_add_page(http, &device_page_7);
#endif
	http_add_page(http, &device_page_8);
	http_add_page(http, &page_action_set);
	http_add_page(http, &page_action_get);
	http_add_page(http, &page_action_all_get);
	http_add_page(http, &page_help);
	return 1;
}
