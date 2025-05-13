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

static void flag_upd(struct agoge_core_cpu *const cpu, const uint8_t flag,
		     const bool cond_met)
{
	if (cond_met) {
		cpu->reg.f |= flag;
	} else {
		cpu->reg.f &= ~flag;
	}
}

static void flag_zero_upd(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	flag_upd(cpu, CPU_FLAG_ZERO, !val);
}

NODISCARD static uint8_t alu_inc(struct agoge_core_cpu *const cpu, uint8_t val)
{
	cpu->reg.f &= ~CPU_FLAG_SUBTRACT;
	flag_upd(cpu, CPU_FLAG_HALF_CARRY, (val & 0x0F) == 0x0F);
	flag_zero_upd(cpu, ++val);

	return val;
}

NODISCARD static uint8_t alu_dec(struct agoge_core_cpu *const cpu, uint8_t val)
{
	cpu->reg.f |= CPU_FLAG_SUBTRACT;
	flag_upd(cpu, CPU_FLAG_HALF_CARRY, !(val & 0x0F));
	flag_zero_upd(cpu, --val);

	return val;
}

static void stack_push(struct agoge_core_cpu *const cpu, const uint16_t val)
{
	agoge_core_bus_write(cpu->bus, --cpu->reg.sp, val >> 8);
	agoge_core_bus_write(cpu->bus, --cpu->reg.sp, val & UINT8_MAX);
}

NODISCARD static uint16_t stack_pop(struct agoge_core_cpu *const cpu)
{
	const uint8_t lo = agoge_core_bus_read(cpu->bus, cpu->reg.sp++);
	const uint8_t hi = agoge_core_bus_read(cpu->bus, cpu->reg.sp++);

	return (hi << 8) | lo;
}

static uint8_t alu_sub_op(struct agoge_core_cpu *const cpu, const uint8_t val,
			  const bool carry)
{
	cpu->reg.f |= CPU_FLAG_SUBTRACT;

	const int res = cpu->reg.a - val - carry;
	const uint8_t diff = res;

	flag_zero_upd(cpu, diff);
	flag_upd(cpu, CPU_FLAG_HALF_CARRY, (cpu->reg.a ^ val ^ res) & 0x10);
	flag_upd(cpu, CPU_FLAG_CARRY, res < 0);

	return diff;
}

static void alu_cp(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	(void)alu_sub_op(cpu, val, 0);
}

static void jp_if(struct agoge_core_cpu *const cpu, const bool cond_met)
{
	const uint16_t addr = read_u16(cpu);

	if (cond_met) {
		cpu->reg.pc = addr;
	}
}

static void ret_if(struct agoge_core_cpu *const cpu, const bool cond_met)
{
	if (cond_met) {
		cpu->reg.pc = stack_pop(cpu);
	}
}

static void call_if(struct agoge_core_cpu *const cpu, const bool cond_met)
{
	const uint16_t addr = read_u16(cpu);

	if (cond_met) {
		stack_push(cpu, cpu->reg.pc);
		cpu->reg.pc = addr;
	}
}

static void jr_if(struct agoge_core_cpu *const cpu, const bool cond_met)
{
	const int8_t off = (int8_t)read_u8(cpu);

	if (cond_met) {
		cpu->reg.pc += off;
	}
}

