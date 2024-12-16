// SPDX-License-Identifier: MIT
//
// Copyright 2024 dgz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the “Software”), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <assert.h>
#include <stddef.h>

#include "agoge-cart-mbc1.h"

#define ROM_BANK_01_ADDR (0x4000)

void agoge_cart_mbc1_init(struct agoge_cart_mbc1 *const mbc)
{
	assert(mbc != NULL);
	mbc->rom_bank = 0x01;
}

uint8_t agoge_cart_mbc1_read(struct agoge_cart_mbc1 *mbc, const uint8_t *data,
			     const uint16_t address)
{
	assert(mbc != NULL);
	assert(data != NULL);

	switch (address >> 12) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
		return data[address];

	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		return data[(address - ROM_BANK_01_ADDR) +
			    (mbc->rom_bank * ROM_BANK_01_ADDR)];

	default:
		return 0xFF;
	}
}
