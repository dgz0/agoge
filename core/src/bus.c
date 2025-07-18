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

#include <string.h>

#include "bus.h"
#include "cart.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_BUS);

uint8_t agoge_core_bus_read(struct agoge_core_ctx *const ctx,
			    const uint16_t addr)
{
	static const void *jmp_tbl[] = {
		[0x0000 ... 0x3FFF] = &&unbanked_rom_read,
		[0x4000 ... 0x7FFF] = &&banked_rom_read,
		[0x8000 ... 0xBFFF] = &&unknown,
		[0xC000 ... 0xDFFF] = &&wram,
		[0xE000 ... 0xFF7F] = &&unknown,
		[0xFF80 ... 0xFFFE] = &&hram,
		[0xFFFF] = &&unknown
	};

	goto *jmp_tbl[addr];

unbanked_rom_read:
	return ctx->bus.cart.data[addr];

banked_rom_read:
	return ctx->bus.cart.banked_read_cb(ctx, addr);

wram:
	return ctx->bus.wram[addr - 0xC000];

hram:
	return ctx->bus.hram[addr - 0xFF80];

unknown:
	LOG_WARN(ctx, "Unknown memory read: $%04X, returning $FF", addr);
	return 0xFF;
}

void agoge_core_bus_write(struct agoge_core_ctx *const ctx, const uint16_t addr,
			  const uint8_t data)
{
	static const void *const jmp_tbl[] = { [0x0000 ... 0xBFFF] = &&unknown,
					       [0xC000 ... 0xDFFF] = &&wram,
					       [0xE000 ... 0xFF00] = &&unknown,
					       [0xFF01] = &&serial_write,
					       [0xFF02 ... 0xFF7F] = &&unknown,
					       [0xFF80 ... 0xFFFE] = &&hram,
					       [0xFFFF] = &&unknown };

	goto *jmp_tbl[addr];

unknown:
	LOG_WARN(ctx, "Unknown memory write: $%04X <- $%02X; ignoring", addr,
		 data);
	return;

wram:
	ctx->bus.wram[addr - 0xC000] = data;
	return;

hram:
	ctx->bus.hram[addr - 0xFF80] = data;
	return;

serial_write:
	ctx->bus.serial.data[ctx->bus.serial.data_size++] = data;

	if (data == '\n') {
		LOG_TRACE(ctx, "Serial output: %s", ctx->bus.serial.data);
		memset(&ctx->bus.serial, 0, sizeof(ctx->bus.serial));
	}
	return;
}

uint8_t agoge_core_bus_peek(struct agoge_core_ctx *const ctx,
			    const uint16_t addr)
{
	return agoge_core_bus_read(ctx, addr);
}
