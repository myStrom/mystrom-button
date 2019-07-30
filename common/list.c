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
#include "list.h"
#include "debug.h"

typedef struct List_Item_T* List_Item_T;

struct List_Item_T {
	List_Item_T next;
	uint8_t data[0];
};

struct List_T {
	List_Item_T head;
	List_Item_T it;
	uint16_t item_size;
};

List_T ICACHE_FLASH_ATTR list_new(uint16_t item_size) {
	List_T list;
	if (!item_size) {
		return 0;
	}

	if (!(list = (void*)malloc(sizeof(struct List_T)))) {
		return 0;
	}
	list->item_size = item_size;
	list->head = 0;
	list->it = 0;
	return list;
}

void ICACHE_FLASH_ATTR list_delete(List_T list) {
	if (!list) {
		return;
	}
	list_clear(list);
	free(list);
}

void ICACHE_FLASH_ATTR list_clear(List_T list) {
	List_Item_T next;
	List_Item_T prev_next;
	if (!list) {
		return;
	}
	if (!list->head) {
		return;
	}

	next = list->head;
	while (next->next) {
		prev_next = next;
		next = next->next;
		free(prev_next);
	}
	if (next) {
		free(next);
	}
	list->head = 0;
}

static List_Item_T ICACHE_FLASH_ATTR list_end(List_T list) {
	List_Item_T next;
	if (!list) {
		return 0;
	}

	if (!list->head) {
		return 0;
	}

	next = list->head;
	while (next->next) {
		next = next->next;
	}
	return next;
}

static List_Item_T ICACHE_FLASH_ATTR list_item_by_index(List_T list, uint16_t index) {
	List_Item_T next;
	uint16_t it = 0;
	if (!list) {
		return 0;
	}

	if (!list->head) {
		return 0;
	}

	if (!index && list->head) {
		return list->head;
	}

	next = list->head;
	while (next->next) {
		next = next->next;
		it++;
		if (it == index) {
			return next;
		}
	}
	return 0;
}

#ifdef LIST_CHANGE_ADD_PROTO
uint8_t ICACHE_FLASH_ATTR list_add_(List_T list, void* data) {
#else
uint8_t ICACHE_FLASH_ATTR list_add(List_T list, void* data) {
#endif
	List_Item_T item;
	List_Item_T end;
	if (!list || !data) {
		return 0;
	}

	if (!(item = (void*)malloc(sizeof(struct List_Item_T) + list->item_size))) {
		return 0;
	}
	item->next = 0;
	memcpy(item->data, data, list->item_size);
	if (!(end = list_end(list))) {
		list->head = item;
	} else {
		end->next = item;
	}
	return 1;
}

void ICACHE_FLASH_ATTR list_rewind(List_T list) {
	if (!list) {
		return;
	}
	list->it = list->head;
}

uint8_t ICACHE_FLASH_ATTR list_next(List_T list, void* data) {
	if (!list || !data) {
		return 0;
	}

	if (list->it) {
		memcpy(data, list->it->data, list->item_size);
		list->it = list->it->next;
		return 1;
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR list_write(List_T list, uint16_t index, void* data) {
	List_Item_T item;
	if (!list || !data) {
		return 0;
	}

	if (!(item  = list_item_by_index(list, index))) {
		return 0;
	}
	memcpy(item->data, data, list->item_size);
	return 1;
}

uint8_t ICACHE_FLASH_ATTR list_read(List_T list, uint16_t index, void* data) {
	List_Item_T item;
	if (!list || !data) {
		return 0;
	}

	if (!(item  = list_item_by_index(list, index))) {
		return 0;
	}

	memcpy(data, item->data, list->item_size);
	return 1;
}

uint16_t ICACHE_FLASH_ATTR list_size(List_T list) {
	List_Item_T next;
	uint16_t size = 0;
	if (!list) {
		return 0;
	}

	if (!list->head) {
		return 0;
	}

	next = list->head;
	if (next) {
		size++;
	}
	while (next->next) {
		next = next->next;
		size++;
	}
	return size;
}

uint8_t ICACHE_FLASH_ATTR list_remove(List_T list, uint16_t index) {
	List_Item_T item;
	uint16_t size;
	if (!list) {
		return 0;
	}
	size = list_size(list);
	if (!size) {
		return 0;
	}
	if (size == 1) {
		free(list->head);
		list->head = 0;
		return 1;
	}
	if (!(item  = list_item_by_index(list, index))) {
		return 0;
	}
	if (!index) {
		list->head = item->next;
	} else if (index == (list_size(list) - 1)) {
		List_Item_T prev;
		if ((prev = list_item_by_index(list, index - 1))) {
			prev->next = NULL;
		}
	} else {
		List_Item_T prev = list_item_by_index(list, index - 1);
		List_Item_T next = list_item_by_index(list, index + 1);
		prev->next = next;
	}
	free(item);
	return 1;
}
