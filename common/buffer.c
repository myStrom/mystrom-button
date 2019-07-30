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


#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <osapi.h>
#include "buffer.h"
#include "array_size.h"

typedef struct {
	char unprint;
	char escape;
} Buffer_Unprint_T;

static Buffer_Unprint_T buffer_converter[] = {
	{'"', '"'},
	{'\\', '\\'},
	{'/', '/'},
	{'\b', 'b'},
	{'\f', 'f'},
	{'\n', 'n'},
	{'\r', 'r'},
	{'\t', 't'},
};

Buffer_T ICACHE_FLASH_ATTR buffer_new(uint16_t capacity) {
	Buffer_T buffer;
	if (!capacity) {
		return NULL;
	}
	if (!(buffer = calloc(1, sizeof(struct Buffer_T) + (sizeof(Buffer_Type_T) * capacity)))) {
		return NULL;
	}
	buffer->capacity = capacity;
	buffer->read_it = 0;
	buffer->write_it = 0;
	buffer->overflow = 0;
	buffer->data = buffer->store;
	return buffer;
}

void ICACHE_FLASH_ATTR buffer_delete(Buffer_T buffer) {
	if (!buffer) {
		return;
	}
	free(buffer);
}

Buffer_T ICACHE_FLASH_ATTR buffer_init(Buffer_T buffer, uint16_t capacity, uint8_t* storage) {
	if (!buffer || !capacity || !storage) {
		return 0;
	}
	memset(buffer, 0, sizeof(struct Buffer_T));
	buffer->capacity = capacity;
	buffer->read_it = 0;
	buffer->write_it = 0;
	buffer->overflow = 0;
	buffer->data = storage;
	return buffer;
}

uint8_t ICACHE_FLASH_ATTR buffer_write(Buffer_T buffer, Buffer_Type_T data) {
	if (!buffer) {
		return 0;
	}
	if (buffer->write_it < buffer->capacity) {
		buffer->data[buffer->write_it++] = data;
		return 1;
	}
	buffer->overflow++;
	return 0;
}

uint16_t ICACHE_FLASH_ATTR buffer_size(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->write_it;
}

uint16_t ICACHE_FLASH_ATTR buffer_size_offset(Buffer_T buffer, uint16_t offset) {
	if (!buffer || (buffer->write_it < offset)) {
		return 0;
	}
	return buffer->write_it - offset;
}

Buffer_Type_T* ICACHE_FLASH_ATTR buffer_data(Buffer_T buffer, uint16_t offset) {
	if (!buffer) {
		return 0;
	}
	if (offset < buffer->capacity) {
		return &buffer->data[offset];
	}
	return 0;
}

uint16_t ICACHE_FLASH_ATTR buffer_clear(Buffer_T buffer) {
	uint16_t size;
	if (!buffer) {
		return 0;
	}
	size = buffer->write_it;
	buffer->write_it = 0;
	buffer->read_it = 0;
	buffer->overflow = 0;
	return size;
}

void ICACHE_FLASH_ATTR buffer_reset_read_counter(Buffer_T buffer) {
	if (!buffer) {
		return;
	}
	buffer->read_it = 0;
}

uint16_t ICACHE_FLASH_ATTR buffer_bytes_to_read(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->write_it - buffer->read_it;
}

uint16_t ICACHE_FLASH_ATTR buffer_read_it(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->read_it;
}

uint16_t ICACHE_FLASH_ATTR buffer_write_it(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->write_it;
}

