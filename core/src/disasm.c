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

#include "agoge/ctx.h"
#include "comp.h"
#include "cpu-defs.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_DISASM);

// clang-format off

#define CB_INSTR(x, trace)					\
	{							\
		.str		= (x),				\
		.str_len	= sizeof((x)) - 1,		\
		.traces		= { (trace), TRACE_REG_F },	\
		.num_traces	= 2				\
	}

#define CB_INSTR_BIT(x)					\
	{						\
		.str		= (x),			\
		.str_len	= sizeof((x)) - 1,	\
		.traces		= { TRACE_REG_F },	\
		.num_traces	= 1			\
	}

#define CB_INSTR_RES_SET(x, trace)			\
	{						\
		.str		= (x),			\
		.str_len	= sizeof((x)) - 1,	\
		.traces		= { (trace) },		\
		.num_traces	= 1			\
	}

// clang-format on

enum op_type { OP_NONE = 0, OP_U8 = 1, OP_U16 = 2, OP_S8 = 3, OP_BRANCH = 4 };

enum trace_type {
	TRACE_REG_B,
	TRACE_REG_C,
	TRACE_REG_D,
	TRACE_REG_E,
	TRACE_REG_F,
	TRACE_REG_H,
	TRACE_REG_L,
	TRACE_REG_A,
	TRACE_MEM_HL
};

struct disasm_entry {
	const char *const fmt;
	const enum op_type op;
};

struct cb_disasm_entry {
	const char *const str;
	const size_t str_len;
	const enum trace_type traces[2];
	const size_t num_traces;
};

