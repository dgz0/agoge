// Copyright 2025 dgz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the “Software”), to deal
// in the Software without restriction, including without limitation the rights
// to use copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
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

struct agoge_core_ctx;

/// The minimum size of a valid cartridge in bytes (32 KiB).
#define AGOGE_CORE_CART_SIZE_MIN (32768)

/// The maximum size of a valid cartridge in bytes (8 MiB).
#define AGOGE_CORE_CART_SIZE_MAX (8388608)

struct agoge_core_cart {
	/// Pointer to the current cartridge data. This must remain valid at
	/// all times if a cartridge is "inserted".
	uint8_t *data;

	uint8_t (*banked_read_cb)(struct agoge_core_ctx *ctx, uint16_t addr);
	unsigned int rom_bank;
};

enum agoge_core_cart_retval {
	AGOGE_CORE_CART_RETVAL_UNSUPPORTED_MBC,
	AGOGE_CORE_CART_RETVAL_INVALID_CHECKSUM,
	AGOGE_CORE_CART_RETVAL_BAD_SIZE,
	AGOGE_CORE_CART_RETVAL_OK
};

enum agoge_core_cart_mbc_type {
	AGOGE_CORE_CART_MBC_ROM_ONLY = 0x00,
	AGOGE_CORE_CART_MBC_MBC1 = 0x01,
};

enum agoge_core_cart_retval agoge_core_cart_set(struct agoge_core_ctx *ctx,
						uint8_t *data,
						size_t data_size);

#ifdef __cplusplus
}
#endif // __cplusplus
