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


#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED 1

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef struct Queue_T* Queue_T;

struct Queue_T {
	uint16_t write_it;
	uint16_t read_it;
	uint16_t capacity;
	uint16_t size;
	uint8_t item_size;
	uint8_t* data;
	uint8_t storage[0];
};

Queue_T queue_new(uint8_t item_size, uint16_t queue_capacity);
Queue_T queue_init(Queue_T queue, uint8_t item_size, uint16_t queue_capacity, uint8_t* storage);
void queue_delete(Queue_T queue);
uint8_t queue_write(Queue_T queue, const void* data);
uint8_t queue_read(Queue_T queue, void* data);
uint16_t queue_size(Queue_T queue);
uint16_t queue_capacity(Queue_T queue);
uint8_t queue_head(Queue_T queue, void* data);
uint8_t queue_next(Queue_T queue);

#endif
