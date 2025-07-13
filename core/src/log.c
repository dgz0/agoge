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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

#define LOG_MSG_MAX (512)

struct str_data {
	const char *const str;
	const size_t len;
};

__attribute__((format(printf, 4, 5))) void agoge_core_log_handle(
	struct agoge_core_log *const log, const enum agoge_core_log_lvl lvl,
	const enum agoge_core_log_ch ch, const char *const fmt, ...)
{
#define STR_DEFINE(str) { (str), sizeof((str)) - 1 }

	static const struct str_data lvl_tbl[] = {
		[AGOGE_CORE_LOG_LVL_INFO] = STR_DEFINE("[info] "),
		[AGOGE_CORE_LOG_LVL_WARN] = STR_DEFINE("[warn] "),
		[AGOGE_CORE_LOG_LVL_ERR] = STR_DEFINE("[error] "),
		[AGOGE_CORE_LOG_LVL_TRACE] = STR_DEFINE("[trace] ")
	};

	static const struct str_data ch_tbl[] = {
		[AGOGE_CORE_LOG_CH_CTX] = STR_DEFINE("[ctx] "),
		[AGOGE_CORE_LOG_CH_BUS] = STR_DEFINE("[bus] "),
		[AGOGE_CORE_LOG_CH_CPU] = STR_DEFINE("[cpu] "),
		[AGOGE_CORE_LOG_CH_CART] = STR_DEFINE("[cart] "),
		[AGOGE_CORE_LOG_CH_DISASM] = STR_DEFINE("[disasm] ")
	};

#undef STR_DEFINE

	char buf[LOG_MSG_MAX];
	size_t len = 0;

	memcpy(&buf[len], lvl_tbl[lvl].str, lvl_tbl[lvl].len);
	len += lvl_tbl[lvl].len;

	memcpy(&buf[len], ch_tbl[ch].str, ch_tbl[ch].len);
	len += ch_tbl[ch].len;

	va_list args;
	va_start(args, fmt);
	len += (size_t)vsprintf(&buf[len], fmt, args);
	va_end(args);

	// clang-format off
	const struct agoge_core_log_msg msg_data = {
		.msg		= buf,
		.msg_len	= len,
		.lvl		= lvl
	};
	// clang-format on

	log->cb(log->udata, &msg_data);
}
