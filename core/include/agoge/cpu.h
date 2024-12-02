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

#include <stdint.h>
#include "bus.h"

#define REG_PAIR_DEFINE(hi, lo, pair)       \
	struct {                            \
		union {                     \
			struct {            \
				uint8_t lo; \
				uint8_t hi; \
			};                  \
			uint16_t pair;      \
		};                          \
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct agoge_cpu {
	struct {
		REG_PAIR_DEFINE(b, c, bc);
		REG_PAIR_DEFINE(d, e, de);
		REG_PAIR_DEFINE(h, l, hl);
		REG_PAIR_DEFINE(a, f, af);

		uint16_t pc;
		uint16_t sp;
	} reg;

	struct agoge_bus *bus;
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
}
#endif // __cplusplus
