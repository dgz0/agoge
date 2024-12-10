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

#include "agoge-compiler.h"
#include "agoge-cpu-defs.h"
#include "agoge-cpu.h"
#include "agoge-bus.h"
#include "agoge-log.h"

NONNULL static void set_flag(struct agoge_cpu *const cpu, const bool condition,
			     const uint8_t flag)
{
	if (condition) {
		cpu->reg.f |= flag;
	} else {
		cpu->reg.f &= ~flag;
	}
}

NONNULL static void zero_flag_set(struct agoge_cpu *const cpu,
				  const bool condition)
{
	set_flag(cpu, condition == 0, CPU_ZERO_FLAG);
}

NONNULL static void carry_flag_set(struct agoge_cpu *const cpu,
				   const bool condition)
{
	set_flag(cpu, condition, CPU_CARRY_FLAG);
}

NONNULL static void half_carry_flag_set(struct agoge_cpu *const cpu,
					const bool condition)
{
	set_flag(cpu, condition, CPU_HALF_CARRY_FLAG);
}

NONNULL NODISCARD static uint8_t alu_inc(struct agoge_cpu *const cpu,
					 uint8_t reg)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;
	half_carry_flag_set(cpu, (reg & 0x0F) == 0x0F);
	zero_flag_set(cpu, ++reg);

	return reg;
}

NONNULL static uint8_t alu_dec(struct agoge_cpu *const cpu, uint8_t reg)
{
	cpu->reg.f |= CPU_SUBTRACT_FLAG;
	half_carry_flag_set(cpu, (reg & 0x0F) == 0);
	zero_flag_set(cpu, --reg);

	return reg;
}

NONNULL static uint8_t read_u8(struct agoge_cpu *const cpu)
{
	return agoge_bus_read(cpu->bus, cpu->reg.pc++);
}

NONNULL static uint16_t read_u16(struct agoge_cpu *const cpu)
{
	const uint8_t lo = read_u8(cpu);
	const uint8_t hi = read_u8(cpu);

	return (uint16_t)((hi << 8) | lo);
}

NONNULL static void stack_push(struct agoge_cpu *const cpu, const uint16_t data)
{
	agoge_bus_write(cpu->bus, --cpu->reg.sp, data >> 8);
	agoge_bus_write(cpu->bus, --cpu->reg.sp, data & 0x00FF);
}

NONNULL static uint16_t stack_pop(struct agoge_cpu *const cpu)
{
	const uint8_t lo = agoge_bus_read(cpu->bus, cpu->reg.sp++);
	const uint8_t hi = agoge_bus_read(cpu->bus, cpu->reg.sp++);

	return (hi << 8) | lo;
}

NONNULL static void jr_if(struct agoge_cpu *const cpu, const bool condition)
{
	const int8_t offset = (int8_t)read_u8(cpu);

	if (condition) {
		cpu->reg.pc += offset;
	}
}

NONNULL static void ret_if(struct agoge_cpu *const cpu, const bool condition)
{
	if (condition) {
		cpu->reg.pc = stack_pop(cpu);
	}
}

NONNULL static void call_if(struct agoge_cpu *const cpu, const bool condition)
{
	const uint16_t address = read_u16(cpu);

	if (condition) {
		stack_push(cpu, cpu->reg.pc);
		cpu->reg.pc = address;
	}
}

NONNULL static void jp_if(struct agoge_cpu *const cpu, const bool condition)
{
	const uint16_t address = read_u16(cpu);

	if (condition) {
		cpu->reg.pc = address;
	}
}

NONNULL static uint8_t alu_swap(struct agoge_cpu *const cpu, uint8_t val)
{
	val = ((val & 0x0F) << 4) | (val >> 4);
	cpu->reg.f = (!val) ? CPU_ZERO_FLAG : 0x00;

	return val;
}

NONNULL static uint8_t alu_srl(struct agoge_cpu *const cpu, uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	const bool carry = val & 1;

	val >>= 1;

	zero_flag_set(cpu, val);
	carry_flag_set(cpu, carry);

	return val;
}

NONNULL NODISCARD static uint8_t alu_rl_helper(struct agoge_cpu *const cpu,
					       uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;

	carry_flag_set(cpu, val & (UINT8_C(1) << 7));

	val = (val << 1) | carry;
	return val;
}

NONNULL NODISCARD static uint8_t alu_rl(struct agoge_cpu *const cpu,
					uint8_t val)
{
	val = alu_rl_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

NONNULL NODISCARD static uint8_t alu_rr_helper(struct agoge_cpu *const cpu,
					       uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

	const bool old_carry = val & 1;
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;

	val = (val >> 1) | (carry << 7);
	carry_flag_set(cpu, old_carry);

	return val;
}

NONNULL NODISCARD static uint8_t alu_rr(struct agoge_cpu *const cpu,
					const uint8_t val)
{
	const uint8_t result = alu_rr_helper(cpu, val);
	zero_flag_set(cpu, result);

	return result;
}

NONNULL NODISCARD static uint8_t alu_rlc_helper(struct agoge_cpu *const cpu,
						uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & (UINT8_C(1) << 7));

	val = (val << 1) | (val >> 7);
	return val;
}

NONNULL NODISCARD static uint8_t alu_rlc(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	val = alu_rlc_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

NONNULL NODISCARD static uint8_t alu_rrc_helper(struct agoge_cpu *const cpu,
						uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & 1);

	val = (val >> 1) | (val << 7);
	return val;
}

NONNULL NODISCARD static uint8_t alu_rrc(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	val = alu_rrc_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

NONNULL NODISCARD static uint16_t alu_add_sp(struct agoge_cpu *const cpu)
{
	cpu->reg.f &= ~(CPU_ZERO_FLAG | CPU_SUBTRACT_FLAG);
	const int8_t s8 = (int8_t)read_u8(cpu);

	const int sum = cpu->reg.sp + s8;

	half_carry_flag_set(cpu, (cpu->reg.sp ^ s8 ^ sum) & 0x10);
	carry_flag_set(cpu, (cpu->reg.sp ^ s8 ^ sum) & 0x100);

	return (uint16_t)sum;
}

NONNULL static void alu_add(struct agoge_cpu *const cpu,
			    const unsigned int addend, const unsigned int carry)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;

	const unsigned int result = cpu->reg.a + addend + carry;
	const uint8_t sum = (uint8_t)result;

	zero_flag_set(cpu, sum);
	half_carry_flag_set(cpu, (cpu->reg.a ^ addend ^ result) & 0x10);
	carry_flag_set(cpu, result > UINT8_MAX);

	cpu->reg.a = sum;
}

NONNULL static void alu_adc(struct agoge_cpu *const cpu, const uint8_t addend)
{
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;
	alu_add(cpu, addend, carry);
}

NONNULL static void alu_add_hl(struct agoge_cpu *const cpu,
			       const uint16_t addend)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;

	const unsigned int sum = cpu->reg.hl + addend;

	half_carry_flag_set(cpu, (cpu->reg.hl ^ addend ^ sum) & 0x1000);
	carry_flag_set(cpu, sum > UINT16_MAX);
	cpu->reg.hl = (uint16_t)sum;
}

