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

#include "agoge/log.h"

void agoge_log_msg(struct agoge_log *log, const enum agoge_log_lvl lvl,
		   const char *fmt, ...);

#define LOG_HANDLE(log, level, args...)                       \
	({                                                    \
		struct agoge_log *logger = (log);             \
                                                              \
		if ((logger->cb) && logger->lvl >= (level)) { \
			agoge_log_msg(logger, (level), args); \
		}                                             \
	})

#define LOG_INFO(log, args...) (LOG_HANDLE((log), AGOGE_LOG_LVL_INFO, args))
#define LOG_WARN(log, args...) (LOG_HANDLE((log), AGOGE_LOG_LVL_WARN, args))
#define LOG_ERR(log, args...) (LOG_HANDLE((log), AGOGE_LOG_LVL_ERR, args))
#define LOG_DBG(log, args...) (LOG_HANDLE((log), AGOGE_LOG_LVL_DBG, args))
#define LOG_TRACE(log, args...) (LOG_HANDLE((log), AGOGE_LOG_LVL_TRACE, args))
