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

#include <string.h>

#include "agoge-bus.h"
#include "agoge-cart.h"
#include "agoge-compiler.h"
#include "agoge-log.h"
#include "agoge-sched.h"
#include "agoge-timer.h"

NONNULL NODISCARD uint8_t agoge_bus_read(struct agoge_bus *const bus,
					 const uint16_t address)
{
	agoge_sched_step(bus->sched);

	switch (address) {
	case 0x0000 ... 0x7FFF:
		return agoge_cart_read(bus->cart, address);

	case 0x8000 ... 0x9FFF:
		return 0xFF;

	case 0xC000 ... 0xDFFF:
		return bus->wram[address - 0xC000];

	case 0xFF05:
		return bus->timer.tima;

	case 0xFF0F:
		return bus->intr_flag;

	case 0xFF44:
		return 0xFF;

	case 0xFF80 ... 0xFFFE:
		return bus->hram[address - 0xFF80];

	default:
		LOG_WARN(bus->log, "Unknown memory read: $%04X, returning $FF",
			 address);
		return 0xFF;
	}
}

NONNULL void agoge_bus_write(struct agoge_bus *bus, const uint16_t address,
			     const uint8_t data)
{
	switch (address) {
	case 0xC000 ... 0xDFFF:
		bus->wram[address - 0xC000] = data;
		break;

	case 0x8000 ... 0x9FFF:
		break;

	case 0xFF80 ... 0xFFFE:
		bus->hram[address - 0xFF80] = data;
		break;

	case 0xFF01:
		bus->buf[bus->buf_n++] = (char)data;

		if (data == '\n') {
			LOG_DBG(bus->log, "Serial output: %s", bus->buf);
			memset(bus->buf, 0, sizeof(bus->buf));
			bus->buf_n = 0;
		}
		break;

	case 0xFF05:
		agoge_timer_write_tima(&bus->timer, data);
		return;

	case 0xFF06:
		bus->timer.tma = data;
		break;

	case 0xFF07:
		agoge_timer_write_tac(&bus->timer, data);
		break;

	default:
		LOG_WARN(bus->log,
			 "Unknown memory write: $%04X <- $%02X; ignoring",
			 address, data);
		break;
	}
	agoge_sched_step(bus->sched);
}
