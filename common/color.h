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


#ifndef COLOR_H_INCLUDED
#define COLOR_H_INCLUDED 1

#ifdef COLOR_DISABLE
	#define COLOR_BLACK
	#define COLOR_RED
	#define COLOR_GREEN
	#define COLOR_YELLOW
	#define COLOR_BLUE
	#define COLOR_MAGENTA
	#define COLOR_CYAN
	#define COLOR_WHITE
	#define COLOR_END
#else
	#define COLOR_BLACK "\e[30m"
	#define COLOR_RED "\e[31m"
	#define COLOR_GREEN "\e[32m"
	#define COLOR_YELLOW "\e[33m"
	#define COLOR_BLUE "\e[34m"
	#define COLOR_MAGENTA "\e[35m"
	#define COLOR_CYAN "\e[36m"
	#define COLOR_WHITE "\e[37m"
	#define COLOR_END "\e[39m"
#endif

#endif
