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

#include <stdbool.h>

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

NODISCARD static uint8_t alu_rr_op(struct agoge_core_cpu *const cpu,
				   uint8_t val)
{
	cpu->reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);

	const bool old_carry = val & 1;
	const bool carry = cpu->reg.f & CPU_FLAG_CARRY;

	val = (val >> 1) | (carry << 7);
	flag_upd(cpu, CPU_FLAG_CARRY, old_carry);

	return val;
}

NODISCARD static uint8_t alu_rr(struct agoge_core_cpu *const cpu, uint8_t val)
{
	val = alu_rr_op(cpu, val);
	flag_zero_upd(cpu, val);

	return val;
}

NODISCARD static uint8_t alu_srl(struct agoge_core_cpu *const cpu, uint8_t val)
{
	cpu->reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	const bool carry = val & 1;

	val >>= 1;

	flag_zero_upd(cpu, val);
	flag_upd(cpu, CPU_FLAG_CARRY, carry);

	return val;
}

static void alu_add_op(struct agoge_core_cpu *const cpu, const uint8_t val,
		       const bool carry)
{
	cpu->reg.f &= ~CPU_FLAG_SUBTRACT;

	const unsigned int res = cpu->reg.a + val + carry;
	const uint8_t sum = res;

	flag_zero_upd(cpu, sum);
	flag_upd(cpu, CPU_FLAG_HALF_CARRY, (cpu->reg.a ^ val ^ res) & 0x10);
	flag_upd(cpu, CPU_FLAG_CARRY, res > UINT8_MAX);

	cpu->reg.a = sum;
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

static void alu_add(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	alu_add_op(cpu, val, 0);
}

static void alu_adc(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	alu_add_op(cpu, val, cpu->reg.f & CPU_FLAG_CARRY);
}

static void alu_sub(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a = alu_sub_op(cpu, val, 0);
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

static void alu_and(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a &= val;
	cpu->reg.f = (!cpu->reg.a) ? (CPU_FLAG_ZERO | CPU_FLAG_HALF_CARRY) :
				     CPU_FLAG_HALF_CARRY;
}

static void alu_xor(struct agoge_core_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a ^= val;
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
		[CPU_OP_DEC_B] = &&dec_b,
		[CPU_OP_LD_B_U8] = &&ld_b_u8,
		[CPU_OP_DEC_C] = &&dec_c,
		[CPU_OP_LD_C_U8] = &&ld_c_u8,
		[CPU_OP_LD_DE_U16] = &&ld_de_u16,
		[CPU_OP_LD_MEM_DE_A] = &&ld_mem_de_a,
		[CPU_OP_INC_DE] = &&inc_de,
		[CPU_OP_INC_D] = &&inc_d,
		[CPU_OP_JR_S8] = &&jr_s8,
		[CPU_OP_LD_A_MEM_DE] = &&ld_a_mem_de,
		[CPU_OP_INC_E] = &&inc_e,
		[CPU_OP_RRA] = &&rra,
		[CPU_OP_JR_NZ_S8] = &&jr_nz_s8,
		[CPU_OP_LD_HL_U16] = &&ld_hl_u16,
		[CPU_OP_LDI_MEM_HL_A] = &&ldi_mem_hl_a,
		[CPU_OP_INC_HL] = &&inc_hl,
		[CPU_OP_INC_H] = &&inc_h,
		[CPU_OP_DEC_H] = &&dec_h,
		[CPU_OP_LD_H_U8] = &&ld_h_u8,
		[CPU_OP_JR_Z_S8] = &&jr_z_s8,
		[CPU_OP_LDI_A_MEM_HL] = &&ldi_a_mem_hl,
		[CPU_OP_INC_L] = &&inc_l,
		[CPU_OP_DEC_L] = &&dec_l,
		[CPU_OP_JR_NC_S8] = &&jr_nc_s8,
		[CPU_OP_LD_SP_U16] = &&ld_sp_u16,
		[CPU_OP_LDD_MEM_HL_A] = &&ldd_mem_hl_a,
		[CPU_OP_DEC_A] = &&dec_a,
		[CPU_OP_LD_A_U8] = &&ld_a_u8,
		[CPU_OP_LD_B_MEM_HL] = &&ld_b_mem_hl,
		[CPU_OP_LD_B_A] = &&ld_b_a,
		[CPU_OP_LD_C_MEM_HL] = &&ld_c_mem_hl,
		[CPU_OP_LD_C_A] = &&ld_c_a,
		[CPU_OP_LD_D_MEM_HL] = &&ld_d_mem_hl,
		[CPU_OP_LD_D_A] = &&ld_d_a,
		[CPU_OP_LD_E_A] = &&ld_e_a,
		[CPU_OP_LD_MEM_HL_B] = &&ld_mem_hl_b,
		[CPU_OP_LD_MEM_HL_C] = &&ld_mem_hl_c,
		[CPU_OP_LD_MEM_HL_D] = &&ld_mem_hl_d,
		[CPU_OP_LD_MEM_HL_A] = &&ld_mem_hl_a,
		[CPU_OP_LD_A_B] = &&ld_a_b,
		[CPU_OP_LD_A_C] = &&ld_a_c,
		[CPU_OP_LD_A_D] = &&ld_a_d,
		[CPU_OP_LD_A_E] = &&ld_a_e,
		[CPU_OP_LD_A_H] = &&ld_a_h,
		[CPU_OP_LD_A_L] = &&ld_a_l,
		[CPU_OP_XOR_A_C] = &&xor_a_c,
		[CPU_OP_XOR_A_MEM_HL] = &&xor_a_mem_hl,
		[CPU_OP_OR_A_C] = &&or_a_c,
		[CPU_OP_OR_A_MEM_HL] = &&or_a_mem_hl,
		[CPU_OP_OR_A_A] = &&or_a_a,
		[CPU_OP_POP_BC] = &&pop_bc,
		[CPU_OP_JP_U16] = &&jp_u16,
		[CPU_OP_CALL_NZ_U16] = &&call_nz_u16,
		[CPU_OP_PUSH_BC] = &&push_bc,
		[CPU_OP_ADD_A_U8] = &&add_a_u8,
		[CPU_OP_RET_Z] = &&ret_z,
		[CPU_OP_RET] = &&ret,
		[CPU_OP_PREFIX_CB] = &&prefix_cb,
		[CPU_OP_CALL_U16] = &&call_u16,
		[CPU_OP_ADC_A_U8] = &&adc_a_u8,
		[CPU_OP_RET_NC] = &&ret_nc,
		[CPU_OP_POP_DE] = &&pop_de,
		[CPU_OP_PUSH_DE] = &&push_de,
		[CPU_OP_SUB_A_U8] = &&sub_a_u8,
		[CPU_OP_LD_MEM_FF00_U8_A] = &&ld_mem_ff00_u8_a,
		[CPU_OP_POP_HL] = &&pop_hl,
		[CPU_OP_PUSH_HL] = &&push_hl,
		[CPU_OP_AND_A_U8] = &&and_a_u8,
		[CPU_OP_LD_MEM_U16_A] = &&ld_mem_u16_a,
		[CPU_OP_XOR_A_U8] = &&xor_a_u8,
		[CPU_OP_LD_A_MEM_FF00_U8] = &&ld_a_mem_ff00_u8,
		[CPU_OP_POP_AF] = &&pop_af,
		[CPU_OP_DI] = &&di,
		[CPU_OP_PUSH_AF] = &&push_af,
		[CPU_OP_LD_A_MEM_U16] = &&ld_a_mem_u16,
		[CPU_OP_CP_A_U8] = &&cp_a_u8
	};

	static const void *const cb_tbl[] = { [CPU_OP_RR_C] = &&rr_c,
					      [CPU_OP_RR_D] = &&rr_d,
					      [CPU_OP_SRL_B] = &&srl_b };

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

dec_b:
	cpu->reg.b = alu_dec(cpu, cpu->reg.b);
	DISPATCH();

ld_b_u8:
	cpu->reg.b = read_u8(cpu);
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

inc_de:
	cpu->reg.de++;
	DISPATCH();

inc_d:
	cpu->reg.d = alu_inc(cpu, cpu->reg.d);
	DISPATCH();

jr_s8:
	jr_if(cpu, true);
	DISPATCH();

ld_a_mem_de:
	cpu->reg.a = agoge_core_bus_read(cpu->bus, cpu->reg.de);
	DISPATCH();

inc_e:
	cpu->reg.e = alu_inc(cpu, cpu->reg.e);
	DISPATCH();

rra:
	cpu->reg.a = alu_rr(cpu, cpu->reg.a);
	cpu->reg.f &= ~CPU_FLAG_ZERO;

	DISPATCH();

jr_nz_s8:
	jr_if(cpu, !(cpu->reg.f & CPU_FLAG_ZERO));
	DISPATCH();

ld_hl_u16:
	cpu->reg.hl = read_u16(cpu);
	DISPATCH();

ldi_mem_hl_a:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl++, cpu->reg.a);
	DISPATCH();

inc_hl:
	cpu->reg.hl++;
	DISPATCH();

inc_h:
	cpu->reg.h = alu_inc(cpu, cpu->reg.h);
	DISPATCH();

dec_h:
	cpu->reg.h = alu_dec(cpu, cpu->reg.h);
	DISPATCH();

ld_h_u8:
	cpu->reg.h = read_u8(cpu);
	DISPATCH();

jr_z_s8:
	jr_if(cpu, cpu->reg.f & CPU_FLAG_ZERO);
	DISPATCH();

ldi_a_mem_hl:
	cpu->reg.a = agoge_core_bus_read(cpu->bus, cpu->reg.hl++);
	DISPATCH();

inc_l:
	cpu->reg.l = alu_inc(cpu, cpu->reg.l);
	DISPATCH();

dec_l:
	cpu->reg.l = alu_dec(cpu, cpu->reg.l);
	DISPATCH();

jr_nc_s8:
	jr_if(cpu, !(cpu->reg.f & CPU_FLAG_CARRY));
	DISPATCH();

ld_sp_u16:
	cpu->reg.sp = read_u16(cpu);
	DISPATCH();

ldd_mem_hl_a:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl--, cpu->reg.a);
	DISPATCH();

