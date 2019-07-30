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
#include <user_interface.h>
#include <osapi.h>
#include "queue.h"

Queue_T ICACHE_FLASH_ATTR queue_new(uint8_t item_size, uint16_t queue_capacity) {
	Queue_T queue;
	if (item_size && queue_capacity && (queue = (void*)malloc(sizeof(struct Queue_T) + (item_size * queue_capacity)))) {
		queue->read_it = 0;
		queue->write_it = 0;
		queue->size = 0;
		queue->capacity = queue_capacity;
		queue->item_size = item_size;
		queue->data = queue->storage;
		return queue;
	}
	return 0;
}

Queue_T ICACHE_FLASH_ATTR queue_init(Queue_T queue, uint8_t item_size, uint16_t queue_capacity, uint8_t* storage) {
	if (!queue || !item_size || !queue_capacity || !storage) {
		return 0;
	}
	queue->read_it = 0;
	queue->write_it = 0;
	queue->size = 0;
	queue->capacity = queue_capacity;
	queue->item_size = item_size;
	queue->data = storage;
	return queue;
}

void ICACHE_FLASH_ATTR queue_delete(Queue_T queue) {
	if (queue) {
		free(queue);
	}
}

uint8_t queue_write(Queue_T queue, const void* data) {
	if (!queue || (queue->size >= queue->capacity)) {
		return 0;
	}
	memcpy(queue->data + (queue->write_it * queue->item_size), (const uint8_t*)data, queue->item_size);
	queue->write_it = (queue->write_it + 1) % queue->capacity;
	queue->size++;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR queue_read(Queue_T queue, void* data) {
	if (!queue || !queue->size) {
		return 0;
	}
	memcpy((uint8_t*)data, queue->data + (queue->read_it * queue->item_size), queue->item_size);
	queue->read_it = (queue->read_it + 1) % queue->capacity;
	queue->size--;
	return 1;
}

uint8_t ICACHE_FLASH_ATTR queue_head(Queue_T queue, void* data) {
	if (!queue || !queue->size) {
		return 0;
	}
	memcpy((uint8_t*)data, queue->data + (queue->read_it * queue->item_size), queue->item_size);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR queue_next(Queue_T queue) {
	if (!queue || !queue->size) {
		return 0;
	}
	queue->read_it = (queue->read_it + 1) % queue->capacity;
	queue->size--;
	return 1;
}

uint16_t ICACHE_FLASH_ATTR queue_size(Queue_T queue) {
	if (!queue) {
		return 0;
	}
	return queue->size;
}

uint16_t ICACHE_FLASH_ATTR queue_capacity(Queue_T queue) {
	if (!queue) {
		return 0;
	}
	return queue->capacity;
}
