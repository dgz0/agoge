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

#include "agoge/ctx.h"

#include "agoge-cpu.h"
#include "agoge-log.h"
#include "agoge-timer.h"

static void setup_ctx_ptrs(struct agoge_ctx *const ctx)
{
	assert(ctx != NULL);

	ctx->cart.log = &ctx->log;

	ctx->bus.cart = &ctx->cart;
	ctx->bus.log = &ctx->log;
	ctx->bus.sched = &ctx->sched;

	ctx->cpu.bus = &ctx->bus;
	ctx->disasm.cpu = &ctx->cpu;

	ctx->bus.timer.sched = &ctx->sched;

	ctx->bus.timer.intr_flag = &ctx->bus.intr_flag;
}

void agoge_ctx_init(struct agoge_ctx *const ctx)
{
	assert(ctx != NULL);

	setup_ctx_ptrs(ctx);
	agoge_ctx_reset(ctx);

	LOG_INFO(&ctx->log, "agoge context initialized");
}

void agoge_ctx_reset(struct agoge_ctx *const ctx)
{
	assert(ctx != NULL);
	agoge_cpu_reset(&ctx->cpu);

	LOG_INFO(&ctx->log, "agoge context reset");
}

void agoge_ctx_step(struct agoge_ctx *const ctx)
{
	assert(ctx != NULL);

	agoge_cpu_step(&ctx->cpu);
}
