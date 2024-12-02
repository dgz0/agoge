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

#include "agoge/cart.h"
#include <assert.h>
#include <stdbool.h>

#include "agoge-cart.h"
#include "agoge-cart-mbc1.h"
#include "agoge-compiler.h"
#include "agoge-log.h"

#define HDR_TITLE_START (0x0134)
#define HDR_TYPE (0x0147)

#define HDR_CSUM (0x014D)
#define HDR_SIZE (0x014F)

#define HDR_MASK_ROM_VER (0x014C)

NODISCARD static bool valid_csum(const uint8_t *const data,
				 const size_t data_size)
{
	assert(data != NULL);
	assert(data_size >= HDR_SIZE);

	uint8_t csum = 0;

	for (size_t i = HDR_TITLE_START; i <= HDR_MASK_ROM_VER; ++i) {
		csum = csum - (uint8_t)(data[i] - 1);
	}
	return data[HDR_CSUM] == csum;
}

NODISCARD static bool set_mbc(struct agoge_cart *const cart,
			      const enum agoge_cart_mbc mbc_type)
{
	assert(cart != NULL);

	switch (mbc_type) {
	case AGOGE_CART_MBC_ROM_ONLY:
		return true;

	case AGOGE_CART_MBC_MBC1:
		agoge_cart_mbc1_init(&cart->mbc1);
		return true;

	default:
		return false;
	}
}

NODISCARD enum agoge_cart_retval agoge_cart_set(struct agoge_cart *const cart,
						uint8_t *const data,
						const size_t data_size)
{
	assert(cart != NULL);
	assert(data != NULL);

	if (unlikely(data_size < HDR_SIZE)) {
		LOG_ERR(cart->log,
			"cartridge setup failed: cart size is too small to be "
			"valid (expected >=%d bytes, got %d)",
			HDR_SIZE, data_size);

		return AGOGE_CART_BAD_HEADER_SIZE;
	}

	if (unlikely(!valid_csum(data, data_size))) {
		LOG_ERR(cart->log,
			"cartridge setup failed: invalid header checksum");

		return AGOGE_CART_INVALID_CHECKSUM;
	}

	if (unlikely(!set_mbc(cart, data[HDR_TYPE]))) {
		LOG_ERR(cart->log, "cartridge setup failed: MBC not supported");
		return AGOGE_CART_UNSUPPORTED_MBC;
	}
	cart->data = data;

	LOG_INFO(cart->log, "cartridge setup successful");
	return AGOGE_CART_OK;
}

NODISCARD uint8_t agoge_cart_read(struct agoge_cart *const cart,
				  const uint16_t address)
{
	assert(cart != NULL);

	switch (cart->data[HDR_TYPE]) {
	case AGOGE_CART_MBC_ROM_ONLY:
		return cart->data[address];

	case AGOGE_CART_MBC_MBC1:
		return agoge_cart_mbc1_read(&cart->mbc1, cart->data, address);

	default:
		assert(false);
	}
	return 0xFF;
}