static void alu_or(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a |= val;
	cpu->reg.f = (!cpu->reg.a) ? CPU_FLAG_ZERO : 0x00;
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

	static const void *const op_tbl[] = {
		[CPU_OP_NOP] = &&nop,
		[CPU_OP_LD_BC_U16] = &&ld_bc_u16,
		[CPU_OP_INC_BC] = &&inc_bc,
		[CPU_OP_DEC_C] = &&dec_c,
		[CPU_OP_LD_C_U8] = &&ld_c_u8,
		[CPU_OP_LD_DE_U16] = &&ld_de_u16,
		[CPU_OP_LD_MEM_DE_A] = &&ld_mem_de_a,
		[CPU_OP_INC_D] = &&inc_d,
		[CPU_OP_JR_S8] = &&jr_s8,
		[CPU_OP_INC_E] = &&inc_e,
		[CPU_OP_JR_NZ_S8] = &&jr_nz_s8,
		[CPU_OP_LD_HL_U16] = &&ld_hl_u16,
		[CPU_OP_INC_HL] = &&inc_hl,
		[CPU_OP_JR_Z_S8] = &&jr_z_s8,
		[CPU_OP_LDI_A_MEM_HL] = &&ldi_a_mem_hl,
		[CPU_OP_LD_SP_U16] = &&ld_sp_u16,
		[CPU_OP_LD_A_U8] = &&ld_a_u8,
		[CPU_OP_LD_B_A] = &&ld_b_a,
		[CPU_OP_LD_A_B] = &&ld_a_b,
		[CPU_OP_LD_A_H] = &&ld_a_h,
		[CPU_OP_LD_A_L] = &&ld_a_l,
		[CPU_OP_OR_A_C] = &&or_a_c,
		[CPU_OP_POP_BC] = &&pop_bc,
		[CPU_OP_JP_U16] = &&jp_u16,
		[CPU_OP_PUSH_BC] = &&push_bc,
		[CPU_OP_RET] = &&ret,
		[CPU_OP_CALL_U16] = &&call_u16,
		[CPU_OP_LD_MEM_FF00_U8_A] = &&ld_mem_ff00_u8_a,
		[CPU_OP_POP_HL] = &&pop_hl,
		[CPU_OP_PUSH_HL] = &&push_hl,
		[CPU_OP_LD_MEM_U16_A] = &&ld_mem_u16_a,
		[CPU_OP_LD_A_MEM_FF00_U8] = &&ld_a_mem_ff00_u8,
		[CPU_OP_POP_AF] = &&pop_af,
		[CPU_OP_DI] = &&di,
		[CPU_OP_PUSH_AF] = &&push_af,
		[CPU_OP_CP_A_U8] = &&cp_a_u8
	};

	uint8_t instr;
	unsigned int steps = run_cycles;

	DISPATCH();

nop:
	DISPATCH();

ld_bc_u16:
	cpu->reg.bc = read_u16(cpu);
	DISPATCH();

inc_bc:
	cpu->reg.bc++;
	DISPATCH();

dec_c:
	cpu->reg.c = alu_dec(cpu, cpu->reg.c);
	DISPATCH();

ld_c_u8:
	cpu->reg.c = read_u8(cpu);
	DISPATCH();

ld_de_u16:
	cpu->reg.de = read_u16(cpu);
	DISPATCH();

ld_mem_de_a:
	agoge_core_bus_write(cpu->bus, cpu->reg.de, cpu->reg.a);
	DISPATCH();

inc_d:
	cpu->reg.d = alu_inc(cpu, cpu->reg.d);
	DISPATCH();

jr_s8:
	jr_if(cpu, true);
	DISPATCH();

inc_e:
	cpu->reg.e = alu_inc(cpu, cpu->reg.e);
	DISPATCH();

jr_nz_s8:
	jr_if(cpu, !(cpu->reg.f & CPU_FLAG_ZERO));
	DISPATCH();

ld_hl_u16:
	cpu->reg.hl = read_u16(cpu);
	DISPATCH();

inc_hl:
	cpu->reg.hl++;
	DISPATCH();

jr_z_s8:
	jr_if(cpu, cpu->reg.f & CPU_FLAG_ZERO);
	DISPATCH();

ldi_a_mem_hl:
	cpu->reg.a = agoge_core_bus_read(cpu->bus, cpu->reg.hl++);
	DISPATCH();

ld_sp_u16:
	cpu->reg.sp = read_u16(cpu);
	DISPATCH();

ld_a_u8:
	cpu->reg.a = read_u8(cpu);
	DISPATCH();

ld_b_a:
	cpu->reg.b = cpu->reg.a;
	DISPATCH();

ld_a_b:
	cpu->reg.a = cpu->reg.b;
	DISPATCH();

ld_a_h:
	cpu->reg.a = cpu->reg.h;
	DISPATCH();

ld_a_l:
	cpu->reg.a = cpu->reg.l;
	DISPATCH();

or_a_c:
	alu_or(cpu, cpu->reg.c);
	DISPATCH();

pop_bc:
	cpu->reg.bc = stack_pop(cpu);
	DISPATCH();

jp_u16:
	jp_if(cpu, true);
	DISPATCH();

push_bc:
	stack_push(cpu, cpu->reg.bc);
	DISPATCH();

ret:
	ret_if(cpu, true);
	DISPATCH();

call_u16:
	call_if(cpu, true);
	DISPATCH();

ld_mem_ff00_u8_a:
	agoge_core_bus_write(cpu->bus, 0xFF00 + read_u8(cpu), cpu->reg.a);
	DISPATCH();

pop_hl:
	cpu->reg.hl = stack_pop(cpu);
	DISPATCH();

push_hl:
	stack_push(cpu, cpu->reg.hl);
	DISPATCH();

ld_mem_u16_a:
	agoge_core_bus_write(cpu->bus, read_u16(cpu), cpu->reg.a);
	DISPATCH();

ld_a_mem_ff00_u8:
	cpu->reg.a = agoge_core_bus_read(cpu->bus, 0xFF00 + read_u8(cpu));
	DISPATCH();

pop_af:
	cpu->reg.af = stack_pop(cpu);
	DISPATCH();

di:
	DISPATCH();

push_af:
	stack_push(cpu, cpu->reg.af);
	DISPATCH();

cp_a_u8:
	alu_cp(cpu, read_u8(cpu));
	DISPATCH();
}
