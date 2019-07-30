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


#ifndef WPS_H_INCLUDED
#define WPS_H_INCLUDED 1

uint8_t wps_start(void);
uint8_t wps_stop(void);
uint8_t wps_is_enabled(void);
uint8_t wps_success_is_event(void);
void wps_fail_fn(void (*cb)(uint8_t shorten));
uint8_t wps_abort(void);

#endif
