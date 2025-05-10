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

#include "cart.h"
#include "comp.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_CART);

#define HDR_ADDR_TITLE_BEG (UINT16_C(0x0134))
#define HDR_ADDR_CART_TYPE (UINT16_C(0x0147))
#define HDR_ADDR_MASK_ROM_VER_NUM (UINT16_C(0x014C))
#define HDR_ADDR_CSUM (UINT16_C(0x014D))

NODISCARD static uint8_t
mbc_banked_read_cb_none(struct agoge_core_cart *const cart, const uint16_t addr)
{
	return cart->data[addr];
}

NODISCARD static uint8_t
mbc_banked_read_cb_mbc1(struct agoge_core_cart *const cart, const uint16_t addr)
{
	return cart->data[(addr - 0x4000) + (cart->rom_bank * 0x4000)];
}

NODISCARD static bool valid_csum(const uint8_t *const data)
{
	uint8_t csum = 0;

	for (size_t addr = HDR_ADDR_TITLE_BEG;
	     addr <= HDR_ADDR_MASK_ROM_VER_NUM; ++addr) {
		csum = csum - data[addr] - 1;
	}
	return data[HDR_ADDR_CSUM] == csum;
}

NODISCARD static bool mbc_set(struct agoge_core_cart *const cart,
			      const uint8_t *const data)
{
	const enum agoge_core_cart_mbc_type type = data[HDR_ADDR_CART_TYPE];

	switch (type) {
	case AGOGE_CORE_CART_MBC_ROM_ONLY:
		cart->banked_read_cb = &mbc_banked_read_cb_none;
		return true;

	case AGOGE_CORE_CART_MBC_MBC1:
		cart->banked_read_cb = &mbc_banked_read_cb_mbc1;
		cart->rom_bank = 1;

		return true;

	default:
		return false;
	}
}

void agoge_core_cart_init(struct agoge_core_cart *const cart,
			  struct agoge_core_log *const log)
{
	cart->log = log;
	LOG_INFO(cart->log, "initialized");
}

NODISCARD enum agoge_core_cart_retval
agoge_core_cart_set(struct agoge_core_cart *const cart, uint8_t *const data,
		    const size_t data_size)
{
	if (unlikely((data_size < AGOGE_CORE_CART_SIZE_MIN) ||
		     (data_size > AGOGE_CORE_CART_SIZE_MAX))) {
		LOG_ERR(cart->log,
			"failed to set cart: bad size - got size %zu",
			data_size);
		return AGOGE_CORE_CART_RETVAL_BAD_SIZE;
	}

	if (unlikely(!valid_csum(data))) {
		return AGOGE_CORE_CART_RETVAL_INVALID_CHECKSUM;
	}

	if (unlikely(!mbc_set(cart, data))) {
		return AGOGE_CORE_CART_RETVAL_UNSUPPORTED_MBC;
	}

	cart->data = data;
	return AGOGE_CORE_CART_RETVAL_OK;
}
