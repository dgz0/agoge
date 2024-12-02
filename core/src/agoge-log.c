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
#include <stdarg.h>
#include <stdio.h>

#include "agoge-compiler.h"
#include "agoge-log.h"

/// @brief Defines the maximum possible length of a log message.
#define LOG_MSG_LEN_MAX (512)

FORMAT_PRINTF(3, 4)
void agoge_log_msg(struct agoge_log *const log, const enum agoge_log_lvl lvl,
		   const char *const fmt, ...)
{
	assert(log != NULL);
	assert(fmt != NULL);

	va_list args;
	va_start(args, fmt);
	char buf[LOG_MSG_LEN_MAX];
	vsprintf(buf, fmt, args);
	va_end(args);

	log->cb(log->udata, buf, 0, lvl);
}
