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

#pragma once

struct agoge_core_ctx;

/// The maximum length of a disassembly result, not counting the NULL
/// terminator.
#define AGOGE_CORE_DISASM_RES_LEN_MAX (256)

enum agoge_core_disasm_op_type {
	AGOGE_CORE_DISASM_OP_NONE = 0,
	AGOGE_CORE_DISASM_OP_U8 = 1,
	AGOGE_CORE_DISASM_OP_U16 = 2,
	AGOGE_CORE_DISASM_OP_S8 = 3,
	AGOGE_CORE_DISASM_OP_BRANCH = 4
};

enum agoge_core_disasm_trace_type {
	AGOGE_CORE_DISASM_TRACE_NONE = 0,
	AGOGE_CORE_DISASM_TRACE_REG_B = 1,
	AGOGE_CORE_DISASM_TRACE_REG_C = 2,
	AGOGE_CORE_DISASM_TRACE_REG_D = 3,
	AGOGE_CORE_DISASM_TRACE_REG_E = 4,
	AGOGE_CORE_DISASM_TRACE_REG_F = 5,
	AGOGE_CORE_DISASM_TRACE_REG_H = 6,
	AGOGE_CORE_DISASM_TRACE_REG_L = 7,
	AGOGE_CORE_DISASM_TRACE_REG_A = 8,
	AGOGE_CORE_DISASM_TRACE_REG_BC = 9,
	AGOGE_CORE_DISASM_TRACE_REG_DE = 10,
	AGOGE_CORE_DISASM_TRACE_REG_HL = 11,
	AGOGE_CORE_DISASM_TRACE_REG_AF = 12,
	AGOGE_CORE_DISASM_TRACE_REG_SP = 13,
	AGOGE_CORE_DISASM_TRACE_MEM_HL = 14
};

struct agoge_core_disasm_entry {
	const char *const fmt;
	const enum agoge_core_disasm_op_type op;
	const enum agoge_core_disasm_trace_type traces[2];
	const size_t num_traces;
};

struct agoge_core_disasm {
	struct {
		char str[AGOGE_CORE_DISASM_RES_LEN_MAX + 1];
		size_t len;
		uint16_t addr;
	} res;

	const struct agoge_core_disasm_entry *curr_trace_entry;
};

/// Disassembles a single instruction.
///
/// @param ctx The emulator context.
/// @param addr The address in the system bus to inspect.
void agoge_core_disasm_single(struct agoge_core_ctx *ctx, uint16_t addr);

/// Prepares the disassembler to trace the next instruction.
///
/// @param ctx The emulator context.
void agoge_core_disasm_trace_before(struct agoge_core_ctx *ctx);

void agoge_core_disasm_trace_after(struct agoge_core_ctx *ctx);
