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


#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <inttypes.h>
#include <stdlib.h>

#define CRLF "\r\n"

typedef struct Buffer_T* Buffer_T;

typedef uint8_t Buffer_Type_T;
typedef uint64_t Buffer_Int_T;

struct Buffer_T {
	uint16_t capacity;
	uint16_t write_it;
	uint16_t read_it;
	uint16_t overflow;
	Buffer_Type_T* data;
	Buffer_Type_T store[0];
};

Buffer_T buffer_new(uint16_t capacity);
void buffer_delete(Buffer_T buffer);
Buffer_T buffer_init(Buffer_T buffer, uint16_t capacity, uint8_t* storage);
uint8_t buffer_write(Buffer_T buffer, Buffer_Type_T data);
uint16_t buffer_size(Buffer_T buffer);
uint16_t buffer_size_offset(Buffer_T buffer, uint16_t offset);
Buffer_Type_T* buffer_data(Buffer_T buffer, uint16_t offset);
uint16_t buffer_clear(Buffer_T buffer);
void buffer_reset_read_counter(Buffer_T buffer);
uint16_t buffer_bytes_to_read(Buffer_T buffer);
uint16_t buffer_read_it(Buffer_T buffer);
uint16_t buffer_write_it(Buffer_T buffer);
Buffer_Type_T buffer_read_next(Buffer_T buffer);
uint8_t buffer_set_writer(Buffer_T buffer, uint16_t offset);
uint8_t buffer_equal(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length);
uint8_t buffer_equal_str(Buffer_T buffer, const char* str);
uint8_t buffer_equal_from_end(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length);
uint8_t buffer_equal_from_end_str(Buffer_T buffer, const char* str);
Buffer_Type_T buffer_read(Buffer_T buffer, uint16_t index);
uint8_t buffer_read_index(Buffer_T buffer, Buffer_Type_T* data, uint16_t index);
uint8_t buffer_write_index(Buffer_T buffer, Buffer_Type_T data, uint16_t index);
uint8_t buffer_overwrite(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length, uint16_t offset);
uint8_t buffer_append(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length);
uint8_t buffer_append_fast(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length);
uint8_t buffer_append_buffer(Buffer_T buffer, Buffer_T from, uint16_t offset, uint16_t length);
uint8_t buffer_puts(Buffer_T buffer, const char* str);
uint8_t buffer_puts_nl(Buffer_T buffer, const char* str);
uint16_t buffer_remaining_write_space(Buffer_T buffer);
uint16_t buffer_capacity(Buffer_T buffer);
uint16_t buffer_capacity_offset(Buffer_T buffer, uint16_t offset);
void buffer_rewind(Buffer_T buffer, uint16_t value);
uint16_t buffer_overflow(Buffer_T buffer);
void buffer_close(Buffer_T buffer);
char* buffer_string(Buffer_T buffer);
Buffer_T buffer_shift_right(Buffer_T buffer, uint16_t positions, uint8_t fill_by);
uint8_t buffer_search(Buffer_T buffer, uint16_t begin, uint8_t data, uint16_t* index);
uint8_t buffer_overwrite_form_end(Buffer_T buffer, const Buffer_Type_T* data, uint16_t length, uint16_t offset);
uint8_t buffer_remove(Buffer_T buffer, uint16_t index);
uint8_t buffer_shift_left(Buffer_T buffer, uint16_t positions);
uint8_t buffer_find_sub_string(Buffer_T buffer, char* str, uint16_t* index);
uint8_t buffer_write_hex(Buffer_T buffer, uint8_t value);
uint8_t buffer_hex(Buffer_T buffer, uint64_t value, uint8_t len);
uint8_t buffer_hex_no_zeros(Buffer_T buffer, uint32_t value);
uint8_t buffer_puts_mac(Buffer_T buffer, uint8_t* mac_array);
uint8_t buffer_puts_ip(Buffer_T buffer, uint32_t addr);
uint8_t buffer_dec(Buffer_T buffer, int32_t value);
uint8_t buffer_real(Buffer_T buffer, int64_t value);
uint8_t buffer_indent(Buffer_T buffer, char separator, uint8_t level);
uint8_t buffer_crlf(Buffer_T buffer);
uint8_t buffer_write_escape(Buffer_T buffer, Buffer_Type_T data);
uint8_t buffer_puts_escape(Buffer_T buffer, const char* str);

#endif
