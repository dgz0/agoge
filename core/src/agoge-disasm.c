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

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "agoge/disasm.h"
#include "agoge-bus.h"
#include "agoge-compiler.h"
#include "agoge-cpu-defs.h"
#include "agoge-log.h"

#define MAX_NUM_TRACES (5)

// The number of spaces to append to the disassembly result for the
// register/memory trace region.
#define TRACE_NUM_SPACES (35)

// The character used to denote the start of the register/memory trace region.
#define TRACE_START_CHAR ("; ")

// The character used to separate multiple traces.
#define TRACE_DELIM_CHAR (", ")

#define OP_NONE (0)
#define OP_U8 (1)
#define OP_U16 (2)
#define OP_S8 (3)

#define TRACE_NONE (0)
#define TRACE_REG_C (1)
#define TRACE_REG_A (2)
#define TRACE_REG_DE (3)
#define TRACE_REG_HL (4)
#define TRACE_REG_HL_SUB (5)
#define TRACE_REG_B (6)
#define TRACE_REG_E (7)
#define TRACE_REG_F (8)
#define TRACE_REG_MEM_DE (9)
#define TRACE_REG_D (10)
#define TRACE_REG_SP (11)
#define TRACE_REG_MEM_U16 (12)
#define TRACE_REG_AF (13)
#define TRACE_REG_BC (14)
#define TRACE_REG_MEM_HL (15)
#define TRACE_REG_L (16)
#define TRACE_REG_H (17)

