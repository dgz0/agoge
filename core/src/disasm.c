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
		.fmt		= (x),				\
		.traces		= { (trace), TRACE_REG_F },	\
		.num_traces	= 2				\
	}

#define CB_INSTR_BIT(x)					\
	{						\
		.fmt		= (x),			\
		.traces		= { TRACE_REG_F },	\
		.num_traces	= 1			\
	}

#define CB_INSTR_RES_SET(x, trace)			\
	{						\
		.fmt		= (x),			\
		.traces		= { (trace) },		\
		.num_traces	= 1			\
	}

#define APPEND_STR(str, len, fmt)					\
	({								\
		memcpy(&(str)[(len)], (fmt), sizeof((fmt)) - 1);	\
		(len) += sizeof((fmt)) - 1;				\
	})

#define TRACE_NUM_SPACES (35)

#define OP_NONE		(AGOGE_CORE_DISASM_OP_NONE)
#define OP_U8		(AGOGE_CORE_DISASM_OP_U8)
#define OP_U16		(AGOGE_CORE_DISASM_OP_U16)
#define OP_S8		(AGOGE_CORE_DISASM_OP_S8)
#define OP_BRANCH	(AGOGE_CORE_DISASM_OP_BRANCH)

#define TRACE_NONE	(AGOGE_CORE_DISASM_TRACE_NONE)
#define TRACE_REG_B	(AGOGE_CORE_DISASM_TRACE_REG_B)
#define TRACE_REG_C	(AGOGE_CORE_DISASM_TRACE_REG_C)
#define TRACE_REG_D	(AGOGE_CORE_DISASM_TRACE_REG_D)
#define TRACE_REG_E	(AGOGE_CORE_DISASM_TRACE_REG_E)
#define TRACE_REG_F	(AGOGE_CORE_DISASM_TRACE_REG_F)
#define TRACE_REG_H	(AGOGE_CORE_DISASM_TRACE_REG_H)
#define TRACE_REG_L	(AGOGE_CORE_DISASM_TRACE_REG_L)
#define TRACE_REG_A	(AGOGE_CORE_DISASM_TRACE_REG_A)
#define TRACE_REG_BC	(AGOGE_CORE_DISASM_TRACE_REG_BC)
#define TRACE_REG_DE	(AGOGE_CORE_DISASM_TRACE_REG_DE)
#define TRACE_REG_HL	(AGOGE_CORE_DISASM_TRACE_REG_HL)
#define TRACE_REG_AF	(AGOGE_CORE_DISASM_TRACE_REG_AF)
#define TRACE_REG_SP	(AGOGE_CORE_DISASM_TRACE_REG_SP)
#define TRACE_MEM_HL	(AGOGE_CORE_DISASM_TRACE_MEM_HL)

// clang-format on

