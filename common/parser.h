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


#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED 1

#include <inttypes.h>
#include "buffer.h"

typedef struct Parser_T* Parser_T;

typedef enum {
	Parser_State_Continue_100                   = 100,
	Parser_State_OK_200                         = 200,
	Parser_State_Movd_Permanently_301           = 301,
	Parser_State_Bad_Request_400                = 400,
	Parser_State_Not_Found_404                  = 404,
	Parser_State_Method_Not_Allowed_405         = 405,
	Parser_State_Internal_Server_Error_500      = 500,
	Parser_State_HTTP_Version_Not_Supported_505 = 505
} Parser_State_T;

typedef enum {
	Parser_Method_UNDEFINE = 0,
	Parser_Method_GET = 1,
	Parser_Method_POST = 2,
	Parser_Method_OPTIONS = 3
} Parser_Method_T;

Parser_T parser_new(void);
void parser_free(Parser_T parser);
const char* parser_token(Parser_T parser);
const char* parser_referer(Parser_T parser);
char* parser_content_type(Parser_T parser);
Parser_State_T parser_do(Parser_T parser, uint8_t data, uint32_t total_len);
char* parser_path(Parser_T parser);
Buffer_T parser_content(Parser_T parser);
uint8_t parser_header_done(Parser_T parser);
Parser_Method_T parser_method(Parser_T parser);
char* parser_decode_url(char* input);
uint32_t parser_data_len(Parser_T parser);

#endif