static const struct disasm_entry op_tbl[] = {
	[CPU_OP_NOP] = { .op = OP_NONE, .fmt = "NOP" },
	[CPU_OP_LD_BC_U16] = { .op = OP_U16, .fmt = "LD BC, $%04X" },
	[CPU_OP_LD_MEM_BC_A] = { .op = OP_NONE, .fmt = "LD (BC), A" },
	[CPU_OP_INC_BC] = { .op = OP_NONE, .fmt = "INC BC" },
	[CPU_OP_INC_B] = { .op = OP_NONE, .fmt = "INC B" },
	[CPU_OP_DEC_B] = { .op = OP_NONE, .fmt = "DEC B" },
	[CPU_OP_LD_B_U8] = { .op = OP_U8, .fmt = "LD B, $%02X" },
	[CPU_OP_RLCA] = { .op = OP_NONE, .fmt = "RLCA" },
	[CPU_OP_LD_MEM_U16_SP] = { .op = OP_U16, .fmt = "LD ($%04X), SP" },
	[CPU_OP_ADD_HL_BC] = { .op = OP_NONE, .fmt = "ADD HL, BC" },
	[CPU_OP_LD_A_MEM_BC] = { .op = OP_BRANCH, .fmt = "LD A, (BC)" },
	[CPU_OP_DEC_BC] = { .op = OP_NONE, .fmt = "DEC BC" },
	[CPU_OP_INC_C] = { .op = OP_NONE, .fmt = "INC C" },
	[CPU_OP_DEC_C] = { .op = OP_NONE, .fmt = "DEC C" },
	[CPU_OP_LD_C_U8] = { .op = OP_U8, .fmt = "LD C, $%02X" },
	[CPU_OP_RRCA] = { .op = OP_NONE, .fmt = "RRCA" },
	[CPU_OP_LD_DE_U16] = { .op = OP_U16, .fmt = "LD DE, $%04X" },
	[CPU_OP_LD_MEM_DE_A] = { .op = OP_NONE, .fmt = "LD (DE), A" },
	[CPU_OP_INC_DE] = { .op = OP_NONE, .fmt = "INC DE" },
	[CPU_OP_INC_D] = { .op = OP_NONE, .fmt = "INC D" },
	[CPU_OP_DEC_D] = { .op = OP_NONE, .fmt = "DEC D" },
	[CPU_OP_LD_D_U8] = { .op = OP_U8, .fmt = "LD D, $%02X" },
	[CPU_OP_RLA] = { .op = OP_NONE, .fmt = "RLA" },
	[CPU_OP_JR_S8] = { .op = OP_BRANCH, .fmt = "JR $%04X" },
	[CPU_OP_ADD_HL_DE] = { .op = OP_NONE, .fmt = "ADD HL, DE" },
	[CPU_OP_LD_A_MEM_DE] = { .op = OP_NONE, .fmt = "LD A, (DE)" },
	[CPU_OP_DEC_DE] = { .op = OP_NONE, .fmt = "DEC DE" },
	[CPU_OP_INC_E] = { .op = OP_NONE, .fmt = "INC E" },
	[CPU_OP_DEC_E] = { .op = OP_NONE, .fmt = "DEC E" },
	[CPU_OP_LD_E_U8] = { .op = OP_U8, .fmt = "LD E, $%02X" },
	[CPU_OP_RRA] = { .op = OP_NONE, .fmt = "RRA" },
	[CPU_OP_JR_NZ_S8] = { .op = OP_BRANCH, .fmt = "JR NZ, $%04X" },
	[CPU_OP_LD_HL_U16] = { .op = OP_U16, .fmt = "LD HL, $%04X" },
	[CPU_OP_LDI_MEM_HL_A] = { .op = OP_NONE, .fmt = "LDI (HL), A" },
	[CPU_OP_INC_HL] = { .op = OP_NONE, .fmt = "INC HL" },
	[CPU_OP_INC_H] = { .op = OP_NONE, .fmt = "INC H" },
	[CPU_OP_DEC_H] = { .op = OP_NONE, .fmt = "DEC H" },
	[CPU_OP_LD_H_U8] = { .op = OP_U8, .fmt = "LD H, $%02X" },
	[CPU_OP_DAA] = { .op = OP_NONE, .fmt = "DAA" },
	[CPU_OP_JR_Z_S8] = { .op = OP_BRANCH, .fmt = "JR Z, $%04X" },
	[CPU_OP_ADD_HL_HL] = { .op = OP_NONE, .fmt = "ADD HL, HL" },
	[CPU_OP_LDI_A_MEM_HL] = { .op = OP_NONE, .fmt = "LD A, (HL+)" },
	[CPU_OP_DEC_HL] = { .op = OP_NONE, .fmt = "DEC HL" },
	[CPU_OP_INC_L] = { .op = OP_NONE, .fmt = "INC L" },
	[CPU_OP_DEC_L] = { .op = OP_NONE, .fmt = "DEC L" },
	[CPU_OP_LD_L_U8] = { .op = OP_U8, .fmt = "LD L, $%02X" },
	[CPU_OP_CPL] = { .op = OP_NONE, .fmt = "CPL" },
	[CPU_OP_JR_NC_S8] = { .op = OP_BRANCH, .fmt = "JR NC, $%04X" },
	[CPU_OP_LD_SP_U16] = { .op = OP_U16, .fmt = "LD SP, $%04X" },
	[CPU_OP_LDD_MEM_HL_A] = { .op = OP_NONE, .fmt = "LDD (HL), A" },
	[CPU_OP_INC_SP] = { .op = OP_NONE, .fmt = "INC SP" },
	[CPU_OP_INC_MEM_HL] = { .op = OP_NONE, .fmt = "INC (HL)" },
	[CPU_OP_DEC_MEM_HL] = { .op = OP_NONE, .fmt = "DEC (HL)" },
	[CPU_OP_LD_MEM_HL_U8] = { .op = OP_U8, .fmt = "LD (HL), $%02X" },
	[CPU_OP_SCF] = { .op = OP_NONE, .fmt = "SCF" },
	[CPU_OP_JR_C_S8] = { .op = OP_BRANCH, .fmt = "JR C, $%04X" },
	[CPU_OP_ADD_HL_SP] = { .op = OP_NONE, .fmt = "ADD HL, SP" },
	[CPU_OP_LDD_A_MEM_HL] = { .op = OP_NONE, .fmt = "LDD A, (HL)" },
	[CPU_OP_DEC_SP] = { .op = OP_NONE, .fmt = "DEC SP" },
	[CPU_OP_INC_A] = { .op = OP_NONE, .fmt = "INC A" },
	[CPU_OP_DEC_A] = { .op = OP_NONE, .fmt = "DEC A" },
	[CPU_OP_LD_A_U8] = { .op = OP_U8, .fmt = "LD A, $%02X" },
	[CPU_OP_CCF] = { .op = OP_NONE, .fmt = "CCF" },
	[CPU_OP_LD_B_B] = { .op = OP_NONE, .fmt = "LD B, B" },
	[CPU_OP_LD_B_C] = { .op = OP_NONE, .fmt = "LD B, C" },
	[CPU_OP_LD_B_D] = { .op = OP_NONE, .fmt = "LD B, D" },
	[CPU_OP_LD_B_E] = { .op = OP_NONE, .fmt = "LD B, E" },
	[CPU_OP_LD_B_H] = { .op = OP_NONE, .fmt = "LD B, H" },
	[CPU_OP_LD_B_L] = { .op = OP_NONE, .fmt = "LD B, L" },
	[CPU_OP_LD_B_MEM_HL] = { .op = OP_NONE, .fmt = "LD B, (HL)" },
	[CPU_OP_LD_B_A] = { .op = OP_NONE, .fmt = "LD B, A" },
	[CPU_OP_LD_C_B] = { .op = OP_NONE, .fmt = "LD C, B" },
	[CPU_OP_LD_C_C] = { .op = OP_NONE, .fmt = "LD C, C" },
	[CPU_OP_LD_C_D] = { .op = OP_NONE, .fmt = "LD C, D" },
	[CPU_OP_LD_C_E] = { .op = OP_NONE, .fmt = "LD C, E" },
	[CPU_OP_LD_C_H] = { .op = OP_NONE, .fmt = "LD C, H" },
	[CPU_OP_LD_C_L] = { .op = OP_NONE, .fmt = "LD C, L" },
	[CPU_OP_LD_C_MEM_HL] = { .op = OP_NONE, .fmt = "LD C, (HL)" },
	[CPU_OP_LD_C_A] = { .op = OP_NONE, .fmt = "LD C, A" },
	[CPU_OP_LD_D_B] = { .op = OP_NONE, .fmt = "LD D, B" },
	[CPU_OP_LD_D_C] = { .op = OP_NONE, .fmt = "LD D, C" },
	[CPU_OP_LD_D_D] = { .op = OP_NONE, .fmt = "LD D, D" },
	[CPU_OP_LD_D_E] = { .op = OP_NONE, .fmt = "LD D, E" },
	[CPU_OP_LD_D_H] = { .op = OP_NONE, .fmt = "LD D, H" },
	[CPU_OP_LD_D_L] = { .op = OP_NONE, .fmt = "LD D, L" },
	[CPU_OP_LD_D_MEM_HL] = { .op = OP_NONE, .fmt = "LD D, (HL)" },
	[CPU_OP_LD_D_A] = { .op = OP_NONE, .fmt = "LD D, A" },
	[CPU_OP_LD_E_B] = { .op = OP_NONE, .fmt = "LD E, B" },
	[CPU_OP_LD_E_C] = { .op = OP_NONE, .fmt = "LD E, C" },
	[CPU_OP_LD_E_D] = { .op = OP_NONE, .fmt = "LD E, D" },
	[CPU_OP_LD_E_E] = { .op = OP_NONE, .fmt = "LD E, E" },
	[CPU_OP_LD_E_H] = { .op = OP_NONE, .fmt = "LD E, H" },
	[CPU_OP_LD_E_L] = { .op = OP_NONE, .fmt = "LD E, L" },
	[CPU_OP_LD_E_MEM_HL] = { .op = OP_NONE, .fmt = "LD E, (HL)" },
	[CPU_OP_LD_E_A] = { .op = OP_NONE, .fmt = "LD E, A" },
	[CPU_OP_LD_H_B] = { .op = OP_NONE, .fmt = "LD H, B" },
	[CPU_OP_LD_H_C] = { .op = OP_NONE, .fmt = "LD H, C" },
	[CPU_OP_LD_H_D] = { .op = OP_NONE, .fmt = "LD H, D" },
	[CPU_OP_LD_H_E] = { .op = OP_NONE, .fmt = "LD H, E" },
	[CPU_OP_LD_H_H] = { .op = OP_NONE, .fmt = "LD H, H" },
	[CPU_OP_LD_H_L] = { .op = OP_NONE, .fmt = "LD H, L" },
	[CPU_OP_LD_H_MEM_HL] = { .op = OP_NONE, .fmt = "LD H, (HL)" },
	[CPU_OP_LD_H_A] = { .op = OP_NONE, .fmt = "LD H, A" },
	[CPU_OP_LD_L_B] = { .op = OP_NONE, .fmt = "LD L, B" },
	[CPU_OP_LD_L_C] = { .op = OP_NONE, .fmt = "LD L, C" },
	[CPU_OP_LD_L_D] = { .op = OP_NONE, .fmt = "LD L, D" },
	[CPU_OP_LD_L_E] = { .op = OP_NONE, .fmt = "LD L, E" },
	[CPU_OP_LD_L_H] = { .op = OP_NONE, .fmt = "LD L, H" },
	[CPU_OP_LD_L_L] = { .op = OP_NONE, .fmt = "LD L, L" },
	[CPU_OP_LD_L_MEM_HL] = { .op = OP_NONE, .fmt = "LD L, (HL)" },
	[CPU_OP_LD_L_A] = { .op = OP_NONE, .fmt = "LD L, A" },
	[CPU_OP_LD_MEM_HL_B] = { .op = OP_NONE, .fmt = "LD (HL), B" },
	[CPU_OP_LD_MEM_HL_C] = { .op = OP_NONE, .fmt = "LD (HL), C" },
	[CPU_OP_LD_MEM_HL_D] = { .op = OP_NONE, .fmt = "LD (HL), D" },
	[CPU_OP_LD_MEM_HL_E] = { .op = OP_NONE, .fmt = "LD (HL), E" },
	[CPU_OP_LD_MEM_HL_H] = { .op = OP_NONE, .fmt = "LD (HL), H" },
	[CPU_OP_LD_MEM_HL_L] = { .op = OP_NONE, .fmt = "LD (HL), L" },
	[CPU_OP_LD_MEM_HL_A] = { .op = OP_NONE, .fmt = "LD (HL), A" },
	[CPU_OP_LD_A_B] = { .op = OP_NONE, .fmt = "LD A, B" },
	[CPU_OP_LD_A_C] = { .op = OP_NONE, .fmt = "LD A, C" },
	[CPU_OP_LD_A_D] = { .op = OP_NONE, .fmt = "LD A, D" },
	[CPU_OP_LD_A_E] = { .op = OP_NONE, .fmt = "LD A, E" },
	[CPU_OP_LD_A_H] = { .op = OP_NONE, .fmt = "LD A, H" },
	[CPU_OP_LD_A_L] = { .op = OP_NONE, .fmt = "LD A, L" },
	[CPU_OP_LD_A_MEM_HL] = { .op = OP_NONE, .fmt = "LD A, (HL)" },
	[CPU_OP_LD_A_A] = { .op = OP_NONE, .fmt = "LD A, A" },
	[CPU_OP_ADD_A_B] = { .op = OP_NONE, .fmt = "ADD A, B" },
	[CPU_OP_ADD_A_C] = { .op = OP_NONE, .fmt = "ADD A, C" },
	[CPU_OP_ADD_A_D] = { .op = OP_NONE, .fmt = "ADD A, D" },
	[CPU_OP_ADD_A_E] = { .op = OP_NONE, .fmt = "ADD A, E" },
	[CPU_OP_ADD_A_H] = { .op = OP_NONE, .fmt = "ADD A, H" },
	[CPU_OP_ADD_A_L] = { .op = OP_NONE, .fmt = "ADD A, L" },
	[CPU_OP_ADD_A_MEM_HL] = { .op = OP_NONE, .fmt = "ADD A, (HL)" },
	[CPU_OP_ADD_A_A] = { .op = OP_NONE, .fmt = "ADD A, A" },
	[CPU_OP_ADC_A_B] = { .op = OP_NONE, .fmt = "ADC A, B" },
	[CPU_OP_ADC_A_C] = { .op = OP_NONE, .fmt = "ADC A, C" },
	[CPU_OP_ADC_A_D] = { .op = OP_NONE, .fmt = "ADC A, D" },
	[CPU_OP_ADC_A_E] = { .op = OP_NONE, .fmt = "ADC A, E" },
	[CPU_OP_ADC_A_H] = { .op = OP_NONE, .fmt = "ADC A, H" },
	[CPU_OP_ADC_A_L] = { .op = OP_NONE, .fmt = "ADC A, L" },
	[CPU_OP_ADC_A_MEM_HL] = { .op = OP_NONE, .fmt = "ADC A, (HL)" },
	[CPU_OP_ADC_A_A] = { .op = OP_NONE, .fmt = "ADC A, A" },
	[CPU_OP_SUB_A_B] = { .op = OP_NONE, .fmt = "SUB A, B" },
	[CPU_OP_SUB_A_C] = { .op = OP_NONE, .fmt = "SUB A, C" },
	[CPU_OP_SUB_A_D] = { .op = OP_NONE, .fmt = "SUB A, D" },
	[CPU_OP_SUB_A_E] = { .op = OP_NONE, .fmt = "SUB A, E" },
	[CPU_OP_SUB_A_H] = { .op = OP_NONE, .fmt = "SUB A, H" },
	[CPU_OP_SUB_A_L] = { .op = OP_NONE, .fmt = "SUB A, L" },
	[CPU_OP_SUB_A_MEM_HL] = { .op = OP_NONE, .fmt = "SUB A, (HL)" },
	[CPU_OP_SUB_A_A] = { .op = OP_NONE, .fmt = "SUB A, A" },
	[CPU_OP_SBC_A_B] = { .op = OP_NONE, .fmt = "SBC A, B" },
	[CPU_OP_SBC_A_C] = { .op = OP_NONE, .fmt = "SBC A, C" },
	[CPU_OP_SBC_A_D] = { .op = OP_NONE, .fmt = "SBC A, D" },
	[CPU_OP_SBC_A_E] = { .op = OP_NONE, .fmt = "SBC A, E" },
	[CPU_OP_SBC_A_H] = { .op = OP_NONE, .fmt = "SBC A, H" },
	[CPU_OP_SBC_A_L] = { .op = OP_NONE, .fmt = "SBC A, L" },
	[CPU_OP_SBC_A_MEM_HL] = { .op = OP_NONE, .fmt = "SBC A, (HL)" },
	[CPU_OP_SBC_A_A] = { .op = OP_NONE, .fmt = "SBC A, A" },
	[CPU_OP_AND_A_B] = { .op = OP_NONE, .fmt = "AND A, B" },
	[CPU_OP_AND_A_C] = { .op = OP_NONE, .fmt = "AND A, C" },
	[CPU_OP_AND_A_D] = { .op = OP_NONE, .fmt = "AND A, D" },
	[CPU_OP_AND_A_E] = { .op = OP_NONE, .fmt = "AND A, E" },
	[CPU_OP_AND_A_H] = { .op = OP_NONE, .fmt = "AND A, H" },
	[CPU_OP_AND_A_L] = { .op = OP_NONE, .fmt = "AND A, L" },
	[CPU_OP_AND_A_MEM_HL] = { .op = OP_NONE, .fmt = "AND A, (HL)" },
	[CPU_OP_AND_A_A] = { .op = OP_NONE, .fmt = "AND A, A" },
	[CPU_OP_XOR_A_B] = { .op = OP_NONE, .fmt = "XOR A, B" },
	[CPU_OP_XOR_A_C] = { .op = OP_NONE, .fmt = "XOR A, C" },
	[CPU_OP_XOR_A_D] = { .op = OP_NONE, .fmt = "XOR A, D" },
	[CPU_OP_XOR_A_E] = { .op = OP_NONE, .fmt = "XOR A, E" },
	[CPU_OP_XOR_A_H] = { .op = OP_NONE, .fmt = "XOR A, H" },
	[CPU_OP_XOR_A_L] = { .op = OP_NONE, .fmt = "XOR A, L" },
	[CPU_OP_XOR_A_MEM_HL] = { .op = OP_NONE, .fmt = "XOR A, (HL)" },
	[CPU_OP_XOR_A_A] = { .op = OP_NONE, .fmt = "XOR A, A" },
	[CPU_OP_OR_A_B] = { .op = OP_NONE, .fmt = "OR A, B" },
	[CPU_OP_OR_A_C] = { .op = OP_NONE, .fmt = "OR A, C" },
	[CPU_OP_OR_A_D] = { .op = OP_NONE, .fmt = "OR A, D" },
	[CPU_OP_OR_A_E] = { .op = OP_NONE, .fmt = "OR A, E" },
	[CPU_OP_OR_A_H] = { .op = OP_NONE, .fmt = "OR A, H" },
	[CPU_OP_OR_A_MEM_HL] = { .op = OP_NONE, .fmt = "OR A, (HL)" },
	[CPU_OP_OR_A_L] = { .op = OP_NONE, .fmt = "OR A, L" },
	[CPU_OP_OR_A_A] = { .op = OP_NONE, .fmt = "OR A, A" },
	[CPU_OP_CP_A_B] = { .op = OP_NONE, .fmt = "CP A, B" },
	[CPU_OP_CP_A_C] = { .op = OP_NONE, .fmt = "CP A, C" },
	[CPU_OP_CP_A_D] = { .op = OP_NONE, .fmt = "CP A, D" },
	[CPU_OP_CP_A_E] = { .op = OP_NONE, .fmt = "CP A, E" },
	[CPU_OP_CP_A_H] = { .op = OP_NONE, .fmt = "CP A, H" },
	[CPU_OP_CP_A_L] = { .op = OP_NONE, .fmt = "CP A, L" },
	[CPU_OP_CP_A_MEM_HL] = { .op = OP_NONE, .fmt = "CP A, (HL)" },
	[CPU_OP_CP_A_A] = { .op = OP_NONE, .fmt = "CP A, A" },
	[CPU_OP_RET_NZ] = { .op = OP_NONE, .fmt = "RET NZ" },
	[CPU_OP_POP_BC] = { .op = OP_NONE, .fmt = "POP BC" },
	[CPU_OP_JP_NZ_U16] = { .op = OP_U16, .fmt = "JP NZ, $%04X" },
	[CPU_OP_JP_U16] = { .op = OP_U16, .fmt = "JP $%04X" },
	[CPU_OP_CALL_NZ_U16] = { .op = OP_U16, .fmt = "CALL NZ, $%04X" },
	[CPU_OP_PUSH_BC] = { .op = OP_NONE, .fmt = "PUSH BC" },
	[CPU_OP_ADD_A_U8] = { .op = OP_U8, .fmt = "ADD A, $%02X" },
	[CPU_OP_RST_00] = { .op = OP_NONE, .fmt = "RST $00" },
	[CPU_OP_RET_Z] = { .op = OP_NONE, .fmt = "RET Z" },
	[CPU_OP_RET] = { .op = OP_NONE, .fmt = "RET" },
	[CPU_OP_JP_Z_U16] = { .op = OP_U16, .fmt = "JP Z, $%04X" },
	[CPU_OP_CALL_Z_U16] = { .op = OP_U16, .fmt = "CALL Z, $%04X" },
	[CPU_OP_CALL_U16] = { .op = OP_U16, .fmt = "CALL $%04X" },
	[CPU_OP_ADC_A_U8] = { .op = OP_U8, .fmt = "ADC A, $%02X" },
	[CPU_OP_RST_08] = { .op = OP_NONE, .fmt = "RST $08" },
	[CPU_OP_RET_NC] = { .op = OP_NONE, .fmt = "RET NC" },
	[CPU_OP_POP_DE] = { .op = OP_NONE, .fmt = "POP DE" },
	[CPU_OP_JP_NC_U16] = { .op = OP_U16, .fmt = "JP NC, $%04X" },
	[CPU_OP_CALL_NC_U16] = { .op = OP_U16, .fmt = "CALL NC, $%04X" },
	[CPU_OP_PUSH_DE] = { .op = OP_NONE, .fmt = "PUSH DE" },
	[CPU_OP_SUB_A_U8] = { .op = OP_U8, .fmt = "SUB A, $%02X" },
	[CPU_OP_RST_10] = { .op = OP_NONE, .fmt = "RST $10" },
	[CPU_OP_RET_C] = { .op = OP_NONE, .fmt = "RET C" },
	[CPU_OP_RETI] = { .op = OP_NONE, .fmt = "RETI" },
	[CPU_OP_JP_C_U16] = { .op = OP_U16, .fmt = "JP C, $%04X" },
	[CPU_OP_CALL_C_U16] = { .op = OP_U16, .fmt = "CALL C, $%04X" },
	[CPU_OP_SBC_A_U8] = { .op = OP_U8, .fmt = "SBC A, $%02X" },
	[CPU_OP_RST_18] = { .op = OP_NONE, .fmt = "RST $18" },
	[CPU_OP_LD_MEM_FF00_U8_A] = { .op = OP_U8,
				      .fmt = "LD (FF00+$%02X), A" },
	[CPU_OP_POP_HL] = { .op = OP_NONE, .fmt = "POP HL" },
	[CPU_OP_LD_MEM_FF00_C_A] = { .op = OP_NONE, .fmt = "LD (FF00+C), A" },
	[CPU_OP_PUSH_HL] = { .op = OP_NONE, .fmt = "PUSH HL" },
	[CPU_OP_AND_A_U8] = { .op = OP_NONE, .fmt = "AND A, $%02X" },
	[CPU_OP_RST_20] = { .op = OP_NONE, .fmt = "RST $20" },
	[CPU_OP_ADD_SP_S8] = { .op = OP_S8, .fmt = "ADD SP, %d" },
	[CPU_OP_JP_HL] = { .op = OP_NONE, .fmt = "JP (HL)" },
	[CPU_OP_LD_MEM_U16_A] = { .op = OP_U16, .fmt = "LD ($%04X), A" },
	[CPU_OP_LD_A_MEM_FF00_U8] = { .op = OP_U8,
				      .fmt = "LD A, (FF00+$%02X)" },

	[CPU_OP_LD_A_MEM_FF00_C] = { .op = OP_NONE, .fmt = "LD A, (FF00+C)" },
	[CPU_OP_POP_AF] = { .op = OP_NONE, .fmt = "POP AF" },
	[CPU_OP_DI] = { .op = OP_NONE, .fmt = "DI" },
	[CPU_OP_PUSH_AF] = { .op = OP_NONE, .fmt = "PUSH AF" },
	[CPU_OP_OR_A_U8] = { .op = OP_U8, .fmt = "OR A, $%02X" },
	[CPU_OP_RST_30] = { .op = OP_NONE, .fmt = "RST $30" },
	[CPU_OP_LD_HL_SP_S8] = { .op = OP_S8, .fmt = "LD HL, SP+%d" },
	[CPU_OP_LD_SP_HL] = { .op = OP_NONE, .fmt = "LD SP, HL" },
	[CPU_OP_LD_A_MEM_U16] = { .op = OP_U16, .fmt = "LD A, ($%04X)" },
	[CPU_OP_EI] = { .op = OP_NONE, .fmt = "EI" },
	[CPU_OP_XOR_A_U8] = { .op = OP_U8, .fmt = "XOR A, $%02X" },
	[CPU_OP_RST_28] = { .op = OP_NONE, .fmt = "RST $28" },
	[CPU_OP_CP_A_U8] = { .op = OP_U8, .fmt = "CP A, $%02X" },
	[CPU_OP_RST_38] = { .op = OP_NONE, .fmt = "RST $38" },
};

