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

/// @file agoge-cpu.c Defines the implementation of the CPU interpreter.
///
/// The interpreter is implemented as a simple fetch-decode-execute scheme
/// without many tricks behind it.
///
/// For the sake of speed and to better exploit compiler optimizations, the
/// interpreter handles instruction execution with two `switch` statements; one
/// for non-CB prefixed instructions, and one for CB-prefixed instructions. Each
/// `case` label is the direct opcode value corresponding to a specific
/// instruction.
///
/// The tradeoff of this approach: there are 501 valid instructions, so there
/// are 501 cases to handle.

#include "agoge-bus.h"
#include "agoge-compiler.h"
#include "agoge-cpu.h"
#include "agoge-cpu-defs.h"
#include "agoge-log.h"
#include "agoge-sched.h"

/// Sets a bit in the Flag register (F) if a given condition is `true`, or
/// clears a bit in the Flag register (F) is a given condition is `false`.
///
/// @param cpu The current CPU instance.
/// @param condition The condition to set or clear a flag.
/// @param flag The flag (bit) to set or clear depending on `condition`.
NONNULL static void set_flag(struct agoge_cpu *const cpu, const bool condition,
			     const uint8_t flag)
{
	if (condition) {
		cpu->reg.f |= flag;
	} else {
		cpu->reg.f &= ~flag;
	}
}

/// Sets the Zero flag bit (Z) in the Flag register (F) if a value is zero, or
/// clears it if a value is non-zero.
///
/// @param cpu The current CPU instance.
/// @param val The value to check.
NONNULL static void zero_flag_set(struct agoge_cpu *const cpu,
				  const uint8_t val)
{
	set_flag(cpu, !val, CPU_ZERO_FLAG);
}

/// Sets the Carry flag bit (C) in the Flag register (F) if a condition is met,
/// or clears the Carry flag bit otherwise.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if we should set the Carry flag, or `false` to clear
/// it.
NONNULL static void carry_flag_set(struct agoge_cpu *const cpu,
				   const bool condition)
{
	set_flag(cpu, condition, CPU_CARRY_FLAG);
}

/// Sets the Half-Carry flag bit (H) in the Flag register (F) if a condition is
/// met, or clears the Half-Carry flag bit otherwise.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if we should set the Half-Carry flag, or `false` to
/// clear  it.
NONNULL static void half_carry_flag_set(struct agoge_cpu *const cpu,
					const bool condition)
{
	set_flag(cpu, condition, CPU_HALF_CARRY_FLAG);
}

/// Handles the `INC n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to increment.
/// @returns The incremented value.
NONNULL NODISCARD static uint8_t alu_inc(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;
	half_carry_flag_set(cpu, (val & 0x0F) == 0x0F);
	zero_flag_set(cpu, ++val);

	return val;
}

/// Handles the `DEC n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to decrement.
/// @returns The decremented value.
NONNULL NODISCARD static uint8_t alu_dec(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	cpu->reg.f |= CPU_SUBTRACT_FLAG;
	half_carry_flag_set(cpu, !(val & 0x0F));
	zero_flag_set(cpu, --val);

	return val;
}

/// Reads a byte from memory at the current program counter, then increments the
/// program counter.
///
/// @param cpu The current CPU instance.
/// @returns The byte from memory.
NONNULL NODISCARD static uint8_t read_u8(struct agoge_cpu *const cpu)
{
	return agoge_bus_read(cpu->bus, cpu->reg.pc++);
}

/// Reads two bytes from memory starting at the current program counter, then
/// increments the program counter by 2.
///
/// @param cpu The current CPU instance.
/// @returns The byte from memory.
NONNULL NODISCARD static uint16_t read_u16(struct agoge_cpu *const cpu)
{
	const uint8_t lo = read_u8(cpu);
	const uint8_t hi = read_u8(cpu);

	return (uint16_t)((hi << 8) | lo);
}

/// Pushes a 16-bit value to the stack.
///
/// @param cpu The current CPU instance.
/// @param val The value to push to the stack.
NONNULL static void stack_push(struct agoge_cpu *const cpu, const uint16_t val)
{
	agoge_bus_write(cpu->bus, --cpu->reg.sp, val >> 8);
	agoge_bus_write(cpu->bus, --cpu->reg.sp, val & 0x00FF);
}

/// Pops two bytes from the stack, forming a 16-bit value.
///
/// @param cpu The current CPU instance.
/// @returns The 16-bit value derived from the stack.
NONNULL NODISCARD static uint16_t stack_pop(struct agoge_cpu *const cpu)
{
	const uint8_t lo = agoge_bus_read(cpu->bus, cpu->reg.sp++);
	const uint8_t hi = agoge_bus_read(cpu->bus, cpu->reg.sp++);

	return (hi << 8) | lo;
}

/// Handles the `JR (CC), s8` instruction.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if the condition to execute the relative jump was
/// met, or `false` otherwise.
NONNULL static void jr_if(struct agoge_cpu *const cpu, const bool condition)
{
	const int8_t offset = (int8_t)read_u8(cpu);

	if (condition) {
		agoge_sched_step(cpu->bus->sched);
		cpu->reg.pc += offset;
	}
}

/// Handles the `RET (CC)` instruction.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if the condition to execute the return from
/// subroutine was met, or `false` otherwise.
NONNULL static void ret_if(struct agoge_cpu *const cpu, const bool condition)
{
	if (condition) {
		cpu->reg.pc = stack_pop(cpu);
	}
}

/// Handles the `CALL (CC), u16` instruction.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if the condition to call a subroutine was met,
/// or `false` otherwise.
NONNULL static void call_if(struct agoge_cpu *const cpu, const bool condition)
{
	const uint16_t address = read_u16(cpu);

	if (condition) {
		stack_push(cpu, cpu->reg.pc);
		cpu->reg.pc = address;
	}
}

/// Handles the `JP (CC), u16` instruction.
///
/// @param cpu The current CPU instance.
/// @param condition `true` if the condition to jump to the specified program
/// counter was met, or `false` otherwise.
NONNULL static void jp_if(struct agoge_cpu *const cpu, const bool condition)
{
	const uint16_t address = read_u16(cpu);

	if (condition) {
		cpu->reg.pc = address;
	}
}

/// Handles the `SWAP n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to swap.
/// @returns The swapped value.
NONNULL NODISCARD static uint8_t alu_swap(struct agoge_cpu *const cpu,
					  uint8_t val)
{
	val = ((val & 0x0F) << 4) | (val >> 4);
	cpu->reg.f = (!val) ? CPU_ZERO_FLAG : 0x00;

	return val;
}

/// Handles the `SRL n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to shift.
/// @returns The shifted value.
NONNULL NODISCARD static uint8_t alu_srl(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	const bool carry = val & 1;

	val >>= 1;

	zero_flag_set(cpu, val);
	carry_flag_set(cpu, carry);

	return val;
}

/// Handles the rotate left instruction functionality, where `n` is an 8-bit
/// value.
///
/// This function does not handle the Zero flag.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rl_helper(struct agoge_cpu *const cpu,
					       uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;

	carry_flag_set(cpu, val & (UINT8_C(1) << 7));

	val = (val << 1) | carry;
	return val;
}

/// Handles the `RL n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rl(struct agoge_cpu *const cpu,
					uint8_t val)
{
	val = alu_rl_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

/// Handles the rotate right instruction functionality, where `n` is an 8-bit
/// value.
///
/// This function does not handle the Zero flag.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
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

/// Handles the `RR n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rr(struct agoge_cpu *const cpu,
					const uint8_t val)
{
	const uint8_t result = alu_rr_helper(cpu, val);
	zero_flag_set(cpu, result);

	return result;
}