NONNULL static uint8_t alu_sub_helper(struct agoge_cpu *const cpu,
				      const uint8_t subtrahend,
				      const unsigned int carry)
{
	cpu->reg.f |= CPU_SUBTRACT_FLAG;

	const int result = cpu->reg.a - subtrahend - carry;

	const uint8_t diff = (uint8_t)result;

	zero_flag_set(cpu, diff);
	half_carry_flag_set(cpu, (cpu->reg.a ^ subtrahend ^ result) & 0x10);
	carry_flag_set(cpu, result < 0);

	return diff;
}

NONNULL static void alu_sub(struct agoge_cpu *const cpu,
			    const uint8_t subtrahend)
{
	cpu->reg.a = alu_sub_helper(cpu, subtrahend, 0);
}

NONNULL static void alu_sbc(struct agoge_cpu *const cpu,
			    const uint8_t subtrahend)
{
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;
	cpu->reg.a = alu_sub_helper(cpu, subtrahend, carry);
}

NONNULL static void alu_cp(struct agoge_cpu *const cpu,
			   const uint8_t subtrahend)
{
	(void)alu_sub_helper(cpu, subtrahend, 0);
}

NONNULL static void alu_xor(struct agoge_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a ^= val;
	cpu->reg.f = (!cpu->reg.a) ? CPU_ZERO_FLAG : 0x00;
}

NONNULL static void alu_and(struct agoge_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a &= val;
	cpu->reg.f = !cpu->reg.a ? (CPU_ZERO_FLAG | CPU_HALF_CARRY_FLAG) :
				   CPU_HALF_CARRY_FLAG;
}

NONNULL static void alu_or(struct agoge_cpu *const cpu, const uint8_t reg)
{
	cpu->reg.a |= reg;
	cpu->reg.f = (!cpu->reg.a) ? CPU_ZERO_FLAG : 0x00;
}

NONNULL static void alu_bit(struct agoge_cpu *const cpu, const unsigned int bit,
			    const uint8_t reg)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;
	cpu->reg.f |= CPU_HALF_CARRY_FLAG;

	if (!(reg & (UINT8_C(1) << bit))) {
		cpu->reg.f |= CPU_ZERO_FLAG;
	} else {
		cpu->reg.f &= ~CPU_ZERO_FLAG;
	}
}

NONNULL static void rst(struct agoge_cpu *const cpu, const uint16_t vec)
{
	stack_push(cpu, cpu->reg.pc);
	cpu->reg.pc = vec;
}

NONNULL static uint8_t alu_sla(struct agoge_cpu *const cpu, uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

	carry_flag_set(cpu, val & (UINT8_C(1) << 7));
	val <<= 1;
	zero_flag_set(cpu, val);

	return val;
}

NONNULL static uint8_t alu_sra(struct agoge_cpu *const cpu, uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & 1);

	val = (val >> 1) | (val & (UINT8_C(1) << 7));

	zero_flag_set(cpu, val);
	return val;
}

NONNULL static uint8_t alu_res(struct agoge_cpu *const cpu,
			       const unsigned int bit, uint8_t val)
{
	val &= ~(UINT8_C(1) << bit);
	return val;
}

NONNULL static uint8_t alu_set(struct agoge_cpu *const cpu,
			       const unsigned int bit, uint8_t val)
{
	val |= (UINT8_C(1) << bit);
	return val;
}

NONNULL void agoge_cpu_reset(struct agoge_cpu *const cpu)
{
	cpu->reg.bc = CPU_PWRUP_REG_BC;
	cpu->reg.de = CPU_PWRUP_REG_DE;
	cpu->reg.hl = CPU_PWRUP_REG_HL;
	cpu->reg.af = CPU_PWRUP_REG_AF;
	cpu->reg.pc = CPU_PWRUP_REG_PC;
	cpu->reg.sp = CPU_PWRUP_REG_SP;
}

