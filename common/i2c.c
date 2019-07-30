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


#include <string.h>
#include <osapi.h>
#include <user_interface.h>
#include "i2c.h"
#include "debug.h"

static u8 ICACHE_FLASH_ATTR i2c_busy(I2C_T i2c) {
	if (!i2c || !i2c->inh || !i2c->inh->busy) {
		return 0;
	}
	return i2c->inh->busy(i2c->owner);
}

static void ICACHE_FLASH_ATTR i2c_wait(I2C_T i2c) {
	if (!i2c || !i2c->inh) {
		return;
	}
	i2c->inh->wait(i2c->owner);
}

static void ICACHE_FLASH_ATTR i2c_write(I2C_T i2c, u8 value) {
	if (!i2c || !i2c->inh) {
		return;
	}
	i2c->inh->write(i2c->owner, value);
}

static u8 ICACHE_FLASH_ATTR i2c_read(I2C_T i2c) {
	if (!i2c || !i2c->inh) {
		return 0;
	}
	return i2c->inh->read(i2c->owner);
}

static void ICACHE_FLASH_ATTR i2c_clk(I2C_T i2c, u8 value) {
	if (!i2c || !i2c->inh) {
		return;
	}
	i2c->inh->clk(i2c->owner, value);
	if (value) {
		while (i2c_busy(i2c)) {
			i2c_wait(i2c);
		}
	}
}

static void ICACHE_FLASH_ATTR i2c_start(I2C_T i2c) {
	if (!i2c) {
		return;
	}
	i2c_wait(i2c);
	i2c_write(i2c, 0);
	i2c_wait(i2c);
	i2c_clk(i2c, 0);
	i2c_wait(i2c);
}

static void ICACHE_FLASH_ATTR i2c_stop(I2C_T i2c) {
	if (!i2c) {
		return;
	}
	i2c_write(i2c, 0);
	i2c_wait(i2c);
	i2c_clk(i2c, 1);
	i2c_wait(i2c);
	i2c_write(i2c, 1);
	i2c_wait(i2c);
}

static u8 ICACHE_FLASH_ATTR i2c_read_byte(I2C_T i2c, u8 ack) {
	u8 value = 0;
	u8 i;
	if (!i2c) {
		return 0;
	}
	for (i = 0; i < 8; i++) {
		value <<= 1;
		i2c_clk(i2c, 1);
		i2c_wait(i2c);
		value |= (i2c_read(i2c) ? 1 : 0);
		i2c_clk(i2c, 0);
		i2c_wait(i2c);
	}
	i2c_write(i2c, ack);/*write no ack*/
	i2c_wait(i2c);
	i2c_clk(i2c, 1);
	i2c_wait(i2c);
	i2c_clk(i2c, 0);
	i2c_wait(i2c);
	i2c_write(i2c, 1);
	i2c_wait(i2c);
	return value;
}

/*return ack value, if 1 than is ok*/
static u8 ICACHE_FLASH_ATTR i2c_write_byte(I2C_T i2c, u8 value) {
	u8 ack = 0;
	u8 i;
	if (!i2c) {
		return 0;
	}
	for (i = 0; i < 8; i++) {
		i2c_write(i2c, (value & 0x80) ? 1 : 0);
		value <<= 1;/*shift data to last bit ready*/
		i2c_wait(i2c);/*wait data stable*/
		i2c_clk(i2c, 1);
		i2c_wait(i2c);
		i2c_clk(i2c, 0);
		/*change data*/
	}
	i2c_write(i2c, 1);/*write neans high impedance, and wait for ack*/
	i2c_wait(i2c);/*wait data stable*/
	i2c_clk(i2c, 1);
	i2c_wait(i2c);
	ack = i2c_read(i2c);
	i2c_clk(i2c, 0);/*release clk write 0*/
	i2c_wait(i2c);
	return !ack;
}

I2C_T ICACHE_FLASH_ATTR i2c_init(I2C_T i2c, const I2C_Inh_T* inh, void* owner) {
	if (!i2c || !inh) {
		return 0;
	}
	if (!inh->clk || !inh->init || !inh->read || !inh->write || !inh->wait) {
		return 0;
	}
	memset(i2c, 0, sizeof(struct I2C_T));
	i2c->inh = inh;
	i2c->owner = owner;
	if (!i2c->inh->init(i2c->owner)) {
		return 0;
	}
	i2c_clk(i2c, 1);
	i2c_write(i2c, 1);
	i2c_wait(i2c);
	return i2c;
}

u8 ICACHE_FLASH_ATTR i2c_write_burst(I2C_T i2c, u8 address, u8 command, u8 length, u8* data) {
	u16 i;
	if (!i2c || !data || !length) {
		return 0;
	}
	i2c_start(i2c);
	if (!i2c_write_byte(i2c, address << 1)) {
		i2c_stop(i2c);
		return 0;
	}
	if (!i2c_write_byte(i2c, command)) {
		i2c_stop(i2c);
		return 0;
	}
	for (i = 0; i < length; i++) {
		if (!i2c_write_byte(i2c, data[i])) {
			i2c_stop(i2c);
			return 0;
		}
	}
	i2c_stop(i2c);
	return 1;
}

u8 ICACHE_FLASH_ATTR i2c_read_burst_common(I2C_T i2c, u8 address, u8 command, u8 length, u8* data, u8 first, u8 end) {
	u16 i;
	if (!i2c || !length || !data) {
		return 0;
	}
	if (!first) {
		i2c_clk(i2c, 1);
		i2c_wait(i2c);
	}
	i2c_start(i2c);
	if (!i2c_write_byte(i2c, address << 1)) {
		i2c_stop(i2c);
		return 0;
	}
	if (!i2c_write_byte(i2c, command)) {
		i2c_stop(i2c);
		return 0;
	}
	i2c_clk(i2c, 1);
	i2c_wait(i2c);
	i2c_start(i2c);
	if (!i2c_write_byte(i2c, (address << 1) | 1)) {
		i2c_stop(i2c);
		return 0;
	}
	for (i = 0; i < length; i++) {
		data[i] = i2c_read_byte(i2c, (i == (length - 1)) ? 1 : 0);
	}
	if (end) {
		i2c_stop(i2c);
	}
	return 1;
}

u8 ICACHE_FLASH_ATTR i2c_read_burst(I2C_T i2c, u8 address, u8 command, u8 length, u8* data) {
	return i2c_read_burst_common(i2c, address, command, length, data, 1, 1);
}

u8 ICACHE_FLASH_ATTR i2c_read_serial(I2C_T i2c, u8 address, u8 command, u8* length, u8* data) {
	uint8_t i = 0;
	uint8_t first = 0;
	uint8_t end = 0;
	uint8_t ret_val = 1;
	
	if (!i2c || !length || !data || !length[0]) {
		return 0;
	}
	while (ret_val && length[i]) {
		if (!i) {
			first = 1;
		} else {
			first = 0;
		}
		if (!length[i + 1]) {
			end = 1;
		} else {
			end = 0;
		}
		ret_val &= i2c_read_burst_common(i2c, address, command + i, length[i], data, first, end);
		data += length[i];
		i++;
	}
	return ret_val;
}

void ICACHE_FLASH_ATTR i2c_deinit(I2C_T i2c) {
	if (!i2c) {
		return;
	}
	i2c_clk(i2c, 1);
	i2c_write(i2c, 1);
	if (i2c->inh && i2c->inh->deinit) {
		i2c->inh->deinit(i2c->owner);
	}
}
