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
#include <stddef.h>

#include "agoge-cpu.h"
#include "agoge-bus.h"
#include "agoge-log.h"

static uint8_t read_u8(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);
	return agoge_bus_read(cpu->bus, cpu->reg.pc++);
}

void agoge_cpu_step(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);

	const uint8_t instr = read_u8(cpu);

	LOG_ERR(cpu->bus->log,
		"Illegal instruction $%02X trapped at program counter $%04X.",
		instr, cpu->reg.pc);
}
