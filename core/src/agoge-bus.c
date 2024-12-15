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
#include <string.h>

#include "agoge-bus.h"
#include "agoge-cart.h"
#include "agoge-compiler.h"
#include "agoge-log.h"
#include "agoge-sched.h"

NONNULL NODISCARD uint8_t agoge_bus_read(struct agoge_bus *const bus,
					 const uint16_t address)
{
	agoge_sched_step(bus->sched);

	if ((address >= 0xFF80) && (address <= 0xFFFE)) {
		return bus->hram[address - 0xFF80];
	}

	switch (address >> 12) {
	case 0x0 ... 0x3:
		return agoge_cart_read(bus->cart, address);

	case 0x4 ... 0x7:
		return agoge_cart_read(bus->cart, address);

	case 0xC ... 0xD:
		return bus->wram[address - 0xC000];

	default:
		LOG_WARN(bus->log, "Unknown memory read: $%04X, returning $FF",
			 address);
		return 0xFF;
	}
}

void agoge_bus_write(struct agoge_bus *bus, const uint16_t address,
		     const uint8_t data)
{
	if ((address >= 0xFF80) && (address <= 0xFFFE)) {
		bus->hram[address - 0xFF80] = data;
		return;
	}

	switch (address >> 12) {
	case 0xC ... 0xD:
		bus->wram[address - 0xC000] = data;
		return;

	case 0xF:
		if ((((address >> 8) & 0xF) == 0xF) &&
		    ((address & 0xFF) == 0x01)) {
			bus->buf[bus->buf_n++] = (char)data;

			if (data == '\n') {
				LOG_DBG(bus->log, "Serial output: %s",
					bus->buf);
				memset(bus->buf, 0, sizeof(bus->buf));
				bus->buf_n = 0;
			}
		}
		return;

	default:
		LOG_WARN(bus->log,
			 "Unknown memory write: $%04X <- $%02X; ignoring",
			 address, data);
	}
	agoge_sched_step(bus->sched);
}