static const struct agoge_core_disasm_entry op_tbl[] = {
	// clang-format off

	[CPU_OP_NOP] = {
		.fmt		= "NOP",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_LD_BC_U16] = {
		.fmt		= "LD BC, $%04X",
		.op		= OP_U16,
		.traces		= { TRACE_REG_BC },
		.num_traces	= 1
	},

	[CPU_OP_LD_MEM_BC_A] = {
		.fmt		= "LD (BC), A",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_INC_BC] = {
		.fmt		= "INC BC",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_BC },
		.num_traces	= 1
	},

	[CPU_OP_INC_B] = {
		.fmt		= "INC B",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_B, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_B] = {
		.fmt		= "DEC B",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_B, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_B_U8] = {
		.fmt		= "LD B, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_B },
		.num_traces	= 1
	},

	[CPU_OP_RLCA] = {
		.fmt		= "RLCA",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_MEM_U16_SP] = {
		.fmt		= "LD ($%04X), SP",
		.op		= OP_U16,
		.traces		= { TRACE_NONE },
		.num_traces	= 1
	},

	[CPU_OP_ADD_HL_BC] = {
		.fmt		= "ADD HL, BC",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_A_MEM_BC] = {
		.fmt		= "LD A, (BC)",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A },
		.num_traces	= 1
	},

	[CPU_OP_DEC_BC] = {
		.fmt		= "DEC BC",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_BC },
		.num_traces	= 1
	},

	[CPU_OP_INC_C] = {
		.fmt		= "INC C",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_C, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_C] = {
		.fmt		= "DEC C",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_C, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_C_U8] = {
		.fmt		= "LD C, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_C },
		.num_traces	= 1
	},

	[CPU_OP_RRCA] = {
		.fmt		= "RRCA",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_DE_U16] = {
		.fmt		= "LD DE, $%04X",
		.op		= OP_U16,
		.traces		= { TRACE_REG_DE },
		.num_traces	= 1
	},

	[CPU_OP_LD_MEM_DE_A] = {
		.fmt		= "LD (DE), A",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_INC_DE] = {
		.fmt		= "INC DE",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_DE },
		.num_traces	= 1
	},

	[CPU_OP_INC_D] = {
		.fmt		= "INC D",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_D, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_D] = {
		.fmt		= "DEC D",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_D, TRACE_REG_F },
		.num_traces	= 2,
	},

	[CPU_OP_LD_D_U8] = {
		.fmt		= "LD D, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_D },
		.num_traces	= 1
	},

	[CPU_OP_RLA] = {
		.fmt		= "RLA",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_JR_S8] = {
		.fmt		= "JR $%04X",
		.op		= OP_BRANCH,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_ADD_HL_DE] = {
		.fmt		= "ADD HL, DE",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_A_MEM_DE] = {
		.fmt		= "LD A, (DE)",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A },
		.num_traces	= 1
	},

	[CPU_OP_DEC_DE] = {
		.fmt		= "DEC DE",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_DE },
		.num_traces	= 1
	},

	[CPU_OP_INC_E] = {
		.fmt		= "INC E",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_E, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_E] = {
		.fmt		= "DEC E",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_E, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_E_U8] = {
		.fmt		= "LD E, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_E },
		.num_traces	= 1
	},

	[CPU_OP_RRA] = {
		.fmt		= "RRA",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_JR_NZ_S8] = {
		.fmt		= "JR NZ, $%04X",
		.op		= OP_BRANCH,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_LD_HL_U16] = {
		.fmt		= "LD HL, $%04X",
		.op		= OP_U16,
		.traces		= { TRACE_REG_HL },
		.num_traces	= 1
	},

	[CPU_OP_LDI_MEM_HL_A] = {
		.fmt		= "LDI (HL), A",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_INC_HL] = {
		.fmt		= "INC HL",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL },
		.num_traces	= 1
	},

	[CPU_OP_INC_H] = {
		.fmt		= "INC H",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_H, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_H] = {
		.fmt		= "DEC H",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_H, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_H_U8] = {
		.fmt		= "LD H, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_H },
		.num_traces	= 1
	},

	[CPU_OP_DAA] = {
		.fmt		= "DAA",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_JR_Z_S8] = {
		.fmt		= "JR Z, $%04X",
		.op		= OP_BRANCH,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_ADD_HL_HL] = {
		.fmt		= "ADD HL, HL",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LDI_A_MEM_HL] = {
		.fmt		= "LDI A, (HL)",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A },
		.num_traces	= 1
	},

	[CPU_OP_DEC_HL] = {
		.fmt		= "DEC HL",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL },
		.num_traces	= 1
	},

	[CPU_OP_INC_L] = {
		.fmt		= "INC L",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_L, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_L] = {
		.fmt		= "DEC L",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_L, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_L_U8] = {
		.fmt		= "LD L, $%02X",
		.op		= OP_U8,
		.traces		= { TRACE_REG_L },
		.num_traces	= 1
	},

	[CPU_OP_CPL] = {
		.fmt		= "CPL",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_JR_NC_S8] = {
		.fmt		= "JR NC, $%04X",
		.op		= OP_BRANCH,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_LD_SP_U16] = {
		.fmt		= "LD SP, $%04X",
		.op		= OP_U16,
		.traces		= { TRACE_REG_SP },
		.num_traces	= 1
	},

	[CPU_OP_LDD_MEM_HL_A] = {
		.fmt		= "LDD (HL), A",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL },
		.num_traces	= 1
	},

	[CPU_OP_INC_SP] = {
		.fmt		= "INC SP",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_SP },
		.num_traces	= 1
	},

	[CPU_OP_INC_MEM_HL] = {
		.fmt		= "INC (HL)",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_DEC_MEM_HL] = {
		.fmt		= "DEC (HL)",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_LD_MEM_HL_U8] = {
		.fmt		= "LD (HL), $%02X",
		.op		= OP_NONE,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_SCF] = {
		.fmt		= "SCF",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_F },
		.num_traces	= 1
	},

	[CPU_OP_JR_C_S8] = {
		.fmt		= "JR C, $%04X",
		.op		= OP_BRANCH,
		.traces		= { TRACE_NONE },
		.num_traces	= 0
	},

	[CPU_OP_ADD_HL_SP] = {
		.fmt		= "ADD HL, SP",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_HL, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LDD_A_MEM_HL] = {
		.fmt		= "LDD A, (HL)",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A },
		.num_traces	= 1
	},

	[CPU_OP_DEC_SP] = {
		.fmt		= "DEC SP",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_SP },
		.num_traces	= 1
	},

	[CPU_OP_INC_A] = {
		.fmt		= "INC A",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_DEC_A] = {
		.fmt		= "DEC A",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A, TRACE_REG_F },
		.num_traces	= 2
	},

	[CPU_OP_LD_A_U8] = {
		.fmt		= "LD A, $%02X",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_A },
		.num_traces	= 1
	},

	[CPU_OP_CCF] = {
		.fmt		= "CCF",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_F },
		.num_traces	= 1
	},

	[CPU_OP_LD_B_B] = {
		.fmt		= "LD B, B",
		.op		= OP_NONE,
		.traces		= { TRACE_REG_B },
		.num_traces	= 1
	},

	[CPU_OP_LD_B_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, C" },
	[CPU_OP_LD_B_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, D" },
	[CPU_OP_LD_B_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, E" },
	[CPU_OP_LD_B_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, H" },
	[CPU_OP_LD_B_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, L" },
	[CPU_OP_LD_B_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD B, (HL)" },
	[CPU_OP_LD_B_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD B, A" },
	[CPU_OP_LD_C_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, B" },
	[CPU_OP_LD_C_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, C" },
	[CPU_OP_LD_C_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, D" },
	[CPU_OP_LD_C_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, E" },
	[CPU_OP_LD_C_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, H" },
	[CPU_OP_LD_C_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, L" },
	[CPU_OP_LD_C_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD C, (HL)" },
	[CPU_OP_LD_C_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD C, A" },
	[CPU_OP_LD_D_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, B" },
	[CPU_OP_LD_D_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, C" },
	[CPU_OP_LD_D_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, D" },
	[CPU_OP_LD_D_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, E" },
	[CPU_OP_LD_D_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, H" },
	[CPU_OP_LD_D_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, L" },
	[CPU_OP_LD_D_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD D, (HL)" },
	[CPU_OP_LD_D_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD D, A" },
	[CPU_OP_LD_E_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, B" },
	[CPU_OP_LD_E_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, C" },
	[CPU_OP_LD_E_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, D" },
	[CPU_OP_LD_E_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, E" },
	[CPU_OP_LD_E_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, H" },
	[CPU_OP_LD_E_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, L" },
	[CPU_OP_LD_E_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD E, (HL)" },
	[CPU_OP_LD_E_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD E, A" },
	[CPU_OP_LD_H_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, B" },
	[CPU_OP_LD_H_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, C" },
	[CPU_OP_LD_H_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, D" },
	[CPU_OP_LD_H_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, E" },
	[CPU_OP_LD_H_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, H" },
	[CPU_OP_LD_H_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, L" },
	[CPU_OP_LD_H_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD H, (HL)" },
	[CPU_OP_LD_H_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD H, A" },
	[CPU_OP_LD_L_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, B" },
	[CPU_OP_LD_L_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, C" },
	[CPU_OP_LD_L_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, D" },
	[CPU_OP_LD_L_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, E" },
	[CPU_OP_LD_L_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, H" },
	[CPU_OP_LD_L_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, L" },
	[CPU_OP_LD_L_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD L, (HL)" },
	[CPU_OP_LD_L_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD L, A" },
	[CPU_OP_LD_MEM_HL_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), B" },
	[CPU_OP_LD_MEM_HL_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), C" },
	[CPU_OP_LD_MEM_HL_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), D" },
	[CPU_OP_LD_MEM_HL_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), E" },
	[CPU_OP_LD_MEM_HL_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), H" },
	[CPU_OP_LD_MEM_HL_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), L" },
	[CPU_OP_LD_MEM_HL_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD (HL), A" },
	[CPU_OP_LD_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, B" },
	[CPU_OP_LD_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, C" },
	[CPU_OP_LD_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, D" },
	[CPU_OP_LD_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, E" },
	[CPU_OP_LD_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, H" },
	[CPU_OP_LD_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, L" },
	[CPU_OP_LD_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "LD A, (HL)" },
	[CPU_OP_LD_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "LD A, A" },
	[CPU_OP_ADD_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, B" },
	[CPU_OP_ADD_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, C" },
	[CPU_OP_ADD_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, D" },
	[CPU_OP_ADD_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, E" },
	[CPU_OP_ADD_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, H" },
	[CPU_OP_ADD_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, L" },
	[CPU_OP_ADD_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "ADD A, (HL)" },
	[CPU_OP_ADD_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADD A, A" },
	[CPU_OP_ADC_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, B" },
	[CPU_OP_ADC_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, C" },
	[CPU_OP_ADC_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, D" },
	[CPU_OP_ADC_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, E" },
	[CPU_OP_ADC_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, H" },
	[CPU_OP_ADC_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, L" },
	[CPU_OP_ADC_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "ADC A, (HL)" },
	[CPU_OP_ADC_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "ADC A, A" },
	[CPU_OP_SUB_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, B" },
	[CPU_OP_SUB_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, C" },
	[CPU_OP_SUB_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, D" },
	[CPU_OP_SUB_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, E" },
	[CPU_OP_SUB_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, H" },
	[CPU_OP_SUB_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, L" },
	[CPU_OP_SUB_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "SUB A, (HL)" },
	[CPU_OP_SUB_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SUB A, A" },
	[CPU_OP_SBC_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, B" },
	[CPU_OP_SBC_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, C" },
	[CPU_OP_SBC_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, D" },
	[CPU_OP_SBC_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, E" },
	[CPU_OP_SBC_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, H" },
	[CPU_OP_SBC_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, L" },
	[CPU_OP_SBC_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "SBC A, (HL)" },
	[CPU_OP_SBC_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "SBC A, A" },
	[CPU_OP_AND_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, B" },
	[CPU_OP_AND_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, C" },
	[CPU_OP_AND_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, D" },
	[CPU_OP_AND_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, E" },
	[CPU_OP_AND_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, H" },
	[CPU_OP_AND_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, L" },
	[CPU_OP_AND_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "AND A, (HL)" },
	[CPU_OP_AND_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "AND A, A" },
	[CPU_OP_XOR_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, B" },
	[CPU_OP_XOR_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, C" },
	[CPU_OP_XOR_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, D" },
	[CPU_OP_XOR_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, E" },
	[CPU_OP_XOR_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, H" },
	[CPU_OP_XOR_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, L" },
	[CPU_OP_XOR_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				  .fmt = "XOR A, (HL)" },
	[CPU_OP_XOR_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "XOR A, A" },
	[CPU_OP_OR_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, B" },
	[CPU_OP_OR_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, C" },
	[CPU_OP_OR_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, D" },
	[CPU_OP_OR_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, E" },
	[CPU_OP_OR_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, H" },
	[CPU_OP_OR_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "OR A, (HL)" },
	[CPU_OP_OR_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, L" },
	[CPU_OP_OR_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "OR A, A" },
	[CPU_OP_CP_A_B] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, B" },
	[CPU_OP_CP_A_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, C" },
	[CPU_OP_CP_A_D] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, D" },
	[CPU_OP_CP_A_E] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, E" },
	[CPU_OP_CP_A_H] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, H" },
	[CPU_OP_CP_A_L] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, L" },
	[CPU_OP_CP_A_MEM_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				 .fmt = "CP A, (HL)" },
	[CPU_OP_CP_A_A] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "CP A, A" },
	[CPU_OP_RET_NZ] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RET NZ" },
	[CPU_OP_POP_BC] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "POP BC" },
	[CPU_OP_JP_NZ_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
			       .fmt = "JP NZ, $%04X" },
	[CPU_OP_JP_U16] = { .op = AGOGE_CORE_DISASM_OP_U16, .fmt = "JP $%04X" },
	[CPU_OP_CALL_NZ_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
				 .fmt = "CALL NZ, $%04X" },
	[CPU_OP_PUSH_BC] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "PUSH BC" },
	[CPU_OP_ADD_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			      .fmt = "ADD A, $%02X" },
	[CPU_OP_RST_00] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $00" },
	[CPU_OP_RET_Z] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RET Z" },
	[CPU_OP_RET] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RET" },
	[CPU_OP_JP_Z_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
			      .fmt = "JP Z, $%04X" },
	[CPU_OP_CALL_Z_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
				.fmt = "CALL Z, $%04X" },
	[CPU_OP_CALL_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
			      .fmt = "CALL $%04X" },
	[CPU_OP_ADC_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			      .fmt = "ADC A, $%02X" },
	[CPU_OP_RST_08] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $08" },
	[CPU_OP_RET_NC] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RET NC" },
	[CPU_OP_POP_DE] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "POP DE" },
	[CPU_OP_JP_NC_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
			       .fmt = "JP NC, $%04X" },
	[CPU_OP_CALL_NC_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
				 .fmt = "CALL NC, $%04X" },
	[CPU_OP_PUSH_DE] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "PUSH DE" },
	[CPU_OP_SUB_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			      .fmt = "SUB A, $%02X" },
	[CPU_OP_RST_10] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $10" },
	[CPU_OP_RET_C] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RET C" },
	[CPU_OP_RETI] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RETI" },
	[CPU_OP_JP_C_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
			      .fmt = "JP C, $%04X" },
	[CPU_OP_CALL_C_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
				.fmt = "CALL C, $%04X" },
	[CPU_OP_SBC_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			      .fmt = "SBC A, $%02X" },
	[CPU_OP_RST_18] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $18" },
	[CPU_OP_LD_MEM_FF00_U8_A] = { .op = AGOGE_CORE_DISASM_OP_U8,
				      .fmt = "LD (FF00+$%02X), A" },
	[CPU_OP_POP_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "POP HL" },
	[CPU_OP_LD_MEM_FF00_C_A] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				     .fmt = "LD (FF00+C), A" },
	[CPU_OP_PUSH_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "PUSH HL" },
	[CPU_OP_AND_A_U8] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			      .fmt = "AND A, $%02X" },
	[CPU_OP_RST_20] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $20" },
	[CPU_OP_ADD_SP_S8] = { .op = AGOGE_CORE_DISASM_OP_S8,
			       .fmt = "ADD SP, %d" },
	[CPU_OP_JP_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "JP (HL)" },
	[CPU_OP_LD_MEM_U16_A] = { .op = AGOGE_CORE_DISASM_OP_U16,
				  .fmt = "LD ($%04X), A" },
	[CPU_OP_LD_A_MEM_FF00_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
				      .fmt = "LD A, (FF00+$%02X)" },

	[CPU_OP_LD_A_MEM_FF00_C] = { .op = AGOGE_CORE_DISASM_OP_NONE,
				     .fmt = "LD A, (FF00+C)" },
	[CPU_OP_POP_AF] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "POP AF" },
	[CPU_OP_DI] = { .fmt = "DI", .op = OP_NONE, .traces = { TRACE_NONE }, .num_traces = 0 },
	[CPU_OP_PUSH_AF] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			     .fmt = "PUSH AF" },
	[CPU_OP_OR_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			     .fmt = "OR A, $%02X" },
	[CPU_OP_RST_30] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $30" },
	[CPU_OP_LD_HL_SP_S8] = { .op = AGOGE_CORE_DISASM_OP_S8,
				 .fmt = "LD HL, SP+%d" },
	[CPU_OP_LD_SP_HL] = { .op = AGOGE_CORE_DISASM_OP_NONE,
			      .fmt = "LD SP, HL" },
	[CPU_OP_LD_A_MEM_U16] = { .op = AGOGE_CORE_DISASM_OP_U16,
				  .fmt = "LD A, ($%04X)" },
	[CPU_OP_EI] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "EI" },
	[CPU_OP_XOR_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			      .fmt = "XOR A, $%02X" },
	[CPU_OP_RST_28] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $28" },
	[CPU_OP_CP_A_U8] = { .op = AGOGE_CORE_DISASM_OP_U8,
			     .fmt = "CP A, $%02X" },
	[CPU_OP_RST_38] = { .op = AGOGE_CORE_DISASM_OP_NONE, .fmt = "RST $38" },

	// clang-format on
};

static const struct agoge_core_disasm_entry cb_tbl[] = {
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

static void append_expanded_flag(struct agoge_core_ctx *const ctx)
{
#define FORMAT(...)                     \
	ctx->disasm.res.len += sprintf( \
		&ctx->disasm.res.str[ctx->disasm.res.len], __VA_ARGS__)

	char dst[sizeof("!Z !N !H !C")];
	memset(dst, 0, sizeof(dst));

	size_t pos = 0;

	static const char flags[] = { 'Z', 'N', 'H', 'C' };

	for (unsigned int ctr = 0; ctr < 4; ++ctr) {
		if (!(ctx->cpu.reg.f & (CPU_FLAG_ZERO >> ctr))) {
			dst[pos++] = '!';
		}
		dst[pos++] = flags[ctr];
		dst[pos++] = ' ';
	}
	dst[--pos] = '\0';
	FORMAT(" (%s)", dst);

#undef FORMAT
}

static void format_instr(struct agoge_core_ctx *const ctx,
			 const struct agoge_core_disasm_entry *const entry)
{
	static const void *const jmp_tbl[] = {
		// clang-format off

		[OP_NONE]	= &&op_none,
		[OP_U8]		= &&op_u8,
		[OP_U16]	= &&op_u16,
		[OP_S8]		= &&op_s8,
		[OP_BRANCH]	= &&op_branch

		// clang-format on
	};

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

static void format_trace_area(struct agoge_core_ctx *const ctx)
{
	const size_t num_spaces = TRACE_NUM_SPACES - ctx->disasm.res.len;

	memset(&ctx->disasm.res.str[ctx->disasm.res.len], ' ', num_spaces);
	ctx->disasm.res.len += num_spaces;

	APPEND_STR(ctx->disasm.res.str, ctx->disasm.res.len, " ; ");
}

static void append_trace(struct agoge_core_ctx *const ctx,
			 const enum agoge_core_disasm_trace_type type)
{
#define FORMAT(...)                     \
	ctx->disasm.res.len += sprintf( \
		&ctx->disasm.res.str[ctx->disasm.res.len], __VA_ARGS__)

	static const void *const jmp_tbl[] = {
		// clang-format off

		[TRACE_REG_B]	= &&trace_reg_b,
		[TRACE_REG_C]	= &&trace_reg_c,
		[TRACE_REG_D]	= &&trace_reg_d,
		[TRACE_REG_E]	= &&trace_reg_e,
		[TRACE_REG_F]	= &&trace_reg_f,
		[TRACE_REG_H]	= &&trace_reg_h,
		[TRACE_REG_L]	= &&trace_reg_l,
		[TRACE_REG_A]	= &&trace_reg_a,
		[TRACE_REG_BC]	= &&trace_reg_bc,
		[TRACE_REG_DE]	= &&trace_reg_de,
		[TRACE_REG_HL]	= &&trace_reg_hl,
		[TRACE_REG_AF]	= &&trace_reg_af,
		[TRACE_REG_SP]	= &&trace_reg_sp,

		// clang-format on
	};

	goto *jmp_tbl[type];

trace_reg_b:
	FORMAT("B=$%02X", ctx->cpu.reg.b);
	return;

trace_reg_c:
	FORMAT("C=$%02X", ctx->cpu.reg.c);
	return;

trace_reg_d:
	FORMAT("D=$%02X", ctx->cpu.reg.d);
	return;

trace_reg_e:
	FORMAT("E=$%02X", ctx->cpu.reg.e);
	return;

trace_reg_f:
	FORMAT("F=$%02X", ctx->cpu.reg.f);
	append_expanded_flag(ctx);

	return;

trace_reg_h:
	FORMAT("H=$%02X", ctx->cpu.reg.h);
	return;

trace_reg_l:
	FORMAT("L=$%02X", ctx->cpu.reg.l);
	return;

trace_reg_a:
	FORMAT("A=$%02X", ctx->cpu.reg.a);
	return;

trace_reg_bc:
	FORMAT("BC=$%04X", ctx->cpu.reg.bc);
	return;

trace_reg_de:
	FORMAT("DE=$%04X", ctx->cpu.reg.de);
	return;

trace_reg_hl:
	FORMAT("HL=$%04X", ctx->cpu.reg.hl);
	return;

trace_reg_af:
	FORMAT("AF=$%04X", ctx->cpu.reg.af);
	return;

trace_reg_sp:
	FORMAT("SP=$%04X", ctx->cpu.reg.sp);
	return;

#undef FORMAT
}

static const struct agoge_core_disasm_entry *
get_disasm_entry(struct agoge_core_ctx *const ctx, const uint16_t addr)
{
	uint8_t instr = agoge_core_bus_peek(ctx, addr);

	if (instr == CPU_OP_PREFIX_CB) {
		instr = agoge_core_bus_peek(ctx, addr + 1);
		return &cb_tbl[instr];
	}
	return &op_tbl[instr];
}

void agoge_core_disasm_single(struct agoge_core_ctx *const ctx,
			      const uint16_t addr)
{
	memset(&ctx->disasm, 0, sizeof(ctx->disasm));
	ctx->disasm.res.addr = addr;

	format_instr(ctx, get_disasm_entry(ctx, addr));
}

void agoge_core_disasm_trace_before(struct agoge_core_ctx *const ctx)
{
	memset(&ctx->disasm, 0, sizeof(ctx->disasm));
	ctx->disasm.res.addr = ctx->cpu.reg.pc;

	const struct agoge_core_disasm_entry *const entry =
		get_disasm_entry(ctx, ctx->cpu.reg.pc);

	format_instr(ctx, entry);

	if (!entry->num_traces) {
		return;
	}

	ctx->disasm.curr_trace_entry = entry;
	format_trace_area(ctx);
}

void agoge_core_disasm_trace_after(struct agoge_core_ctx *const ctx)
{
	if (!ctx->disasm.curr_trace_entry) {
		LOG_TRACE(ctx, "$%04X: %s", ctx->disasm.res.addr,
			  ctx->disasm.res.str);
		return;
	}

	const struct agoge_core_disasm_entry *entry =
		ctx->disasm.curr_trace_entry;

	append_trace(ctx, entry->traces[0]);

	if (entry->num_traces - 1) {
		for (size_t i = 1; i < entry->num_traces; ++i) {
			APPEND_STR(ctx->disasm.res.str, ctx->disasm.res.len,
				   ", ");
			append_trace(ctx, entry->traces[i]);
		}
	}
	LOG_TRACE(ctx, "$%04X: %s", ctx->disasm.res.addr, ctx->disasm.res.str);
}