static const struct cb_disasm_entry cb_tbl[] = {
	// clang-format off

	[CPU_OP_RLC_B]		= CB_INSTR("RLC B", TRACE_REG_B),
	[CPU_OP_RLC_C]		= CB_INSTR("RLC C", TRACE_REG_C),
	[CPU_OP_RLC_D]		= CB_INSTR("RLC D", TRACE_REG_D),
	[CPU_OP_RLC_E]		= CB_INSTR("RLC E", TRACE_REG_E),
	[CPU_OP_RLC_H]		= CB_INSTR("RLC H", TRACE_REG_H),
	[CPU_OP_RLC_L]		= CB_INSTR("RLC L", TRACE_REG_L),
	[CPU_OP_RLC_MEM_HL]	= CB_INSTR("RLC (HL)", TRACE_MEM_HL),
	[CPU_OP_RLC_A]		= CB_INSTR("RLC A", TRACE_REG_A),
	[CPU_OP_RRC_B]		= CB_INSTR("RRC B", TRACE_REG_B),
	[CPU_OP_RRC_C]		= CB_INSTR("RRC C", TRACE_REG_C),
	[CPU_OP_RRC_D]		= CB_INSTR("RRC D", TRACE_REG_D),
	[CPU_OP_RRC_E]		= CB_INSTR("RRC E", TRACE_REG_E),
	[CPU_OP_RRC_H]		= CB_INSTR("RRC H", TRACE_REG_H),
	[CPU_OP_RRC_L]		= CB_INSTR("RRC L", TRACE_REG_L),
	[CPU_OP_RRC_MEM_HL]	= CB_INSTR("RRC (HL)", TRACE_MEM_HL),
	[CPU_OP_RRC_A]		= CB_INSTR("RRC A", TRACE_REG_A),
	[CPU_OP_RL_B]		= CB_INSTR("RL B", TRACE_REG_B),
	[CPU_OP_RL_C]		= CB_INSTR("RL C", TRACE_REG_C),
	[CPU_OP_RL_D]		= CB_INSTR("RL D", TRACE_REG_D),
	[CPU_OP_RL_E]		= CB_INSTR("RL E", TRACE_REG_E),
	[CPU_OP_RL_H]		= CB_INSTR("RL H", TRACE_REG_H),
	[CPU_OP_RL_L]		= CB_INSTR("RL L", TRACE_REG_L),
	[CPU_OP_RL_MEM_HL]	= CB_INSTR("RL (HL)", TRACE_MEM_HL),
	[CPU_OP_RL_A]		= CB_INSTR("RL A", TRACE_REG_A),
	[CPU_OP_RR_B]		= CB_INSTR("RR B", TRACE_REG_B),
	[CPU_OP_RR_C]		= CB_INSTR("RR C", TRACE_REG_C),
	[CPU_OP_RR_D]		= CB_INSTR("RR D", TRACE_REG_D),
	[CPU_OP_RR_E]		= CB_INSTR("RR E", TRACE_REG_E),
	[CPU_OP_RR_H]		= CB_INSTR("RR H", TRACE_REG_H),
	[CPU_OP_RR_L]		= CB_INSTR("RR L", TRACE_REG_L),
	[CPU_OP_RR_MEM_HL]	= CB_INSTR("RR (HL)", TRACE_MEM_HL),
	[CPU_OP_RR_A]		= CB_INSTR("RR A", TRACE_REG_A),
	[CPU_OP_SLA_B]		= CB_INSTR("SLA B", TRACE_REG_B),
	[CPU_OP_SLA_C]		= CB_INSTR("SLA C", TRACE_REG_C),
	[CPU_OP_SLA_D]		= CB_INSTR("SLA D", TRACE_REG_D),
	[CPU_OP_SLA_E]		= CB_INSTR("SLA E", TRACE_REG_E),
	[CPU_OP_SLA_H]		= CB_INSTR("SLA H", TRACE_REG_H),
	[CPU_OP_SLA_L]		= CB_INSTR("SLA L", TRACE_REG_L),
	[CPU_OP_SLA_MEM_HL]	= CB_INSTR("SLA (HL)", TRACE_MEM_HL),
	[CPU_OP_SLA_A]		= CB_INSTR("SLA A", TRACE_REG_A),
	[CPU_OP_SRA_B]		= CB_INSTR("SRA B", TRACE_REG_B),
	[CPU_OP_SRA_C]		= CB_INSTR("SRA C", TRACE_REG_C),
	[CPU_OP_SRA_D]		= CB_INSTR("SRA D", TRACE_REG_D),
	[CPU_OP_SRA_E]		= CB_INSTR("SRA E", TRACE_REG_E),
	[CPU_OP_SRA_H]		= CB_INSTR("SRA H", TRACE_REG_H),
	[CPU_OP_SRA_L]		= CB_INSTR("SRA L", TRACE_REG_L),
	[CPU_OP_SRA_MEM_HL]	= CB_INSTR("SRA (HL)", TRACE_MEM_HL),
	[CPU_OP_SRA_A]		= CB_INSTR("SRA A", TRACE_REG_A),
	[CPU_OP_SWAP_B]		= CB_INSTR("SWAP B", TRACE_REG_B),
	[CPU_OP_SWAP_C]		= CB_INSTR("SWAP C", TRACE_REG_C),
	[CPU_OP_SWAP_D]		= CB_INSTR("SWAP D", TRACE_REG_D),
	[CPU_OP_SWAP_E]		= CB_INSTR("SWAP E", TRACE_REG_E),
	[CPU_OP_SWAP_H]		= CB_INSTR("SWAP H", TRACE_REG_H),
	[CPU_OP_SWAP_L]		= CB_INSTR("SWAP L", TRACE_REG_L),
	[CPU_OP_SWAP_MEM_HL]	= CB_INSTR("SWAP (HL)", TRACE_MEM_HL),
	[CPU_OP_SWAP_A]		= CB_INSTR("SWAP A", TRACE_REG_A),
	[CPU_OP_SRL_B]		= CB_INSTR("SRL B", TRACE_REG_B),
	[CPU_OP_SRL_C]		= CB_INSTR("SRL C", TRACE_REG_C),
	[CPU_OP_SRL_D]		= CB_INSTR("SRL D", TRACE_REG_D),
	[CPU_OP_SRL_E]		= CB_INSTR("SRL E", TRACE_REG_E),
	[CPU_OP_SRL_H]		= CB_INSTR("SRL H", TRACE_REG_H),
	[CPU_OP_SRL_L]		= CB_INSTR("SRL L", TRACE_REG_L),
	[CPU_OP_SRL_MEM_HL]	= CB_INSTR("SRL (HL)", TRACE_MEM_HL),
	[CPU_OP_SRL_A]		= CB_INSTR("SRL A", TRACE_REG_A),
	[CPU_OP_BIT_0_B]	= CB_INSTR_BIT("BIT 0, B"),
	[CPU_OP_BIT_0_C]	= CB_INSTR_BIT("BIT 0, C"),
	[CPU_OP_BIT_0_D]	= CB_INSTR_BIT("BIT 0, D"),
	[CPU_OP_BIT_0_E]	= CB_INSTR_BIT("BIT 0, E"),
	[CPU_OP_BIT_0_H]	= CB_INSTR_BIT("BIT 0, H"),
	[CPU_OP_BIT_0_L]	= CB_INSTR_BIT("BIT 0, L"),
	[CPU_OP_BIT_0_MEM_HL]	= CB_INSTR_BIT("BIT 0, (HL)"),
	[CPU_OP_BIT_0_A]	= CB_INSTR_BIT("BIT 0, A"),
	[CPU_OP_BIT_1_B]	= CB_INSTR_BIT("BIT 1, B"),
	[CPU_OP_BIT_1_C]	= CB_INSTR_BIT("BIT 1, C"),
	[CPU_OP_BIT_1_D]	= CB_INSTR_BIT("BIT 1, D"),
	[CPU_OP_BIT_1_E]	= CB_INSTR_BIT("BIT 1, E"),
	[CPU_OP_BIT_1_H]	= CB_INSTR_BIT("BIT 1, H"),
	[CPU_OP_BIT_1_L]	= CB_INSTR_BIT("BIT 1, L"),
	[CPU_OP_BIT_1_MEM_HL]	= CB_INSTR_BIT("BIT 1, (HL)"),
	[CPU_OP_BIT_1_A]	= CB_INSTR_BIT("BIT 1, A"),
	[CPU_OP_BIT_2_B]	= CB_INSTR_BIT("BIT 2, B"),
	[CPU_OP_BIT_2_C]	= CB_INSTR_BIT("BIT 2, C"),
	[CPU_OP_BIT_2_D]	= CB_INSTR_BIT("BIT 2, D"),
	[CPU_OP_BIT_2_E]	= CB_INSTR_BIT("BIT 2, E"),
	[CPU_OP_BIT_2_H]	= CB_INSTR_BIT("BIT 2, H"),
	[CPU_OP_BIT_2_L]	= CB_INSTR_BIT("BIT 2, L"),
	[CPU_OP_BIT_2_MEM_HL]	= CB_INSTR_BIT("BIT 2, (HL)"),
	[CPU_OP_BIT_2_A]	= CB_INSTR_BIT("BIT 2, A"),
	[CPU_OP_BIT_3_B]	= CB_INSTR_BIT("BIT 3, B"),
	[CPU_OP_BIT_3_C]	= CB_INSTR_BIT("BIT 3, C"),
	[CPU_OP_BIT_3_D]	= CB_INSTR_BIT("BIT 3, D"),
	[CPU_OP_BIT_3_E]	= CB_INSTR_BIT("BIT 3, E"),
	[CPU_OP_BIT_3_H]	= CB_INSTR_BIT("BIT 3, H"),
	[CPU_OP_BIT_3_L]	= CB_INSTR_BIT("BIT 3, L"),
	[CPU_OP_BIT_3_MEM_HL]	= CB_INSTR_BIT("BIT 3, (HL)"),
	[CPU_OP_BIT_3_A]	= CB_INSTR_BIT("BIT 3, A"),
	[CPU_OP_BIT_4_B]	= CB_INSTR_BIT("BIT 4, B"),
	[CPU_OP_BIT_4_C]	= CB_INSTR_BIT("BIT 4, C"),
	[CPU_OP_BIT_4_D]	= CB_INSTR_BIT("BIT 4, D"),
	[CPU_OP_BIT_4_E]	= CB_INSTR_BIT("BIT 4, E"),
	[CPU_OP_BIT_4_H]	= CB_INSTR_BIT("BIT 4, H"),
	[CPU_OP_BIT_4_L]	= CB_INSTR_BIT("BIT 4, L"),
	[CPU_OP_BIT_4_MEM_HL]	= CB_INSTR_BIT("BIT 4, (HL)"),
	[CPU_OP_BIT_4_A]	= CB_INSTR_BIT("BIT 4, A"),
	[CPU_OP_BIT_5_B]	= CB_INSTR_BIT("BIT 5, B"),
	[CPU_OP_BIT_5_C]	= CB_INSTR_BIT("BIT 5, C"),
	[CPU_OP_BIT_5_D]	= CB_INSTR_BIT("BIT 5, D"),
	[CPU_OP_BIT_5_E]	= CB_INSTR_BIT("BIT 5, E"),
	[CPU_OP_BIT_5_H]	= CB_INSTR_BIT("BIT 5, H"),
	[CPU_OP_BIT_5_L]	= CB_INSTR_BIT("BIT 5, L"),
	[CPU_OP_BIT_5_MEM_HL]	= CB_INSTR_BIT("BIT 5, (HL)"),
	[CPU_OP_BIT_5_A]	= CB_INSTR_BIT("BIT 5, A"),
	[CPU_OP_BIT_6_B]	= CB_INSTR_BIT("BIT 6, B"),
	[CPU_OP_BIT_6_C]	= CB_INSTR_BIT("BIT 6, C"),
	[CPU_OP_BIT_6_D]	= CB_INSTR_BIT("BIT 6, D"),
	[CPU_OP_BIT_6_E]	= CB_INSTR_BIT("BIT 6, E"),
	[CPU_OP_BIT_6_H]	= CB_INSTR_BIT("BIT 6, H"),
	[CPU_OP_BIT_6_L]	= CB_INSTR_BIT("BIT 6, L"),
	[CPU_OP_BIT_6_MEM_HL]	= CB_INSTR_BIT("BIT 6, (HL)"),
	[CPU_OP_BIT_6_A]	= CB_INSTR_BIT("BIT 6, A"),
	[CPU_OP_BIT_7_B]	= CB_INSTR_BIT("BIT 7, B"),
	[CPU_OP_BIT_7_C]	= CB_INSTR_BIT("BIT 7, C"),
	[CPU_OP_BIT_7_D]	= CB_INSTR_BIT("BIT 7, D"),
	[CPU_OP_BIT_7_E]	= CB_INSTR_BIT("BIT 7, E"),
	[CPU_OP_BIT_7_H]	= CB_INSTR_BIT("BIT 7, H"),
	[CPU_OP_BIT_7_L]	= CB_INSTR_BIT("BIT 7, L"),
	[CPU_OP_BIT_7_MEM_HL]	= CB_INSTR_BIT("BIT 7, (HL)"),
	[CPU_OP_BIT_7_A]	= CB_INSTR_BIT("BIT 7, A"),
	[CPU_OP_RES_0_B]	= CB_INSTR_RES_SET("RES 0, B", TRACE_REG_B),
	[CPU_OP_RES_0_C]	= CB_INSTR_RES_SET("RES 0, C", TRACE_REG_C),
	[CPU_OP_RES_0_D]	= CB_INSTR_RES_SET("RES 0, D", TRACE_REG_D),
	[CPU_OP_RES_0_E]	= CB_INSTR_RES_SET("RES 0, E", TRACE_REG_E),
	[CPU_OP_RES_0_H]	= CB_INSTR_RES_SET("RES 0, H", TRACE_REG_H),
	[CPU_OP_RES_0_L]	= CB_INSTR_RES_SET("RES 0, L", TRACE_REG_L),
	[CPU_OP_RES_0_MEM_HL]	= CB_INSTR_RES_SET("RES 0, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_0_A]	= CB_INSTR_RES_SET("RES 0, A", TRACE_REG_A),
	[CPU_OP_RES_1_B]	= CB_INSTR_RES_SET("RES 1, B", TRACE_REG_B),
	[CPU_OP_RES_1_C]	= CB_INSTR_RES_SET("RES 1, C", TRACE_REG_C),
	[CPU_OP_RES_1_D]	= CB_INSTR_RES_SET("RES 1, D", TRACE_REG_D),
	[CPU_OP_RES_1_E]	= CB_INSTR_RES_SET("RES 1, E", TRACE_REG_E),
	[CPU_OP_RES_1_H]	= CB_INSTR_RES_SET("RES 1, H", TRACE_REG_H),
	[CPU_OP_RES_1_L]	= CB_INSTR_RES_SET("RES 1, L", TRACE_REG_L),
	[CPU_OP_RES_1_MEM_HL]	= CB_INSTR_RES_SET("RES 1, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_1_A]	= CB_INSTR_RES_SET("RES 1, A", TRACE_REG_A),
	[CPU_OP_RES_2_B]	= CB_INSTR_RES_SET("RES 2, B", TRACE_REG_B),
	[CPU_OP_RES_2_C]	= CB_INSTR_RES_SET("RES 2, C", TRACE_REG_C),
	[CPU_OP_RES_2_D]	= CB_INSTR_RES_SET("RES 2, D", TRACE_REG_D),
	[CPU_OP_RES_2_E]	= CB_INSTR_RES_SET("RES 2, E", TRACE_REG_E),
	[CPU_OP_RES_2_H]	= CB_INSTR_RES_SET("RES 2, H", TRACE_REG_H),
	[CPU_OP_RES_2_L]	= CB_INSTR_RES_SET("RES 2, L", TRACE_REG_L),
	[CPU_OP_RES_2_MEM_HL]	= CB_INSTR_RES_SET("RES 2, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_2_A]	= CB_INSTR_RES_SET("RES 2, A", TRACE_REG_A),
	[CPU_OP_RES_3_B]	= CB_INSTR_RES_SET("RES 3, B", TRACE_REG_B),
	[CPU_OP_RES_3_C]	= CB_INSTR_RES_SET("RES 3, C", TRACE_REG_C),
	[CPU_OP_RES_3_D]	= CB_INSTR_RES_SET("RES 3, D", TRACE_REG_D),
	[CPU_OP_RES_3_E]	= CB_INSTR_RES_SET("RES 3, E", TRACE_REG_E),
	[CPU_OP_RES_3_H]	= CB_INSTR_RES_SET("RES 3, H", TRACE_REG_H),
	[CPU_OP_RES_3_L]	= CB_INSTR_RES_SET("RES 3, L", TRACE_REG_L),
	[CPU_OP_RES_3_MEM_HL]	= CB_INSTR_RES_SET("RES 3, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_3_A]	= CB_INSTR_RES_SET("RES 3, A", TRACE_REG_A),
	[CPU_OP_RES_4_B]	= CB_INSTR_RES_SET("RES 4, B", TRACE_REG_B),
	[CPU_OP_RES_4_C]	= CB_INSTR_RES_SET("RES 4, C", TRACE_REG_C),
	[CPU_OP_RES_4_D]	= CB_INSTR_RES_SET("RES 4, D", TRACE_REG_D),
	[CPU_OP_RES_4_E]	= CB_INSTR_RES_SET("RES 4, E", TRACE_REG_E),
	[CPU_OP_RES_4_H]	= CB_INSTR_RES_SET("RES 4, H", TRACE_REG_H),
	[CPU_OP_RES_4_L]	= CB_INSTR_RES_SET("RES 4, L", TRACE_REG_L),
	[CPU_OP_RES_4_MEM_HL]	= CB_INSTR_RES_SET("RES 4, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_4_A]	= CB_INSTR_RES_SET("RES 4, A", TRACE_REG_A),
	[CPU_OP_RES_5_B]	= CB_INSTR_RES_SET("RES 5, B", TRACE_REG_B),
	[CPU_OP_RES_5_C]	= CB_INSTR_RES_SET("RES 5, C", TRACE_REG_C),
	[CPU_OP_RES_5_D]	= CB_INSTR_RES_SET("RES 5, D", TRACE_REG_D),
	[CPU_OP_RES_5_E]	= CB_INSTR_RES_SET("RES 5, E", TRACE_REG_E),
	[CPU_OP_RES_5_H]	= CB_INSTR_RES_SET("RES 5, H", TRACE_REG_H),
	[CPU_OP_RES_5_L]	= CB_INSTR_RES_SET("RES 5, L", TRACE_REG_L),
	[CPU_OP_RES_5_MEM_HL]	= CB_INSTR_RES_SET("RES 5, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_5_A]	= CB_INSTR_RES_SET("RES 5, A", TRACE_REG_A),
	[CPU_OP_RES_6_B]	= CB_INSTR_RES_SET("RES 6, B", TRACE_REG_B),
	[CPU_OP_RES_6_C]	= CB_INSTR_RES_SET("RES 6, C", TRACE_REG_C),
	[CPU_OP_RES_6_D]	= CB_INSTR_RES_SET("RES 6, D", TRACE_REG_D),
	[CPU_OP_RES_6_E]	= CB_INSTR_RES_SET("RES 6, E", TRACE_REG_E),
	[CPU_OP_RES_6_H]	= CB_INSTR_RES_SET("RES 6, H", TRACE_REG_H),
	[CPU_OP_RES_6_L]	= CB_INSTR_RES_SET("RES 6, L", TRACE_REG_L),
	[CPU_OP_RES_6_MEM_HL]	= CB_INSTR_RES_SET("RES 6, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_6_A]	= CB_INSTR_RES_SET("RES 6, A", TRACE_REG_A),
	[CPU_OP_RES_7_B]	= CB_INSTR_RES_SET("RES 7, B", TRACE_REG_B),
	[CPU_OP_RES_7_C]	= CB_INSTR_RES_SET("RES 7, C", TRACE_REG_C),
	[CPU_OP_RES_7_D]	= CB_INSTR_RES_SET("RES 7, D", TRACE_REG_D),
	[CPU_OP_RES_7_E]	= CB_INSTR_RES_SET("RES 7, E", TRACE_REG_E),
	[CPU_OP_RES_7_H]	= CB_INSTR_RES_SET("RES 7, H", TRACE_REG_H),
	[CPU_OP_RES_7_L]	= CB_INSTR_RES_SET("RES 7, L", TRACE_REG_L),
	[CPU_OP_RES_7_MEM_HL]	= CB_INSTR_RES_SET("RES 7, (HL)", TRACE_MEM_HL),
	[CPU_OP_RES_7_A]	= CB_INSTR_RES_SET("RES 7, A", TRACE_REG_A),
	[CPU_OP_SET_0_B]	= CB_INSTR_RES_SET("SET 0, B", TRACE_REG_B),
	[CPU_OP_SET_0_C]	= CB_INSTR_RES_SET("SET 0, C", TRACE_REG_C),
	[CPU_OP_SET_0_D]	= CB_INSTR_RES_SET("SET 0, D", TRACE_REG_D),
	[CPU_OP_SET_0_E]	= CB_INSTR_RES_SET("SET 0, E", TRACE_REG_E),
	[CPU_OP_SET_0_H]	= CB_INSTR_RES_SET("SET 0, H", TRACE_REG_H),
	[CPU_OP_SET_0_L]	= CB_INSTR_RES_SET("SET 0, L", TRACE_REG_L),
	[CPU_OP_SET_0_MEM_HL]	= CB_INSTR_RES_SET("SET 0, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_0_A]	= CB_INSTR_RES_SET("SET 0, A", TRACE_REG_A),
	[CPU_OP_SET_1_B]	= CB_INSTR_RES_SET("SET 1, B", TRACE_REG_B),
	[CPU_OP_SET_1_C]	= CB_INSTR_RES_SET("SET 1, C", TRACE_REG_C),
	[CPU_OP_SET_1_D]	= CB_INSTR_RES_SET("SET 1, D", TRACE_REG_D),
	[CPU_OP_SET_1_E]	= CB_INSTR_RES_SET("SET 1, E", TRACE_REG_E),
	[CPU_OP_SET_1_H]	= CB_INSTR_RES_SET("SET 1, H", TRACE_REG_H),
	[CPU_OP_SET_1_L]	= CB_INSTR_RES_SET("SET 1, L", TRACE_REG_L),
	[CPU_OP_SET_1_MEM_HL]	= CB_INSTR_RES_SET("SET 1, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_1_A]	= CB_INSTR_RES_SET("SET 1, A", TRACE_REG_A),
	[CPU_OP_SET_2_B]	= CB_INSTR_RES_SET("SET 2, B", TRACE_REG_B),
	[CPU_OP_SET_2_C]	= CB_INSTR_RES_SET("SET 2, C", TRACE_REG_C),
	[CPU_OP_SET_2_D]	= CB_INSTR_RES_SET("SET 2, D", TRACE_REG_D),
	[CPU_OP_SET_2_E]	= CB_INSTR_RES_SET("SET 2, E", TRACE_REG_E),
	[CPU_OP_SET_2_H]	= CB_INSTR_RES_SET("SET 2, H", TRACE_REG_H),
	[CPU_OP_SET_2_L]	= CB_INSTR_RES_SET("SET 2, L", TRACE_REG_L),
	[CPU_OP_SET_2_MEM_HL]	= CB_INSTR_RES_SET("SET 2, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_2_A]	= CB_INSTR_RES_SET("SET 2, A", TRACE_REG_A),
	[CPU_OP_SET_3_B]	= CB_INSTR_RES_SET("SET 3, B", TRACE_REG_B),
	[CPU_OP_SET_3_C]	= CB_INSTR_RES_SET("SET 3, C", TRACE_REG_C),
	[CPU_OP_SET_3_D]	= CB_INSTR_RES_SET("SET 3, D", TRACE_REG_D),
	[CPU_OP_SET_3_E]	= CB_INSTR_RES_SET("SET 3, E", TRACE_REG_E),
	[CPU_OP_SET_3_H]	= CB_INSTR_RES_SET("SET 3, H", TRACE_REG_H),
	[CPU_OP_SET_3_L]	= CB_INSTR_RES_SET("SET 3, L", TRACE_REG_L),
	[CPU_OP_SET_3_MEM_HL]	= CB_INSTR_RES_SET("SET 3, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_3_A]	= CB_INSTR_RES_SET("SET 3, A", TRACE_REG_A),
	[CPU_OP_SET_4_B]	= CB_INSTR_RES_SET("SET 4, B", TRACE_REG_B),
	[CPU_OP_SET_4_C]	= CB_INSTR_RES_SET("SET 4, C", TRACE_REG_C),
	[CPU_OP_SET_4_D]	= CB_INSTR_RES_SET("SET 4, D", TRACE_REG_D),
	[CPU_OP_SET_4_E]	= CB_INSTR_RES_SET("SET 4, E", TRACE_REG_E),
	[CPU_OP_SET_4_H]	= CB_INSTR_RES_SET("SET 4, H", TRACE_REG_H),
	[CPU_OP_SET_4_L]	= CB_INSTR_RES_SET("SET 4, L", TRACE_REG_L),
	[CPU_OP_SET_4_MEM_HL]	= CB_INSTR_RES_SET("SET 4, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_4_A]	= CB_INSTR_RES_SET("SET 4, A", TRACE_REG_A),
	[CPU_OP_SET_5_B]	= CB_INSTR_RES_SET("SET 5, B", TRACE_REG_B),
	[CPU_OP_SET_5_C]	= CB_INSTR_RES_SET("SET 5, C", TRACE_REG_C),
	[CPU_OP_SET_5_D]	= CB_INSTR_RES_SET("SET 5, D", TRACE_REG_D),
	[CPU_OP_SET_5_E]	= CB_INSTR_RES_SET("SET 5, E", TRACE_REG_E),
	[CPU_OP_SET_5_H]	= CB_INSTR_RES_SET("SET 5, H", TRACE_REG_H),
	[CPU_OP_SET_5_L]	= CB_INSTR_RES_SET("SET 5, L", TRACE_REG_L),
	[CPU_OP_SET_5_MEM_HL]	= CB_INSTR_RES_SET("SET 5, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_5_A]	= CB_INSTR_RES_SET("SET 5, A", TRACE_REG_A),
	[CPU_OP_SET_6_B]	= CB_INSTR_RES_SET("SET 6, B", TRACE_REG_B),
	[CPU_OP_SET_6_C]	= CB_INSTR_RES_SET("SET 6, C", TRACE_REG_C),
	[CPU_OP_SET_6_D]	= CB_INSTR_RES_SET("SET 6, D", TRACE_REG_D),
	[CPU_OP_SET_6_E]	= CB_INSTR_RES_SET("SET 6, E", TRACE_REG_E),
	[CPU_OP_SET_6_H]	= CB_INSTR_RES_SET("SET 6, H", TRACE_REG_H),
	[CPU_OP_SET_6_L]	= CB_INSTR_RES_SET("SET 6, L", TRACE_REG_L),
	[CPU_OP_SET_6_MEM_HL]	= CB_INSTR_RES_SET("SET 6, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_6_A]	= CB_INSTR_RES_SET("SET 6, A", TRACE_REG_A),
	[CPU_OP_SET_7_B]	= CB_INSTR_RES_SET("SET 7, B", TRACE_REG_B),
	[CPU_OP_SET_7_C]	= CB_INSTR_RES_SET("SET 7, C", TRACE_REG_C),
	[CPU_OP_SET_7_D]	= CB_INSTR_RES_SET("SET 7, D", TRACE_REG_D),
	[CPU_OP_SET_7_E]	= CB_INSTR_RES_SET("SET 7, E", TRACE_REG_E),
	[CPU_OP_SET_7_H]	= CB_INSTR_RES_SET("SET 7, H", TRACE_REG_H),
	[CPU_OP_SET_7_L]	= CB_INSTR_RES_SET("SET 7, L", TRACE_REG_L),
	[CPU_OP_SET_7_MEM_HL]	= CB_INSTR_RES_SET("SET 7, (HL)", TRACE_MEM_HL),
	[CPU_OP_SET_7_A]	= CB_INSTR_RES_SET("SET 7, A", TRACE_REG_A)

	// clang-format on
};

NODISCARD static uint8_t read_u8(struct agoge_core_ctx *const ctx)
{
	return agoge_core_bus_peek(ctx, ctx->disasm.res.addr + 1);
}

NODISCARD static uint16_t read_u16(struct agoge_core_ctx *const ctx)
{
	const uint8_t lo = agoge_core_bus_peek(ctx, ctx->disasm.res.addr + 1);
	const uint8_t hi = agoge_core_bus_peek(ctx, ctx->disasm.res.addr + 2);

	return (hi << 8) | lo;
}

static void format_cb_instr(struct agoge_core_ctx *const ctx,
			    const uint8_t instr)
{
	const struct cb_disasm_entry *const entry = &cb_tbl[instr];

	memcpy(ctx->disasm.res.str, entry->str, entry->str_len);
	ctx->disasm.res.len = entry->str_len;
}

static void format_instr(struct agoge_core_ctx *const ctx, const uint8_t instr)
{
	static const void *const jmp_tbl[] = { [OP_NONE] = &&op_none,
					       [OP_U8] = &&op_u8,
					       [OP_U16] = &&op_u16,
					       [OP_S8] = &&op_s8,
					       [OP_BRANCH] = &&op_branch };

	const struct disasm_entry *const entry = &op_tbl[instr];

	goto *jmp_tbl[entry->op];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

op_none:
	ctx->disasm.res.len = sprintf(ctx->disasm.res.str, entry->fmt);
	return;

op_u8:
	ctx->disasm.res.len =
		sprintf(ctx->disasm.res.str, entry->fmt, read_u8(ctx));
	return;

op_u16:
	ctx->disasm.res.len =
		sprintf(ctx->disasm.res.str, entry->fmt, read_u16(ctx));
	return;

op_s8:
	ctx->disasm.res.len =
		sprintf(ctx->disasm.res.str, entry->fmt, (int8_t)read_u8(ctx));

	return;

op_branch:
	ctx->disasm.res.len =
		sprintf(ctx->disasm.res.str, entry->fmt,
			((int8_t)read_u8(ctx)) + ctx->disasm.res.addr + 2);
	return;

#pragma GCC diagnostic pop
}

void agoge_core_disasm_single(struct agoge_core_ctx *const ctx,
			      const uint16_t addr)
{
	memset(&ctx->disasm.res, 0, sizeof(ctx->disasm.res));
	ctx->disasm.res.addr = addr;

	uint8_t instr = agoge_core_bus_peek(ctx, addr);

	if (instr == 0xCB) {
		instr = agoge_core_bus_peek(ctx, addr + 1);
		format_cb_instr(ctx, instr);
	} else {
		format_instr(ctx, instr);
	}
	LOG_TRACE(ctx, "$%04X: %s", ctx->disasm.res.addr, ctx->disasm.res.str);
}
