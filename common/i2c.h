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


#ifndef I2C_H_INCLUDED
#define I2C_H_INCLUDED 1

#include <c_types.h>

typedef struct I2C_T* I2C_T;

typedef struct {
	u8 (*init)(void* owner);
	void (*write)(void* owner, u8 value);
	u8 (*read)(void* owner);
	void (*clk)(void* owner, u8 value);
	void (*wait)(void* owner);
	void (*deinit)(void* owner);
	u8 (*busy)(void* owner);
} I2C_Inh_T;

struct I2C_T {
	const I2C_Inh_T* inh;
	void* owner;
};

I2C_T i2c_init(I2C_T i2c, const I2C_Inh_T* inh, void* owner);
u8 i2c_write_8(I2C_T i2c, u8 address, u8 command, u8 data);
u8 i2c_read_8(I2C_T i2c, u8 address, u8 command, u8* data);
u8 i2c_write_burst(I2C_T i2c, u8 address, u8 command, u8 length, u8* data);
u8 i2c_read_burst(I2C_T i2c, u8 address, u8 command, u8 length, u8* data);
u8 i2c_read_serial(I2C_T i2c, u8 address, u8 command, u8* length, u8* data);
void i2c_deinit(I2C_T i2c);

#endif /* I2C_H_ */