#define LD_R8_R8(instr, trace)                                              \
	{                                                                   \
		.fmt = (instr), .op = OP_NONE, .num_traces = 1, .traces = { \
			(trace)                                             \
		}                                                           \
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct disasm_entry {
	const char *const fmt;
	const unsigned int op;
	const unsigned int num_traces;
	const unsigned int traces[MAX_NUM_TRACES];
};

#pragma GCC diagnostic pop

static const struct disasm_entry op_tbl[] = {
	[CPU_OP_NOP] = { .fmt = "NOP",
			 .op = OP_NONE,
			 .num_traces = 0,
			 .traces = { TRACE_NONE } },

	[CPU_OP_LD_MEM_DE_A] = { .fmt = "LD (DE), A",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_MEM_DE } },

	[CPU_OP_LD_B_B] = LD_R8_R8("LD B, B", TRACE_REG_B),
	[CPU_OP_LD_B_C] = LD_R8_R8("LD B, C", TRACE_REG_B),
	[CPU_OP_LD_B_D] = LD_R8_R8("LD B, D", TRACE_REG_B),
	[CPU_OP_LD_B_E] = LD_R8_R8("LD B, E", TRACE_REG_B),
	[CPU_OP_LD_B_H] = LD_R8_R8("LD B, H", TRACE_REG_B),
	[CPU_OP_LD_B_L] = LD_R8_R8("LD B, L", TRACE_REG_B),
	[CPU_OP_LD_B_MEM_HL] = LD_R8_R8("LD B, (HL)", TRACE_REG_B),
	[CPU_OP_LD_B_A] = LD_R8_R8("LD B, A", TRACE_REG_B),
	[CPU_OP_LD_C_B] = LD_R8_R8("LD C, B", TRACE_REG_C),
	[CPU_OP_LD_C_C] = LD_R8_R8("LD C, C", TRACE_REG_C),
	[CPU_OP_LD_C_D] = LD_R8_R8("LD C, D", TRACE_REG_C),
	[CPU_OP_LD_C_E] = LD_R8_R8("LD C, E", TRACE_REG_C),
	[CPU_OP_LD_C_H] = LD_R8_R8("LD C, H", TRACE_REG_C),
	[CPU_OP_LD_C_L] = LD_R8_R8("LD C, L", TRACE_REG_C),
	[CPU_OP_LD_C_MEM_HL] = LD_R8_R8("LD C, (HL)", TRACE_REG_C),
	[CPU_OP_LD_C_A] = LD_R8_R8("LD C, A", TRACE_REG_C),
	[CPU_OP_LD_D_B] = LD_R8_R8("LD D, B", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, C", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, D", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, E", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, H", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, L", TRACE_REG_D),
	[CPU_OP_LD_D_C] = LD_R8_R8("LD D, (HL)", TRACE_REG_D),
	[CPU_OP_LD_D_MEM_HL] = LD_R8_R8("LD D, A", TRACE_REG_D),
	[CPU_OP_LD_D_A] = LD_R8_R8("LD D, A", TRACE_REG_D),
	[CPU_OP_LD_E_B] = LD_R8_R8("LD E, B", TRACE_REG_E),
	[CPU_OP_LD_E_C] = LD_R8_R8("LD E, C", TRACE_REG_E),
	[CPU_OP_LD_E_D] = LD_R8_R8("LD E, D", TRACE_REG_E),
	[CPU_OP_LD_E_E] = LD_R8_R8("LD E, E", TRACE_REG_E),
	[CPU_OP_LD_E_H] = LD_R8_R8("LD E, H", TRACE_REG_E),
	[CPU_OP_LD_E_L] = LD_R8_R8("LD E, L", TRACE_REG_E),
	[CPU_OP_LD_E_MEM_HL] = LD_R8_R8("LD E, (HL)", TRACE_REG_E),
	[CPU_OP_LD_E_A] = LD_R8_R8("LD E, A", TRACE_REG_E),

	[CPU_OP_INC_C] = { .fmt = "INC C",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_C, TRACE_REG_F } },

	[CPU_OP_DEC_C] = { .fmt = "DEC C",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_C, TRACE_REG_F } },

	[CPU_OP_INC_A] = { .fmt = "INC A",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_DEC_A] = { .fmt = "DEC A",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_DEC_SP] = { .fmt = "DEC SP",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_SP } },

	[CPU_OP_DEC_L] = { .fmt = "DEC L",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_L, TRACE_REG_F } },

	[CPU_OP_DEC_B] = { .fmt = "DEC B",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_B, TRACE_REG_F } },

	[CPU_OP_LD_C_U8] = { .fmt = "LD C, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_C } },

	[CPU_OP_LD_DE_U16] = { .fmt = "LD DE, $%04X",
			       .op = OP_U16,
			       .num_traces = 1,
			       .traces = { TRACE_REG_DE } },

	[CPU_OP_INC_D] = { .fmt = "INC D",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_D, TRACE_REG_F } },

	[CPU_OP_LD_D_U8] = { .fmt = "LD D, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_D } },

	[CPU_OP_INC_L] = { .fmt = "INC L",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_L, TRACE_REG_F } },

	[CPU_OP_INC_E] = { .fmt = "INC E",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_E, TRACE_REG_F } },

	[CPU_OP_DEC_E] = { .fmt = "DEC E",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_E, TRACE_REG_F } },

	[CPU_OP_JR_NZ_S8] = { .fmt = "JR NZ, $%04X",
			      .op = OP_S8,
			      .num_traces = 0,
			      .traces = { TRACE_NONE } },

	[CPU_OP_LD_HL_U16] = { .fmt = "LD HL, $%04X",
			       .op = OP_U16,
			       .num_traces = 1,
			       .traces = { TRACE_REG_HL } },

	[CPU_OP_LDI_A_MEM_HL] = { .fmt = "LDI A, (HL)",
				  .op = OP_NONE,
				  .num_traces = 3,
				  .traces = { TRACE_REG_HL_SUB, TRACE_REG_A,
					      TRACE_REG_HL } },

	[CPU_OP_LD_A_B] = { .fmt = "LD A, B",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_JP_U16] = { .fmt = "JP $%04X",
			    .op = OP_U16,
			    .num_traces = 0,
			    .traces = { TRACE_NONE } },

	[CPU_OP_LD_B_A] = { .fmt = "LD B, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_B } },

	[CPU_OP_DI] = { .fmt = "DI",
			.op = OP_NONE,
			.num_traces = 0,
			.traces = { TRACE_NONE } },

	[CPU_OP_LD_SP_U16] = { .fmt = "LD SP, $%04X",
			       .op = OP_U16,
			       .num_traces = 1,
			       .traces = { TRACE_REG_SP } },

	[CPU_OP_LD_MEM_U16_A] = { .fmt = "LD ($%04X), A",
				  .op = OP_U16,
				  .num_traces = 0,
				  .traces = { TRACE_NONE } },

	[CPU_OP_LD_A_U8] = { .fmt = "LD A, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_A } },

	[LD_MEM_FF00_U8_A] = { .fmt = "LD ($FF00+$%02X), A",
			       .op = OP_U8,
			       .num_traces = 0,
			       .traces = { TRACE_NONE } },

	[CPU_OP_CALL_U16] = { .fmt = "CALL $%04X",
			      .op = OP_U16,
			      .num_traces = 1,
			      .traces = { TRACE_REG_SP } },

	[CPU_OP_LD_A_L] = { .fmt = "LD A, L",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_LD_A_H] = { .fmt = "LD A, H",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_JR_S8] = { .fmt = "JR $%04X",
			   .op = OP_S8,
			   .num_traces = 0,
			   .traces = { TRACE_NONE } },

	[CPU_OP_RET] = { .fmt = "RET",
			 .op = OP_NONE,
			 .num_traces = 1,
			 .traces = { TRACE_REG_SP } },

	[CPU_OP_PUSH_HL] = { .fmt = "PUSH HL",
			     .op = OP_NONE,
			     .num_traces = 1,
			     .traces = { TRACE_REG_SP } },

	[CPU_OP_POP_HL] = { .fmt = "POP HL",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_HL, TRACE_REG_SP } },

	[CPU_OP_POP_DE] = { .fmt = "POP DE",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_HL, TRACE_REG_SP } },

	[CPU_OP_PUSH_AF] = { .fmt = "PUSH AF",
			     .op = OP_NONE,
			     .num_traces = 1,
			     .traces = { TRACE_REG_SP } },

	[CPU_OP_INC_HL] = { .fmt = "INC HL",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_HL } },

	[CPU_OP_POP_AF] = { .fmt = "POP AF",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_AF, TRACE_REG_SP } },

	[CPU_OP_PUSH_BC] = { .fmt = "PUSH BC",
			     .op = OP_NONE,
			     .num_traces = 1,
			     .traces = { TRACE_REG_SP } },

	[CPU_OP_PUSH_DE] = { .fmt = "PUSH DE",
			     .op = OP_NONE,
			     .num_traces = 1,
			     .traces = { TRACE_REG_SP } },

	[CPU_OP_POP_BC] = { .fmt = "POP BC",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_BC, TRACE_REG_SP } },

	[CPU_OP_LD_BC_U16] = { .fmt = "LD BC, $%04X",
			       .op = OP_U16,
			       .num_traces = 1,
			       .traces = { TRACE_REG_BC } },

	[CPU_OP_INC_BC] = { .fmt = "INC BC",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_BC } },

	[CPU_OP_OR_A_B] = { .fmt = "OR A, B",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_OR_A_C] = { .fmt = "OR A, C",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_OR_A_MEM_HL] = { .fmt = "OR A, (HL)",
				 .op = OP_NONE,
				 .num_traces = 2,
				 .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_OR_A_A] = { .fmt = "OR A, A",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_JR_Z_S8] = { .fmt = "JR Z, $%04X",
			     .op = OP_S8,
			     .num_traces = 0,
			     .traces = { TRACE_NONE } },

	[CPU_OP_LD_A_MEM_FF00_U8] = { .fmt = "LD A, ($FF00+$%02X)",
				      .op = OP_U8,
				      .num_traces = 1,
				      .traces = { TRACE_REG_A } },

	[CPU_OP_CP_A_U8] = { .fmt = "CP A, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_F } },

	[CPU_OP_ADD_A_U8] = { .fmt = "ADD A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LD_A_MEM_U16] = { .fmt = "LD A, ($%04X)",
				  .op = OP_U16,
				  .num_traces = 1,
				  .traces = { TRACE_REG_A } },

	[CPU_OP_DEC_MEM_HL] = { .fmt = "DEC (HL)",
				.op = OP_NONE,
				.num_traces = 1,
				.traces = { TRACE_REG_F } },

	[CPU_OP_AND_A_U8] = { .fmt = "AND A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_CALL_NZ_U16] = { .fmt = "CALL NZ, $%04X",
				 .op = OP_U16,
				 .num_traces = 1,
				 .traces = { TRACE_REG_SP } },

	[CPU_OP_LD_B_U8] = { .fmt = "LD B, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_B } },

	[CPU_OP_LD_H_U8] = { .fmt = "LD H, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_H } },

	[CPU_OP_LD_MEM_HL_B] = { .fmt = "LD (HL), B",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_MEM_HL } },

	[CPU_OP_LD_MEM_HL_C] = { .fmt = "LD (HL), C",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_MEM_HL } },

	[CPU_OP_LD_MEM_HL_D] = { .fmt = "LD (HL), D",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_MEM_HL } },

	[CPU_OP_LD_MEM_HL_A] = { .fmt = "LD (HL), A",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_MEM_HL } },

	[CPU_OP_INC_H] = { .fmt = "INC H",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_H, TRACE_REG_F } },

	[CPU_OP_DEC_H] = { .fmt = "DEC H",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_H, TRACE_REG_F } },

	[CPU_OP_RRA] = { .fmt = "RRA",
			 .op = OP_NONE,
			 .num_traces = 2,
			 .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LD_A_MEM_DE] = { .fmt = "LD A, (DE)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_A } },

	[CPU_OP_INC_DE] = { .fmt = "INC DE",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_DE } },

	[CPU_OP_XOR_A_C] = { .fmt = "XOR A, C",
			     .op = OP_NONE,
			     .num_traces = 2,
			     .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_XOR_A_L] = { .fmt = "XOR A, L",
			     .op = OP_NONE,
			     .num_traces = 2,
			     .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LDI_MEM_HL_A] = { .fmt = "LDI (HL), A",
				  .op = OP_NONE,
				  .num_traces = 2,
				  .traces = { TRACE_REG_HL_SUB,
					      TRACE_REG_HL } },

	[CPU_OP_SUB_A_U8] = { .fmt = "SUB A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LDD_HL_A] = { .fmt = "LDD (HL), A",
			      .op = OP_NONE,
			      .num_traces = 1,
			      .traces = { TRACE_REG_HL_SUB, TRACE_REG_HL } },

	[CPU_OP_LD_B_MEM_HL] = { .fmt = "LD B, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_B } },

	[CPU_OP_LD_C_MEM_HL] = { .fmt = "LD C, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_C } },

	[CPU_OP_LD_D_MEM_HL] = { .fmt = "LD D, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_D } },

	[CPU_OP_XOR_A_MEM_HL] = { .fmt = "XOR A, (HL)",
				  .op = OP_NONE,
				  .num_traces = 2,
				  .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_ADD_HL_DE] = { .fmt = "ADD HL, DE",
			       .op = OP_NONE,
			       .num_traces = 2,
			       .traces = { TRACE_REG_HL, TRACE_REG_F } },

	[CPU_OP_ADD_HL_BC] = { .fmt = "ADD HL, BC",
			       .op = OP_NONE,
			       .num_traces = 2,
			       .traces = { TRACE_REG_HL, TRACE_REG_F } },

	[CPU_OP_ADD_HL_HL] = { .fmt = "ADD HL, HL",
			       .op = OP_NONE,
			       .num_traces = 2,
			       .traces = { TRACE_REG_HL, TRACE_REG_F } },

	[CPU_OP_JR_NC_S8] = { .fmt = "JR NC, $%04X",
			      .op = OP_S8,
			      .num_traces = 0,
			      .traces = { TRACE_NONE } },

	[CPU_OP_LD_E_A] = { .fmt = "LD E, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_E } },

	[CPU_OP_LD_H_D] = { .fmt = "LD H, D",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_H } },

	[CPU_OP_XOR_A_U8] = { .fmt = "XOR A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LD_A_C] = { .fmt = "LD A, C",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_LD_C_A] = { .fmt = "LD C, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_C } },

	[CPU_OP_LD_A_D] = { .fmt = "LD A, D",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_LD_D_A] = { .fmt = "LD D, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_D } },

	[CPU_OP_LD_A_E] = { .fmt = "LD A, E",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_A } },

	[CPU_OP_ADC_A_U8] = { .fmt = "ADC A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_RET_NC] = { .fmt = "RET NC",
			    .op = OP_NONE,
			    .num_traces = 0,
			    .traces = { TRACE_NONE } },

	[CPU_OP_RET_Z] = { .fmt = "RET Z",
			   .op = OP_NONE,
			   .num_traces = 0,
			   .traces = { TRACE_NONE } },

	[CPU_OP_LD_L_MEM_HL] = { .fmt = "LD L, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_L } },

	[CPU_OP_LD_E_MEM_HL] = { .fmt = "LD E, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_E } },

	[CPU_OP_LD_L_A] = { .fmt = "LD L, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_L } },

	[CPU_OP_JP_HL] = { .fmt = "JP (HL)",
			   .op = OP_NONE,
			   .num_traces = 0,
			   .traces = { TRACE_NONE } },

	[CPU_OP_JP_NZ_U16] = { .fmt = "JP NZ, $%04X",
			       .op = OP_U16,
			       .num_traces = 0,
			       .traces = { TRACE_NONE } },

	[CPU_OP_CP_A_E] = { .fmt = "CP A, E",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_INC_B] = { .fmt = "INC B",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_B, TRACE_REG_F } },

	[CPU_OP_LD_H_A] = { .fmt = "LD H, A",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_H } },

	[CPU_OP_LD_A_MEM_HL] = { .fmt = "LD A, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_A } },

	[CPU_OP_RET_C] = { .fmt = "RET C",
			   .op = OP_NONE,
			   .num_traces = 0,
			   .traces = { TRACE_NONE } },

	[CPU_OP_JR_C_S8] = { .fmt = "JR C, $%04X",
			     .op = OP_S8,
			     .num_traces = 0,
			     .traces = { TRACE_NONE } },

	[CPU_OP_DAA] = { .fmt = "DAA",
			 .op = OP_NONE,
			 .num_traces = 2,
			 .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_CPL] = { .fmt = "CPL",
			 .op = OP_NONE,
			 .num_traces = 2,
			 .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_SCF] = { .fmt = "SCF",
			 .op = OP_NONE,
			 .num_traces = 1,
			 .traces = { TRACE_REG_F } },

	[CPU_OP_LD_L_U8] = { .fmt = "LD L, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_L } },

	[CPU_OP_XOR_A_A] = { .fmt = "XOR A, A",
			     .op = OP_NONE,
			     .num_traces = 2,
			     .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_LD_E_L] = { .fmt = "LD E, L",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_E } },

	[CPU_OP_DEC_DE] = { .fmt = "DEC DE",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_DE } },

	[CPU_OP_LD_MEM_HL_E] = { .fmt = "LD (HL), E",
				 .op = OP_NONE,
				 .num_traces = 0,
				 .traces = { TRACE_NONE } },

	[CPU_OP_LD_MEM_U16_SP] = { .fmt = "LD ($%04X), SP",
				   .op = OP_U16,
				   .num_traces = 0,
				   .traces = { TRACE_NONE } },

	[CPU_OP_LD_H_MEM_HL] = { .fmt = "LD H, (HL)",
				 .op = OP_NONE,
				 .num_traces = 1,
				 .traces = { TRACE_REG_H } },

	[CPU_OP_LD_SP_HL] = { .fmt = "LD SP, HL",
			      .op = OP_NONE,
			      .num_traces = 1,
			      .traces = { TRACE_REG_SP } },

	[CPU_OP_LD_L_E] = { .fmt = "LD L, E",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_L } },

	[CPU_OP_INC_SP] = { .fmt = "INC SP",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_SP } },

	[CPU_OP_ADD_HL_SP] = { .fmt = "ADD HL, SP",
			       .op = OP_NONE,
			       .num_traces = 2,
			       .traces = { TRACE_REG_HL, TRACE_REG_F } },

	[CPU_OP_ADD_SP_S8] = { .fmt = "ADD SP, $%02X",
			       .op = OP_NONE,
			       .num_traces = 2,
			       .traces = { TRACE_REG_SP, TRACE_REG_F } },

	[CPU_OP_LD_HL_SP_S8] = { .fmt = "LD HL, SP+$%02X",
				 .op = OP_NONE,
				 .num_traces = 2,
				 .traces = { TRACE_REG_HL, TRACE_REG_F } },

	[CPU_OP_LD_MEM_HL_U8] = { .fmt = "LD (HL), $%02X",
				  .op = OP_U8,
				  .num_traces = 0,
				  .traces = { TRACE_NONE } },

	[CPU_OP_LD_E_U8] = { .fmt = "LD E, $%02X",
			     .op = OP_U8,
			     .num_traces = 1,
			     .traces = { TRACE_REG_E } },

	[CPU_OP_OR_A_U8] = { .fmt = "OR A, $%02X",
			     .op = OP_U8,
			     .num_traces = 2,
			     .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_SBC_A_U8] = { .fmt = "SBC A, $%02X",
			      .op = OP_U8,
			      .num_traces = 2,
			      .traces = { TRACE_REG_A, TRACE_REG_F } },

	[CPU_OP_DEC_BC] = { .fmt = "DEC BC",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_BC } },

	[CPU_OP_DEC_HL] = { .fmt = "DEC HL",
			    .op = OP_NONE,
			    .num_traces = 1,
			    .traces = { TRACE_REG_HL } }
};

