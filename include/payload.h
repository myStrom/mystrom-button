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


#ifndef PAYLOAD_H_INCLUDED
#define PAYLOAD_H_INCLUDED 1

#include <inttypes.h>
#include "peri.h"

typedef struct Payload_T* Payload_T;

Payload_T payload_new(void);
uint8_t payload_action(Payload_T p, const char* mac, Btn_Action_T action, uint16_t value);
uint8_t payload_connect(Payload_T p, uint8_t yes);
uint8_t payload_connected(Payload_T p);
uint16_t payload_size(Payload_T p);

#endif