/// Handles the rotate left with carry instruction functionality, where `n` is
/// an 8-bit value.
///
/// This function does not handle the Zero flag.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rlc_helper(struct agoge_cpu *const cpu,
						uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & (UINT8_C(1) << 7));

	val = (val << 1) | (val >> 7);
	return val;
}

/// Handles the `RLC n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rlc(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	val = alu_rlc_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

/// Handles the rotate right with carry instruction functionality, where `n` is
/// an 8-bit value.
///
/// This function does not handle the Zero flag.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rrc_helper(struct agoge_cpu *const cpu,
						uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & 1);

	val = (val >> 1) | (val << 7);
	return val;
}

/// Handles the `RRC n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to rotate.
/// @returns The rotated value.
NONNULL NODISCARD static uint8_t alu_rrc(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	val = alu_rrc_helper(cpu, val);
	zero_flag_set(cpu, val);

	return val;
}

/// Handles the addition of the Accumulator (register A) and the stack pointer.
///
/// @param cpu The current CPU instance.
/// @returns The sum of the Accumulator (register A) and the stack pointer.
NONNULL NODISCARD static uint16_t alu_add_sp(struct agoge_cpu *const cpu)
{
	cpu->reg.f &= ~(CPU_ZERO_FLAG | CPU_SUBTRACT_FLAG);
	const int8_t s8 = (int8_t)read_u8(cpu);

	const int sum = cpu->reg.sp + s8;

	half_carry_flag_set(cpu, (cpu->reg.sp ^ s8 ^ sum) & 0x10);
	carry_flag_set(cpu, (cpu->reg.sp ^ s8 ^ sum) & 0x100);

	return (uint16_t)sum;
}

/// Handles the `ADD` and `ADC` instructions.
///
/// @param cpu The current CPU instance.
/// @param addend The value to add to the Accumulator (register A).
/// @param carry The value of the carry flag if this function should operate as
/// the `ADC` instruction.
NONNULL static void alu_add_helper(struct agoge_cpu *const cpu,
				   const uint8_t addend,
				   const unsigned int carry)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;

	const unsigned int result = cpu->reg.a + addend + carry;
	const uint8_t sum = (uint8_t)result;

	zero_flag_set(cpu, sum);
	half_carry_flag_set(cpu, (cpu->reg.a ^ addend ^ result) & 0x10);
	carry_flag_set(cpu, result > UINT8_MAX);

	cpu->reg.a = sum;
}

/// Handles the `ADD` instruction.
///
/// @param cpu The current CPU instance.
/// @param addend The value to add to the Accumulator (register A).
NONNULL static void alu_add(struct agoge_cpu *const cpu, const uint8_t addend)
{
	alu_add_helper(cpu, addend, 0);
}

/// Handles the `ADC` instruction.
///
/// @param cpu The current CPU instance.
/// @param addend The value to add to the Accumulator (register A).
NONNULL static void alu_adc(struct agoge_cpu *const cpu, const uint8_t addend)
{
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;
	alu_add_helper(cpu, addend, carry);
}

/// Handles the `ADD HL, nn` instruction, where `nn` is a 16-bit value.
///
/// @param cpu The current CPU instance.
/// @param addend The value to add to the Accumulator (register A).
NONNULL static void alu_add_hl(struct agoge_cpu *const cpu,
			       const uint16_t addend)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;

	const unsigned int sum = cpu->reg.hl + addend;

	half_carry_flag_set(cpu, (cpu->reg.hl ^ addend ^ sum) & 0x1000);
	carry_flag_set(cpu, sum > UINT16_MAX);

	cpu->reg.hl = (uint16_t)sum;
}

/// Handles the logic for the `SUB`, `SBC`, and `CP` instructions.
///
/// @param cpu The current CPU instance.
/// @param subtrahend The value to subtract from the Accumulator (register A).
/// @param carry The value of the carry flag if this function should operate as
/// the `SBC` instruction.
NONNULL static uint8_t alu_sub_helper(struct agoge_cpu *const cpu,
				      const uint8_t subtrahend,
				      const uint8_t carry)
{
	cpu->reg.f |= CPU_SUBTRACT_FLAG;

	const int result = cpu->reg.a - subtrahend - carry;

	const uint8_t diff = (uint8_t)result;

	zero_flag_set(cpu, diff);
	half_carry_flag_set(cpu, (cpu->reg.a ^ subtrahend ^ result) & 0x10);
	carry_flag_set(cpu, result < 0);

	return diff;
}

/// Implements the `SUB` instruction.
///
/// @param cpu The current CPU instance.
/// @param subtrahend The value to subtract from the Accumulator (register A).
NONNULL static void alu_sub(struct agoge_cpu *const cpu,
			    const uint8_t subtrahend)
{
	cpu->reg.a = alu_sub_helper(cpu, subtrahend, 0);
}

/// Implements the `SBC` instruction.
///
/// @param cpu The current CPU instance.
/// @param subtrahend The value to subtract from the Accumulator (register A).
NONNULL static void alu_sbc(struct agoge_cpu *const cpu,
			    const uint8_t subtrahend)
{
	const bool carry = cpu->reg.f & CPU_CARRY_FLAG;
	cpu->reg.a = alu_sub_helper(cpu, subtrahend, carry);
}

/// Implements the `CP` instruction.
///
/// @param cpu The current CPU instance.
/// @param subtrahend The value to subtract from the Accumulator (register A).
NONNULL static void alu_cp(struct agoge_cpu *const cpu,
			   const uint8_t subtrahend)
{
	(void)alu_sub_helper(cpu, subtrahend, 0);
}

/// Implements the `XOR n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to bitwise XOR against the Accumulator (register A).
NONNULL static void alu_xor(struct agoge_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a ^= val;
	cpu->reg.f = (!cpu->reg.a) ? CPU_ZERO_FLAG : 0x00;
}

/// Implements the `AND n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to bitwise AND against the Accumulator (register A).
NONNULL static void alu_and(struct agoge_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a &= val;
	cpu->reg.f = !cpu->reg.a ? (CPU_ZERO_FLAG | CPU_HALF_CARRY_FLAG) :
				   CPU_HALF_CARRY_FLAG;
}

/// Implements the `OR n` instruction, where `n` is an 8-bit value.
///
/// @param cpu The current CPU instance.
/// @param val The value to bitwise OR against the Accumulator (register A).
NONNULL static void alu_or(struct agoge_cpu *const cpu, const uint8_t val)
{
	cpu->reg.a |= val;
	cpu->reg.f = (!cpu->reg.a) ? CPU_ZERO_FLAG : 0x00;
}

/// Implements the `BIT b, n` instruction.
///
/// @param cpu The current CPU instance.
/// @param bit The bit to inspect.
/// @param val The value to check if bit is set or not.
NONNULL static void alu_bit(struct agoge_cpu *const cpu, const unsigned int bit,
			    const uint8_t val)
{
	cpu->reg.f &= ~CPU_SUBTRACT_FLAG;
	cpu->reg.f |= CPU_HALF_CARRY_FLAG;
	zero_flag_set(cpu, val & (UINT8_C(1) << bit));
}

/// Implements the `RST n` instruction.
///
/// @param cpu The current CPU instance.
/// @param vec The vector to jump to.
NONNULL static void rst(struct agoge_cpu *const cpu, const uint16_t vec)
{
	stack_push(cpu, cpu->reg.pc);
	cpu->reg.pc = vec;
}