dec_a:
	cpu->reg.a = alu_dec(cpu, cpu->reg.a);
	DISPATCH();

ld_a_u8:
	cpu->reg.a = read_u8(cpu);
	DISPATCH();

ld_b_mem_hl:
	cpu->reg.b = agoge_core_bus_read(cpu->bus, cpu->reg.hl);
	DISPATCH();

ld_b_a:
	cpu->reg.b = cpu->reg.a;
	DISPATCH();

ld_c_mem_hl:
	cpu->reg.c = agoge_core_bus_read(cpu->bus, cpu->reg.hl);
	DISPATCH();

ld_c_a:
	cpu->reg.c = cpu->reg.a;
	DISPATCH();

ld_d_mem_hl:
	cpu->reg.d = agoge_core_bus_read(cpu->bus, cpu->reg.hl);
	DISPATCH();

ld_d_a:
	cpu->reg.d = cpu->reg.a;
	DISPATCH();

ld_e_a:
	cpu->reg.e = cpu->reg.a;
	DISPATCH();

ld_mem_hl_b:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.b);
	DISPATCH();

ld_mem_hl_c:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.c);
	DISPATCH();

ld_mem_hl_d:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.d);
	DISPATCH();

ld_mem_hl_a:
	agoge_core_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.a);
	DISPATCH();

