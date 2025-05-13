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
	[CPU_OP_LD_BC_U16] = { .op = OP_U16, .fmt = "LD BC, $%04X" },
	[CPU_OP_INC_BC] = { .op = OP_NONE, .fmt = "INC BC" },
	[CPU_OP_DEC_C] = { .op = OP_NONE, .fmt = "DEC C" },
	[CPU_OP_LD_C_U8] = { .op = OP_U8, .fmt = "LD C, $%02X" },
	[CPU_OP_LD_DE_U16] = { .op = OP_U16, .fmt = "LD DE, $%04X" },
	[CPU_OP_LD_MEM_DE_A] = { .op = OP_NONE, .fmt = "LD (DE), A" },
	[CPU_OP_INC_D] = { .op = OP_NONE, .fmt = "INC D" },
	[CPU_OP_JR_S8] = { .op = OP_S8, .fmt = "JR $%04X" },
	[CPU_OP_INC_E] = { .op = OP_NONE, .fmt = "INC E" },
	[CPU_OP_JR_NZ_S8] = { .op = OP_S8, .fmt = "JR NZ, $%04X" },
	[CPU_OP_LD_HL_U16] = { .op = OP_U16, .fmt = "LD HL, $%04X" },
	[CPU_OP_INC_HL] = { .op = OP_NONE, .fmt = "INC HL" },
	[CPU_OP_JR_Z_S8] = { .op = OP_S8, .fmt = "JR Z, $%04X" },
	[CPU_OP_LDI_A_MEM_HL] = { .op = OP_NONE, .fmt = "LD A, (HL+)" },
	[CPU_OP_LD_SP_U16] = { .op = OP_U16, .fmt = "LD SP, $%04X" },
	[CPU_OP_LD_A_U8] = { .op = OP_U8, .fmt = "LD A, $%02X" },
	[CPU_OP_LD_B_A] = { .op = OP_NONE, .fmt = "LD B, A" },
	[CPU_OP_LD_A_B] = { .op = OP_NONE, .fmt = "LD A, B" },
	[CPU_OP_LD_A_H] = { .op = OP_NONE, .fmt = "LD A, H" },
	[CPU_OP_LD_A_L] = { .op = OP_NONE, .fmt = "LD A, L" },
	[CPU_OP_OR_A_C] = { .op = OP_NONE, .fmt = "OR A, C" },
	[CPU_OP_JP_U16] = { .op = OP_U16, .fmt = "JP $%04X" },
	[CPU_OP_PUSH_BC] = { .op = OP_NONE, .fmt = "PUSH BC" },
	[CPU_OP_RET] = { .op = OP_NONE, .fmt = "RET" },
	[CPU_OP_CALL_U16] = { .op = OP_U16, .fmt = "CALL $%04X" },
	[CPU_OP_LD_MEM_FF00_U8_A] = { .op = OP_U8,
				      .fmt = "LD (FF00+$%02X), A" },
	[CPU_OP_POP_HL] = { .op = OP_NONE, .fmt = "POP HL" },
	[CPU_OP_PUSH_HL] = { .op = OP_NONE, .fmt = "PUSH HL" },
	[CPU_OP_LD_MEM_U16_A] = { .op = OP_U16, .fmt = "LD ($%04X), A" },
	[CPU_OP_POP_AF] = { .op = OP_NONE, .fmt = "POP AF" },
	[CPU_OP_DI] = { .op = OP_NONE, .fmt = "DI" },
	[CPU_OP_PUSH_AF] = { .op = OP_NONE, .fmt = "PUSH AF" }
};

NODISCARD static uint8_t read_u8(struct agoge_core_disasm *const disasm)
{
	return agoge_core_bus_peek(disasm->bus, disasm->res.addr + 1);
}

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
	static const void *const jmp_tbl[] = { [OP_NONE] = &&op_none,
					       [OP_U8] = &&op_u8,
					       [OP_U16] = &&op_u16,
					       [OP_S8] = &&op_s8 };

	goto *jmp_tbl[entry->op];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

op_none:
	disasm->res.len = sprintf(disasm->res.str, entry->fmt);
	return;

op_u8:
	disasm->res.len = sprintf(disasm->res.str, entry->fmt, read_u8(disasm));
	return;

op_u16:
	disasm->res.len =
		sprintf(disasm->res.str, entry->fmt, read_u16(disasm));
	return;

op_s8:
	disasm->res.len =
		sprintf(disasm->res.str, entry->fmt,
			((int8_t)read_u8(disasm)) + disasm->res.addr + 2);
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