/// Implements the `SLA n` instruction.
///
/// @param cpu The current CPU instance.
/// @param val The value to shift.
/// @returns The shifted value.
NONNULL NODISCARD static uint8_t alu_sla(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

	carry_flag_set(cpu, val & (UINT8_C(1) << 7));
	val <<= 1;
	zero_flag_set(cpu, val);

	return val;
}

/// Implements the `SRA n` instruction.
///
/// @param cpu The current CPU instance.
/// @param val The value to shift.
/// @returns The shifted value.
NONNULL NODISCARD static uint8_t alu_sra(struct agoge_cpu *const cpu,
					 uint8_t val)
{
	cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
	carry_flag_set(cpu, val & 1);

	val = (val >> 1) | (val & (UINT8_C(1) << 7));

	zero_flag_set(cpu, val);
	return val;
}

/// Resets the CPU interpreter to the state immediately following boot ROM
/// execution.
///
/// @param cpu The current CPU instance.
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
	const uint8_t instr = read_u8(cpu);

	switch (instr) {
	case CPU_OP_NOP:
		return;

	case CPU_OP_LD_BC_U16:
		cpu->reg.bc = read_u16(cpu);
		return;

	case CPU_OP_LD_MEM_BC_A:
		agoge_bus_write(cpu->bus, cpu->reg.bc, cpu->reg.a);
		return;

	case CPU_OP_INC_BC:
		cpu->reg.bc++;
		return;

	case CPU_OP_INC_B:
		cpu->reg.b = alu_inc(cpu, cpu->reg.b);
		return;

	case CPU_OP_DEC_B:
		cpu->reg.b = alu_dec(cpu, cpu->reg.b);
		return;

	case CPU_OP_LD_B_U8:
		cpu->reg.b = read_u8(cpu);
		return;

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

	case CPU_OP_ADD_HL_BC:
		alu_add_hl(cpu, cpu->reg.bc);
		return;

	case CPU_OP_LD_A_MEM_BC:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.bc);
		return;

	case CPU_OP_DEC_BC:
		cpu->reg.bc--;
		return;

	case CPU_OP_INC_C:
		cpu->reg.c = alu_inc(cpu, cpu->reg.c);
		return;

	case CPU_OP_DEC_C:
		cpu->reg.c = alu_dec(cpu, cpu->reg.c);
		return;

	case CPU_OP_LD_C_U8:
		cpu->reg.c = read_u8(cpu);
		return;

	case CPU_OP_RRCA:
		cpu->reg.a = alu_rrc_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	case CPU_OP_LD_DE_U16:
		cpu->reg.de = read_u16(cpu);
		return;

	case CPU_OP_LD_MEM_DE_A:
		agoge_bus_write(cpu->bus, cpu->reg.de, cpu->reg.a);
		return;

	case CPU_OP_INC_DE:
		cpu->reg.de++;
		return;

	case CPU_OP_INC_D:
		cpu->reg.d = alu_inc(cpu, cpu->reg.d);
		return;

	case CPU_OP_DEC_D:
		cpu->reg.d = alu_dec(cpu, cpu->reg.d);
		return;

	case CPU_OP_LD_D_U8:
		cpu->reg.d = read_u8(cpu);
		return;

	case CPU_OP_RLA:
		cpu->reg.a = alu_rl_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	case CPU_OP_JR_S8:
		jr_if(cpu, true);
		return;

	case CPU_OP_ADD_HL_DE:
		alu_add_hl(cpu, cpu->reg.de);
		return;

	case CPU_OP_LD_A_MEM_DE:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.de);
		return;

	case CPU_OP_DEC_DE:
		cpu->reg.de--;
		return;

	case CPU_OP_INC_E:
		cpu->reg.e = alu_inc(cpu, cpu->reg.e);
		return;

	case CPU_OP_DEC_E:
		cpu->reg.e = alu_dec(cpu, cpu->reg.e);
		return;

	case CPU_OP_LD_E_U8:
		cpu->reg.e = read_u8(cpu);
		return;

	case CPU_OP_RRA:
		cpu->reg.a = alu_rr_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	case CPU_OP_JR_NZ_S8:
		jr_if(cpu, !(cpu->reg.f & CPU_ZERO_FLAG));
		return;

	case CPU_OP_LD_HL_U16:
		cpu->reg.hl = read_u16(cpu);
		return;

	case CPU_OP_LDI_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl++, cpu->reg.a);
		return;

	case CPU_OP_INC_HL:
		cpu->reg.hl++;
		return;

	case CPU_OP_INC_H:
		cpu->reg.h = alu_inc(cpu, cpu->reg.h);
		return;

	case CPU_OP_DEC_H:
		cpu->reg.h = alu_dec(cpu, cpu->reg.h);
		return;

	case CPU_OP_LD_H_U8:
		cpu->reg.h = read_u8(cpu);
		return;

	case CPU_OP_DAA: {
		uint8_t correction = 0;

		if (cpu->reg.f & CPU_HALF_CARRY_FLAG) {
			correction |= 0x06;
		}

		if (cpu->reg.f & CPU_CARRY_FLAG) {
			correction |= 0x60;
		}

		if (!(cpu->reg.f & CPU_SUBTRACT_FLAG)) {
			if ((cpu->reg.a & 0x0F) > 0x09) {
				correction |= 0x06;
			}

			if (cpu->reg.a > 0x99) {
				correction |= 0x60;
			}
			cpu->reg.a += correction;
		} else {
			cpu->reg.a -= correction;
		}
		zero_flag_set(cpu, cpu->reg.a);
		carry_flag_set(cpu, correction & 0x60);
		cpu->reg.f &= ~CPU_HALF_CARRY_FLAG;

		return;
	}

	case CPU_OP_JR_Z_S8:
		jr_if(cpu, cpu->reg.f & CPU_ZERO_FLAG);
		return;

	case CPU_OP_ADD_HL_HL:
		alu_add_hl(cpu, cpu->reg.hl);
		return;

	case CPU_OP_LDI_A_MEM_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl++);
		return;

	case CPU_OP_DEC_HL:
		cpu->reg.hl--;
		return;

	case CPU_OP_INC_L:
		cpu->reg.l = alu_inc(cpu, cpu->reg.l);
		return;

	case CPU_OP_DEC_L:
		cpu->reg.l = alu_dec(cpu, cpu->reg.l);
		return;

	case CPU_OP_LD_L_U8:
		cpu->reg.l = read_u8(cpu);
		return;

	case CPU_OP_CPL:
		cpu->reg.a = ~cpu->reg.a;
		cpu->reg.f |= (CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

		return;

	case CPU_OP_JR_NC_S8:
		jr_if(cpu, !(cpu->reg.f & CPU_CARRY_FLAG));
		return;

	case CPU_OP_LD_SP_U16:
		cpu->reg.sp = read_u16(cpu);
		return;

	case CPU_OP_LDD_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl--, cpu->reg.a);
		return;

	case CPU_OP_INC_SP:
		cpu->reg.sp++;
		return;

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

	case CPU_OP_JR_C_S8:
		jr_if(cpu, cpu->reg.f & CPU_CARRY_FLAG);
		return;

	case CPU_OP_ADD_HL_SP:
		alu_add_hl(cpu, cpu->reg.sp);
		return;

	case CPU_OP_LDD_A_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl--);
		return;

	case CPU_OP_DEC_SP:
		cpu->reg.sp--;
		return;

	case CPU_OP_INC_A:
		cpu->reg.a = alu_inc(cpu, cpu->reg.a);
		return;

	case CPU_OP_DEC_A:
		cpu->reg.a = alu_dec(cpu, cpu->reg.a);
		return;

	case CPU_OP_LD_A_U8:
		cpu->reg.a = read_u8(cpu);
		return;

	case CPU_OP_CCF:
		cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
		cpu->reg.f ^= CPU_CARRY_FLAG;

		return;

	case CPU_OP_LD_B_B:
		cpu->reg.b = cpu->reg.b;
		return;

	case CPU_OP_LD_B_C:
		cpu->reg.b = cpu->reg.c;
		return;

	case CPU_OP_LD_B_D:
		cpu->reg.b = cpu->reg.d;
		return;

	case CPU_OP_LD_B_E:
		cpu->reg.b = cpu->reg.e;
		return;

	case CPU_OP_LD_B_H:
		cpu->reg.b = cpu->reg.h;
		return;

	case CPU_OP_LD_B_L:
		cpu->reg.b = cpu->reg.l;
		return;

	case CPU_OP_LD_B_MEM_HL:
		cpu->reg.b = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_B_A:
		cpu->reg.b = cpu->reg.a;
		return;

	case CPU_OP_LD_C_B:
		cpu->reg.c = cpu->reg.b;
		return;

	case CPU_OP_LD_C_C:
		cpu->reg.c = cpu->reg.c;
		return;

	case CPU_OP_LD_C_D:
		cpu->reg.c = cpu->reg.d;
		return;

	case CPU_OP_LD_C_E:
		cpu->reg.c = cpu->reg.e;
		return;

	case CPU_OP_LD_C_H:
		cpu->reg.c = cpu->reg.h;
		return;

	case CPU_OP_LD_C_L:
		cpu->reg.c = cpu->reg.l;
		return;

	case CPU_OP_LD_C_MEM_HL:
		cpu->reg.c = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_C_A:
		cpu->reg.c = cpu->reg.a;
		return;

	case CPU_OP_LD_D_B:
		cpu->reg.d = cpu->reg.b;
		return;

	case CPU_OP_LD_D_C:
		cpu->reg.d = cpu->reg.c;
		return;

	case CPU_OP_LD_D_D:
		cpu->reg.d = cpu->reg.d;
		return;

	case CPU_OP_LD_D_E:
		cpu->reg.d = cpu->reg.e;
		return;

	case CPU_OP_LD_D_H:
		cpu->reg.d = cpu->reg.h;
		return;

	case CPU_OP_LD_D_L:
		cpu->reg.d = cpu->reg.l;
		return;

	case CPU_OP_LD_D_MEM_HL:
		cpu->reg.d = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_D_A:
		cpu->reg.d = cpu->reg.a;
		return;

	case CPU_OP_LD_E_B:
		cpu->reg.e = cpu->reg.b;
		return;

	case CPU_OP_LD_E_C:
		cpu->reg.e = cpu->reg.c;
		return;

	case CPU_OP_LD_E_D:
		cpu->reg.e = cpu->reg.d;
		return;

	case CPU_OP_LD_E_E:
		cpu->reg.e = cpu->reg.e;
		return;

	case CPU_OP_LD_E_H:
		cpu->reg.e = cpu->reg.h;
		return;

	case CPU_OP_LD_E_L:
		cpu->reg.e = cpu->reg.l;
		return;

	case CPU_OP_LD_E_MEM_HL:
		cpu->reg.e = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_E_A:
		cpu->reg.e = cpu->reg.a;
		return;

	case CPU_OP_LD_H_B:
		cpu->reg.h = cpu->reg.b;
		return;

	case CPU_OP_LD_H_C:
		cpu->reg.h = cpu->reg.c;
		return;

	case CPU_OP_LD_H_D:
		cpu->reg.h = cpu->reg.d;
		return;

	case CPU_OP_LD_H_E:
		cpu->reg.h = cpu->reg.e;
		return;

	case CPU_OP_LD_H_H:
		cpu->reg.h = cpu->reg.h;
		return;

	case CPU_OP_LD_H_L:
		cpu->reg.h = cpu->reg.l;
		return;

	case CPU_OP_LD_H_MEM_HL:
		cpu->reg.h = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_H_A:
		cpu->reg.h = cpu->reg.a;
		return;

	case CPU_OP_LD_L_B:
		cpu->reg.l = cpu->reg.b;
		return;

	case CPU_OP_LD_L_C:
		cpu->reg.l = cpu->reg.c;
		return;

	case CPU_OP_LD_L_D:
		cpu->reg.l = cpu->reg.d;
		return;

	case CPU_OP_LD_L_E:
		cpu->reg.l = cpu->reg.e;
		return;

	case CPU_OP_LD_L_H:
		cpu->reg.l = cpu->reg.h;
		return;

	case CPU_OP_LD_L_L:
		cpu->reg.l = cpu->reg.l;
		return;

	case CPU_OP_LD_L_MEM_HL:
		cpu->reg.l = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_L_A:
		cpu->reg.l = cpu->reg.a;
		return;

	case CPU_OP_LD_MEM_HL_B:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.b);
		return;

	case CPU_OP_LD_MEM_HL_C:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.c);
		return;

	case CPU_OP_LD_MEM_HL_D:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.d);
		return;

	case CPU_OP_LD_MEM_HL_E:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.e);
		return;

	case CPU_OP_LD_MEM_HL_H:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.h);
		return;

	case CPU_OP_LD_MEM_HL_L:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.l);
		return;

	case CPU_OP_LD_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl, cpu->reg.a);
		return;

	case CPU_OP_LD_A_B:
		cpu->reg.a = cpu->reg.b;
		return;

	case CPU_OP_LD_A_C:
		cpu->reg.a = cpu->reg.c;
		return;

	case CPU_OP_LD_A_D:
		cpu->reg.a = cpu->reg.d;
		return;

	case CPU_OP_LD_A_E:
		cpu->reg.a = cpu->reg.e;
		return;

	case CPU_OP_LD_A_H:
		cpu->reg.a = cpu->reg.h;
		return;

	case CPU_OP_LD_A_L:
		cpu->reg.a = cpu->reg.l;
		return;

	case CPU_OP_LD_A_MEM_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl);
		return;

	case CPU_OP_LD_A_A:
		cpu->reg.a = cpu->reg.a;
		return;

	case CPU_OP_ADD_A_B:
		alu_add(cpu, cpu->reg.b);
		return;

	case CPU_OP_ADD_A_C:
		alu_add(cpu, cpu->reg.c);
		return;

	case CPU_OP_ADD_A_D:
		alu_add(cpu, cpu->reg.d);
		return;

	case CPU_OP_ADD_A_E:
		alu_add(cpu, cpu->reg.e);
		return;

	case CPU_OP_ADD_A_H:
		alu_add(cpu, cpu->reg.h);
		return;

	case CPU_OP_ADD_A_L:
		alu_add(cpu, cpu->reg.l);
		return;

	case CPU_OP_ADD_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		alu_add(cpu, data);

		return;
	}

	case CPU_OP_ADD_A_A:
		alu_add(cpu, cpu->reg.a);
		return;

	case CPU_OP_ADC_A_B:
		alu_adc(cpu, cpu->reg.b);
		return;

	case CPU_OP_ADC_A_C:
		alu_adc(cpu, cpu->reg.c);
		return;

	case CPU_OP_ADC_A_D:
		alu_adc(cpu, cpu->reg.d);
		return;

	case CPU_OP_ADC_A_E:
		alu_adc(cpu, cpu->reg.e);
		return;

	case CPU_OP_ADC_A_H:
		alu_adc(cpu, cpu->reg.h);
		return;

	case CPU_OP_ADC_A_L:
		alu_adc(cpu, cpu->reg.l);
		return;

	case CPU_OP_ADC_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_adc(cpu, data);
		return;
	}

	case CPU_OP_ADC_A_A:
		alu_adc(cpu, cpu->reg.a);
		return;

	case CPU_OP_SUB_A_B:
		alu_sub(cpu, cpu->reg.b);
		return;

	case CPU_OP_SUB_A_C:
		alu_sub(cpu, cpu->reg.c);
		return;

	case CPU_OP_SUB_A_D:
		alu_sub(cpu, cpu->reg.d);
		return;

	case CPU_OP_SUB_A_E:
		alu_sub(cpu, cpu->reg.e);
		return;

	case CPU_OP_SUB_A_H:
		alu_sub(cpu, cpu->reg.h);
		return;

	case CPU_OP_SUB_A_L:
		alu_sub(cpu, cpu->reg.l);
		return;

	case CPU_OP_SUB_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_sub(cpu, data);
		return;
	}

	case CPU_OP_SUB_A_A:
		alu_sub(cpu, cpu->reg.a);
		return;

	case CPU_OP_SBC_A_B:
		alu_sbc(cpu, cpu->reg.b);
		return;

	case CPU_OP_SBC_A_C:
		alu_sbc(cpu, cpu->reg.c);
		return;

	case CPU_OP_SBC_A_D:
		alu_sbc(cpu, cpu->reg.d);
		return;

	case CPU_OP_SBC_A_E:
		alu_sbc(cpu, cpu->reg.e);
		return;

	case CPU_OP_SBC_A_H:
		alu_sbc(cpu, cpu->reg.h);
		return;

	case CPU_OP_SBC_A_L:
		alu_sbc(cpu, cpu->reg.l);
		return;

	case CPU_OP_SBC_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		alu_sbc(cpu, data);

		return;
	}

	case CPU_OP_SBC_A_A:
		alu_sbc(cpu, cpu->reg.a);
		return;

	case CPU_OP_AND_A_B:
		alu_and(cpu, cpu->reg.b);
		return;

	case CPU_OP_AND_A_C:
		alu_and(cpu, cpu->reg.c);
		return;

	case CPU_OP_AND_A_D:
		alu_and(cpu, cpu->reg.d);
		return;

	case CPU_OP_AND_A_E:
		alu_and(cpu, cpu->reg.e);
		return;

	case CPU_OP_AND_A_H:
		alu_and(cpu, cpu->reg.h);
		return;

	case CPU_OP_AND_A_L:
		alu_and(cpu, cpu->reg.l);
		return;

	case CPU_OP_AND_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_and(cpu, data);
		return;
	}

	case CPU_OP_AND_A_A:
		alu_and(cpu, cpu->reg.a);
		return;

	case CPU_OP_XOR_A_B:
		alu_xor(cpu, cpu->reg.b);
		return;

	case CPU_OP_XOR_A_C:
		alu_xor(cpu, cpu->reg.c);
		return;

	case CPU_OP_XOR_A_D:
		alu_xor(cpu, cpu->reg.d);
		return;

	case CPU_OP_XOR_A_E:
		alu_xor(cpu, cpu->reg.e);
		return;

	case CPU_OP_XOR_A_H:
		alu_xor(cpu, cpu->reg.h);
		return;

	case CPU_OP_XOR_A_L:
		alu_xor(cpu, cpu->reg.l);
		return;

	case CPU_OP_XOR_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);

		alu_xor(cpu, data);
		return;
	}

	case CPU_OP_XOR_A_A:
		alu_xor(cpu, cpu->reg.a);
		return;

	case CPU_OP_OR_A_B:
		alu_or(cpu, cpu->reg.b);
		return;

	case CPU_OP_OR_A_C:
		alu_or(cpu, cpu->reg.c);
		return;

	case CPU_OP_OR_A_D:
		alu_or(cpu, cpu->reg.d);
		return;

	case CPU_OP_OR_A_E:
		alu_or(cpu, cpu->reg.e);
		return;

	case CPU_OP_OR_A_H:
		alu_or(cpu, cpu->reg.h);
		return;

	case CPU_OP_OR_A_L:
		alu_or(cpu, cpu->reg.l);
		return;

	case CPU_OP_OR_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		alu_or(cpu, data);

		return;
	}

	case CPU_OP_OR_A_A:
		alu_or(cpu, cpu->reg.a);
		return;

	case CPU_OP_CP_A_B:
		alu_cp(cpu, cpu->reg.b);
		return;

	case CPU_OP_CP_A_C:
		alu_cp(cpu, cpu->reg.c);
		return;

	case CPU_OP_CP_A_D:
		alu_cp(cpu, cpu->reg.d);
		return;

	case CPU_OP_CP_A_E:
		alu_cp(cpu, cpu->reg.e);
		return;

	case CPU_OP_CP_A_H:
		alu_cp(cpu, cpu->reg.h);
		return;

	case CPU_OP_CP_A_L:
		alu_cp(cpu, cpu->reg.l);
		return;

	case CPU_OP_CP_A_MEM_HL: {
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
		alu_cp(cpu, data);

		return;
	}

	case CPU_OP_CP_A_A:
		alu_cp(cpu, cpu->reg.a);
		return;

	case CPU_OP_RET_NZ:
		ret_if(cpu, !(cpu->reg.f & CPU_ZERO_FLAG));
		return;

	case CPU_OP_POP_BC:
		cpu->reg.bc = stack_pop(cpu);
		return;

	case CPU_OP_JP_NZ_U16:
		jp_if(cpu, !(cpu->reg.f & CPU_ZERO_FLAG));
		return;

	case CPU_OP_JP_U16:
		jp_if(cpu, true);
		return;

	case CPU_OP_CALL_NZ_U16:
		call_if(cpu, !(cpu->reg.f & CPU_ZERO_FLAG));
		return;

	case CPU_OP_PUSH_BC:
		stack_push(cpu, cpu->reg.bc);
		return;

	case CPU_OP_ADD_A_U8: {
		const uint8_t data = read_u8(cpu);

		alu_add(cpu, data);
		return;
	}

	case CPU_OP_RST_00:
		rst(cpu, 0x0000);
		return;

	case CPU_OP_RET_Z:
		ret_if(cpu, cpu->reg.f & CPU_ZERO_FLAG);
		return;

	case CPU_OP_RET:
		ret_if(cpu, true);
		return;

	case CPU_OP_JP_Z_U16:
		jp_if(cpu, cpu->reg.f & CPU_ZERO_FLAG);
		return;

	case CPU_OP_PREFIX_CB: {
		const uint8_t cb_instr = read_u8(cpu);

		switch (cb_instr) {
		case CPU_OP_RLC_B:
			cpu->reg.b = alu_rlc(cpu, cpu->reg.b);
			return;

		case CPU_OP_RLC_C:
			cpu->reg.c = alu_rlc(cpu, cpu->reg.c);
			return;

		case CPU_OP_RLC_D:
			cpu->reg.d = alu_rlc(cpu, cpu->reg.d);
			return;

		case CPU_OP_RLC_E:
			cpu->reg.e = alu_rlc(cpu, cpu->reg.e);
			return;

		case CPU_OP_RLC_H:
			cpu->reg.h = alu_rlc(cpu, cpu->reg.h);
			return;

		case CPU_OP_RLC_L:
			cpu->reg.l = alu_rlc(cpu, cpu->reg.l);
			return;

		case CPU_OP_RLC_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_rlc(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RLC_A:
			cpu->reg.a = alu_rlc(cpu, cpu->reg.a);
			return;

		case CPU_OP_RRC_B:
			cpu->reg.b = alu_rrc(cpu, cpu->reg.b);
			return;

		case CPU_OP_RRC_C:
			cpu->reg.c = alu_rrc(cpu, cpu->reg.c);
			return;

		case CPU_OP_RRC_D:
			cpu->reg.d = alu_rrc(cpu, cpu->reg.d);
			return;

		case CPU_OP_RRC_E:
			cpu->reg.e = alu_rrc(cpu, cpu->reg.e);
			return;

		case CPU_OP_RRC_H:
			cpu->reg.h = alu_rrc(cpu, cpu->reg.h);
			return;

		case CPU_OP_RRC_L:
			cpu->reg.l = alu_rrc(cpu, cpu->reg.l);
			return;

		case CPU_OP_RRC_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_rrc(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RRC_A:
			cpu->reg.a = alu_rrc(cpu, cpu->reg.a);
			return;

		case CPU_OP_RL_B:
			cpu->reg.b = alu_rl(cpu, cpu->reg.b);
			return;

		case CPU_OP_RL_C:
			cpu->reg.c = alu_rl(cpu, cpu->reg.c);
			return;

		case CPU_OP_RL_D:
			cpu->reg.d = alu_rl(cpu, cpu->reg.d);
			return;

		case CPU_OP_RL_E:
			cpu->reg.e = alu_rl(cpu, cpu->reg.e);
			return;

		case CPU_OP_RL_H:
			cpu->reg.h = alu_rl(cpu, cpu->reg.h);
			return;

		case CPU_OP_RL_L:
			cpu->reg.l = alu_rl(cpu, cpu->reg.l);
			return;

		case CPU_OP_RL_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_rl(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RL_A:
			cpu->reg.a = alu_rl(cpu, cpu->reg.a);
			return;

		case CPU_OP_RR_B:
			cpu->reg.b = alu_rr(cpu, cpu->reg.b);
			return;

		case CPU_OP_RR_C:
			cpu->reg.c = alu_rr(cpu, cpu->reg.c);
			return;

		case CPU_OP_RR_D:
			cpu->reg.d = alu_rr(cpu, cpu->reg.d);
			return;

		case CPU_OP_RR_E:
			cpu->reg.e = alu_rr(cpu, cpu->reg.e);
			return;

		case CPU_OP_RR_H:
			cpu->reg.h = alu_rr(cpu, cpu->reg.h);
			return;

		case CPU_OP_RR_L:
			cpu->reg.l = alu_rr(cpu, cpu->reg.l);
			return;

		case CPU_OP_RR_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_rr(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RR_A:
			cpu->reg.a = alu_rr(cpu, cpu->reg.a);
			return;

		case CPU_OP_SLA_B:
			cpu->reg.b = alu_sla(cpu, cpu->reg.b);
			return;

		case CPU_OP_SLA_C:
			cpu->reg.c = alu_sla(cpu, cpu->reg.c);
			return;

		case CPU_OP_SLA_D:
			cpu->reg.d = alu_sla(cpu, cpu->reg.d);
			return;

		case CPU_OP_SLA_E:
			cpu->reg.e = alu_sla(cpu, cpu->reg.e);
			return;

		case CPU_OP_SLA_H:
			cpu->reg.h = alu_sla(cpu, cpu->reg.h);
			return;

		case CPU_OP_SLA_L:
			cpu->reg.l = alu_sla(cpu, cpu->reg.l);
			return;

		case CPU_OP_SLA_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_sla(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SLA_A:
			cpu->reg.a = alu_sla(cpu, cpu->reg.a);
			return;

		case CPU_OP_SRA_B:
			cpu->reg.b = alu_sra(cpu, cpu->reg.b);
			return;

		case CPU_OP_SRA_C:
			cpu->reg.c = alu_sra(cpu, cpu->reg.c);
			return;

		case CPU_OP_SRA_D:
			cpu->reg.d = alu_sra(cpu, cpu->reg.d);
			return;

		case CPU_OP_SRA_E:
			cpu->reg.e = alu_sra(cpu, cpu->reg.e);
			return;

		case CPU_OP_SRA_H:
			cpu->reg.h = alu_sra(cpu, cpu->reg.h);
			return;

		case CPU_OP_SRA_L:
			cpu->reg.l = alu_sra(cpu, cpu->reg.l);
			return;

		case CPU_OP_SRA_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_sra(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SRA_A:
			cpu->reg.a = alu_sra(cpu, cpu->reg.a);
			return;

		case CPU_OP_SWAP_B:
			cpu->reg.b = alu_swap(cpu, cpu->reg.b);
			return;

		case CPU_OP_SWAP_C:
			cpu->reg.c = alu_swap(cpu, cpu->reg.c);
			return;

		case CPU_OP_SWAP_D:
			cpu->reg.d = alu_swap(cpu, cpu->reg.d);
			return;

		case CPU_OP_SWAP_E:
			cpu->reg.e = alu_swap(cpu, cpu->reg.e);
			return;

		case CPU_OP_SWAP_H:
			cpu->reg.h = alu_swap(cpu, cpu->reg.h);
			return;

		case CPU_OP_SWAP_L:
			cpu->reg.l = alu_swap(cpu, cpu->reg.l);
			return;

		case CPU_OP_SWAP_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_swap(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SWAP_A:
			cpu->reg.a = alu_swap(cpu, cpu->reg.a);
			return;

		case CPU_OP_SRL_B:
			cpu->reg.b = alu_srl(cpu, cpu->reg.b);
			return;

		case CPU_OP_SRL_C:
			cpu->reg.c = alu_srl(cpu, cpu->reg.c);
			return;

		case CPU_OP_SRL_D:
			cpu->reg.d = alu_srl(cpu, cpu->reg.d);
			return;

		case CPU_OP_SRL_E:
			cpu->reg.e = alu_srl(cpu, cpu->reg.e);
			return;

		case CPU_OP_SRL_H:
			cpu->reg.h = alu_srl(cpu, cpu->reg.h);
			return;

		case CPU_OP_SRL_L:
			cpu->reg.l = alu_srl(cpu, cpu->reg.l);
			return;

		case CPU_OP_SRL_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data = alu_srl(cpu, data);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SRL_A:
			cpu->reg.a = alu_srl(cpu, cpu->reg.a);
			return;

		case CPU_OP_BIT_0_B:
			alu_bit(cpu, 0, cpu->reg.b);
			return;

		case CPU_OP_BIT_0_C:
			alu_bit(cpu, 0, cpu->reg.c);
			return;

		case CPU_OP_BIT_0_D:
			alu_bit(cpu, 0, cpu->reg.d);
			return;

		case CPU_OP_BIT_0_E:
			alu_bit(cpu, 0, cpu->reg.e);
			return;

		case CPU_OP_BIT_0_H:
			alu_bit(cpu, 0, cpu->reg.h);
			return;

		case CPU_OP_BIT_0_L:
			alu_bit(cpu, 0, cpu->reg.l);
			return;

		case CPU_OP_BIT_0_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 0, data);
			return;
		}

		case CPU_OP_BIT_0_A:
			alu_bit(cpu, 0, cpu->reg.a);
			return;

		case CPU_OP_BIT_1_B:
			alu_bit(cpu, 1, cpu->reg.b);
			return;

		case CPU_OP_BIT_1_C:
			alu_bit(cpu, 1, cpu->reg.c);
			return;

		case CPU_OP_BIT_1_D:
			alu_bit(cpu, 1, cpu->reg.d);
			return;

		case CPU_OP_BIT_1_E:
			alu_bit(cpu, 1, cpu->reg.e);
			return;

		case CPU_OP_BIT_1_H:
			alu_bit(cpu, 1, cpu->reg.h);
			return;

		case CPU_OP_BIT_1_L:
			alu_bit(cpu, 1, cpu->reg.l);
			return;

		case CPU_OP_BIT_1_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 1, data);
			return;
		}

		case CPU_OP_BIT_1_A:
			alu_bit(cpu, 1, cpu->reg.a);
			return;

		case CPU_OP_BIT_2_B:
			alu_bit(cpu, 2, cpu->reg.b);
			return;

		case CPU_OP_BIT_2_C:
			alu_bit(cpu, 2, cpu->reg.c);
			return;

		case CPU_OP_BIT_2_D:
			alu_bit(cpu, 2, cpu->reg.d);
			return;

		case CPU_OP_BIT_2_E:
			alu_bit(cpu, 2, cpu->reg.e);
			return;

		case CPU_OP_BIT_2_H:
			alu_bit(cpu, 2, cpu->reg.h);
			return;

		case CPU_OP_BIT_2_L:
			alu_bit(cpu, 2, cpu->reg.l);
			return;

		case CPU_OP_BIT_2_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 2, data);
			return;
		}

		case CPU_OP_BIT_2_A:
			alu_bit(cpu, 2, cpu->reg.a);
			return;

		case CPU_OP_BIT_3_B:
			alu_bit(cpu, 3, cpu->reg.b);
			return;

		case CPU_OP_BIT_3_C:
			alu_bit(cpu, 3, cpu->reg.c);
			return;

		case CPU_OP_BIT_3_D:
			alu_bit(cpu, 3, cpu->reg.d);
			return;

		case CPU_OP_BIT_3_E:
			alu_bit(cpu, 3, cpu->reg.e);
			return;

		case CPU_OP_BIT_3_H:
			alu_bit(cpu, 3, cpu->reg.h);
			return;

		case CPU_OP_BIT_3_L:
			alu_bit(cpu, 3, cpu->reg.l);
			return;

		case CPU_OP_BIT_3_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 3, data);
			return;
		}

		case CPU_OP_BIT_3_A:
			alu_bit(cpu, 3, cpu->reg.a);
			return;

		case CPU_OP_BIT_4_B:
			alu_bit(cpu, 4, cpu->reg.b);
			return;

		case CPU_OP_BIT_4_C:
			alu_bit(cpu, 4, cpu->reg.c);
			return;

		case CPU_OP_BIT_4_D:
			alu_bit(cpu, 4, cpu->reg.d);
			return;

		case CPU_OP_BIT_4_E:
			alu_bit(cpu, 4, cpu->reg.e);
			return;

		case CPU_OP_BIT_4_H:
			alu_bit(cpu, 4, cpu->reg.h);
			return;

		case CPU_OP_BIT_4_L:
			alu_bit(cpu, 4, cpu->reg.l);
			return;

		case CPU_OP_BIT_4_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 4, data);
			return;
		}

		case CPU_OP_BIT_4_A:
			alu_bit(cpu, 4, cpu->reg.a);
			return;

		case CPU_OP_BIT_5_B:
			alu_bit(cpu, 5, cpu->reg.b);
			return;

		case CPU_OP_BIT_5_C:
			alu_bit(cpu, 5, cpu->reg.c);
			return;

		case CPU_OP_BIT_5_D:
			alu_bit(cpu, 5, cpu->reg.d);
			return;

		case CPU_OP_BIT_5_E:
			alu_bit(cpu, 5, cpu->reg.e);
			return;

		case CPU_OP_BIT_5_H:
			alu_bit(cpu, 5, cpu->reg.h);
			return;

		case CPU_OP_BIT_5_L:
			alu_bit(cpu, 5, cpu->reg.l);
			return;

		case CPU_OP_BIT_5_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 5, data);
			return;
		}

		case CPU_OP_BIT_5_A:
			alu_bit(cpu, 5, cpu->reg.a);
			return;

		case CPU_OP_BIT_6_B:
			alu_bit(cpu, 6, cpu->reg.b);
			return;

		case CPU_OP_BIT_6_C:
			alu_bit(cpu, 6, cpu->reg.c);
			return;

		case CPU_OP_BIT_6_D:
			alu_bit(cpu, 6, cpu->reg.d);
			return;

		case CPU_OP_BIT_6_E:
			alu_bit(cpu, 6, cpu->reg.e);
			return;

		case CPU_OP_BIT_6_H:
			alu_bit(cpu, 6, cpu->reg.h);
			return;

		case CPU_OP_BIT_6_L:
			alu_bit(cpu, 6, cpu->reg.l);
			return;

		case CPU_OP_BIT_6_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 6, data);
			return;
		}

		case CPU_OP_BIT_6_A:
			alu_bit(cpu, 6, cpu->reg.a);
			return;

		case CPU_OP_BIT_7_B:
			alu_bit(cpu, 7, cpu->reg.b);
			return;

		case CPU_OP_BIT_7_C:
			alu_bit(cpu, 7, cpu->reg.c);
			return;

		case CPU_OP_BIT_7_D:
			alu_bit(cpu, 7, cpu->reg.d);
			return;

		case CPU_OP_BIT_7_E:
			alu_bit(cpu, 7, cpu->reg.e);
			return;

		case CPU_OP_BIT_7_H:
			alu_bit(cpu, 7, cpu->reg.h);
			return;

		case CPU_OP_BIT_7_L:
			alu_bit(cpu, 7, cpu->reg.l);
			return;

		case CPU_OP_BIT_7_MEM_HL: {
			const uint8_t data =
				agoge_bus_read(cpu->bus, cpu->reg.hl);

			alu_bit(cpu, 7, data);
			return;
		}

		case CPU_OP_BIT_7_A:
			alu_bit(cpu, 7, cpu->reg.a);
			return;

		case CPU_OP_RES_0_B:
			cpu->reg.b &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_C:
			cpu->reg.c &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_D:
			cpu->reg.d &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_E:
			cpu->reg.e &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_H:
			cpu->reg.h &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_L:
			cpu->reg.l &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_0_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 0);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_0_A:
			cpu->reg.a &= ~(UINT8_C(1) << 0);
			return;

		case CPU_OP_RES_1_B:
			cpu->reg.b &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_C:
			cpu->reg.c &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_D:
			cpu->reg.d &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_E:
			cpu->reg.e &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_H:
			cpu->reg.h &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_L:
			cpu->reg.l &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_1_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 1);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_1_A:
			cpu->reg.a &= ~(UINT8_C(1) << 1);
			return;

		case CPU_OP_RES_2_B:
			cpu->reg.b &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_C:
			cpu->reg.c &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_D:
			cpu->reg.d &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_E:
			cpu->reg.e &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_H:
			cpu->reg.h &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_L:
			cpu->reg.l &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_2_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 2);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_2_A:
			cpu->reg.a &= ~(UINT8_C(1) << 2);
			return;

		case CPU_OP_RES_3_B:
			cpu->reg.b &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_C:
			cpu->reg.c &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_D:
			cpu->reg.d &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_E:
			cpu->reg.e &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_H:
			cpu->reg.h &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_L:
			cpu->reg.l &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_3_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 3);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_3_A:
			cpu->reg.a &= ~(UINT8_C(1) << 3);
			return;

		case CPU_OP_RES_4_B:
			cpu->reg.b &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_C:
			cpu->reg.c &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_D:
			cpu->reg.d &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_E:
			cpu->reg.e &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_H:
			cpu->reg.h &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_L:
			cpu->reg.l &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_4_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 4);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_4_A:
			cpu->reg.a &= ~(UINT8_C(1) << 4);
			return;

		case CPU_OP_RES_5_B:
			cpu->reg.b &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_C:
			cpu->reg.c &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_D:
			cpu->reg.d &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_E:
			cpu->reg.e &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_H:
			cpu->reg.h &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_L:
			cpu->reg.l &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_5_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 5);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_5_A:
			cpu->reg.a &= ~(UINT8_C(1) << 5);
			return;

		case CPU_OP_RES_6_B:
			cpu->reg.b &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_C:
			cpu->reg.c &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_D:
			cpu->reg.d &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_E:
			cpu->reg.e &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_H:
			cpu->reg.h &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_L:
			cpu->reg.l &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_6_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 6);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_6_A:
			cpu->reg.a &= ~(UINT8_C(1) << 6);
			return;

		case CPU_OP_RES_7_B:
			cpu->reg.b &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_C:
			cpu->reg.c &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_D:
			cpu->reg.d &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_E:
			cpu->reg.e &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_H:
			cpu->reg.h &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_L:
			cpu->reg.l &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_RES_7_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data &= ~(UINT8_C(1) << 7);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_RES_7_A:
			cpu->reg.a &= ~(UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_0_B:
			cpu->reg.b |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_C:
			cpu->reg.c |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_D:
			cpu->reg.d |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_E:
			cpu->reg.e |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_H:
			cpu->reg.h |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_L:
			cpu->reg.l |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_0_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 0);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_0_A:
			cpu->reg.a |= (UINT8_C(1) << 0);
			return;

		case CPU_OP_SET_1_B:
			cpu->reg.b |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_C:
			cpu->reg.c |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_D:
			cpu->reg.d |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_E:
			cpu->reg.e |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_H:
			cpu->reg.h |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_L:
			cpu->reg.l |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_1_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 1);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_1_A:
			cpu->reg.a |= (UINT8_C(1) << 1);
			return;

		case CPU_OP_SET_2_B:
			cpu->reg.b |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_C:
			cpu->reg.c |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_D:
			cpu->reg.d |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_E:
			cpu->reg.e |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_H:
			cpu->reg.h |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_L:
			cpu->reg.l |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_2_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 2);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_2_A:
			cpu->reg.a |= (UINT8_C(1) << 2);
			return;

		case CPU_OP_SET_3_B:
			cpu->reg.b |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_C:
			cpu->reg.c |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_D:
			cpu->reg.d |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_E:
			cpu->reg.e |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_H:
			cpu->reg.h |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_L:
			cpu->reg.l |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_3_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 3);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_3_A:
			cpu->reg.a |= (UINT8_C(1) << 3);
			return;

		case CPU_OP_SET_4_B:
			cpu->reg.b |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_C:
			cpu->reg.c |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_D:
			cpu->reg.d |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_E:
			cpu->reg.e |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_H:
			cpu->reg.h |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_L:
			cpu->reg.l |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_4_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 4);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_4_A:
			cpu->reg.a |= (UINT8_C(1) << 4);
			return;

		case CPU_OP_SET_5_B:
			cpu->reg.b |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_C:
			cpu->reg.c |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_D:
			cpu->reg.d |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_E:
			cpu->reg.e |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_H:
			cpu->reg.h |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_L:
			cpu->reg.l |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_5_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 5);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_5_A:
			cpu->reg.a |= (UINT8_C(1) << 5);
			return;

		case CPU_OP_SET_6_B:
			cpu->reg.b |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_C:
			cpu->reg.c |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_D:
			cpu->reg.d |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_E:
			cpu->reg.e |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_H:
			cpu->reg.h |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_L:
			cpu->reg.l |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_6_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 6);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_6_A:
			cpu->reg.a |= (UINT8_C(1) << 6);
			return;

		case CPU_OP_SET_7_B:
			cpu->reg.b |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_C:
			cpu->reg.c |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_D:
			cpu->reg.d |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_E:
			cpu->reg.e |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_H:
			cpu->reg.h |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_L:
			cpu->reg.l |= (UINT8_C(1) << 7);
			return;

		case CPU_OP_SET_7_MEM_HL: {
			uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl);
			data |= (UINT8_C(1) << 7);
			agoge_bus_write(cpu->bus, cpu->reg.hl, data);

			return;
		}

		case CPU_OP_SET_7_A:
			cpu->reg.a |= (UINT8_C(1) << 7);
			return;

		default:
			UNREACHABLE;
		}
	}

	case CPU_OP_CALL_Z_U16:
		call_if(cpu, cpu->reg.f & CPU_ZERO_FLAG);
		return;

	case CPU_OP_CALL_U16:
		call_if(cpu, true);
		return;

	case CPU_OP_ADC_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_adc(cpu, u8);
		return;
	}

	case CPU_OP_RST_08:
		rst(cpu, 0x0008);
		return;

	case CPU_OP_RET_NC:
		ret_if(cpu, !(cpu->reg.f & CPU_CARRY_FLAG));
		return;

	case CPU_OP_POP_DE:
		cpu->reg.de = stack_pop(cpu);
		return;

	case CPU_OP_JP_NC_U16:
		jp_if(cpu, !(cpu->reg.f & CPU_CARRY_FLAG));
		return;

	case CPU_OP_CALL_NC_U16:
		call_if(cpu, !(cpu->reg.f & CPU_CARRY_FLAG));
		return;

	case CPU_OP_PUSH_DE:
		stack_push(cpu, cpu->reg.de);
		return;

	case CPU_OP_SUB_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_sub(cpu, u8);
		return;
	}

	case CPU_OP_RST_10:
		rst(cpu, 0x0010);
		return;

	case CPU_OP_RET_C:
		ret_if(cpu, cpu->reg.f & CPU_CARRY_FLAG);
		return;

	case CPU_OP_RETI:
		ret_if(cpu, true);
		return;

	case CPU_OP_JP_C_U16:
		jp_if(cpu, cpu->reg.f & CPU_CARRY_FLAG);
		return;

	case CPU_OP_CALL_C_U16:
		call_if(cpu, cpu->reg.f & CPU_CARRY_FLAG);
		return;

	case CPU_OP_SBC_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_sbc(cpu, u8);
		return;
	}

	case CPU_OP_RST_18:
		rst(cpu, 0x0018);
		return;

	case LD_MEM_FF00_U8_A: {
		const uint16_t u16 = 0xFF00 + read_u8(cpu);

		agoge_bus_write(cpu->bus, u16, cpu->reg.a);
		return;
	}

	case CPU_OP_POP_HL:
		cpu->reg.hl = stack_pop(cpu);
		return;

	case CPU_OP_LD_MEM_FF00_C_A:
		agoge_bus_write(cpu->bus, 0xFF00 + cpu->reg.c, cpu->reg.a);
		return;

	case CPU_OP_PUSH_HL:
		stack_push(cpu, cpu->reg.hl);
		return;

	case CPU_OP_AND_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_and(cpu, u8);
		return;
	}

	case CPU_OP_RST_20:
		rst(cpu, 0x0020);
		return;

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

	case CPU_OP_RST_28:
		rst(cpu, 0x0028);
		return;

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

	case CPU_OP_PUSH_AF:
		stack_push(cpu, cpu->reg.af);
		return;

	case CPU_OP_OR_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_or(cpu, u8);
		return;
	}

	case CPU_OP_RST_30:
		rst(cpu, 0x0030);
		return;

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

	case CPU_OP_EI:
		return;

	case CPU_OP_CP_A_U8: {
		const uint8_t u8 = read_u8(cpu);

		alu_cp(cpu, u8);
		return;
	}

	case CPU_OP_RST_38:
		rst(cpu, 0x0038);
		return;

	default:
		LOG_ERR(cpu->bus->log,
			"Illegal instruction $%02X trapped at program counter "
			"$%04X.",
			instr, cpu->reg.pc);
	}
}
