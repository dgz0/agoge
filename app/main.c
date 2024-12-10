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

#include "agoge/disasm.h"
#include "agoge/log.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "agoge/ctx.h"

static uint8_t data[64000];

static size_t open_rom(const char *const prog_name, const char *const file_name)
{
	assert(file_name != NULL);
	assert(prog_name != NULL);

	struct stat st;

	if (stat(file_name, &st) < 0) {
		fprintf(stderr, "%s: Unable to get file size of %s: %s\n",
			prog_name, file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	const size_t file_size = (size_t)st.st_size;
	FILE *file = fopen(file_name, "rb");

	if (!file) {
		fprintf(stderr, "%s: Failed to open ROM %s: %s\n", prog_name,
			file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	const size_t num_bytes_read = fread(data, 1, file_size, file);

	if ((ferror(file)) || (num_bytes_read != file_size)) {
		fprintf(stderr, "%s: Error reading ROM %s: %s\n", prog_name,
			file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return file_size;
}

static void log_cb(void *const udata, const char *const str,
		   const size_t str_len, const enum agoge_log_lvl lvl)
{
	if (lvl == AGOGE_LOG_LVL_DBG) {
		printf("%s\n", str);
		fflush(stdout);
	}

	if (lvl == AGOGE_LOG_LVL_ERR)
		abort();
}

static void load_cart(struct agoge_ctx *const ctx, const char *const prog_name,
		      const char *const file_name)
{
	const size_t file_size = open_rom(prog_name, file_name);

	const enum agoge_cart_retval ret =
		agoge_cart_set(&ctx->cart, data, file_size);

	if (!ret) {
		exit(EXIT_FAILURE);
	}
}

static void setup_ctx(struct agoge_ctx *const ctx)
{
	ctx->log.cb = &log_cb;
	ctx->log.lvl = AGOGE_LOG_LVL_TRACE;
	ctx->log.udata = ctx;
	ctx->disasm.send_to_logger = true;
	agoge_ctx_init(ctx);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "%s: Syntax: %s <rom_file>\n", argv[0],
			argv[0]);
		return EXIT_FAILURE;
	}

	struct agoge_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));

	setup_ctx(&ctx);
	load_cart(&ctx, argv[0], argv[1]);

	for (;;) {
		agoge_disasm_trace_before(&ctx.disasm);
		agoge_ctx_step(&ctx);
		agoge_disasm_trace_after(&ctx.disasm);
	}
	return EXIT_SUCCESS;
}