static const struct disasm_entry cb_tbl[] = {
	[CPU_OP_RR_C] = { .fmt = "RR C",
			  .op = OP_NONE,
			  .num_traces = 2,
			  .traces = { TRACE_REG_C, TRACE_REG_F } },

	[CPU_OP_RR_E] = { .fmt = "RR E",
			  .op = OP_NONE,
			  .num_traces = 2,
			  .traces = { TRACE_REG_E, TRACE_REG_F } },

	[CPU_OP_RR_D] = { .fmt = "RR D",
			  .op = OP_NONE,
			  .num_traces = 2,
			  .traces = { TRACE_REG_D, TRACE_REG_F } },

	[CPU_OP_SRL_B] = { .fmt = "SRL B",
			   .op = OP_NONE,
			   .num_traces = 2,
			   .traces = { TRACE_REG_B, TRACE_REG_F } },

	[CPU_OP_SWAP_A] = { .fmt = "SWAP A",
			    .op = OP_NONE,
			    .num_traces = 2,
			    .traces = { TRACE_REG_A, TRACE_REG_F } }
};

static const char *get_mem_name(const uint16_t address)
{
	static const char *const names[] = { [0x0 ... 0x7] = "CART",
					     [0x8 ... 0x9] = "VRAM",
					     [0xA ... 0xB] = "EXT_RAM",
					     [0xC ... 0xD] = "WRAM",
					     [0xE ... 0xF] = "DNT" };
	return names[address >> 12];
}

