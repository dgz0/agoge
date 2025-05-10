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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>

enum agoge_core_log_lvl {
	AGOGE_CORE_LOG_LVL_OFF = 0,
	AGOGE_CORE_LOG_LVL_INFO = 1,
	AGOGE_CORE_LOG_LVL_WARN = 2,
	AGOGE_CORE_LOG_LVL_ERR = 3,
	AGOGE_CORE_LOG_LVL_DBG = 4,
	AGOGE_CORE_LOG_LVL_TRACE = 5
};

enum agoge_core_log_ch {
	AGOGE_CORE_LOG_CH_CTX = 0,
	AGOGE_CORE_LOG_CH_BUS = 1,
	AGOGE_CORE_LOG_CH_CPU = 2,
};

struct agoge_core_log_msg {
	const char *const msg;
	const size_t msg_len;
	const enum agoge_core_log_ch ch;
	const enum agoge_core_log_lvl lvl;
};

struct agoge_core_log {
	void *udata;
	void (*cb)(void *udata, const struct agoge_core_log_msg *msg);

	enum agoge_core_log_lvl curr_lvl;
	unsigned int ch_enabled;
};

#ifdef __cplusplus
}
#endif // __cplusplus
