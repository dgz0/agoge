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

#include "comp.h"
#include "cpu.h"
#include "cpu-defs.h"
#include "bus.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_CPU);

NODISCARD static uint8_t read_u8(struct agoge_core_cpu *const cpu)
{
	return agoge_core_bus_read(cpu->bus, cpu->reg.pc++);
}

NODISCARD static uint16_t read_u16(struct agoge_core_cpu *const cpu)
{
	const uint8_t lo = read_u8(cpu);
	const uint8_t hi = read_u8(cpu);

	return (hi << 8) | lo;
}

static void jp_if(struct agoge_core_cpu *const cpu, const bool cond_met)
{
	const uint16_t addr = read_u16(cpu);

	if (cond_met) {
		cpu->reg.pc = addr;
	}
}

void agoge_core_cpu_init(struct agoge_core_cpu *const cpu,
			 struct agoge_core_bus *const bus,
			 struct agoge_core_log *const log)
{
	cpu->bus = bus;
	cpu->log = log;

	LOG_INFO(cpu->log, "initialized");
}

void agoge_core_cpu_reset(struct agoge_core_cpu *const cpu)
{
	cpu->reg.pc = CPU_PWRUP_REG_PC;
}

void agoge_core_cpu_run(struct agoge_core_cpu *const cpu,
			const unsigned int run_cycles)
{
#define DISPATCH()                            \
	({                                    \
		if (unlikely(steps-- == 0)) { \
			return;               \
		}                             \
		instr = read_u8(cpu);         \
		goto *op_tbl[instr];          \
	})

	static const void *const op_tbl[] = { [CPU_OP_NOP] = &&nop,
					      [CPU_OP_LD_HL_U16] = &&ld_hl_u16,
					      [CPU_OP_LD_B_A] = &&ld_b_a,
					      [CPU_OP_JP_U16] = &&jp_u16 };

	uint8_t instr;
	unsigned int steps = run_cycles;

	DISPATCH();

nop:
	DISPATCH();

ld_hl_u16:
	cpu->reg.hl = read_u16(cpu);
	DISPATCH();

ld_b_a:
	cpu->reg.b = cpu->reg.a;
	DISPATCH();

jp_u16:
	jp_if(cpu, true);
	DISPATCH();
}
