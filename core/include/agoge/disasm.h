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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>
#include "cpu.h"

#define AGOGE_DISASM_LEN_MAX (512)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct agoge_disasm {
	struct {
		char str[AGOGE_DISASM_LEN_MAX + 1];
		size_t len;
	} res;

	struct {
		uint16_t pc;
		unsigned int num_traces;
		const unsigned int *traces;
	} state;

	struct agoge_cpu *cpu;
	bool send_to_logger;
};

#pragma GCC diagnostic pop

void agoge_disasm_trace_before(struct agoge_disasm *disasm);
void agoge_disasm_trace_after(struct agoge_disasm *disasm);

#ifdef __cplusplus
}
#endif // __cplusplus