static uint8_t read_u8(struct agoge_disasm *const disasm, const uint16_t pc)
{
	assert(disasm != NULL);
	return agoge_bus_get(disasm->cpu->bus, pc);
}

static uint16_t read_u16(struct agoge_disasm *const disasm, const uint16_t pc)
{
	assert(disasm != NULL);

	const uint8_t lo = read_u8(disasm, pc + 1);
	const uint8_t hi = read_u8(disasm, pc + 2);

	return (uint16_t)((hi << 8) | lo);
}

static void append_trace(struct agoge_disasm *const disasm,
			 const unsigned int trace)
{
	assert(disasm != NULL);

	switch (trace) {
	case TRACE_REG_DE:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"DE=$%04X", disasm->cpu->reg.de);
		break;

	case TRACE_REG_HL:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"HL=$%04X", disasm->cpu->reg.hl);
		break;

	case TRACE_REG_BC:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"BC=$%04X", disasm->cpu->reg.bc);
		break;

	case TRACE_REG_B:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"B=$%02X", disasm->cpu->reg.b);
		break;

	case TRACE_REG_C:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"C=$%02X", disasm->cpu->reg.c);
		break;

	case TRACE_REG_D:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"D=$%02X", disasm->cpu->reg.d);
		break;

	case TRACE_REG_H:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"H=$%02X", disasm->cpu->reg.h);
		break;

	case TRACE_REG_A:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"A=$%02X", disasm->cpu->reg.a);
		break;

	case TRACE_REG_F:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"F=$%02X", disasm->cpu->reg.f);
		break;

	case TRACE_REG_E:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"E=$%02X", disasm->cpu->reg.e);
		break;

	case TRACE_REG_L:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"L=$%02X", disasm->cpu->reg.l);
		break;

	case TRACE_REG_HL_SUB:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"SRC=$%04X", disasm->cpu->reg.hl - 1);
		break;

	case TRACE_REG_MEM_DE:
		disasm->res.len += (size_t)sprintf(
			&disasm->res.str[disasm->res.len], "DST=%s",
			get_mem_name(disasm->cpu->reg.de));
		break;

	case TRACE_REG_MEM_HL:
		disasm->res.len += (size_t)sprintf(
			&disasm->res.str[disasm->res.len], "DST=%s",
			get_mem_name(disasm->cpu->reg.hl));
		break;

	case TRACE_REG_SP:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"SP=$%04X", disasm->cpu->reg.sp);
		break;

	case TRACE_REG_AF:
		disasm->res.len +=
			(size_t)sprintf(&disasm->res.str[disasm->res.len],
					"AF=$%04X", disasm->cpu->reg.af);
		break;

	default:
		break;
	}
}