NONNULL void agoge_cpu_step(struct agoge_cpu *const cpu)
{
#define LD_R8_R8(op, dst, src) \
	case (op):             \
		(dst) = (src); \
		return

#define LD_R8_MEM_HL(op, dst)                                  \
	case (op):                                             \
		(dst) = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		return

#define LD_MEM_HL_R8(op, src)                                  \
	case (op):                                             \
		agoge_bus_write(cpu->bus, cpu->reg.hl, (src)); \
		return

#define LD_R16_U16(op, dst)            \
	case (op):                     \
		(dst) = read_u16(cpu); \
		return

#define INC_R16(op, dst) \
	case (op):       \
		(dst)++; \
		return

#define INC_R8(op, src_dst)                          \
	case (op):                                   \
		(src_dst) = alu_inc(cpu, (src_dst)); \
		return

#define DEC_R8(op, src_dst)                          \
	case (op):                                   \
		(src_dst) = alu_dec(cpu, (src_dst)); \
		return

#define LD_R8_U8(op, dst)             \
	case (op):                    \
		(dst) = read_u8(cpu); \
		return

#define ADD_HL_R16(op, r16)             \
	case (op):                      \
		alu_add_hl(cpu, (r16)); \
		return

#define DEC_R16(op, r16) \
	case (op):       \
		(r16)--; \
		return

#define LD_MEM_R16_A(op, r16)                                 \
	case (op):                                            \
		agoge_bus_write(cpu->bus, (r16), cpu->reg.a); \
		return

#define JR_CC(op, cond)             \
	case (op):                  \
		jr_if(cpu, (cond)); \
		return

#define JP_CC(op, cond)             \
	case (op):                  \
		jp_if(cpu, (cond)); \
		return

#define RET_CC(op, cond)             \
	case (op):                   \
		ret_if(cpu, (cond)); \
		return

#define CALL_CC(op, cond)             \
	case (op):                    \
		call_if(cpu, (cond)); \
		return

#define BIT_OP(op, func, src)       \
	case (op):                  \
		(func)(cpu, (src)); \
		return

#define BIT_OP_HL(op, func)                                               \
	case (op): {                                                      \
		const uint8_t u8 = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		(func)(cpu, u8);                                          \
		return;                                                   \
	}

#define LD_A_MEM_R16(op, src)                                 \
	case (op):                                            \
		cpu->reg.a = agoge_bus_read(cpu->bus, (src)); \
		return

#define PUSH(op, src)                   \
	case (op):                      \
		stack_push(cpu, (src)); \
		return

#define POP(op, dst)                    \
	case (op):                      \
		(dst) = stack_pop(cpu); \
		return

#define CP(op, src)                 \
	case (op):                  \
		alu_cp(cpu, (src)); \
		return

#define CB_BIT_OP(op, func, src_dst)                \
	case (op):                                  \
		(src_dst) = (func)(cpu, (src_dst)); \
		return

#define CB_BIT_OP_HL(op, func)                                        \
	case (op): {                                                  \
		uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		data = (func)(cpu, data);                             \
		agoge_bus_write(cpu->bus, cpu->reg.hl, data);         \
                                                                      \
		return;                                               \
	}

#define BIT(op, bit, reg)                   \
	case (op):                          \
		alu_bit(cpu, (bit), (reg)); \
		return

#define BIT_HL(op, bit)                                                    \
	case (op): {                                                       \
		const uint8_t val = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		alu_bit(cpu, (bit), val);                                  \
		return;                                                    \
	}

#define RES(op, bit, reg)                           \
	case (op):                                  \
		(reg) = alu_res(cpu, (bit), (reg)); \
		return

#define RES_HL(op, bit)                                              \
	case (op): {                                                 \
		uint8_t val = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		val = alu_res(cpu, (bit), val);                      \
		agoge_bus_write(cpu->bus, cpu->reg.hl, val);         \
                                                                     \
		return;                                              \
	}

#define SET(op, bit, reg)                           \
	case (op):                                  \
		(reg) = alu_set(cpu, (bit), (reg)); \
		return

#define SET_HL(op, bit)                                              \
	case (op): {                                                 \
		uint8_t val = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		val = alu_set(cpu, (bit), val);                      \
		agoge_bus_write(cpu->bus, cpu->reg.hl, val);         \
                                                                     \
		return;                                              \
	}

#define RST(op, vec)             \
	case (op):               \
		rst(cpu, (vec)); \
		return

#define ADD_R8(op, r8)                 \
	case (op):                     \
		alu_add(cpu, (r8), 0); \
		return

#define ADC_R8(op, r8)              \
	case (op):                  \
		alu_adc(cpu, (r8)); \
		return

#define SUB_R8(op, r8)              \
	case (op):                  \
		alu_sub(cpu, (r8)); \
		return

#define SBC_R8(op, r8)              \
	case (op):                  \
		alu_sbc(cpu, (r8)); \
		return

	// clang-format off

	const uint8_t instr = read_u8(cpu);

	switch (instr) {
	case CPU_OP_NOP:
		return;

	LD_R16_U16(CPU_OP_LD_BC_U16, cpu->reg.bc);
	LD_MEM_R16_A(CPU_OP_LD_MEM_BC_A, cpu->reg.bc);
	INC_R16(CPU_OP_INC_BC, cpu->reg.bc);
	INC_R8(CPU_OP_INC_B, cpu->reg.b);
	DEC_R8(CPU_OP_DEC_B, cpu->reg.b);
	LD_R8_U8(CPU_OP_LD_B_U8, cpu->reg.b);

	case CPU_OP_RLCA:
		cpu->reg.a = alu_rlc_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	case CPU_OP_LD_MEM_U16_SP: {
		const uint16_t u16 = read_u16(cpu);

		agoge_bus_write(cpu->bus, u16 + 0, cpu->reg.sp & 0x00FF);
		agoge_bus_write(cpu->bus, u16 + 1, cpu->reg.sp >> 8);

		return;
	}

	ADD_HL_R16(CPU_OP_ADD_HL_BC, cpu->reg.bc);
	LD_A_MEM_R16(CPU_OP_LD_A_MEM_BC, cpu->reg.bc);
	DEC_R16(CPU_OP_DEC_BC, cpu->reg.bc);
	INC_R8(CPU_OP_INC_C, cpu->reg.c);
	DEC_R8(CPU_OP_DEC_C, cpu->reg.c);
	LD_R8_U8(CPU_OP_LD_C_U8, cpu->reg.c);

	case CPU_OP_RRCA:
		cpu->reg.a = alu_rrc_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	LD_R16_U16(CPU_OP_LD_DE_U16, cpu->reg.de);
	LD_MEM_R16_A(CPU_OP_LD_MEM_DE_A, cpu->reg.de);
	INC_R16(CPU_OP_INC_DE, cpu->reg.de);
	INC_R8(CPU_OP_INC_D, cpu->reg.d);
	DEC_R8(CPU_OP_DEC_D, cpu->reg.d);
	LD_R8_U8(CPU_OP_LD_D_U8, cpu->reg.d);

	case CPU_OP_RLA:
		cpu->reg.a = alu_rl_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	JR_CC(CPU_OP_JR_S8, true);
	ADD_HL_R16(CPU_OP_ADD_HL_DE, cpu->reg.de);
	LD_A_MEM_R16(CPU_OP_LD_A_MEM_DE, cpu->reg.de);
	DEC_R16(CPU_OP_DEC_DE, cpu->reg.de);
	INC_R8(CPU_OP_INC_E, cpu->reg.e);
	DEC_R8(CPU_OP_DEC_E, cpu->reg.e);
	LD_R8_U8(CPU_OP_LD_E_U8, cpu->reg.e);

	case CPU_OP_RRA:
		cpu->reg.a = alu_rr_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	JR_CC(CPU_OP_JR_NZ_S8, !(cpu->reg.f & CPU_ZERO_FLAG));

	LD_R16_U16(CPU_OP_LD_HL_U16, cpu->reg.hl);

	case CPU_OP_LDI_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl++, cpu->reg.a);
		return;

	INC_R16(CPU_OP_INC_HL, cpu->reg.hl);
	INC_R8(CPU_OP_INC_H, cpu->reg.h);
	DEC_R8(CPU_OP_DEC_H, cpu->reg.h);
	LD_R8_U8(CPU_OP_LD_H_U8, cpu->reg.h);

	case CPU_OP_DAA:
		return;

	JR_CC(CPU_OP_JR_Z_S8, cpu->reg.f & CPU_ZERO_FLAG);
	ADD_HL_R16(CPU_OP_ADD_HL_HL, cpu->reg.hl);

	case CPU_OP_LDI_A_MEM_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl++);
		return;

	DEC_R16(CPU_OP_DEC_HL, cpu->reg.hl);
	INC_R8(CPU_OP_INC_L, cpu->reg.l);
	DEC_R8(CPU_OP_DEC_L, cpu->reg.l);
	LD_R8_U8(CPU_OP_LD_L_U8, cpu->reg.l);

	case CPU_OP_CPL:
		cpu->reg.a = ~cpu->reg.a;
		cpu->reg.f |= (CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

		return;

	JR_CC(CPU_OP_JR_NC_S8, !(cpu->reg.f & CPU_CARRY_FLAG));
	LD_R16_U16(CPU_OP_LD_SP_U16, cpu->reg.sp);

	case CPU_OP_LDD_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl--, cpu->reg.a);
		return;

	INC_R16(CPU_OP_INC_SP, cpu->reg.sp);

	case CPU_OP_INC_MEM_HL: {
		uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		data = alu_inc(cpu, data);
		agoge_bus_write(cpu->bus, cpu->reg.hl, data);

		return;
	}

	case CPU_OP_DEC_MEM_HL: {
		uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		data = alu_dec(cpu, data);
		agoge_bus_write(cpu->bus, cpu->reg.hl, data);

		return;
	}

	case CPU_OP_LD_MEM_HL_U8: {
		const uint8_t u8 = read_u8(cpu);

		agoge_bus_write(cpu->bus, cpu->reg.hl, u8);
		return;
	}

	case CPU_OP_SCF:
		cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
		cpu->reg.f |= CPU_CARRY_FLAG;

		return;

	JR_CC(CPU_OP_JR_C_S8, cpu->reg.f & CPU_CARRY_FLAG);
	ADD_HL_R16(CPU_OP_ADD_HL_SP, cpu->reg.sp);

	case CPU_OP_LDD_A_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl--);
		return;

	DEC_R16(CPU_OP_DEC_SP, cpu->reg.sp);
	INC_R8(CPU_OP_INC_A, cpu->reg.a);
	DEC_R8(CPU_OP_DEC_A, cpu->reg.a);
	LD_R8_U8(CPU_OP_LD_A_U8, cpu->reg.a);

	case CPU_OP_CCF:
		cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
		carry_flag_set(cpu, !(cpu->reg.f & CPU_CARRY_FLAG));

		return;

	LD_R8_R8(CPU_OP_LD_B_B, cpu->reg.b, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_B_C, cpu->reg.b, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_B_D, cpu->reg.b, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_B_E, cpu->reg.b, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_B_H, cpu->reg.b, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_B_L, cpu->reg.b, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_B_MEM_HL, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_B_A, cpu->reg.b, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_C_B, cpu->reg.c, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_C_C, cpu->reg.c, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_C_D, cpu->reg.c, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_C_E, cpu->reg.c, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_C_H, cpu->reg.c, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_C_L, cpu->reg.c, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_C_MEM_HL, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_C_A, cpu->reg.c, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_D_B, cpu->reg.d, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_D_C, cpu->reg.d, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_D_D, cpu->reg.d, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_D_E, cpu->reg.d, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_D_H, cpu->reg.d, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_D_L, cpu->reg.d, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_D_MEM_HL, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_D_A, cpu->reg.d, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_E_B, cpu->reg.e, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_E_C, cpu->reg.e, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_E_D, cpu->reg.e, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_E_E, cpu->reg.e, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_E_H, cpu->reg.e, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_E_L, cpu->reg.e, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_E_MEM_HL, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_E_A, cpu->reg.e, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_H_B, cpu->reg.h, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_H_C, cpu->reg.h, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_H_D, cpu->reg.h, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_H_E, cpu->reg.h, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_H_H, cpu->reg.h, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_H_L, cpu->reg.h, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_H_MEM_HL, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_H_A, cpu->reg.h, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_L_B, cpu->reg.l, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_L_C, cpu->reg.l, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_L_D, cpu->reg.l, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_L_E, cpu->reg.l, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_L_H, cpu->reg.l, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_L_L, cpu->reg.l, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_L_MEM_HL, cpu->reg.l);
	LD_R8_R8(CPU_OP_LD_L_A, cpu->reg.l, cpu->reg.a);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_B, cpu->reg.b);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_C, cpu->reg.c);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_D, cpu->reg.d);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_E, cpu->reg.e);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_H, cpu->reg.h);
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_L, cpu->reg.l);
	// HALT, not implemented
	LD_MEM_HL_R8(CPU_OP_LD_MEM_HL_A, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_A_B, cpu->reg.a, cpu->reg.b);
	LD_R8_R8(CPU_OP_LD_A_C, cpu->reg.a, cpu->reg.c);
	LD_R8_R8(CPU_OP_LD_A_D, cpu->reg.a, cpu->reg.d);
	LD_R8_R8(CPU_OP_LD_A_E, cpu->reg.a, cpu->reg.e);
	LD_R8_R8(CPU_OP_LD_A_H, cpu->reg.a, cpu->reg.h);
	LD_R8_R8(CPU_OP_LD_A_L, cpu->reg.a, cpu->reg.l);
	LD_R8_MEM_HL(CPU_OP_LD_A_MEM_HL, cpu->reg.a);
	LD_R8_R8(CPU_OP_LD_A_A, cpu->reg.a, cpu->reg.a);
	ADD_R8(CPU_OP_ADD_A_B, cpu->reg.b);
	ADD_R8(CPU_OP_ADD_A_C, cpu->reg.c);
	ADD_R8(CPU_OP_ADD_A_D, cpu->reg.d);
	ADD_R8(CPU_OP_ADD_A_E, cpu->reg.e);
	ADD_R8(CPU_OP_ADD_A_H, cpu->reg.h);
	ADD_R8(CPU_OP_ADD_A_L, cpu->reg.l);

	case CPU_OP_ADD_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_add(cpu, data, 0);
		return;
	}

	ADD_R8(CPU_OP_ADD_A_A, cpu->reg.a);
	ADC_R8(CPU_OP_ADC_A_B, cpu->reg.b);
	ADC_R8(CPU_OP_ADC_A_C, cpu->reg.c);
	ADC_R8(CPU_OP_ADC_A_D, cpu->reg.d);
	ADC_R8(CPU_OP_ADC_A_E, cpu->reg.e);
	ADC_R8(CPU_OP_ADC_A_H, cpu->reg.h);
	ADC_R8(CPU_OP_ADC_A_L, cpu->reg.l);

	case CPU_OP_ADC_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_adc(cpu, data);
		return;
	}

	ADC_R8(CPU_OP_ADC_A_A, cpu->reg.a);
	SUB_R8(CPU_OP_SUB_A_B, cpu->reg.b);
	SUB_R8(CPU_OP_SUB_A_C, cpu->reg.c);
	SUB_R8(CPU_OP_SUB_A_D, cpu->reg.d);
	SUB_R8(CPU_OP_SUB_A_E, cpu->reg.e);
	SUB_R8(CPU_OP_SUB_A_H, cpu->reg.h);
	SUB_R8(CPU_OP_SUB_A_L, cpu->reg.l);

	case CPU_OP_SUB_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_sub(cpu, data);
		return;
	}

	SUB_R8(CPU_OP_SUB_A_A, cpu->reg.a);
	SBC_R8(CPU_OP_SBC_A_B, cpu->reg.b);
	SBC_R8(CPU_OP_SBC_A_C, cpu->reg.c);
	SBC_R8(CPU_OP_SBC_A_D, cpu->reg.d);
	SBC_R8(CPU_OP_SBC_A_E, cpu->reg.e);
	SBC_R8(CPU_OP_SBC_A_H, cpu->reg.h);
	SBC_R8(CPU_OP_SBC_A_L, cpu->reg.l);

	case CPU_OP_SBC_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_sbc(cpu, data);
		return;
	}

	SBC_R8(CPU_OP_SBC_A_A, cpu->reg.a);
	BIT_OP(CPU_OP_AND_A_B, alu_and, cpu->reg.b);
	BIT_OP(CPU_OP_AND_A_C, alu_and, cpu->reg.c);
	BIT_OP(CPU_OP_AND_A_D, alu_and, cpu->reg.d);
	BIT_OP(CPU_OP_AND_A_E, alu_and, cpu->reg.e);
	BIT_OP(CPU_OP_AND_A_H, alu_and, cpu->reg.h);
	BIT_OP(CPU_OP_AND_A_L, alu_and, cpu->reg.l);

	case CPU_OP_AND_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_and(cpu, data);
		return;
	}

	BIT_OP(CPU_OP_AND_A_A, alu_and, cpu->reg.a);
	BIT_OP(CPU_OP_XOR_A_B, alu_xor, cpu->reg.b);
	BIT_OP(CPU_OP_XOR_A_C, alu_xor, cpu->reg.c);
	BIT_OP(CPU_OP_XOR_A_D, alu_xor, cpu->reg.d);
	BIT_OP(CPU_OP_XOR_A_E, alu_xor, cpu->reg.e);
	BIT_OP(CPU_OP_XOR_A_H, alu_xor, cpu->reg.h);
	BIT_OP(CPU_OP_XOR_A_L, alu_xor, cpu->reg.l);
	BIT_OP_HL(CPU_OP_XOR_A_MEM_HL, alu_xor);
	BIT_OP(CPU_OP_XOR_A_A, alu_xor, cpu->reg.a);
	BIT_OP(CPU_OP_OR_A_B, alu_or, cpu->reg.b);
	BIT_OP(CPU_OP_OR_A_C, alu_or, cpu->reg.c);
	BIT_OP(CPU_OP_OR_A_D, alu_or, cpu->reg.d);
	BIT_OP(CPU_OP_OR_A_E, alu_or, cpu->reg.e);
	BIT_OP(CPU_OP_OR_A_H, alu_or, cpu->reg.h);
	BIT_OP(CPU_OP_OR_A_L, alu_or, cpu->reg.l);

	case CPU_OP_OR_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		alu_or(cpu, data);

		return;
	}

	BIT_OP(CPU_OP_OR_A_A, alu_or, cpu->reg.a);
	CP(CPU_OP_CP_A_B, cpu->reg.b);
	CP(CPU_OP_CP_A_C, cpu->reg.c);
	CP(CPU_OP_CP_A_D, cpu->reg.d);
	CP(CPU_OP_CP_A_E, cpu->reg.e);
	CP(CPU_OP_CP_A_H, cpu->reg.h);
	CP(CPU_OP_CP_A_L, cpu->reg.l);

	case CPU_OP_CP_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_cp(cpu, data);
		return;
	}

	CP(CPU_OP_CP_A_A, cpu->reg.a);
	RET_CC(CPU_OP_RET_NZ, !(cpu->reg.f & CPU_ZERO_FLAG));
	POP(CPU_OP_POP_BC, cpu->reg.bc);
	JP_CC(CPU_OP_JP_NZ_U16, !(cpu->reg.f & CPU_ZERO_FLAG));
	JP_CC(CPU_OP_JP_U16, true);
	CALL_CC(CPU_OP_CALL_NZ_U16, !(cpu->reg.f & CPU_ZERO_FLAG));
	PUSH(CPU_OP_PUSH_BC, cpu->reg.bc);

	case CPU_OP_ADD_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_add(cpu, u8, 0);
		return;
	}

	RST(CPU_OP_RST_00, 0x0000);
	RET_CC(CPU_OP_RET_Z, cpu->reg.f & CPU_ZERO_FLAG);
	RET_CC(CPU_OP_RET, true);
	JP_CC(CPU_OP_JP_Z_U16, cpu->reg.f & CPU_ZERO_FLAG);

	case CPU_OP_PREFIX_CB: {
		const uint8_t cb_instr = read_u8(cpu);

		switch (cb_instr) {
			CB_BIT_OP(CPU_OP_RLC_B, alu_rlc, cpu->reg.b);
			CB_BIT_OP(CPU_OP_RLC_C, alu_rlc, cpu->reg.c);
			CB_BIT_OP(CPU_OP_RLC_D, alu_rlc, cpu->reg.d);
			CB_BIT_OP(CPU_OP_RLC_E, alu_rlc, cpu->reg.e);
			CB_BIT_OP(CPU_OP_RLC_H, alu_rlc, cpu->reg.h);
			CB_BIT_OP(CPU_OP_RLC_L, alu_rlc, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_RLC_MEM_HL, alu_rlc);
			CB_BIT_OP(CPU_OP_RLC_A, alu_rlc, cpu->reg.a);
			CB_BIT_OP(CPU_OP_RRC_B, alu_rrc, cpu->reg.b);
			CB_BIT_OP(CPU_OP_RRC_C, alu_rrc, cpu->reg.c);
			CB_BIT_OP(CPU_OP_RRC_D, alu_rrc, cpu->reg.d);
			CB_BIT_OP(CPU_OP_RRC_E, alu_rrc, cpu->reg.e);
			CB_BIT_OP(CPU_OP_RRC_H, alu_rrc, cpu->reg.h);
			CB_BIT_OP(CPU_OP_RRC_L, alu_rrc, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_RRC_MEM_HL, alu_rrc);
			CB_BIT_OP(CPU_OP_RRC_A, alu_rrc, cpu->reg.a);
			CB_BIT_OP(CPU_OP_RL_B, alu_rl, cpu->reg.b);
			CB_BIT_OP(CPU_OP_RL_C, alu_rl, cpu->reg.c);
			CB_BIT_OP(CPU_OP_RL_D, alu_rl, cpu->reg.d);
			CB_BIT_OP(CPU_OP_RL_E, alu_rl, cpu->reg.e);
			CB_BIT_OP(CPU_OP_RL_H, alu_rl, cpu->reg.h);
			CB_BIT_OP(CPU_OP_RL_L, alu_rl, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_RL_MEM_HL, alu_rl);
			CB_BIT_OP(CPU_OP_RL_A, alu_rl, cpu->reg.a);
			CB_BIT_OP(CPU_OP_RR_B, alu_rr, cpu->reg.b);
			CB_BIT_OP(CPU_OP_RR_C, alu_rr, cpu->reg.c);
			CB_BIT_OP(CPU_OP_RR_D, alu_rr, cpu->reg.d);
			CB_BIT_OP(CPU_OP_RR_E, alu_rr, cpu->reg.e);
			CB_BIT_OP(CPU_OP_RR_H, alu_rr, cpu->reg.h);
			CB_BIT_OP(CPU_OP_RR_L, alu_rr, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_RR_MEM_HL, alu_rr);
			CB_BIT_OP(CPU_OP_RR_A, alu_rr, cpu->reg.a);
			CB_BIT_OP(CPU_OP_SLA_B, alu_sla, cpu->reg.b);
			CB_BIT_OP(CPU_OP_SLA_C, alu_sla, cpu->reg.c);
			CB_BIT_OP(CPU_OP_SLA_D, alu_sla, cpu->reg.d);
			CB_BIT_OP(CPU_OP_SLA_E, alu_sla, cpu->reg.e);
			CB_BIT_OP(CPU_OP_SLA_H, alu_sla, cpu->reg.h);
			CB_BIT_OP(CPU_OP_SLA_L, alu_sla, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_SLA_MEM_HL, alu_sla);
			CB_BIT_OP(CPU_OP_SLA_A, alu_sla, cpu->reg.a);
			CB_BIT_OP(CPU_OP_SRA_B, alu_sra, cpu->reg.b);
			CB_BIT_OP(CPU_OP_SRA_C, alu_sra, cpu->reg.c);
			CB_BIT_OP(CPU_OP_SRA_D, alu_sra, cpu->reg.d);
			CB_BIT_OP(CPU_OP_SRA_E, alu_sra, cpu->reg.e);
			CB_BIT_OP(CPU_OP_SRA_H, alu_sra, cpu->reg.h);
			CB_BIT_OP(CPU_OP_SRA_L, alu_sra, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_SRA_MEM_HL, alu_sra);
			CB_BIT_OP(CPU_OP_SRA_A, alu_sra, cpu->reg.a);
			CB_BIT_OP(CPU_OP_SWAP_B, alu_swap, cpu->reg.b);
			CB_BIT_OP(CPU_OP_SWAP_C, alu_swap, cpu->reg.c);
			CB_BIT_OP(CPU_OP_SWAP_D, alu_swap, cpu->reg.d);
			CB_BIT_OP(CPU_OP_SWAP_E, alu_swap, cpu->reg.e);
			CB_BIT_OP(CPU_OP_SWAP_H, alu_swap, cpu->reg.h);
			CB_BIT_OP(CPU_OP_SWAP_L, alu_swap, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_SWAP_MEM_HL, alu_swap);
			CB_BIT_OP(CPU_OP_SWAP_A, alu_swap, cpu->reg.a);
			CB_BIT_OP(CPU_OP_SRL_B, alu_srl, cpu->reg.b);
			CB_BIT_OP(CPU_OP_SRL_C, alu_srl, cpu->reg.c);
			CB_BIT_OP(CPU_OP_SRL_D, alu_srl, cpu->reg.d);
			CB_BIT_OP(CPU_OP_SRL_E, alu_srl, cpu->reg.e);
			CB_BIT_OP(CPU_OP_SRL_H, alu_srl, cpu->reg.h);
			CB_BIT_OP(CPU_OP_SRL_L, alu_srl, cpu->reg.l);
			CB_BIT_OP_HL(CPU_OP_SRL_MEM_HL, alu_srl);
			CB_BIT_OP(CPU_OP_SRL_A, alu_srl, cpu->reg.a);
			BIT(CPU_OP_BIT_0_B, 0, cpu->reg.b);
			BIT(CPU_OP_BIT_0_C, 0, cpu->reg.c);
			BIT(CPU_OP_BIT_0_D, 0, cpu->reg.d);
			BIT(CPU_OP_BIT_0_E, 0, cpu->reg.e);
			BIT(CPU_OP_BIT_0_H, 0, cpu->reg.h);
			BIT(CPU_OP_BIT_0_L, 0, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_0_MEM_HL, 0);
			BIT(CPU_OP_BIT_0_A, 0, cpu->reg.a);
			BIT(CPU_OP_BIT_1_B, 1, cpu->reg.b);
			BIT(CPU_OP_BIT_1_C, 1, cpu->reg.c);
			BIT(CPU_OP_BIT_1_D, 1, cpu->reg.d);
			BIT(CPU_OP_BIT_1_E, 1, cpu->reg.e);
			BIT(CPU_OP_BIT_1_H, 1, cpu->reg.h);
			BIT(CPU_OP_BIT_1_L, 1, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_1_MEM_HL, 1);
			BIT(CPU_OP_BIT_1_A, 1, cpu->reg.a);
			BIT(CPU_OP_BIT_2_B, 2, cpu->reg.b);
			BIT(CPU_OP_BIT_2_C, 2, cpu->reg.c);
			BIT(CPU_OP_BIT_2_D, 2, cpu->reg.d);
			BIT(CPU_OP_BIT_2_E, 2, cpu->reg.e);
			BIT(CPU_OP_BIT_2_H, 2, cpu->reg.h);
			BIT(CPU_OP_BIT_2_L, 2, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_2_MEM_HL, 2);
			BIT(CPU_OP_BIT_2_A, 2, cpu->reg.a);
			BIT(CPU_OP_BIT_3_B, 3, cpu->reg.b);
			BIT(CPU_OP_BIT_3_C, 3, cpu->reg.c);
			BIT(CPU_OP_BIT_3_D, 3, cpu->reg.d);
			BIT(CPU_OP_BIT_3_E, 3, cpu->reg.e);
			BIT(CPU_OP_BIT_3_H, 3, cpu->reg.h);
			BIT(CPU_OP_BIT_3_L, 3, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_3_MEM_HL, 3);
			BIT(CPU_OP_BIT_3_A, 3, cpu->reg.a);
			BIT(CPU_OP_BIT_4_B, 4, cpu->reg.b);
			BIT(CPU_OP_BIT_4_C, 4, cpu->reg.c);
			BIT(CPU_OP_BIT_4_D, 4, cpu->reg.d);
			BIT(CPU_OP_BIT_4_E, 4, cpu->reg.e);
			BIT(CPU_OP_BIT_4_H, 4, cpu->reg.h);
			BIT(CPU_OP_BIT_4_L, 4, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_4_MEM_HL, 4);
			BIT(CPU_OP_BIT_4_A, 4, cpu->reg.a);
			BIT(CPU_OP_BIT_5_B, 5, cpu->reg.b);
			BIT(CPU_OP_BIT_5_C, 5, cpu->reg.c);
			BIT(CPU_OP_BIT_5_D, 5, cpu->reg.d);
			BIT(CPU_OP_BIT_5_E, 5, cpu->reg.e);
			BIT(CPU_OP_BIT_5_H, 5, cpu->reg.h);
			BIT(CPU_OP_BIT_5_L, 5, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_5_MEM_HL, 5);
			BIT(CPU_OP_BIT_5_A, 5, cpu->reg.a);
			BIT(CPU_OP_BIT_6_B, 6, cpu->reg.b);
			BIT(CPU_OP_BIT_6_C, 6, cpu->reg.c);
			BIT(CPU_OP_BIT_6_D, 6, cpu->reg.d);
			BIT(CPU_OP_BIT_6_E, 6, cpu->reg.e);
			BIT(CPU_OP_BIT_6_H, 6, cpu->reg.h);
			BIT(CPU_OP_BIT_6_L, 6, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_6_MEM_HL, 6);
			BIT(CPU_OP_BIT_6_A, 6, cpu->reg.a);
			BIT(CPU_OP_BIT_7_B, 7, cpu->reg.b);
			BIT(CPU_OP_BIT_7_C, 7, cpu->reg.c);
			BIT(CPU_OP_BIT_7_D, 7, cpu->reg.d);
			BIT(CPU_OP_BIT_7_E, 7, cpu->reg.e);
			BIT(CPU_OP_BIT_7_H, 7, cpu->reg.h);
			BIT(CPU_OP_BIT_7_L, 7, cpu->reg.l);
			BIT_HL(CPU_OP_BIT_7_MEM_HL, 7);
			BIT(CPU_OP_BIT_7_A, 7, cpu->reg.a);
			RES(CPU_OP_RES_0_B, 0, cpu->reg.b);
			RES(CPU_OP_RES_0_C, 0, cpu->reg.c);
			RES(CPU_OP_RES_0_D, 0, cpu->reg.d);
			RES(CPU_OP_RES_0_E, 0, cpu->reg.e);
			RES(CPU_OP_RES_0_H, 0, cpu->reg.h);
			RES(CPU_OP_RES_0_L, 0, cpu->reg.l);
			RES_HL(CPU_OP_RES_0_MEM_HL, 0);
			RES(CPU_OP_RES_0_A, 0, cpu->reg.a);
			RES(CPU_OP_RES_1_B, 1, cpu->reg.b);
			RES(CPU_OP_RES_1_C, 1, cpu->reg.c);
			RES(CPU_OP_RES_1_D, 1, cpu->reg.d);
			RES(CPU_OP_RES_1_E, 1, cpu->reg.e);
			RES(CPU_OP_RES_1_H, 1, cpu->reg.h);
			RES(CPU_OP_RES_1_L, 1, cpu->reg.l);
			RES_HL(CPU_OP_RES_1_MEM_HL, 1);
			RES(CPU_OP_RES_1_A, 1, cpu->reg.a);
			RES(CPU_OP_RES_2_B, 2, cpu->reg.b);
			RES(CPU_OP_RES_2_C, 2, cpu->reg.c);
			RES(CPU_OP_RES_2_D, 2, cpu->reg.d);
			RES(CPU_OP_RES_2_E, 2, cpu->reg.e);
			RES(CPU_OP_RES_2_H, 2, cpu->reg.h);
			RES(CPU_OP_RES_2_L, 2, cpu->reg.l);
			RES_HL(CPU_OP_RES_2_MEM_HL, 2);
			RES(CPU_OP_RES_2_A, 2, cpu->reg.a);
			RES(CPU_OP_RES_3_B, 3, cpu->reg.b);
			RES(CPU_OP_RES_3_C, 3, cpu->reg.c);
			RES(CPU_OP_RES_3_D, 3, cpu->reg.d);
			RES(CPU_OP_RES_3_E, 3, cpu->reg.e);
			RES(CPU_OP_RES_3_H, 3, cpu->reg.h);
			RES(CPU_OP_RES_3_L, 3, cpu->reg.l);
			RES_HL(CPU_OP_RES_3_MEM_HL, 3);
			RES(CPU_OP_RES_3_A, 3, cpu->reg.a);
			RES(CPU_OP_RES_4_B, 4, cpu->reg.b);
			RES(CPU_OP_RES_4_C, 4, cpu->reg.c);
			RES(CPU_OP_RES_4_D, 4, cpu->reg.d);
			RES(CPU_OP_RES_4_E, 4, cpu->reg.e);
			RES(CPU_OP_RES_4_H, 4, cpu->reg.h);
			RES(CPU_OP_RES_4_L, 4, cpu->reg.l);
			RES_HL(CPU_OP_RES_4_MEM_HL, 4);
			RES(CPU_OP_RES_4_A, 4, cpu->reg.a);
			RES(CPU_OP_RES_5_B, 5, cpu->reg.b);
			RES(CPU_OP_RES_5_C, 5, cpu->reg.c);
			RES(CPU_OP_RES_5_D, 5, cpu->reg.d);
			RES(CPU_OP_RES_5_E, 5, cpu->reg.e);
			RES(CPU_OP_RES_5_H, 5, cpu->reg.h);
			RES(CPU_OP_RES_5_L, 5, cpu->reg.l);
			RES_HL(CPU_OP_RES_5_MEM_HL, 5);
			RES(CPU_OP_RES_5_A, 5, cpu->reg.a);
			RES(CPU_OP_RES_6_B, 6, cpu->reg.b);
			RES(CPU_OP_RES_6_C, 6, cpu->reg.c);
			RES(CPU_OP_RES_6_D, 6, cpu->reg.d);
			RES(CPU_OP_RES_6_E, 6, cpu->reg.e);
			RES(CPU_OP_RES_6_H, 6, cpu->reg.h);
			RES(CPU_OP_RES_6_L, 6, cpu->reg.l);
			RES_HL(CPU_OP_RES_6_MEM_HL, 6);
			RES(CPU_OP_RES_6_A, 6, cpu->reg.a);
			RES(CPU_OP_RES_7_B, 7, cpu->reg.b);
			RES(CPU_OP_RES_7_C, 7, cpu->reg.c);
			RES(CPU_OP_RES_7_D, 7, cpu->reg.d);
			RES(CPU_OP_RES_7_E, 7, cpu->reg.e);
			RES(CPU_OP_RES_7_H, 7, cpu->reg.h);
			RES(CPU_OP_RES_7_L, 7, cpu->reg.l);
			RES_HL(CPU_OP_RES_7_MEM_HL, 7);
			RES(CPU_OP_RES_7_A, 7, cpu->reg.a);
			SET(CPU_OP_SET_0_B, 0, cpu->reg.b);
			SET(CPU_OP_SET_0_C, 0, cpu->reg.c);
			SET(CPU_OP_SET_0_D, 0, cpu->reg.d);
			SET(CPU_OP_SET_0_E, 0, cpu->reg.e);
			SET(CPU_OP_SET_0_H, 0, cpu->reg.h);
			SET(CPU_OP_SET_0_L, 0, cpu->reg.l);
			SET_HL(CPU_OP_SET_0_MEM_HL, 0);
			SET(CPU_OP_SET_0_A, 0, cpu->reg.a);
			SET(CPU_OP_SET_1_B, 1, cpu->reg.b);
			SET(CPU_OP_SET_1_C, 1, cpu->reg.c);
			SET(CPU_OP_SET_1_D, 1, cpu->reg.d);
			SET(CPU_OP_SET_1_E, 1, cpu->reg.e);
			SET(CPU_OP_SET_1_H, 1, cpu->reg.h);
			SET(CPU_OP_SET_1_L, 1, cpu->reg.l);
			SET_HL(CPU_OP_SET_1_MEM_HL, 1);
			SET(CPU_OP_SET_1_A, 1, cpu->reg.a);
			SET(CPU_OP_SET_2_B, 2, cpu->reg.b);
			SET(CPU_OP_SET_2_C, 2, cpu->reg.c);
			SET(CPU_OP_SET_2_D, 2, cpu->reg.d);
			SET(CPU_OP_SET_2_E, 2, cpu->reg.e);
			SET(CPU_OP_SET_2_H, 2, cpu->reg.h);
			SET(CPU_OP_SET_2_L, 2, cpu->reg.l);
			SET_HL(CPU_OP_SET_2_MEM_HL, 2);
			SET(CPU_OP_SET_2_A, 2, cpu->reg.a);
			SET(CPU_OP_SET_3_B, 3, cpu->reg.b);
			SET(CPU_OP_SET_3_C, 3, cpu->reg.c);
			SET(CPU_OP_SET_3_D, 3, cpu->reg.d);
			SET(CPU_OP_SET_3_E, 3, cpu->reg.e);
			SET(CPU_OP_SET_3_H, 3, cpu->reg.h);
			SET(CPU_OP_SET_3_L, 3, cpu->reg.l);
			SET_HL(CPU_OP_SET_3_MEM_HL, 3);
			SET(CPU_OP_SET_3_A, 3, cpu->reg.a);
			SET(CPU_OP_SET_4_B, 4, cpu->reg.b);
			SET(CPU_OP_SET_4_C, 4, cpu->reg.c);
			SET(CPU_OP_SET_4_D, 4, cpu->reg.d);
			SET(CPU_OP_SET_4_E, 4, cpu->reg.e);
			SET(CPU_OP_SET_4_H, 4, cpu->reg.h);
			SET(CPU_OP_SET_4_L, 4, cpu->reg.l);
			SET_HL(CPU_OP_SET_4_MEM_HL, 4);
			SET(CPU_OP_SET_4_A, 4, cpu->reg.a);
			SET(CPU_OP_SET_5_B, 5, cpu->reg.b);
			SET(CPU_OP_SET_5_C, 5, cpu->reg.c);
			SET(CPU_OP_SET_5_D, 5, cpu->reg.d);
			SET(CPU_OP_SET_5_E, 5, cpu->reg.e);
			SET(CPU_OP_SET_5_H, 5, cpu->reg.h);
			SET(CPU_OP_SET_5_L, 5, cpu->reg.l);
			SET_HL(CPU_OP_SET_5_MEM_HL, 5);
			SET(CPU_OP_SET_5_A, 5, cpu->reg.a);
			SET(CPU_OP_SET_6_B, 6, cpu->reg.b);
			SET(CPU_OP_SET_6_C, 6, cpu->reg.c);
			SET(CPU_OP_SET_6_D, 6, cpu->reg.d);
			SET(CPU_OP_SET_6_E, 6, cpu->reg.e);
			SET(CPU_OP_SET_6_H, 6, cpu->reg.h);
			SET(CPU_OP_SET_6_L, 6, cpu->reg.l);
			SET_HL(CPU_OP_SET_6_MEM_HL, 6);
			SET(CPU_OP_SET_6_A, 6, cpu->reg.a);
			SET(CPU_OP_SET_7_B, 7, cpu->reg.b);
			SET(CPU_OP_SET_7_C, 7, cpu->reg.c);
			SET(CPU_OP_SET_7_D, 7, cpu->reg.d);
			SET(CPU_OP_SET_7_E, 7, cpu->reg.e);
			SET(CPU_OP_SET_7_H, 7, cpu->reg.h);
			SET(CPU_OP_SET_7_L, 7, cpu->reg.l);
			SET_HL(CPU_OP_SET_7_MEM_HL, 7);
			SET(CPU_OP_SET_7_A, 7, cpu->reg.a);
			default: UNREACHABLE;
		}
		return;
	}

	CALL_CC(CPU_OP_CALL_Z_U16, cpu->reg.f & CPU_ZERO_FLAG);
	CALL_CC(CPU_OP_CALL_U16, true);

	case CPU_OP_ADC_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_adc(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_08, 0x0008);
	RET_CC(CPU_OP_RET_NC, !(cpu->reg.f & CPU_CARRY_FLAG));
	POP(CPU_OP_POP_DE, cpu->reg.de);
	JP_CC(CPU_OP_JP_NC_U16, !(cpu->reg.f & CPU_CARRY_FLAG));
	CALL_CC(CPU_OP_CALL_NC_U16, !(cpu->reg.f & CPU_CARRY_FLAG));
	PUSH(CPU_OP_PUSH_DE, cpu->reg.de);

	case CPU_OP_SUB_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_sub(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_10, 0x0010);
	RET_CC(CPU_OP_RET_C, cpu->reg.f & CPU_CARRY_FLAG);
	RET_CC(CPU_OP_RETI, true); // FIX!
	JP_CC(CPU_OP_JP_C_U16, cpu->reg.f & CPU_CARRY_FLAG);
	CALL_CC(CPU_OP_CALL_C_U16, cpu->reg.f & CPU_CARRY_FLAG);

	case CPU_OP_SBC_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_sbc(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_18, 0x0018);

	case LD_MEM_FF00_U8_A: {
		const uint16_t u16 = 0xFF00 + read_u8(cpu);

		agoge_bus_write(cpu->bus, u16, cpu->reg.a);
		return;
	}

	POP(CPU_OP_POP_HL, cpu->reg.hl);

	case CPU_OP_LD_MEM_FF00_C_A:
		agoge_bus_write(cpu->bus, 0xFF00 + cpu->reg.c, cpu->reg.a);
		return;

	PUSH(CPU_OP_PUSH_HL, cpu->reg.hl);

	case CPU_OP_AND_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_and(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_20, 0x0020);

	case CPU_OP_ADD_SP_S8:
		cpu->reg.sp = alu_add_sp(cpu);
		return;

	case CPU_OP_JP_HL:
		cpu->reg.pc = cpu->reg.hl;
		return;

	case CPU_OP_LD_MEM_U16_A: {
		const uint16_t u16 = read_u16(cpu);
		agoge_bus_write(cpu->bus, u16, cpu->reg.a);

		return;
	}

	case CPU_OP_XOR_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_xor(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_28, 0x0028);

	case CPU_OP_LD_A_MEM_FF00_U8: {
		const uint8_t u8 = read_u8(cpu);
		cpu->reg.a = agoge_bus_read(cpu->bus, 0xFF00 + u8);

		return;
	}

	case CPU_OP_POP_AF:
		cpu->reg.af = stack_pop(cpu) & ~0x0F;
		return;

	case CPU_OP_LD_A_MEM_FF00_C:
		cpu->reg.a = agoge_bus_read(cpu->bus, 0xFF00 + cpu->reg.c);
		return;

	case CPU_OP_DI:
		return;

	PUSH(CPU_OP_PUSH_AF, cpu->reg.af);

	case CPU_OP_OR_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_or(cpu, u8);
		return;
	}

	RST(CPU_OP_RST_30, 0x0030);

	case CPU_OP_LD_HL_SP_S8:
		cpu->reg.hl = alu_add_sp(cpu);
		return;

	case CPU_OP_LD_SP_HL:
		cpu->reg.sp = cpu->reg.hl;
		return;

	case CPU_OP_LD_A_MEM_U16: {
		const uint16_t u16 = read_u16(cpu);

		cpu->reg.a = agoge_bus_read(cpu->bus, u16);
		return;
	}

	case CPU_OP_CP_A_U8: {
		const uint8_t subtrahend = read_u8(cpu);

		alu_cp(cpu, subtrahend);
		return;
	}

	RST(CPU_OP_RST_38, 0x0038);

	default:
		LOG_ERR(cpu->bus->log,
			"Illegal instruction $%02X trapped at program counter "
			"$%04X.",
			instr, cpu->reg.pc);
		return;
	}
}
