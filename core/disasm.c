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
	[CPU_OP_INC_B] = { .op = OP_NONE, .fmt = "INC B" },
	[CPU_OP_DEC_B] = { .op = OP_NONE, .fmt = "DEC B" },
	[CPU_OP_LD_B_U8] = { .op = OP_U8, .fmt = "LD B, $%02X" },
	[CPU_OP_LD_MEM_U16_SP] = { .op = OP_U16, .fmt = "LD ($%04X), SP" },
	[CPU_OP_INC_C] = { .op = OP_NONE, .fmt = "INC C" },
	[CPU_OP_DEC_C] = { .op = OP_NONE, .fmt = "DEC C" },
	[CPU_OP_LD_C_U8] = { .op = OP_U8, .fmt = "LD C, $%02X" },
	[CPU_OP_LD_DE_U16] = { .op = OP_U16, .fmt = "LD DE, $%04X" },
	[CPU_OP_LD_MEM_DE_A] = { .op = OP_NONE, .fmt = "LD (DE), A" },
	[CPU_OP_INC_DE] = { .op = OP_NONE, .fmt = "INC DE" },
	[CPU_OP_INC_D] = { .op = OP_NONE, .fmt = "INC D" },
	[CPU_OP_JR_S8] = { .op = OP_S8, .fmt = "JR $%04X" },
	[CPU_OP_LD_A_MEM_DE] = { .op = OP_NONE, .fmt = "LD A, (DE)" },
	[CPU_OP_INC_E] = { .op = OP_NONE, .fmt = "INC E" },
	[CPU_OP_DEC_E] = { .op = OP_NONE, .fmt = "DEC E" },
	[CPU_OP_RRA] = { .op = OP_NONE, .fmt = "RRA" },
	[CPU_OP_JR_NZ_S8] = { .op = OP_S8, .fmt = "JR NZ, $%04X" },
	[CPU_OP_LD_HL_U16] = { .op = OP_U16, .fmt = "LD HL, $%04X" },
	[CPU_OP_LDI_MEM_HL_A] = { .op = OP_NONE, .fmt = "LDI (HL), A" },
	[CPU_OP_INC_HL] = { .op = OP_NONE, .fmt = "INC HL" },
	[CPU_OP_INC_H] = { .op = OP_NONE, .fmt = "INC H" },
	[CPU_OP_DEC_H] = { .op = OP_NONE, .fmt = "DEC H" },
	[CPU_OP_LD_H_U8] = { .op = OP_U8, .fmt = "LD H, $%02X" },
	[CPU_OP_DAA] = { .op = OP_NONE, .fmt = "DAA" },
	[CPU_OP_JR_Z_S8] = { .op = OP_S8, .fmt = "JR Z, $%04X" },
	[CPU_OP_ADD_HL_HL] = { .op = OP_NONE, .fmt = "ADD HL, HL" },
	[CPU_OP_LDI_A_MEM_HL] = { .op = OP_NONE, .fmt = "LD A, (HL+)" },
	[CPU_OP_INC_L] = { .op = OP_NONE, .fmt = "INC L" },
	[CPU_OP_DEC_L] = { .op = OP_NONE, .fmt = "DEC L" },
	[CPU_OP_LD_L_U8] = { .op = OP_U8, .fmt = "LD L, $%02X" },
	[CPU_OP_CPL] = { .op = OP_NONE, .fmt = "CPL" },
	[CPU_OP_JR_NC_S8] = { .op = OP_S8, .fmt = "JR NC, $%04X" },
	[CPU_OP_LD_SP_U16] = { .op = OP_U16, .fmt = "LD SP, $%04X" },
	[CPU_OP_LDD_MEM_HL_A] = { .op = OP_NONE, .fmt = "LDD (HL), A" },
	[CPU_OP_INC_SP] = { .op = OP_NONE, .fmt = "INC SP" },
	[CPU_OP_DEC_MEM_HL] = { .op = OP_NONE, .fmt = "DEC (HL)" },
	[CPU_OP_JR_C_S8] = { .op = OP_S8, .fmt = "JR C, $%04X" },
	[CPU_OP_INC_A] = { .op = OP_NONE, .fmt = "INC A" },
	[CPU_OP_DEC_A] = { .op = OP_NONE, .fmt = "DEC A" },
	[CPU_OP_LD_A_U8] = { .op = OP_U8, .fmt = "LD A, $%02X" },
	[CPU_OP_LD_B_MEM_HL] = { .op = OP_NONE, .fmt = "LD B, (HL)" },
	[CPU_OP_LD_B_A] = { .op = OP_NONE, .fmt = "LD B, A" },
	[CPU_OP_LD_C_MEM_HL] = { .op = OP_NONE, .fmt = "LD C, (HL)" },
	[CPU_OP_LD_C_A] = { .op = OP_NONE, .fmt = "LD C, A" },
	[CPU_OP_LD_D_MEM_HL] = { .op = OP_NONE, .fmt = "LD D, (HL)" },
	[CPU_OP_LD_D_A] = { .op = OP_NONE, .fmt = "LD D, A" },
	[CPU_OP_LD_E_L] = { .op = OP_NONE, .fmt = "LD E, L" },
	[CPU_OP_LD_E_MEM_HL] = { .op = OP_NONE, .fmt = "LD E, (HL)" },
	[CPU_OP_LD_E_A] = { .op = OP_NONE, .fmt = "LD E, A" },
	[CPU_OP_LD_H_D] = { .op = OP_NONE, .fmt = "LD H, D" },
	[CPU_OP_LD_H_MEM_HL] = { .op = OP_NONE, .fmt = "LD H, (HL)" },
	[CPU_OP_LD_H_A] = { .op = OP_NONE, .fmt = "LD H, A" },
	[CPU_OP_LD_L_MEM_HL] = { .op = OP_NONE, .fmt = "LD L, (HL)" },
	[CPU_OP_LD_L_E] = { .op = OP_NONE, .fmt = "LD L, E" },
	[CPU_OP_LD_L_A] = { .op = OP_NONE, .fmt = "LD L, A" },
	[CPU_OP_LD_MEM_HL_B] = { .op = OP_NONE, .fmt = "LD (HL), B" },
	[CPU_OP_LD_MEM_HL_C] = { .op = OP_NONE, .fmt = "LD (HL), C" },
	[CPU_OP_LD_MEM_HL_D] = { .op = OP_NONE, .fmt = "LD (HL), D" },
	[CPU_OP_LD_MEM_HL_E] = { .op = OP_NONE, .fmt = "LD (HL), E" },
	[CPU_OP_LD_MEM_HL_A] = { .op = OP_NONE, .fmt = "LD (HL), A" },
	[CPU_OP_LD_A_B] = { .op = OP_NONE, .fmt = "LD A, B" },
	[CPU_OP_LD_A_C] = { .op = OP_NONE, .fmt = "LD A, C" },
	[CPU_OP_LD_A_D] = { .op = OP_NONE, .fmt = "LD A, D" },
	[CPU_OP_LD_A_E] = { .op = OP_NONE, .fmt = "LD A, E" },
	[CPU_OP_LD_A_H] = { .op = OP_NONE, .fmt = "LD A, H" },
	[CPU_OP_LD_A_L] = { .op = OP_NONE, .fmt = "LD A, L" },
	[CPU_OP_LD_A_MEM_HL] = { .op = OP_NONE, .fmt = "LD A, (HL)" },
	[CPU_OP_XOR_A_C] = { .op = OP_NONE, .fmt = "XOR A, C" },
	[CPU_OP_XOR_A_L] = { .op = OP_NONE, .fmt = "XOR A, L" },
	[CPU_OP_XOR_A_MEM_HL] = { .op = OP_NONE, .fmt = "XOR A, (HL)" },
	[CPU_OP_XOR_A_A] = { .op = OP_NONE, .fmt = "XOR A, A" },
	[CPU_OP_OR_A_C] = { .op = OP_NONE, .fmt = "OR A, C" },
	[CPU_OP_OR_A_MEM_HL] = { .op = OP_NONE, .fmt = "OR A, (HL)" },
	[CPU_OP_OR_A_A] = { .op = OP_NONE, .fmt = "OR A, A" },
	[CPU_OP_CP_A_B] = { .op = OP_NONE, .fmt = "CP A, B" },
	[CPU_OP_CP_A_C] = { .op = OP_NONE, .fmt = "CP A, C" },
	[CPU_OP_CP_A_D] = { .op = OP_NONE, .fmt = "CP A, D" },
	[CPU_OP_CP_A_E] = { .op = OP_NONE, .fmt = "CP A, E" },
	[CPU_OP_POP_BC] = { .op = OP_NONE, .fmt = "POP BC" },
	[CPU_OP_JP_NZ_U16] = { .op = OP_U16, .fmt = "JP NZ, $%04X" },
	[CPU_OP_JP_U16] = { .op = OP_U16, .fmt = "JP $%04X" },
	[CPU_OP_CALL_NZ_U16] = { .op = OP_U16, .fmt = "CALL NZ, $%04X" },
	[CPU_OP_PUSH_BC] = { .op = OP_NONE, .fmt = "PUSH BC" },
	[CPU_OP_ADD_A_U8] = { .op = OP_U8, .fmt = "ADD A, $%02X" },
	[CPU_OP_RET_Z] = { .op = OP_NONE, .fmt = "RET Z" },
	[CPU_OP_RET] = { .op = OP_NONE, .fmt = "RET" },
	[CPU_OP_CALL_U16] = { .op = OP_U16, .fmt = "CALL $%04X" },
	[CPU_OP_ADC_A_U8] = { .op = OP_U8, .fmt = "ADC A, $%02X" },
	[CPU_OP_RET_NC] = { .op = OP_NONE, .fmt = "RET NC" },
	[CPU_OP_POP_DE] = { .op = OP_NONE, .fmt = "POP DE" },
	[CPU_OP_PUSH_DE] = { .op = OP_NONE, .fmt = "PUSH DE" },
	[CPU_OP_SUB_A_U8] = { .op = OP_U8, .fmt = "SUB A, $%02X" },
	[CPU_OP_RET_C] = { .op = OP_NONE, .fmt = "RET C" },
	[CPU_OP_LD_MEM_FF00_U8_A] = { .op = OP_U8,
				      .fmt = "LD (FF00+$%02X), A" },
	[CPU_OP_POP_HL] = { .op = OP_NONE, .fmt = "POP HL" },
	[CPU_OP_PUSH_HL] = { .op = OP_NONE, .fmt = "PUSH HL" },
	[CPU_OP_AND_A_U8] = { .op = OP_NONE, .fmt = "AND A, $%02X" },
	[CPU_OP_JP_HL] = { .op = OP_NONE, .fmt = "JP (HL)" },
	[CPU_OP_LD_MEM_U16_A] = { .op = OP_U16, .fmt = "LD ($%04X), A" },
	[CPU_OP_LD_A_MEM_FF00_U8] = { .op = OP_U8,
				      .fmt = "LD A, (FF00+$%02X)" },
	[CPU_OP_POP_AF] = { .op = OP_NONE, .fmt = "POP AF" },
	[CPU_OP_DI] = { .op = OP_NONE, .fmt = "DI" },
	[CPU_OP_PUSH_AF] = { .op = OP_NONE, .fmt = "PUSH AF" },
	[CPU_OP_LD_SP_HL] = { .op = OP_NONE, .fmt = "LD SP, HL" },
	[CPU_OP_LD_A_MEM_U16] = { .op = OP_U16, .fmt = "LD A, ($%04X)" },
	[CPU_OP_XOR_A_U8] = { .op = OP_U8, .fmt = "XOR A, $%02X" },
	[CPU_OP_CP_A_U8] = { .op = OP_U8, .fmt = "CP A, $%02X" }
};

static const struct disasm_entry cb_tbl[] = {
	[CPU_OP_RR_C] = { .op = OP_NONE, .fmt = "RR C" },
	[CPU_OP_RR_D] = { .op = OP_NONE, .fmt = "RR D" },
	[CPU_OP_RR_E] = { .op = OP_NONE, .fmt = "RR E" },
	[CPU_OP_SRL_B] = { .op = OP_NONE, .fmt = "SRL B" },
	[CPU_OP_SWAP_A] = { .op = OP_NONE, .fmt = "SWAP A" }
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
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

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

	const struct disasm_entry *entry;

	if (instr == 0xCB) {
		instr = agoge_core_bus_peek(disasm->bus, addr + 1);
		entry = &cb_tbl[instr];
		LOG_TRACE(disasm->log, "$CB instr = $%02X", instr);
	} else {
		entry = &op_tbl[instr];
		LOG_TRACE(disasm->log, "instr = $%02X", instr);
	}

	memset(&disasm->res, 0, sizeof(disasm->res));
	disasm->res.addr = addr;

	format_instr(disasm, entry);
	LOG_TRACE(disasm->log, "$%04X: %s", disasm->res.addr, disasm->res.str);
}
