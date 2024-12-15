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

/// @file ctx.h Defines the public interface of an agoge emulator context.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "bus.h"
#include "cart.h"
#include "cpu.h"
#include "disasm.h"
#include "log.h"
#include "sched.h"

/// @brief Defines a full agoge emulator context.
///
/// This context is what applications should primarily work with; it is a self-
/// encapsulated instance of an emulator that allows multiple instances within
/// the same application and promotes separation of concerns.
struct agoge_ctx {
	struct agoge_cart cart;
	struct agoge_bus bus;
	struct agoge_cpu cpu;
	struct agoge_disasm disasm;
	struct agoge_log log;
	struct agoge_sched sched;
};

void agoge_ctx_init(struct agoge_ctx *ctx);

void agoge_ctx_reset(struct agoge_ctx *ctx);
void agoge_ctx_step(struct agoge_ctx *ctx);

#ifdef __cplusplus
}
#endif // __cplusplus