static void process_traces(struct agoge_disasm *const disasm)
{
	append_trace(disasm, disasm->state.traces[0]);

	if (disasm->state.num_traces) {
		for (unsigned int i = 1; i < disasm->state.num_traces; ++i) {
			disasm->res.len += (size_t)sprintf(
				&disasm->res.str[disasm->res.len],
				TRACE_DELIM_CHAR);
			append_trace(disasm, disasm->state.traces[i]);
		}
	}
}

static void format_trace_area(struct agoge_disasm *const disasm)
{
	const size_t num_spaces = TRACE_NUM_SPACES - disasm->res.len;

	memset(&disasm->res.str[disasm->res.len], ' ',
	       (unsigned long)num_spaces);
	disasm->res.len += num_spaces;

	memcpy(&disasm->res.str[disasm->res.len], TRACE_START_CHAR,
	       sizeof(TRACE_START_CHAR) - 1);
	disasm->res.len += sizeof(TRACE_START_CHAR) - 1;
}

void agoge_disasm_trace_before(struct agoge_disasm *const disasm)
{
	assert(disasm != NULL);

	memset(&disasm->res, 0, sizeof(disasm->res));

	disasm->state.pc = disasm->cpu->reg.pc;

	uint8_t instr = agoge_bus_get(disasm->cpu->bus, disasm->cpu->reg.pc);

	const struct disasm_entry *entry;

	if (instr == CPU_OP_PREFIX_CB) {
		return;
		instr = agoge_bus_get(disasm->cpu->bus,
				       disasm->cpu->reg.pc + 1);
		entry = &cb_tbl[instr];
	} else {
		if (instr != 0xFF)
			entry = &op_tbl[instr];
	}

	if (entry->fmt == NULL)
		return;

	disasm->state.traces = &entry->traces[0];
	disasm->state.num_traces = entry->num_traces;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"

	switch (entry->op) {
	case OP_NONE:
		disasm->res.len = (size_t)sprintf(disasm->res.str, entry->fmt);
		break;

	case OP_U8: {
		const uint8_t imm8 = read_u8(disasm, disasm->state.pc + 1);

		disasm->res.len =
			(size_t)sprintf(disasm->res.str, entry->fmt, imm8);
		break;
	}

	case OP_S8: {
		const int8_t s8 = (int8_t)read_u8(disasm, disasm->state.pc + 1);

		disasm->res.len = (size_t)sprintf(disasm->res.str, entry->fmt,
						  disasm->cpu->reg.pc + s8);
		break;
	}

	case OP_U16: {
		const uint16_t imm16 = read_u16(disasm, disasm->state.pc);

		disasm->res.len =
			(size_t)sprintf(disasm->res.str, entry->fmt, imm16);
		break;
	}

	default:
		UNREACHABLE;
	}

#pragma GCC diagnostic pop
}

void agoge_disasm_trace_after(struct agoge_disasm *const disasm)
{
	assert(disasm != NULL);

	if (!disasm->state.num_traces) {
		if (disasm->send_to_logger) {
			LOG_TRACE(disasm->cpu->bus->log, "$%04X: %s",
				  disasm->state.pc, disasm->res.str);
		}
		return;
	}

	format_trace_area(disasm);
	process_traces(disasm);

	if (disasm->send_to_logger) {
		LOG_TRACE(disasm->cpu->bus->log, "$%04X: %s", disasm->state.pc,
			  disasm->res.str);
	}
}
