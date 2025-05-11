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

#include "bus.h"
#include "cart.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_BUS);

void agoge_core_bus_init(struct agoge_core_bus *const bus,
			 struct agoge_core_log *const log)
{
	bus->log = log;

	agoge_core_cart_init(&bus->cart, bus->log);
	LOG_INFO(bus->log, "initialized");
}

uint8_t agoge_core_bus_read(struct agoge_core_bus *const bus,
			    const uint16_t addr)
{
	static const void *jmp_tbl[] = {
		[0x0000 ... 0x3FFF] = &&unbanked_rom_read,
		[0x4000 ... 0x7FFF] = &&banked_rom_read,
		[0x8000 ... 0xFFFF] = &&unknown
	};

	goto *jmp_tbl[addr];

unbanked_rom_read:
	return bus->cart.data[addr];

banked_rom_read:
	return bus->cart.banked_read_cb(&bus->cart, addr);

unknown:
	LOG_WARN(bus->log, "Unknown memory read: $%04X, returning $FF", addr);
	return 0xFF;
}

uint8_t agoge_core_bus_peek(struct agoge_core_bus *const bus,
			    const uint16_t addr)
{
	return agoge_core_bus_read(bus, addr);
}
