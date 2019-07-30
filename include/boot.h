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


#ifndef BOOT_H_INCLUDED
#define BOOT_H_INCLUDED 1

#include <inttypes.h>
#include "http.h"

typedef enum {
	BOOT_STATE_GETIP = 0,
	BOOT_STATE_GETIP_WAIT = 1,
	BOOT_STATE_HTTP_REQUEST_CONNECT = 2,
	BOOT_STATE_HTTP_REQUEST_CONNECTING = 3,
	BOOT_STATE_HTTP_REQUEST_SEND = 4,
	BOOT_STATE_HTTP_REQUEST_CONNECTED = 5,
	BOOT_STATE_HTTP_GET_VERSION_SUCCESS = 6,
	BOOT_STATE_HTTP_NO_UPDATE_REQUIRED = 7,
	BOOT_STATE_HTTP_UPDATE_REQUIRED = 8,
	BOOT_STATE_HTTP_UPDATE_ERROR = 9,
	BOOT_STATE_HTTP_UPDATE_SUCCESS = 10,
	BOOT_STATE_HTTP_SEND_NEXT_REQUEST = 11,
	BOOT_STATE_HTTP_NEXT_REQUEST = 12,
	BOOT_STATE_HTTP_CONNECT_AGAIN = 13,
	BOOT_STATE_HTTP_CONNECTION_AGAIN = 14,
	BOOT_STATE_ERROR = 15,
	BOOT_STATE_WAIT_FOR_CLOSE = 16,
	BOOT_STATE_WAIT_FOR_END_CLOSE = 17,
	BOOT_STATE_SUCCESS = 18
} Boot_State_T;

void boot_init(void);
uint8_t boot_http(Http_T http);

#endif
