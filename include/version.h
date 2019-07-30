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


#ifndef VERSION_H_INCLUDED
#define VERSION_H_INCLUDED 1

#define VERSION_MAJOR		2
#define VERSION_MINOR		74
#define VERSION_BUILD		12

#define _NUM_TO_STR(x) #x
#define NUM_TO_STR(x) _NUM_TO_STR(x)

#define APP_VERSION			NUM_TO_STR(VERSION_MAJOR) "." NUM_TO_STR(VERSION_MINOR) "." NUM_TO_STR(VERSION_BUILD)

#endif
