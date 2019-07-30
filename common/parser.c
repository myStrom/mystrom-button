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


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <osapi.h>
#include "parser.h"
#include "slash.h"
#include "rule.h"
#include "item.h"
#include "debug.h"

typedef enum {
	Parser_Internal_Status_METHOD_URI_VERSION,
	Parser_Internal_Status_HEADER_SEARCH,
	Parser_Internal_Status_POST_CONTENT,
	Parser_Internal_Status_POST_CONTENT_READ,
	Parser_Internal_Status_END,
	Parser_Internal_Status_ERROR,
	Parser_Internal_Status_HEADERS_SEARCH
} Parser_Internal_Status_T;

struct Parser_T {
	uint8_t storage[256];
	struct Buffer_T buffer;
	struct Buffer_T content;
	Slash_T slash;
	Parser_Internal_Status_T status;
	Parser_Method_T method;
	uint32_t post_length;
	char* token;
	char* referer;
	char* path;
	char* content_type;
	uint8_t* content_storage;
	union {
		uint8_t flags;
		struct {
			uint8_t header_done : 1;
		};
	};
};

Parser_T ICACHE_FLASH_ATTR parser_new(void) {
	Parser_T parser;
	if (!(parser = (void*)malloc(sizeof(struct Parser_T)))) {
		return NULL;
	}
	memset(parser, 0, sizeof(struct Parser_T));
	buffer_init(&parser->buffer, sizeof(parser->storage), parser->storage);
	parser->method = Parser_Method_UNDEFINE;
	parser->status = Parser_Internal_Status_METHOD_URI_VERSION;
	return parser;
}

void ICACHE_FLASH_ATTR parser_free(Parser_T parser) {
	if (!parser) {
		return;
	}
	if (parser->content_storage) {
		free(parser->content_storage);
	}
	if (parser->path) {
		free(parser->path);
	}
	if (parser->token) {
		free(parser->token);
	}
	if (parser->referer) {
		free(parser->referer);
	}
	if (parser->content_type) {
		free(parser->content_type);
	}
	free(parser);
}

const char* ICACHE_FLASH_ATTR parser_token(Parser_T parser) {
	if (!parser) {
		return NULL;
	}
	return parser->token;
}

const char* ICACHE_FLASH_ATTR parser_referer(Parser_T parser) {
	if (!parser) {
		return NULL;
	}
	return parser->referer;
}

char* ICACHE_FLASH_ATTR parser_content_type(Parser_T parser) {
	if (!parser) {
		return NULL;
	}
	return parser->content_type;
}

uint32_t ICACHE_FLASH_ATTR parser_data_len(Parser_T parser) {
	if (!parser) {
		return 0;
	}
	return parser->post_length;
}

char* ICACHE_FLASH_ATTR parser_decode_url(char* input) {
	uint8_t digit;
	uint16_t i;
	uint16_t j = 0;
	uint8_t k;
	if (!input) {
		return NULL;
	}
	for (i = 0; i < strlen(input); i++) {
		if (input[i] == '%') {
			if ((i + 2) < strlen(input)) {
				digit = 0;
				for (k = 0; k < 2; k++) {
					i++;
					digit <<= 4;
					if ((input[i] >= '0') && (input[i] <= '9')) {
						digit |= (input[i] - '0') & 0x0F;
					} else {
						if ((input[i] >= 'A') && (input[i] <= 'F')) {
							digit |= (10 + input[i] - 'A') & 0x0F;
						} else {
							if ((input[i] >= 'a') && (input[i] <= 'f')) {
								digit |= (10 + input[i] - 'a') & 0x0F;
							}
						}
					}
				}
				input[j] = digit;
			} else {
				break;
			}
		} else {
			input[j] = input[i];
		}
		j++;
	}
	input[j] = 0;
	return input;
}

static char* ICACHE_FLASH_ATTR parser_header_name_format(char* name) {
	uint16_t i;
	uint8_t big = 1;
	if (!name) {
		return NULL;
	}
	for (i = 0; i < strlen(name); i++) {
		if (name[i] == '-') {
			big = 1;
		} else {
			if (big) {
				name[i] = toupper(name[i]);
			} else {
				name[i] = tolower(name[i]);
			}
			big = 0;
		}
	}
	return name;
}

