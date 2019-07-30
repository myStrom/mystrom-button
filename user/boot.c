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
#include <osapi.h>
#include <user_interface.h>
#include <espconn.h>
#include <upgrade.h>
#include <eagle_soc.h>
#include "boot.h"
#include "debug.h"
#include "buffer.h"
#include "slash.h"
#include "crc.h"
#include "array_size.h"
#include "color.h"
#include "sleep.h"

#define BOOT_MAX_PAGES 248
#define BOOT_MAX_IMAGE_SIZE ((uint32_t)BOOT_MAX_PAGES * SPI_FLASH_SEC_SIZE)

extern volatile uint32_t download_process;


static uint8_t ICACHE_FLASH_ATTR boot_write_data(uint32_t* image_size, uint8_t* data, size_t size) {
	if (!data) {
		return 0;
	}
	if (!size) {
		return 1;
	}
	if (image_size) {
		*image_size += size;
		if (*image_size >= BOOT_MAX_IMAGE_SIZE) {
			debug_describe_P(COLOR_RED "Image too big" COLOR_END);
			return 0;
		}
	}
	system_upgrade(data, size);
	return 1;
}

static uint8_t ICACHE_FLASH_ATTR boot_write(uint32_t* image_size, Buffer_T parser, Buffer_T tail) {
	if (!parser || !tail) {
		return 0;
	}
	if (buffer_size(parser) >= buffer_capacity(tail)) {
		if (!boot_write_data(image_size, buffer_data(tail, 0), buffer_size(tail))) {
		return 0;
		}
		if (!boot_write_data(image_size, buffer_data(parser, 0), buffer_size(parser) - buffer_capacity(tail))) {
			return 0;
		}
		buffer_clear(tail);
		buffer_append_fast(tail, buffer_data(parser, buffer_size(parser) - buffer_capacity(tail)), buffer_capacity(tail));
	} else {
		if (buffer_size(parser) > buffer_remaining_write_space(tail)) {
			if (!boot_write_data(image_size, buffer_data(tail, 0), buffer_size(parser) - buffer_remaining_write_space(tail))) {
				return 0;
			}
			buffer_shift_left(tail, buffer_size(parser) - buffer_remaining_write_space(tail));
		}
		buffer_append_fast(tail, buffer_data(parser, 0), buffer_size(parser));
	}
	return 1;
}

typedef struct {
	enum {
		BEGIN_SIGNATURE,
		HEADER_DISPOSITION,
		HEADER_TYPE,
		STATE_CRLF,
		BEGIN_DATA,
		WRITE_DATA
	} state;
	char boundary[128];
	char bound[128];
	uint8_t tail_sorage[128];
	struct Buffer_T tail;
	uint32_t erase_counter;
	uint32_t image_size;
	uint8_t success : 1;
} Load_Req_T;

static void ICACHE_FLASH_ATTR load_erase(Load_Req_T* load) {
	if (!load) {
		return;
	}
	if (load->erase_counter > 0) {
		if (load->erase_counter >= LIMIT_ERASE_SIZE) {
			debug_put('E');
			system_upgrade_erase_flash(0xFFFF);
			load->erase_counter -= LIMIT_ERASE_SIZE;
		} else {
			debug_put('e');
			system_upgrade_erase_flash(load->erase_counter);
			load->erase_counter = 0;
		}
	}
}

