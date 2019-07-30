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


#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED 1

#include <inttypes.h>

typedef struct List_T* List_T;

List_T list_new(uint16_t item_size);
void list_delete(List_T list);
#ifdef LIST_CHANGE_ADD_PROTO
uint8_t list_add_(List_T list, void* data);
#else
uint8_t list_add(List_T list, void* data);
#endif
void list_rewind(List_T list);
uint8_t list_next(List_T list, void* data);
uint8_t list_write(List_T list, uint16_t index, void* data);
uint8_t list_read(List_T list, uint16_t index, void* data);
uint16_t list_size(List_T list);
void list_clear(List_T list);
uint8_t list_remove(List_T list, uint16_t index);

#endif
