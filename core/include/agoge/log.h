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

/// @file log.h Defines the public interface of the agoge logger.
///
/// Rough example of usage:
///
///        #include <agoge/ctx.h>
///
///        static void log_cb(void *const udata,
///                           const char *const str,
///                           const size_t str_len,
///                           const enum agoge_log_lvl lvl)
///        {
///                printf("log message: %s\n", str);
///        }
///
///        int main(void)
///        {
///            struct agoge_ctx ctx;
///            memset(&ctx, 0, sizeof(ctx));
///
///            ctx.log.lvl = AGOGE_LOG_LVL_INFO;
///            ctx.udata = &ctx; // optional
///            ctx.log.cb = &log_cb;
///        }

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>

/// @brief Defines the various log levels of the logger.
///
/// A higher log level means more verbosity, and it will include messages from
/// lower priority log levels. For example, if the application sets the log
/// level to debug, then the application will receive debug, error, warning and
/// information level messages.
enum agoge_log_lvl {
	AGOGE_LOG_LVL_INFO = 0,
	AGOGE_LOG_LVL_WARN = 1,
	AGOGE_LOG_LVL_ERR = 2,
	AGOGE_LOG_LVL_DBG = 3,
	AGOGE_LOG_LVL_TRACE = 4
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/// @brief Defines the structure of the logger.
struct agoge_log {
	void *udata;
	void (*cb)(void *udata, const char *str, const size_t str_len,
		   const enum agoge_log_lvl lvl);
	enum agoge_log_lvl lvl;
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
}
#endif // __cplusplus