Buffer_Type_T ICACHE_FLASH_ATTR buffer_read_next(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	if (buffer->read_it < buffer->write_it) {
		return buffer->data[buffer->read_it++];
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR buffer_equal(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length) {
	if (!buffer || !data) {
		return 0;
	}
	if (!length || (length > buffer->write_it)) {
		return 0;
	}
	do {
		length--;
		if (buffer->data[length] != data[length]) {
			return 0;
		}
	} while (length);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_equal_str(Buffer_T buffer, const char* str) {
	if (!buffer || !str) {
		return 0;
	}
	return buffer_equal(buffer, (uint8_t*)str, strlen(str));
}

uint8_t ICACHE_FLASH_ATTR buffer_equal_from_end(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length) {
	uint16_t index;
	if (!buffer || !length || (length > buffer->write_it)) {
		return 0;
	}
	index = buffer->write_it;
	do {
		length--;
		index--;
		if (buffer->data[index] != data[length]) {
			return 0;
		}
	} while (length);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_equal_from_end_str(Buffer_T buffer, const char* str) {
	if (!buffer || !str) {
		return 0;
	}
	return buffer_equal_from_end(buffer, (uint8_t*)str, strlen(str));
}

Buffer_Type_T ICACHE_FLASH_ATTR buffer_read(Buffer_T buffer, uint16_t index) {
	if (!buffer) {
		return 0;
	}
	if (index < buffer->write_it) {
		return buffer->data[index];
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR buffer_read_index(Buffer_T buffer, Buffer_Type_T* data, uint16_t index) {
	if (!buffer || !data) {
		return 0;
	}
	if (index < buffer->write_it) {
		*data = buffer->data[index];
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR buffer_set_writer(Buffer_T buffer, uint16_t offset) {
	if (!buffer || (offset > buffer->capacity)) {
		return 0;
	}
	buffer->write_it = offset;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_write_index(Buffer_T buffer, Buffer_Type_T data, uint16_t index) {
	if (!buffer || (index >= buffer->capacity)) {
		return 0;
	}
	buffer->data[index] = data;
	if (buffer->write_it <= index) {
		buffer->write_it = index + 1;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_overwrite(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length, uint16_t offset) {
	uint16_t i;
	if (!buffer || !data || !length) {
		return 0;
	}
	for (i = 0; i < length; i++) {
		if (!buffer_write_index(buffer, *(data + i), offset + i)) {
			return 0;
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_overwrite_form_end(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length, uint16_t offset) {
	uint16_t i;
	if (!buffer || !data || !length) {
		return 0;
	}
	for (i = length; i; i--) {
		buffer_write_index(buffer, *(data + i - 1), offset + i - 1);
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_append(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length) {
	uint16_t i;
	uint8_t ret_val = 1;
	if (!buffer || !data) {
		return 0;
	}
	for (i = 0; i < length; i++) {
		if (!buffer_write(buffer, *(data + i))) {
			ret_val = 0;
		}
	}
	return ret_val;
}

uint8_t ICACHE_FLASH_ATTR buffer_append_fast(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length) {
	uint16_t space;
	if (!buffer || !data || !length) {
		return 0;
	}
	space = buffer->capacity - buffer->write_it;
	if (!space) {
		return 0;
	}
	if (space < length) {
		length = space;
	}
	memcpy(buffer->data + buffer->write_it, data, length);
	buffer->write_it += length;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_append_buffer(Buffer_T buffer, Buffer_T from, uint16_t offset, uint16_t length) {
	if (!buffer || !from || !length) {
		return 0;
	}
	if (((uint32_t)offset + length) > buffer_size(from)) {
		return 0;
	}
	return buffer_append_fast(buffer, buffer_data(from, offset), length);
}

uint8_t ICACHE_FLASH_ATTR buffer_puts(Buffer_T buffer, const char* str) {
	if (!buffer || !str || !(*str)) {
		return 0;
	}
	return buffer_append(buffer, (const Buffer_Type_T*)str, strlen(str));
}

uint8_t ICACHE_FLASH_ATTR buffer_puts_nl(Buffer_T buffer, const char* str) {
	uint32_t len;
	if (!buffer || !str) {
		return 0;
	}
	len = strlen(str);
	if (len < 2) {
		return 0;
	}
	if (str[len - 1] != '\n') {
		uint8_t ret = 1;
		ret *= buffer_append(buffer, (const Buffer_Type_T*)str, strlen(str) - 1);
		ret *= buffer_write(buffer, '\n');
		return ret;
	}
	return buffer_puts(buffer, str);
}

uint16_t ICACHE_FLASH_ATTR buffer_remaining_write_space(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->capacity - buffer->write_it;
}

uint16_t ICACHE_FLASH_ATTR buffer_capacity(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->capacity;
}

void ICACHE_FLASH_ATTR buffer_rewind(Buffer_T buffer, uint16_t value) {
	if (!buffer) {
		return;
	}
	if (buffer->read_it > value) {
		buffer->read_it -= value;
	} else {
		buffer->read_it = 0;
	}
}

uint16_t ICACHE_FLASH_ATTR buffer_capacity_offset(Buffer_T buffer, uint16_t offset) {
	if (!buffer) {
		return 0;
	}
	if (buffer->capacity > offset) {
		return buffer->capacity - offset;
	}
	return 0;
}

uint16_t ICACHE_FLASH_ATTR buffer_overflow(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer->overflow;
}

void ICACHE_FLASH_ATTR buffer_close(Buffer_T buffer) {
	if (buffer_size(buffer) < buffer_capacity(buffer)) {
		if ((!buffer_size(buffer)) || buffer_read(buffer, buffer_size(buffer) - 1)) {
			buffer_write_index(buffer, 0, buffer_size(buffer));
		}
		return;
	}
	buffer_write_index(buffer, 0, buffer_capacity(buffer) - 1);
}

char* ICACHE_FLASH_ATTR buffer_string(Buffer_T buffer) {
	if (!buffer) {
		return NULL;
	}
	buffer_close(buffer);
	return (char*)buffer_data(buffer, 0);
}

Buffer_T ICACHE_FLASH_ATTR buffer_shift_right(Buffer_T buffer, uint16_t positions, uint8_t fill_by) {
	int32_t i;
	uint16_t last_buffer_size;
	if (!buffer) {
		return 0;
	}
	if (!positions || !buffer_size(buffer)) {
		return buffer;
	}
	last_buffer_size = buffer_size(buffer);
	for (i = last_buffer_size - 1; i >= 0; i--) {
		buffer_write_index(buffer, buffer_read(buffer, i), i + positions);
	}
	for (i = 0; i < positions; i++) {
		buffer_write_index(buffer, fill_by, i);
	}
	return buffer;
}

uint8_t ICACHE_FLASH_ATTR buffer_search(Buffer_T buffer, uint16_t begin, uint8_t data, uint16_t* index) {
	uint16_t i;
	if (!buffer) {
		return 0;
	}
	for (i = begin; i < buffer_size(buffer); i++) {
		if (buffer_read(buffer, i) == data) {
			if (index) {
				*index = i;
			}
			return 1;
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR buffer_remove(Buffer_T buffer, uint16_t index) {
	if (!buffer || (index >= buffer_size(buffer))) {
		return 0;
	}
	return buffer_overwrite_form_end(buffer, buffer_data(buffer, index), buffer_size(buffer) - index, index);
}

uint8_t ICACHE_FLASH_ATTR buffer_shift_left(Buffer_T buffer, uint16_t positions) {
	uint16_t i;
	if (!buffer || !positions || (positions > buffer->write_it)) {
		return 0;
	}
	for (i = 0; i < (buffer->write_it - positions); i++) {
		buffer->data[i] = buffer->data[i + positions];
	}
	buffer->write_it -= positions;
	if (buffer->read_it > positions) {
		buffer->read_it -= positions;
	} else {
		buffer->read_it = 0;
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_find_sub_string(Buffer_T buffer, char* str, uint16_t* index) {
	uint16_t search_len;
	uint16_t find_index = 0;
	uint16_t begin_index = 0;
	uint16_t i;
	if (!buffer || !str) {
		return 0;
	}
	search_len = strlen(str);
	for (i = 0; i < buffer_size(buffer); i++) {
		if (find_index == search_len) {
			break;
		}
		if (buffer_read(buffer, i) == str[find_index]) {
			if (!find_index) {
				begin_index = i;
			}
			find_index++;
		} else {
			find_index = 0;
		}
	}
	if (find_index == search_len) {
		if (index) {
			*index = begin_index;
		}
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR buffer_write_hex(Buffer_T buffer, uint8_t value) {
	uint8_t ret[2];
	uint8_t part;
	uint8_t i;
	if (!buffer) {
		return 0;
	}
	for (i = 0; i < 2; i++) {
		part = value & 0x0F;
		if (part > 9) {
			ret[i] = part - 10 + 'A';
		} else {
			ret[i] = part + '0';
		}
		value >>= 4;
	}
	return buffer_write(buffer, ret[1]) && buffer_write(buffer, ret[0]);
}

uint8_t ICACHE_FLASH_ATTR buffer_hex(Buffer_T buffer, uint64_t value, uint8_t len) {
	int8_t i;
	if (!buffer || !len || (len > 8)) {
		return 0;
	}
	for (i = (len - 1); i >= 0; i--) {
		if (!buffer_write_hex(buffer, (value >> (i * 8)) & 0xFF)) {
			return 0;
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_hex_no_zeros(Buffer_T buffer, uint32_t value) {
	char buf[64];
	if (!buffer) {
		return 0;
	}
	if (!value) {
		return buffer_write(buffer, '0');
	}
	snprintf(buf, sizeof(buf), "%X", value);
	return buffer_puts(buffer, buf);
}

uint8_t ICACHE_FLASH_ATTR buffer_puts_mac(Buffer_T buffer, uint8_t* mac_array) {
	uint8_t i;
	int8_t j;
	uint8_t tmp;
	if (!buffer || !mac_array) {
		return 0;
	}
	for (i = 0; i < 6; ++i) {
		for (j = 1; j >= 0; --j) {
			if (j) {
				tmp = mac_array[i] >> 4;
			} else {
				tmp = mac_array[i] & 0x0f;
			}
			if (!buffer_write(buffer, tmp + (tmp < 10 ? 48 : 55))) {
				return 0;
			}
		}
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_puts_ip(Buffer_T buffer, uint32_t addr) {
	if (!buffer) {
		return 0;
	}
	buffer_dec(buffer, (addr >> 0) & 0xFF);
	buffer_write(buffer, '.');
	buffer_dec(buffer, (addr >> 8) & 0xFF);
	buffer_write(buffer, '.');
	buffer_dec(buffer, (addr >> 16) & 0xFF);
	buffer_write(buffer, '.');
	buffer_dec(buffer, (addr >> 24) & 0xFF);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_dec(Buffer_T buffer, int32_t value) {
	char storage[64];
	if (!buffer) {
		return 0;
	}
	snprintf(storage, sizeof(storage), "%d", value);
	buffer_puts(buffer, storage);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_real(Buffer_T buffer, int64_t value) {
	char storage[128];
	if (!buffer) {
		return 0;
	}
	snprintf(storage, sizeof(storage), "%d", (int32_t)value / 1000);
	buffer_puts(buffer, storage);
	if (value < 0) {
		value *= -1;
	}
	if (value % 1000) {
		buffer_write(buffer, '.');
		if (value < 100) {
			buffer_write(buffer, '0');
		}
		if (value < 10) {
			buffer_write(buffer, '0');
		}
		snprintf(storage, sizeof(storage), "%d", (int32_t)value % 1000);
		buffer_puts(buffer, storage);
	}
	return 1;
}

uint8_t ICACHE_FLASH_ATTR buffer_indent(Buffer_T buffer, char separator, uint8_t level) {
	uint16_t i;
	uint8_t res = 1;
	if (!buffer || !separator) {
		return 0;
	}
	for (i = 0; i < level; i++) {
		if (!buffer_write(buffer, separator)) {
			res = 0;
		}
	}
	return res;
}

uint8_t ICACHE_FLASH_ATTR buffer_crlf(Buffer_T buffer) {
	if (!buffer) {
		return 0;
	}
	return buffer_puts(buffer, CRLF);
}

uint8_t ICACHE_FLASH_ATTR buffer_write_escape(Buffer_T buffer, Buffer_Type_T data) {
	uint16_t i;
	uint8_t ret_val = 1;
	if (!buffer) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(buffer_converter); i++) {
		if (data == buffer_converter[i].unprint) {
			ret_val *= buffer_write(buffer, '\\');
			ret_val *= buffer_write(buffer, buffer_converter[i].escape);
			return ret_val;
		}
	}
	if ((data < ' ') || (data > 126)) {
		ret_val *= buffer_write(buffer, '\\');
		ret_val *= buffer_write(buffer, 'u');
		ret_val *= buffer_write_hex(buffer, 0);
		ret_val *= buffer_write_hex(buffer, data);
		return ret_val;
	}
	return buffer_write(buffer, data);
}

uint8_t ICACHE_FLASH_ATTR buffer_puts_escape(Buffer_T buffer, const char* str) {
	uint16_t i;
	uint8_t ret_val = 1;
	if (!buffer || !str) {
		return 0;
	}
	for (i = 0; i < strlen(str); i++) {
		ret_val *= buffer_write_escape(buffer, (Buffer_Type_T)str[i]);
	}
	return ret_val;
}
