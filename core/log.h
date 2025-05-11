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

#pragma once

#include <stdint.h>
#include "agogecore/log.h"

void agoge_core_log_handle(struct agoge_core_log *log,
			   enum agoge_core_log_lvl lvl,
			   enum agoge_core_log_ch ch, const char *fmt, ...);

#define LOG_CHANNEL(x) static const enum agoge_core_log_ch __LOG_CHANNEL__ = (x)

#define LOG_HANDLE(log, ch, level, args...)                                     \
	({                                                                      \
		struct agoge_core_log *const m_log = (log);                     \
                                                                                \
		if ((m_log->cb) && ((m_log->curr_lvl) >= (level)) &&            \
		    ((m_log->ch_enabled & (UINT32_C(1) << __LOG_CHANNEL__)))) { \
			agoge_core_log_handle(m_log, (level), ch, args);        \
		}                                                               \
	})

#define LOG_INFO(log, args...) \
	LOG_HANDLE((log), __LOG_CHANNEL__, AGOGE_CORE_LOG_LVL_INFO, args)

#define LOG_WARN(log, args...) \
	LOG_HANDLE((log), __LOG_CHANNEL__, AGOGE_CORE_LOG_LVL_WARN, args)

#define LOG_ERR(log, args...) \
	LOG_HANDLE((log), __LOG_CHANNEL__, AGOGE_CORE_LOG_LVL_ERR, args)

#define LOG_TRACE(log, args...) \
	LOG_HANDLE((log), __LOG_CHANNEL__, AGOGE_CORE_LOG_LVL_TRACE, args)
