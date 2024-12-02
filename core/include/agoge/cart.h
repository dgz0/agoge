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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>
#include <stdint.h>

#include "cart-mbc1.h"
#include "log.h"

struct agoge_cart {
	struct agoge_log *log;
	uint8_t *data;

	struct agoge_cart_mbc1 mbc1;
};

enum agoge_cart_mbc {
	AGOGE_CART_MBC_ROM_ONLY = 0x00,
	AGOGE_CART_MBC_MBC1 = 0x01
};

enum agoge_cart_retval {
	AGOGE_CART_UNSUPPORTED_MBC = -3,
	AGOGE_CART_BAD_HEADER_SIZE = -2,
	AGOGE_CART_INVALID_CHECKSUM = -1,
	AGOGE_CART_OK = 1
};

enum agoge_cart_retval agoge_cart_set(struct agoge_cart *cart, uint8_t *data,
				      const size_t data_size);

#ifdef __cplusplus
}
#endif // __cplusplus
