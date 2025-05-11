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
#include <string.h>

#include "comp.h"
#include "cpu-defs.h"
#include "disasm.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_DISASM);

enum op_type { OP_NONE = 0, OP_U8 = 1, OP_U16 = 2, OP_S8 = 3, OP_BRANCH = 4 };

struct disasm_entry {
	const char *const fmt;
	const enum op_type op;
};

static const struct disasm_entry op_tbl[] = {
	[CPU_OP_NOP] = { .op = OP_NONE, .fmt = "NOP" },
	[CPU_OP_LD_HL_U16] = { .op = OP_U16, .fmt = "LD HL, $%04X" },
	[CPU_OP_JP_U16] = { .op = OP_U16, .fmt = "JP $%04X" }
};

NODISCARD static uint16_t read_u16(struct agoge_core_disasm *const disasm)
{
	const uint8_t lo =
		agoge_core_bus_peek(disasm->bus, disasm->res.addr + 1);

	const uint8_t hi =
		agoge_core_bus_peek(disasm->bus, disasm->res.addr + 2);

	return (hi << 8) | lo;
}

static void format_instr(struct agoge_core_disasm *const disasm,
			 const struct disasm_entry *const entry)
{
	static const void *const jmp_tbl[] = {
		[OP_NONE] = &&op_none, [OP_U16] = &&op_u16
	};

	goto *jmp_tbl[entry->op];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

op_none:
	disasm->res.len = sprintf(disasm->res.str, entry->fmt);
	return;

op_u16:
	disasm->res.len =
		sprintf(disasm->res.str, entry->fmt, read_u16(disasm));
	return;

#pragma GCC diagnostic pop
}

void agoge_core_disasm_init(struct agoge_core_disasm *const disasm,
			    struct agoge_core_bus *const bus,
			    struct agoge_core_log *const log)
{
	disasm->bus = bus;
	disasm->log = log;
}

void agoge_core_disasm_single(struct agoge_core_disasm *const disasm,
			      const uint16_t addr)
{
	uint8_t instr = agoge_core_bus_peek(disasm->bus, addr);
	LOG_TRACE(disasm->log, "instr = $%02X", instr);

	const struct disasm_entry *entry = &op_tbl[instr];

	memset(&disasm->res, 0, sizeof(disasm->res));
	disasm->res.addr = addr;

	format_instr(disasm, entry);
	LOG_TRACE(disasm->log, "$%04X: %s", disasm->res.addr, disasm->res.str);
}
