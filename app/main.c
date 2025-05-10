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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "agogecore/ctx.h"

#define WHT "\e[1;97m"
#define RESET "\x1B[0m"

static void log_cb(void *const udata,
		   const struct agoge_core_log_msg *const msg)
{
	switch (msg->lvl) {
	case AGOGE_CORE_LOG_LVL_INFO:
		printf(WHT "%s\n" RESET, msg->msg);
		return;

	default:
		__builtin_unreachable();
	}
}

int main(void)
{
	struct agoge_core_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));

	ctx.log.cb = &log_cb;
	ctx.log.curr_lvl = AGOGE_CORE_LOG_LVL_INFO;
	ctx.log.ch_enabled |= (UINT32_C(1) << AGOGE_CORE_LOG_CH_CTX);

	agoge_core_ctx_init(&ctx);
	return EXIT_SUCCESS;
}
