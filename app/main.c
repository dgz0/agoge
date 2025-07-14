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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#include "agoge/ctx.h"

#define RED "\e[1;91m"
#define YEL "\e[1;93m"
#define WHT "\e[1;97m"
#define PURPLE "\e[0;95m"
#define RESET "\x1B[0m"

static size_t rom_size;
static uint8_t rom[AGOGE_CORE_CART_SIZE_MAX];
static struct agoge_core_ctx ctx;

static void log_cb(struct agoge_core_ctx *const m_ctx,
		   const struct agoge_core_log_msg *const msg)
{
	(void)m_ctx;

	switch (msg->lvl) {
	case AGOGE_CORE_LOG_LVL_INFO:
		printf(WHT "%s\n" RESET, msg->msg);
		return;

	case AGOGE_CORE_LOG_LVL_WARN:
		printf(YEL "%s\n" RESET, msg->msg);
		return;

	case AGOGE_CORE_LOG_LVL_ERR:
		printf(RED "%s\n" RESET, msg->msg);
		return;

	case AGOGE_CORE_LOG_LVL_DBG:
	case AGOGE_CORE_LOG_LVL_TRACE:
		printf(PURPLE "%s\n" RESET, msg->msg);
		return;

	case AGOGE_CORE_LOG_LVL_OFF:
	default:
		__builtin_unreachable();
	}
}

static bool open_rom(const char *const rom_file)
{
	struct stat st;

	if (stat(rom_file, &st) < 0) {
		fprintf(stderr, "Unable to get file size of ROM %s: %s\n",
			rom_file, strerror(errno));
		return false;
	}

	FILE *rom_file_handle = fopen(rom_file, "rb");

	if (rom_file_handle == NULL) {
		fprintf(stderr, "Unable to open ROM %s: %s\n", rom_file,
			strerror(errno));
		return false;
	}

	rom_size = fread(rom, 1, st.st_size, rom_file_handle);

	if ((rom_size != (size_t)st.st_size) || (ferror(rom_file_handle))) {
		fprintf(stderr, "Error reading ROM %s: %s\n", rom_file,
			strerror(errno));
		fclose(rom_file_handle);

		return false;
	}

	fclose(rom_file_handle);
	return true;
}

static void setup_ctx(void)
{
	ctx.log.cb = &log_cb;
	ctx.log.curr_lvl = AGOGE_CORE_LOG_LVL_TRACE;

	ctx.log.ch_enabled |=
		AGOGE_CORE_LOG_CH_CTX_BIT | AGOGE_CORE_LOG_CH_BUS_BIT |
		AGOGE_CORE_LOG_CH_CART_BIT | AGOGE_CORE_LOG_CH_DISASM_BIT;

	agoge_core_ctx_reset(&ctx);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "%s: Missing required argument.\n", argv[0]);
		fprintf(stderr, "Syntax: %s <rom_file>\n", argv[0]);

		return EXIT_FAILURE;
	}
	setup_ctx();

	if (!open_rom(argv[1])) {
		return EXIT_FAILURE;
	}

	const enum agoge_core_cart_retval ret =
		agoge_core_cart_set(&ctx, rom, rom_size);

	if (ret != AGOGE_CORE_CART_RETVAL_OK) {
		fprintf(stderr, "agoge_core_cart_set error, see log\n");
		return EXIT_FAILURE;
	}

	for (;;) {
		agoge_core_disasm_trace_before(&ctx);
		agoge_core_ctx_step(&ctx, 1);
		agoge_core_disasm_trace_after(&ctx);
	}
	return EXIT_SUCCESS;
}
