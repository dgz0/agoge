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

#include "agoge/cpu.h"
#include <assert.h>
#include <stddef.h>

#include "agoge-cpu-defs.h"
#include "agoge-cpu.h"
#include "agoge-bus.h"
#include "agoge-log.h"

static bool zero_flag_is_set(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);
	return cpu->reg.f & (UINT8_C(1) << 7);
}

static void zero_flag_set(struct agoge_cpu *const cpu, const bool condition)
{
	if (condition == 0) {
		cpu->reg.f |= (UINT8_C(1) << 7);
	} else {
		cpu->reg.f &= ~(UINT8_C(1) << 7);
	}
}

static void carry_flag_set(struct agoge_cpu *const cpu, const bool condition)
{
	if (condition) {
		cpu->reg.f |= (UINT8_C(1) << 4);
	} else {
		cpu->reg.f &= ~(UINT8_C(1) << 4);
	}
}

static uint8_t alu_inc(struct agoge_cpu *const cpu, uint8_t reg)
{
	assert(cpu != NULL);

	zero_flag_set(cpu, ++reg);
	return reg;
}

static uint8_t alu_dec(struct agoge_cpu *const cpu, uint8_t reg)
{
	assert(cpu != NULL);

	zero_flag_set(cpu, --reg);
	return reg;
}

static uint8_t read_u8(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);
	return agoge_bus_read(cpu->bus, cpu->reg.pc++);
}

static uint16_t read_u16(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);

	const uint8_t lo = read_u8(cpu);
	const uint8_t hi = read_u8(cpu);

	return (uint16_t)((hi << 8) | lo);
}

static void stack_push(struct agoge_cpu *const cpu, const uint16_t data)
{
	agoge_bus_write(cpu->bus, --cpu->reg.sp, data >> 8);
	agoge_bus_write(cpu->bus, --cpu->reg.sp, data & 0x00FF);
}

static uint16_t stack_pop(struct agoge_cpu *const cpu)
{
	const uint8_t lo = agoge_bus_read(cpu->bus, cpu->reg.sp++);
	const uint8_t hi = agoge_bus_read(cpu->bus, cpu->reg.sp++);

	return (hi << 8) | lo;
}

static void jr_if(struct agoge_cpu *const cpu, const bool condition)
{
	assert(cpu != NULL);

	const int8_t offset = (int8_t)read_u8(cpu);

	if (condition) {
		cpu->reg.pc += offset;
	}
}

static void ret_if(struct agoge_cpu *const cpu, const bool condition)
{
	assert(cpu != NULL);

	if (condition) {
		cpu->reg.pc = stack_pop(cpu);
	}
}

static void call_if(struct agoge_cpu *const cpu, const bool condition)
{
	assert(cpu != NULL);

	const uint16_t address = read_u16(cpu);

	if (condition) {
		stack_push(cpu, cpu->reg.pc);
		cpu->reg.pc = address;
	}
}

static uint8_t alu_srl(struct agoge_cpu *const cpu, uint8_t val)
{
	const bool carry = val & 1;

	val >>= 1;

	zero_flag_set(cpu, val);
	carry_flag_set(cpu, carry);

	return val;
}

static uint8_t alu_rr_helper(struct agoge_cpu *const cpu, uint8_t val)
{
	assert(cpu != NULL);

	const bool old_carry = val & 1;
	const bool carry = cpu->reg.f & (UINT8_C(1) << 4);

	val = (val >> 1) | (carry << 7);
	carry_flag_set(cpu, old_carry);

	return val;
}

static uint8_t alu_rr(struct agoge_cpu *const cpu, const uint8_t val)
{
	const uint8_t result = alu_rr_helper(cpu, val);
	zero_flag_set(cpu, result);

	return result;
}

static void alu_add(struct agoge_cpu *const cpu, const uint8_t addend)
{
	assert(cpu != NULL);

	const unsigned int result = cpu->reg.a + addend;
	const uint8_t sum = (uint8_t)result;

	zero_flag_set(cpu, sum);
	carry_flag_set(cpu, result > UINT8_MAX);

	cpu->reg.a = sum;
}

static void alu_add_hl(struct agoge_cpu *const cpu, const uint16_t addend)
{
	const unsigned int sum = cpu->reg.hl + addend;

	carry_flag_set(cpu, sum > UINT16_MAX);
	cpu->reg.hl = (uint16_t)sum;
}

static uint8_t alu_sub(struct agoge_cpu *const cpu, const uint8_t subtrahend)
{
	assert(cpu != NULL);

	const int result = cpu->reg.a - subtrahend;

	const uint8_t diff = (uint8_t)result;

	zero_flag_set(cpu, diff);
	carry_flag_set(cpu, result < 0);

	return diff;
}

