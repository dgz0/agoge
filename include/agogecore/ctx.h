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

/// @file ctx.h Defines the interface of an agoge context.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "cpu.h"
#include "bus.h"
#include "disasm.h"
#include "log.h"

/// Defines an agoge context.
///
/// An `agoge_core_ctx` is a full, self-contained and isolated emulator
/// instance. The majority of functions frontends use will be used through a
/// given context.
struct agoge_core_ctx {
	/// The system bus instance to use for this context.
	struct agoge_core_bus bus;

	/// The logger instance to use for this context.
	struct agoge_core_log log;

	/// The CPU instance to use for this context.
	struct agoge_core_cpu cpu;

	/// The disassembler instance to use for this context.
	struct agoge_core_disasm disasm;
};

/// @brief Initializes an agoge emulator context.
///
/// It is highly recommended to create the context somewhere, set it to zero,
/// and set some initial parameters for logging.
///
/// @param ctx The context to initialize.
void agoge_core_ctx_init(struct agoge_core_ctx *ctx);

void agoge_core_ctx_reset(struct agoge_core_ctx *ctx);

void agoge_core_ctx_step(struct agoge_core_ctx *ctx, unsigned int num_cycles);

#ifdef __cplusplus
}
#endif // __cplusplus
