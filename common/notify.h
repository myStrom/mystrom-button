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


#ifndef NOTIFY_H_INCLUDED
#define NOTIFY_H_INCLUDED 1

#include <inttypes.h>
#include "json.h"
#include "buffer.h"
#include "ns.h"

#define NOTIFY_HEADER_SIZE (1024)

typedef struct Notify_T* Notify_T;

Notify_T notify_new(const char* url,
					const char* headers,
					const char* post_data,
					const char* args,
					void (*done_cb)(Notify_T notify, uint8_t error),
					uint8_t (*recv_cb)(Notify_T notify, uint8_t* data, uint32_t len),
					void* owner,
					uint32_t timeout_ms,
					Ns_Method_T method);
void notify_delete(Notify_T notify);
void* notify_owner(Notify_T notify);
void notify_timeout(Notify_T notify, uint32_t time);

#endif