static Parser_State_T ICACHE_FLASH_ATTR parser_return(Parser_T parser, Parser_State_T state) {
	if (!parser) {
		return Parser_State_Internal_Server_Error_500;
	}
	switch (state) {
		case Parser_State_Continue_100:
			return Parser_State_Continue_100;

		case Parser_State_OK_200:
			parser->status = Parser_Internal_Status_END;
			return Parser_State_OK_200;

		default:
			buffer_clear(&parser->buffer);
			parser->status = Parser_Internal_Status_ERROR;
			return state;
	}
	return state;
}

Parser_State_T ICACHE_FLASH_ATTR parser_do(Parser_T parser, uint8_t data, uint32_t total_len) {
	if (!parser) {
		return Parser_State_Internal_Server_Error_500;
	}
	if (parser->status != Parser_Internal_Status_POST_CONTENT_READ) {
		buffer_write(&parser->buffer, data);
		if (buffer_overflow(&parser->buffer)) {
			buffer_set_writer(&parser->buffer, buffer_capacity(&parser->buffer) - 2);
			buffer_puts(&parser->buffer, "\r\n");
		}
	} else {
		if (total_len > buffer_capacity(&parser->content)) {
			uint16_t size = buffer_size(&parser->content);
			uint8_t* new_storage = NULL;
			new_storage = realloc(parser->content_storage, size + total_len + 1);
			if (!new_storage) {
				debug_describe_P("Cannot alloc content storage");
				if (parser->content_storage) {
					free(parser->content_storage);
					parser->content_storage = NULL;
				}
				return parser_return(parser, Parser_State_Internal_Server_Error_500);
			} else {
				debug_describe_P("Buffer realloc");
			}
			parser->content_storage = new_storage;
			buffer_init(&parser->content, size + total_len + 1, parser->content_storage);
			buffer_set_writer(&parser->content, size);
		}
		buffer_write(&parser->content, data);
	}
	switch (parser->status) {
		case Parser_Internal_Status_METHOD_URI_VERSION: {
			char* method;
			char* url;
			char* version;
			if (!buffer_equal_from_end(&parser->buffer, (uint8_t*)"\r\n", 2)) {
				return parser_return(parser, Parser_State_Continue_100);
			}
			buffer_set_writer(&parser->buffer, buffer_size(&parser->buffer) - 2);
			if (!slash_init(&parser->slash, buffer_string(&parser->buffer), ' ')) {
				return parser_return(parser, Parser_State_Internal_Server_Error_500);
			}
			if (!(method = slash_next(&parser->slash)) ||
				!(url = slash_next(&parser->slash)) ||
				!(version = slash_next(&parser->slash))) {
				debug_describe_P("Bad start line");
				return parser_return(parser, Parser_State_Bad_Request_400);
			}
			if (!strcmp((char*)method, "GET")) {
				parser->method = Parser_Method_GET;
			} else {
				if (!strcmp((char*)method, "POST")) {
					parser->method = Parser_Method_POST;
				} else {
					if (!strcmp((char*)method, "OPTIONS")) {
						parser->method = Parser_Method_OPTIONS;
					} else {
					debug_describe_P("Method not supported");
					return parser_return(parser, Parser_State_Method_Not_Allowed_405);
				}
			}
			}
			if (!(parser->path = strdup(url))) {
				return parser_return(parser, Parser_State_Internal_Server_Error_500);
			}
			if (strcmp((char*)version, "HTTP/1.1")) {
				return parser_return(parser, Parser_State_HTTP_Version_Not_Supported_505);
			}
			buffer_clear(&parser->buffer);
			parser->status = Parser_Internal_Status_HEADER_SEARCH;
			return parser_return(parser, Parser_State_Continue_100);
		}
		break;

		case Parser_Internal_Status_HEADER_SEARCH: {
			char* name;
			char* value;
			if (!buffer_equal_from_end(&parser->buffer, (uint8_t*)"\r\n", 2)) {
				return Parser_State_Continue_100;
			}
			if (buffer_size(&parser->buffer) == 2) {
				/*end of header, read content if required*/
				uint32_t len;
				if ((parser->method == Parser_Method_GET) ||
					(parser->method == Parser_Method_OPTIONS) ||
					!parser->post_length) {
					buffer_clear(&parser->buffer);
					parser->header_done = 1;
					return parser_return(parser, Parser_State_OK_200);
				}
				buffer_clear(&parser->buffer);
				if (parser->post_length > 1500) {
					len = 1500;
				} else {
					len = parser->post_length;
				}
				if (!(parser->content_storage = malloc(len + 1))) {
					return parser_return(parser, Parser_State_Internal_Server_Error_500);
				}
				memset(parser->content_storage, 0, len + 1);
				buffer_init(&parser->content, len + 1, parser->content_storage);
				parser->status = Parser_Internal_Status_POST_CONTENT_READ;
				parser->header_done = 1;
				return parser_return(parser, Parser_State_Continue_100);
			}
			buffer_set_writer(&parser->buffer, buffer_size(&parser->buffer) - 2);
			if (!slash_init_delimeters(&parser->slash, buffer_string(&parser->buffer), ":")) {
				return parser_return(parser, Parser_State_Internal_Server_Error_500);
			}
			if (!(name = parser_header_name_format(slash_next(&parser->slash)))) {
				debug_describe_P("No header name");
				return parser_return(parser, Parser_State_Bad_Request_400);
			}
			value = slash_current(&parser->slash);
			slash_escape(&value, " ");
			if (!(value = parser_decode_url(value))) {
				buffer_clear(&parser->buffer);
				return parser_return(parser, Parser_State_Continue_100);
			}
			if (!strcasecmp((char*)name, "Content-Length")) {
				int32_t ret_val;
				if (!rule_check_digit(value, 0, 1 * 1024 * 1024 -1, &ret_val)) {
					debug_describe("Rule lenght check");
					return parser_return(parser, Parser_State_Bad_Request_400);
				}
				parser->post_length = ret_val;
				debug_value(parser->post_length);
			} else {
				if (!strcasecmp((char*)name, "Referer")) {
					if (parser->referer) {
						free(parser->referer);
					}
					parser->referer = strdup(value);
					debug_printf("Referer given: %s\n", value);
				} else {
					if (!strcasecmp((char*)name, "Token")) {
						if (parser->token) {
							free(parser->token);
						}
						parser->token = strdup(value);
					} else {
						if (!strcasecmp((char*)name, "Content-Type")) {
							if (parser->content_type) {
								free(parser->content_type);
							}
							parser->content_type = strdup(value);
						}
					}
				}
			}
			buffer_clear(&parser->buffer);
			return parser_return(parser, Parser_State_Continue_100);
		}
		break;

		case Parser_Internal_Status_POST_CONTENT_READ: {
			if (buffer_overflow(&parser->content)) {
				return parser_return(parser, Parser_State_Bad_Request_400);
			}
			if (buffer_size(&parser->content) < parser->post_length) {
				return parser_return(parser, Parser_State_Continue_100);
			}
			return parser_return(parser, Parser_State_OK_200);
		}
		break;

		case Parser_Internal_Status_END: {
			return Parser_State_OK_200;
		}
		break;

		case Parser_Internal_Status_ERROR: {
			buffer_clear(&parser->buffer);
		}
		break;

		default:
			break;
	}
	return Parser_State_Internal_Server_Error_500;
}

char* ICACHE_FLASH_ATTR parser_path(Parser_T parser) {
	if (!parser) {
		return NULL;
	}
	return parser->path;
}

Buffer_T ICACHE_FLASH_ATTR parser_content(Parser_T parser) {
	if (!parser || !parser->content_storage) {
		return NULL;
	}
	return &parser->content;
}

uint8_t ICACHE_FLASH_ATTR parser_header_done(Parser_T parser) {
	if (!parser) {
		return 0;
	}
	return parser->header_done;
}

Parser_Method_T ICACHE_FLASH_ATTR parser_method(Parser_T parser) {
	if (!parser) {
		return Parser_Method_UNDEFINE;
	}
	return parser->method;
}