static void alu_cp(struct agoge_cpu *const cpu, const uint8_t subtrahend)
{
	(void)alu_sub(cpu, subtrahend);
}

static void alu_xor(struct agoge_cpu *const cpu, const uint8_t val)
{
	assert(cpu != NULL);

	cpu->reg.a ^= val;
	cpu->reg.f = (!cpu->reg.a) ? 0x80 : 0x00;
}

static void alu_and(struct agoge_cpu *const cpu, const uint8_t val)
{
	assert(cpu != NULL);

	cpu->reg.a &= val;
	cpu->reg.f = (!cpu->reg.a) ? 0xA0 : 0x20;
}

static void alu_or(struct agoge_cpu *const cpu, const uint8_t reg)
{
	assert(cpu != NULL);

	cpu->reg.a |= reg;
	cpu->reg.f = (!cpu->reg.a) ? 0x80 : 0x00;
}

void agoge_cpu_reset(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);

	cpu->reg.bc = CPU_PWRUP_REG_BC;
	cpu->reg.de = CPU_PWRUP_REG_DE;
	cpu->reg.hl = CPU_PWRUP_REG_HL;
	cpu->reg.af = CPU_PWRUP_REG_AF;
	cpu->reg.pc = CPU_PWRUP_REG_PC;
	cpu->reg.sp = CPU_PWRUP_REG_SP;
}

