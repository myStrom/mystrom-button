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


#ifndef NS_H_INCLUDED
#define NS_H_INCLUDED 1

#include <inttypes.h>
#include "json.h"
#include "http.h"

typedef struct Ns_T* Ns_T;

typedef uint8_t (*Ns_Recv_Cb)(void* owner, uint8_t* data, uint32_t len);
typedef void (*Ns_Done_Cb)(void* owner, uint8_t error);

typedef enum {
	NS_METHOD_GET,
	NS_METHOD_POST,
	NS_METHOD_DELETE,
	NS_METHOD_PUT,
} Ns_Method_T;

Ns_T ns_new(void);
void ns_delete(Ns_T ns);
uint8_t ns_post(Ns_T ns, const char* url, Json_T json, void* owner, Ns_Recv_Cb recv_cb, Ns_Done_Cb done_cb);
uint8_t ns_register(Ns_T ns, Http_Request_T req, uint8_t* index);
uint8_t ns_unregister(Ns_T ns, uint8_t index);
uint8_t ns_flush(Ns_T ns);
uint16_t ns_size(Ns_T ns);
uint8_t ns_connect(Ns_T ns, uint8_t yes);
uint8_t ns_connected(Ns_T ns);
void ns_timeout(Ns_T ns, uint32_t time_ms);
uint8_t ns_query(Ns_T ns, const char* url, const char* args, void* owner, Ns_Recv_Cb recv_cb, Ns_Done_Cb done_cb);
uint16_t ns_multi_query(Ns_T ns, char* url, const char* args, void* owner, Ns_Recv_Cb recv_cb, Ns_Done_Cb done_cb);

#endif
