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

/// @file ctx.c Defines the implementation of an agoge context.

#include "agogecore/ctx.h"
#include "bus.h"
#include "cart.h"
#include "cpu.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_CTX);

void agoge_core_ctx_init(struct agoge_core_ctx *const ctx)
{
	agoge_core_bus_init(&ctx->bus, &ctx->log);
	agoge_core_cpu_init(&ctx->cpu, &ctx->bus, &ctx->log);

	LOG_INFO(&ctx->log, "initialized");
}

void agoge_core_ctx_step(struct agoge_core_ctx *const ctx,
			 const unsigned int num_cycles)
{
	agoge_core_cpu_run(&ctx->cpu, num_cycles);
}