ld_a_b:
	cpu->reg.a = cpu->reg.b;
	DISPATCH();

ld_a_c:
	cpu->reg.a = cpu->reg.c;
	DISPATCH();

ld_a_d:
	cpu->reg.a = cpu->reg.d;
	DISPATCH();

ld_a_e:
	cpu->reg.a = cpu->reg.e;
	DISPATCH();

ld_a_h:
	cpu->reg.a = cpu->reg.h;
	DISPATCH();

ld_a_l:
	cpu->reg.a = cpu->reg.l;
	DISPATCH();

xor_a_c:
	alu_xor(cpu, cpu->reg.c);
	DISPATCH();

xor_a_mem_hl:
	alu_xor(cpu, agoge_core_bus_read(cpu->bus, cpu->reg.hl));
	DISPATCH();

or_a_c:
	alu_or(cpu, cpu->reg.c);
	DISPATCH();

or_a_mem_hl:
	alu_or(cpu, agoge_core_bus_read(cpu->bus, cpu->reg.hl));
	DISPATCH();

or_a_a:
	alu_or(cpu, cpu->reg.a);
	DISPATCH();

pop_bc:
	cpu->reg.bc = stack_pop(cpu);
	DISPATCH();

jp_u16:
	jp_if(cpu, true);
	DISPATCH();

call_nz_u16:
	call_if(cpu, !(cpu->reg.f & CPU_FLAG_ZERO));
	DISPATCH();

push_bc:
	stack_push(cpu, cpu->reg.bc);
	DISPATCH();

add_a_u8:
	alu_add(cpu, read_u8(cpu));
	DISPATCH();

ret_z:
	ret_if(cpu, cpu->reg.f & CPU_FLAG_ZERO);
	DISPATCH();

ret:
	ret_if(cpu, true);
	DISPATCH();

prefix_cb:
	instr = read_u8(cpu);
	goto *cb_tbl[instr];

rr_c:
	cpu->reg.c = alu_rr(cpu, cpu->reg.c);
	DISPATCH();

rr_d:
	cpu->reg.d = alu_rr(cpu, cpu->reg.d);
	DISPATCH();

srl_b:
	cpu->reg.b = alu_srl(cpu, cpu->reg.b);
	DISPATCH();

call_u16:
	call_if(cpu, true);
	DISPATCH();

adc_a_u8:
	alu_adc(cpu, read_u8(cpu));
	DISPATCH();

ret_nc:
	ret_if(cpu, !(cpu->reg.f & CPU_FLAG_CARRY));
	DISPATCH();

pop_de:
	cpu->reg.de = stack_pop(cpu);
	DISPATCH();

push_de:
	stack_push(cpu, cpu->reg.de);
	DISPATCH();

sub_a_u8:
	alu_sub(cpu, read_u8(cpu));
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

and_a_u8:
	alu_and(cpu, read_u8(cpu));
	DISPATCH();

ld_mem_u16_a:
	agoge_core_bus_write(cpu->bus, read_u16(cpu), cpu->reg.a);
	DISPATCH();

xor_a_u8:
	alu_xor(cpu, read_u8(cpu));
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

ld_a_mem_u16:
	cpu->reg.a = agoge_core_bus_read(cpu->bus, read_u16(cpu));
	DISPATCH();

cp_a_u8:
	alu_cp(cpu, read_u8(cpu));
	DISPATCH();
}
