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

/// @file bus.h Defines the interface for the system bus.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "cart.h"

struct agoge_core_ctx;

#define AGOGE_CORE_BUS_HRAM_SIZE (127)
#define AGOGE_CORE_BUS_WRAM_SIZE (8192)
#define AGOGE_CORE_BUS_SERIAL_SIZE (128)

/// Defines the system bus contents.
struct agoge_core_bus {
	uint8_t wram[AGOGE_CORE_BUS_WRAM_SIZE];
	uint8_t hram[AGOGE_CORE_BUS_HRAM_SIZE];

	/// The cartridge instance to use for this context.
	struct agoge_core_cart cart;

	struct {
		char data[AGOGE_CORE_BUS_SERIAL_SIZE];
		size_t data_size;
	} serial;
};

/// @brief Retrieves a byte from an emulated memory address without interfering
/// with emulation operations.
///
/// @param ctx The system bus instance.
/// @param addr The address to retrieve a byte from.
/// @returns The byte retrieved from the emulated memory map.
uint8_t agoge_core_bus_peek(struct agoge_core_ctx *ctx, uint16_t addr);

#ifdef __cplusplus
}
#endif // __cplusplus
