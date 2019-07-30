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


#ifndef PROGSTR_H_INCLUDED
#define PROGSTR_H_INCLUDED 1

#include <c_types.h>

#define PSTR(s) (__extension__({static const char __c[] ICACHE_RODATA_ATTR __attribute__((aligned(4))) = (s); &__c[0];}))

#endif
