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


#ifndef COLLECT_H_INCLUDED
#define COLLECT_H_INCLUDED 1


#include <inttypes.h>
#include "store.h"
#include "json.h"

typedef struct Collect_T* Collect_T;

Collect_T collect_new(void);
void collect_delete(Collect_T collect);
uint8_t collect_size(Collect_T collect);
void collect_unregister_all(Collect_T collect);
uint8_t collect_belongs(Collect_T collect, const char* mac) ;
Collect_Item_T* collect_get(Collect_T collect, const char* mac);
Collect_Item_T* collect_get_by_index(Collect_T collect, uint8_t id);
uint8_t collect_erase(Collect_T collect);
uint8_t collect_item_get_type(Collect_Item_T* item);
uint8_t collect_item_is_child(Collect_Item_T* item);
void collect_item_register(Collect_Item_T* item);
void collect_item_unregister(Collect_Item_T* item);
uint8_t collect_item_registered(Collect_Item_T* item);
Json_T collect_get_device(Collect_T collect, const char* mac);
Json_T collect_get_devices(Collect_T collect);
Json_T collect_get_status(Collect_T collect, const char* mac);
void collect_stop(Collect_T collect);
void collect_start(Collect_T collect);
uint8_t collect_get_info(Collect_T collect, Buffer_T buffer);

#endif