void agoge_cpu_step(struct agoge_cpu *const cpu)
{
	assert(cpu != NULL);

	const uint8_t instr = read_u8(cpu);

	switch (instr) {
	case CPU_OP_NOP:
		return;

	case CPU_OP_LD_BC_U16:
		cpu->reg.bc = read_u16(cpu);
		return;

	case CPU_OP_INC_BC:
		cpu->reg.bc++;
		return;

	case CPU_OP_DEC_B:
		cpu->reg.b = alu_dec(cpu, cpu->reg.b);
		return;

	case CPU_OP_INC_DE:
		cpu->reg.de++;
		return;

	case CPU_OP_ADD_HL_DE:
		alu_add_hl(cpu, cpu->reg.de);
		return;

	case CPU_OP_XOR_A_C:
		alu_xor(cpu, cpu->reg.c);
		return;

	case CPU_OP_DEC_C:
		cpu->reg.c = alu_dec(cpu, cpu->reg.c);
		return;

	case CPU_OP_LD_A_MEM_DE:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.de);
		return;

	case CPU_OP_INC_HL:
		cpu->reg.hl++;
		return;

	case CPU_OP_LD_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.a);
		return;

	case CPU_OP_INC_H:
		cpu->reg.h = alu_inc(cpu, cpu->reg.h);
		return;

	case CPU_OP_INC_L:
		cpu->reg.l = alu_inc(cpu, cpu->reg.l);
		return;

	case CPU_OP_LDI_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl++, cpu->reg.a);
		return;

	case CPU_OP_LD_B_U8:
		cpu->reg.b = read_u8(cpu);
		return;

	case CPU_OP_LD_C_U8:
		cpu->reg.c = read_u8(cpu);
		return;

	case CPU_OP_LD_DE_U16:
		cpu->reg.de = read_u16(cpu);
		return;

	case CPU_OP_SUB_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		cpu->reg.a = alu_sub(cpu, u8);
		return;
	}

	case CPU_OP_OR_A_A:
		alu_or(cpu, cpu->reg.a);
		return;

	case CPU_OP_PUSH_DE:
		stack_push(cpu, cpu->reg.de);
		return;

	case CPU_OP_RRA:
		cpu->reg.a = alu_rr_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~(UINT8_C(1) << 0);
		return;

	case CPU_OP_DEC_L:
		cpu->reg.l = alu_dec(cpu, cpu->reg.l);
		return;

	case CPU_OP_LD_C_MEM_HL:
		cpu->reg.c = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_B_MEM_HL:
		cpu->reg.b = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_D_MEM_HL:
		cpu->reg.d = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LDD_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl--, cpu->reg.a);
		return;

	case CPU_OP_ADD_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_add(cpu, u8);
		return;
	}

	case CPU_OP_LD_H_U8:
		cpu->reg.h = read_u8(cpu);
		return;

	case CPU_OP_XOR_A_MEM_HL: {
		const uint8_t u8 = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_xor(cpu, u8);
		break;
	}

	case CPU_OP_INC_D:
		cpu->reg.d = alu_inc(cpu, cpu->reg.d);
		return;

	case CPU_OP_LD_SP_U16:
		cpu->reg.sp = read_u16(cpu);
		return;

	case CPU_OP_INC_E:
		cpu->reg.e = alu_inc(cpu, cpu->reg.e);
		return;

	case CPU_OP_LD_MEM_DE_A:
		agoge_bus_write(cpu->bus, cpu->reg.de, cpu->reg.a);
		return;

	case CPU_OP_JR_S8:
		jr_if(cpu, true);
		return;

	case CPU_OP_POP_BC:
		cpu->reg.bc = stack_pop(cpu);
		return;

	case CPU_OP_JR_NZ_S8:
		jr_if(cpu, !zero_flag_is_set(cpu));
		return;

	case CPU_OP_LD_HL_U16:
		cpu->reg.hl = read_u16(cpu);
		return;

	case CPU_OP_LDI_A_MEM_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl++);
		return;

	case CPU_OP_JR_Z_S8:
		jr_if(cpu, zero_flag_is_set(cpu));
		return;

	case CPU_OP_OR_A_C:
		alu_or(cpu, cpu->reg.c);
		return;

	case CPU_OP_LD_B_A:
		cpu->reg.b = cpu->reg.a;
		return;

	case CPU_OP_LD_A_H:
		cpu->reg.a = cpu->reg.h;
		return;

	case CPU_OP_LD_A_L:
		cpu->reg.a = cpu->reg.l;
		return;

	case CPU_OP_LD_A_B:
		cpu->reg.a = cpu->reg.b;
		return;

	case CPU_OP_JP_U16:
		cpu->reg.pc = read_u16(cpu);
		return;

	case CPU_OP_RET:
		ret_if(cpu, true);
		return;

	case CPU_OP_CALL_U16:
		call_if(cpu, true);
		return;

	case CPU_OP_LD_MEM_U16_A: {
		const uint16_t u16 = read_u16(cpu);
		agoge_bus_write(cpu->bus, u16, cpu->reg.a);

		return;
	}

	case CPU_OP_PUSH_HL:
		stack_push(cpu, cpu->reg.hl);
		return;

	case CPU_OP_POP_HL:
		cpu->reg.hl = stack_pop(cpu);
		return;

	case CPU_OP_DI:
		return;

	case CPU_OP_PREFIX_CB: {
		const uint8_t cb_instr = read_u8(cpu);

		switch (cb_instr) {
		case CPU_OP_RR_C:
			cpu->reg.c = alu_rr(cpu, cpu->reg.c);
			return;

		case CPU_OP_RR_D:
			cpu->reg.d = alu_rr(cpu, cpu->reg.d);
			return;

		case CPU_OP_SRL_B:
			cpu->reg.b = alu_srl(cpu, cpu->reg.b);
			return;

		default:
			LOG_ERR(cpu->bus->log,
				"Illegal instruction CB:$%02X trapped at "
				"program counter $%04X.",
				cb_instr, cpu->reg.pc);
			break;
		}
		return;
	}

	case CPU_OP_PUSH_AF:
		stack_push(cpu, cpu->reg.af);
		return;

	case CPU_OP_POP_AF:
		cpu->reg.af = stack_pop(cpu);
		return;

	case CPU_OP_LD_A_U8:
		cpu->reg.a = read_u8(cpu);
		return;

	case CPU_OP_PUSH_BC:
		stack_push(cpu, cpu->reg.bc);
		return;

	case LD_MEM_FF00_U8_A: {
		const uint16_t u16 = 0xFF00 + read_u8(cpu);

		agoge_bus_write(cpu->bus, u16, cpu->reg.a);
		return;
	}

	case CPU_OP_LD_A_MEM_FF00_U8: {
		const uint8_t u8 = read_u8(cpu);
		cpu->reg.a = agoge_bus_read(cpu->bus, 0xFF00 + u8);

		return;
	}

	case CPU_OP_LD_A_MEM_U16: {
		const uint16_t u16 = read_u16(cpu);

		cpu->reg.a = agoge_bus_read(cpu->bus, u16);
		return;
	}

	case CPU_OP_AND_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_and(cpu, u8);
		return;
	}

	case CPU_OP_CALL_NZ_U16:
		call_if(cpu, !zero_flag_is_set(cpu));
		return;

	case CPU_OP_CP_A_U8: {
		const uint8_t subtrahend = read_u8(cpu);

		alu_cp(cpu, subtrahend);
		return;
	}

	default:
		LOG_ERR(cpu->bus->log,
			"Illegal instruction $%02X trapped at program counter "
			"$%04X.",
			instr, cpu->reg.pc);
		return;
	}
}