static Parser_State_T ICACHE_FLASH_ATTR handle_load(Http_Request_T req) {
	Buffer_T parser;
	Load_Req_T* load;
	Buffer_T tail;
	char* ct;
	char* part;
	char* tok_s;
	char* tok_c;
	char* name;
	char* value;
	uint32_t bound_size;
	uint16_t end;

	load = http_req_data(req);
	if (!load) {
		if (download_process) {
			return Parser_State_Method_Not_Allowed_405;
		}
		load = malloc(sizeof(Load_Req_T));
		if (!load) {
			return Parser_State_Internal_Server_Error_500;
		}
		sleep_lock(SLEEP_UPGRADE);
		memset(load, 0, sizeof(Load_Req_T));
		buffer_init(&load->tail, sizeof(load->tail_sorage), load->tail_sorage);
		ct = parser_content_type(http_req_parser(req));
		if (!ct ||!strlen(ct)) {
			debug_describe_P(COLOR_RED "Not found content length header" COLOR_END);
			goto exit;
		}
		if (!(part = strtok_r(ct, "; ", &tok_s)) || strcmp(part, "multipart/form-data")) {
			debug_describe_P(COLOR_RED "Bad content type" COLOR_END);
			goto exit;
		}
		if (!(part = strtok_r(NULL, "; ", &tok_s)) || !strlen(part)) {
			debug_describe_P(COLOR_RED "Unable to select boundary part" COLOR_END);
			goto exit;
		}
		if (!(name = strtok_r(part, "=", &tok_c)) || strcmp(name, "boundary")) {
			debug_describe_P(COLOR_RED "Bad boundary name" COLOR_END);
			goto exit;
		}
		if (!(value = strtok_r(NULL, "=", &tok_c)) || !strlen(value)) {
			debug_describe_P(COLOR_RED "Bad boundary value" COLOR_END);
			goto exit;
		}
		strncpy(load->boundary, value, sizeof(load->boundary) - 1);
		load->erase_counter = parser_data_len(http_req_parser(req));
		debug_value(load->erase_counter);
		http_req_set_data(req, load);
		download_process = 1;
	}
	parser = parser_content(http_req_parser(req));
	while (1) {
		tail = &load->tail;
		bound_size = strlen(load->bound);
		buffer_reset_read_counter(parser);
		if (load->state != WRITE_DATA) {
			if (!buffer_find_sub_string(parser, "\r\n", &end)) {
				break;
			}
			buffer_write_index(parser, 0, end);
		}
		switch (load->state) {
			case BEGIN_SIGNATURE:
				memset(load->bound, 0, sizeof(load->bound));
				snprintf(load->bound, sizeof(load->bound), "\r\n%s--\r\n", (char*)buffer_data(parser, 0));
				while (buffer_size(parser) && strlen((char*)buffer_data(parser, 0))) {
					if (!strcmp((char*)buffer_data(parser, 0), load->boundary)) {
						load->state = HEADER_DISPOSITION;
						buffer_init(&load->tail, strlen(load->bound), load->tail_sorage);
						break;
					}
					buffer_shift_left(parser, 1);
					end--;
				}
				if (load->state == BEGIN_SIGNATURE) {
					debug_describe_P(COLOR_RED "Bad boundary" COLOR_END);
					goto exit;
				}
				break;

			case HEADER_DISPOSITION:
				if (!(part = slash_value((char*)buffer_data(parser, 0), "Content-Disposition", ": "))) {
					debug_describe_P(COLOR_RED "No content disposition header" COLOR_END);
					goto exit;
				}
				if (!(value = strtok_r(part, "; ", &tok_c)) || strcmp(value, "form-data")) {
					debug_describe_P(COLOR_RED "Content is not form-data" COLOR_END);
					goto exit;
				}
				load->state = HEADER_TYPE;
				break;

			case HEADER_TYPE:
				if (!(part = slash_value((char*)buffer_data(parser, 0), "Content-Type", ": "))) {
					debug_describe_P(COLOR_RED "No content type header" COLOR_END);
					goto exit;
				}
				if (strcmp(part, "application/octet-stream") && strcmp(part, "application/macbinary")) {
					debug_printf(COLOR_RED "Wrong content type: %s" COLOR_END, part);
					goto exit;
				}
				load->state = STATE_CRLF;
				break;

			case STATE_CRLF:
				debug_describe_P("Open file");
				system_upgrade_init();
				system_upgrade_flag_set(UPGRADE_FLAG_START);
				load->state = BEGIN_DATA;
				break;

			case WRITE_DATA:
				load_erase(load);
				if (!boot_write(&load->image_size, parser, tail)) {
					goto exit;
				}
				if (buffer_equal_from_end_str(tail, load->bound)) {
					debug_put('*');
					debug_value(load->image_size);
					debug_value(buffer_size(parser));
					debug_value(bound_size);
					debug_value(strlen(load->bound));
					system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
					debug_describe_P(COLOR_GREEN "Upgrade success" COLOR_END);
					load->success = 1;
					return Parser_State_OK_200;
				} else {
					debug_put('.');
				}
				break;

			default:
				break;
		}
		if (load->state != WRITE_DATA) {
			buffer_shift_left(parser, end + 2);
			if (load->state == BEGIN_DATA) {
				load->state = WRITE_DATA;
			}
		} else {
			buffer_clear(parser);
			break;
		}
	}
	return Parser_State_Continue_100;
exit:
	if (load) {
		free(load);
		http_req_set_data(req, 0);
	}
	download_process = 0;
	sleep_unlock(SLEEP_UPGRADE);
	return Parser_State_Bad_Request_400;
}

static void ICACHE_FLASH_ATTR handle_load_close(Http_Request_T req) {
	Load_Req_T* load;
	uint8_t flag;
	if (!req) {
		return;
	}
	load = http_req_data(req);
	if (load) {
		free(load);
		http_req_set_data(req, 0);
	}
	flag = system_upgrade_flag_check();
	if (load->success && (flag == UPGRADE_FLAG_FINISH)) {
		system_upgrade_reboot();
	} else {
		if (flag == UPGRADE_FLAG_START) {
			system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
			system_upgrade_deinit();
		}
	}
	download_process = 0;
	sleep_unlock(SLEEP_UPGRADE);
	debug_describe_P("Load close");
}

static const struct Http_Page_T page_load ICACHE_RODATA_ATTR = {
	.path = "load",
	.content = NULL,
	.type = "text/plain",
	.exec_req = handle_load,
	.close_cb = handle_load_close,//handle_reboot,
	.len = 0,
	.dynamic = 1,
	.method = Parser_Method_POST,
	.query_rules = NULL,
	.query_rules_amount = 0,
	.panel = 1,
	.multipart = 1
};

uint8_t ICACHE_FLASH_ATTR boot_http(Http_T http) {
	if (!http) {
		return 0;
	}
	http_add_page(http, &page_load);
	return 1;
}

void ICACHE_FLASH_ATTR boot_init(void) {
	system_upgrade_init();
	if (system_upgrade_userbin_check()) {
		debug_describe_P(COLOR_CYAN "Boot from page B" COLOR_END);
	} else {
		debug_describe_P(COLOR_CYAN "Boot from page A" COLOR_END);
	}
}
