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
/// are 501 cases to handle. This is also not particularly great for code size
/// should that be a concern in the future.

#include "agoge-bus.h"
#include "agoge-compiler.h"
#include "agoge-cpu.h"
#include "agoge-cpu-defs.h"
#include "agoge-log.h"

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
	set_flag(cpu, val == 0, CPU_ZERO_FLAG);
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
	half_carry_flag_set(cpu, (val & 0x0F) == 0);
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

	if (!(val & (UINT8_C(1) << bit))) {
		cpu->reg.f |= CPU_ZERO_FLAG;
	} else {
		cpu->reg.f &= ~CPU_ZERO_FLAG;
	}
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

/// Implements the `RES b, n` instruction.
///
/// @param bit The bit to clear.
/// @param val The value to clear the bit on.
/// @returns The modified value.
NONNULL NODISCARD static uint8_t alu_res(const unsigned int bit,
					 const uint8_t val)
{
	return val & ~(UINT8_C(1) << bit);
}

/// Implements the `SET b, n` instruction.
///
/// @param bit The bit to set.
/// @param val The value to set the bit on.
/// @returns The modified value.
NONNULL NODISCARD static uint8_t alu_set(const unsigned int bit,
					 const uint8_t val)
{
	return val | (UINT8_C(1) << bit);
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

#define BRANCH(op, func, cond)       \
	case (op):                   \
		(func)(cpu, (cond)); \
		return

#define BIT_OP_HL(op, func)                                               \
	case (op): {                                                      \
		const uint8_t u8 = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		(func)(cpu, u8);                                          \
		return;                                                   \
	}

#define OP_U8(op, func)                          \
	case (op): {                             \
		const uint8_t u8 = read_u8(cpu); \
		(func)(cpu, u8);                 \
		return;                          \
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

#define OP(op, func, src_dst)                       \
	case (op):                                  \
		(src_dst) = (func)(cpu, (src_dst)); \
		return

#define OP_A(op, func, src)         \
	case (op):                  \
		(func)(cpu, (src)); \
		return

#define BITOP(op, func, bit, val)             \
	case (op):                            \
		(val) = (func)((bit), (val)); \
		return

#define OP_MEM_HL(op, func)                                           \
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

#define BITOP_HL(op, func, bit)                                      \
	case (op): {                                                 \
		uint8_t val = agoge_bus_read(cpu->bus, cpu->reg.hl); \
		val = (func)((bit), val);                            \
		agoge_bus_write(cpu->bus, cpu->reg.hl, val);         \
                                                                     \
		return;                                              \
	}

#define OP_A_MEM_HL(op, func)                                               \
	case (op): {                                                        \
		const uint8_t data = agoge_bus_read(cpu->bus, cpu->reg.hl); \
                                                                            \
		(func)(cpu, data);                                          \
		return;                                                     \
	}

#define RST(op, vec)             \
	case (op):               \
		rst(cpu, (vec)); \
		return

	// clang-format off

	const uint8_t instr = read_u8(cpu);

	switch (instr) {
	case CPU_OP_NOP: return;
	LD_R16_U16(CPU_OP_LD_BC_U16, cpu->reg.bc);
	LD_MEM_R16_A(CPU_OP_LD_MEM_BC_A, cpu->reg.bc);
	INC_R16(CPU_OP_INC_BC, cpu->reg.bc);
	OP(CPU_OP_INC_B, alu_inc, cpu->reg.b);
	OP(CPU_OP_DEC_B, alu_dec, cpu->reg.b);
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
	OP(CPU_OP_INC_C, alu_inc, cpu->reg.c);
	OP(CPU_OP_DEC_C, alu_dec, cpu->reg.c);
	LD_R8_U8(CPU_OP_LD_C_U8, cpu->reg.c);

	case CPU_OP_RRCA:
		cpu->reg.a = alu_rrc_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	LD_R16_U16(CPU_OP_LD_DE_U16, cpu->reg.de);
	LD_MEM_R16_A(CPU_OP_LD_MEM_DE_A, cpu->reg.de);
	INC_R16(CPU_OP_INC_DE, cpu->reg.de);
	OP(CPU_OP_INC_D, alu_inc, cpu->reg.d);
	OP(CPU_OP_DEC_D, alu_dec, cpu->reg.d);
	LD_R8_U8(CPU_OP_LD_D_U8, cpu->reg.d);

	case CPU_OP_RLA:
		cpu->reg.a = alu_rl_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	BRANCH(CPU_OP_JR_S8, jr_if, true);
	ADD_HL_R16(CPU_OP_ADD_HL_DE, cpu->reg.de);
	LD_A_MEM_R16(CPU_OP_LD_A_MEM_DE, cpu->reg.de);
	DEC_R16(CPU_OP_DEC_DE, cpu->reg.de);
	OP(CPU_OP_INC_E, alu_inc, cpu->reg.e);
	OP(CPU_OP_DEC_E, alu_dec, cpu->reg.e);
	LD_R8_U8(CPU_OP_LD_E_U8, cpu->reg.e);

	case CPU_OP_RRA:
		cpu->reg.a = alu_rr_helper(cpu, cpu->reg.a);
		cpu->reg.f &= ~CPU_ZERO_FLAG;

		return;

	BRANCH(CPU_OP_JR_NZ_S8, jr_if, !(cpu->reg.f & CPU_ZERO_FLAG));
	LD_R16_U16(CPU_OP_LD_HL_U16, cpu->reg.hl);

	case CPU_OP_LDI_MEM_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl++, cpu->reg.a);
		return;

	INC_R16(CPU_OP_INC_HL, cpu->reg.hl);
	OP(CPU_OP_INC_H, alu_inc, cpu->reg.h);
	OP(CPU_OP_DEC_H, alu_dec, cpu->reg.h);
	LD_R8_U8(CPU_OP_LD_H_U8, cpu->reg.h);

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

	BRANCH(CPU_OP_JR_Z_S8, jr_if, cpu->reg.f & CPU_ZERO_FLAG);
	ADD_HL_R16(CPU_OP_ADD_HL_HL, cpu->reg.hl);

	case CPU_OP_LDI_A_MEM_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl++);
		return;

	DEC_R16(CPU_OP_DEC_HL, cpu->reg.hl);
	OP(CPU_OP_INC_L, alu_inc, cpu->reg.l);
	OP(CPU_OP_DEC_L, alu_dec, cpu->reg.l);
	LD_R8_U8(CPU_OP_LD_L_U8, cpu->reg.l);

	case CPU_OP_CPL:
		cpu->reg.a = ~cpu->reg.a;
		cpu->reg.f |= (CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);

		return;

	BRANCH(CPU_OP_JR_NC_S8, jr_if, !(cpu->reg.f & CPU_CARRY_FLAG));
	LD_R16_U16(CPU_OP_LD_SP_U16, cpu->reg.sp);

	case CPU_OP_LDD_HL_A:
		agoge_bus_write(cpu->bus, cpu->reg.hl--, cpu->reg.a);
		return;

	INC_R16(CPU_OP_INC_SP, cpu->reg.sp);
	OP_MEM_HL(CPU_OP_INC_MEM_HL, alu_inc);
	OP_MEM_HL(CPU_OP_DEC_MEM_HL, alu_dec);

	case CPU_OP_LD_MEM_HL_U8: {
		const uint8_t u8 = read_u8(cpu);

		agoge_bus_write(cpu->bus, cpu->reg.hl, u8);
		return;
	}

	case CPU_OP_SCF:
		cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
		cpu->reg.f |= CPU_CARRY_FLAG;

		return;

	BRANCH(CPU_OP_JR_C_S8, jr_if, cpu->reg.f & CPU_CARRY_FLAG);
	ADD_HL_R16(CPU_OP_ADD_HL_SP, cpu->reg.sp);

	case CPU_OP_LDD_A_HL:
		cpu->reg.a = agoge_bus_read(cpu->bus, cpu->reg.hl--);
		return;

	DEC_R16(CPU_OP_DEC_SP, cpu->reg.sp);
	OP(CPU_OP_INC_A, alu_inc, cpu->reg.a);
	OP(CPU_OP_DEC_A, alu_dec, cpu->reg.a);
	LD_R8_U8(CPU_OP_LD_A_U8, cpu->reg.a);

	case CPU_OP_CCF:
		cpu->reg.f &= ~(CPU_SUBTRACT_FLAG | CPU_HALF_CARRY_FLAG);
		cpu->reg.f ^= CPU_CARRY_FLAG;

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
	OP_A(CPU_OP_ADD_A_B, alu_add, cpu->reg.b);
	OP_A(CPU_OP_ADD_A_C, alu_add, cpu->reg.c);
	OP_A(CPU_OP_ADD_A_D, alu_add, cpu->reg.d);
	OP_A(CPU_OP_ADD_A_E, alu_add, cpu->reg.e);
	OP_A(CPU_OP_ADD_A_H, alu_add, cpu->reg.h);
	OP_A(CPU_OP_ADD_A_L, alu_add, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_ADD_A_MEM_HL, alu_add);
	OP_A(CPU_OP_ADD_A_A, alu_add, cpu->reg.a);
	OP_A(CPU_OP_ADC_A_B, alu_adc, cpu->reg.b);
	OP_A(CPU_OP_ADC_A_C, alu_adc, cpu->reg.c);
	OP_A(CPU_OP_ADC_A_D, alu_adc, cpu->reg.d);
	OP_A(CPU_OP_ADC_A_E, alu_adc, cpu->reg.e);
	OP_A(CPU_OP_ADC_A_H, alu_adc, cpu->reg.h);
	OP_A(CPU_OP_ADC_A_L, alu_adc, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_ADC_A_MEM_HL, alu_adc);
	OP_A(CPU_OP_ADC_A_A, alu_adc, cpu->reg.a);
	OP_A(CPU_OP_SUB_A_B, alu_sub, cpu->reg.b);
	OP_A(CPU_OP_SUB_A_C, alu_sub, cpu->reg.c);
	OP_A(CPU_OP_SUB_A_D, alu_sub, cpu->reg.d);
	OP_A(CPU_OP_SUB_A_E, alu_sub, cpu->reg.e);
	OP_A(CPU_OP_SUB_A_H, alu_sub, cpu->reg.h);
	OP_A(CPU_OP_SUB_A_L, alu_sub, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_SUB_A_MEM_HL, alu_sub);
	OP_A(CPU_OP_SUB_A_A, alu_sub, cpu->reg.a);
	OP_A(CPU_OP_SBC_A_B, alu_sbc, cpu->reg.b);
	OP_A(CPU_OP_SBC_A_C, alu_sbc, cpu->reg.c);
	OP_A(CPU_OP_SBC_A_D, alu_sbc, cpu->reg.d);
	OP_A(CPU_OP_SBC_A_E, alu_sbc, cpu->reg.e);
	OP_A(CPU_OP_SBC_A_H, alu_sbc, cpu->reg.h);
	OP_A(CPU_OP_SBC_A_L, alu_sbc, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_SBC_A_MEM_HL, alu_sbc);
	OP_A(CPU_OP_SBC_A_A, alu_sbc, cpu->reg.a);
	OP_A(CPU_OP_AND_A_B, alu_and, cpu->reg.b);
	OP_A(CPU_OP_AND_A_C, alu_and, cpu->reg.c);
	OP_A(CPU_OP_AND_A_D, alu_and, cpu->reg.d);
	OP_A(CPU_OP_AND_A_E, alu_and, cpu->reg.e);
	OP_A(CPU_OP_AND_A_H, alu_and, cpu->reg.h);
	OP_A(CPU_OP_AND_A_L, alu_and, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_AND_A_MEM_HL, alu_and);
	OP_A(CPU_OP_AND_A_A, alu_and, cpu->reg.a);
	OP_A(CPU_OP_XOR_A_B, alu_xor, cpu->reg.b);
	OP_A(CPU_OP_XOR_A_C, alu_xor, cpu->reg.c);
	OP_A(CPU_OP_XOR_A_D, alu_xor, cpu->reg.d);
	OP_A(CPU_OP_XOR_A_E, alu_xor, cpu->reg.e);
	OP_A(CPU_OP_XOR_A_H, alu_xor, cpu->reg.h);
	OP_A(CPU_OP_XOR_A_L, alu_xor, cpu->reg.l);
	BIT_OP_HL(CPU_OP_XOR_A_MEM_HL, alu_xor);
	OP_A(CPU_OP_XOR_A_A, alu_xor, cpu->reg.a);
	OP_A(CPU_OP_OR_A_B, alu_or, cpu->reg.b);
	OP_A(CPU_OP_OR_A_C, alu_or, cpu->reg.c);
	OP_A(CPU_OP_OR_A_D, alu_or, cpu->reg.d);
	OP_A(CPU_OP_OR_A_E, alu_or, cpu->reg.e);
	OP_A(CPU_OP_OR_A_H, alu_or, cpu->reg.h);
	OP_A(CPU_OP_OR_A_L, alu_or, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_OR_A_MEM_HL, alu_or);
	OP_A(CPU_OP_OR_A_A, alu_or, cpu->reg.a);
	OP_A(CPU_OP_CP_A_B, alu_cp, cpu->reg.b);
	OP_A(CPU_OP_CP_A_C, alu_cp, cpu->reg.c);
	OP_A(CPU_OP_CP_A_D, alu_cp, cpu->reg.d);
	OP_A(CPU_OP_CP_A_E, alu_cp, cpu->reg.e);
	OP_A(CPU_OP_CP_A_H, alu_cp, cpu->reg.h);
	OP_A(CPU_OP_CP_A_L, alu_cp, cpu->reg.l);
	OP_A_MEM_HL(CPU_OP_CP_A_MEM_HL, alu_cp);
	OP_A(CPU_OP_CP_A_A, alu_cp, cpu->reg.a);
	BRANCH(CPU_OP_RET_NZ, ret_if, !(cpu->reg.f & CPU_ZERO_FLAG));
	POP(CPU_OP_POP_BC, cpu->reg.bc);
	BRANCH(CPU_OP_JP_NZ_U16, jp_if, !(cpu->reg.f & CPU_ZERO_FLAG));
	BRANCH(CPU_OP_JP_U16, jp_if, true);
	BRANCH(CPU_OP_CALL_NZ_U16, call_if, !(cpu->reg.f & CPU_ZERO_FLAG));
	PUSH(CPU_OP_PUSH_BC, cpu->reg.bc);
	OP_U8(CPU_OP_ADD_A_U8, alu_add);
	RST(CPU_OP_RST_00, 0x0000);
	BRANCH(CPU_OP_RET_Z, ret_if, cpu->reg.f & CPU_ZERO_FLAG);
	BRANCH(CPU_OP_RET, ret_if, true);
	BRANCH(CPU_OP_JP_Z_U16, jp_if, cpu->reg.f & CPU_ZERO_FLAG);

	case CPU_OP_PREFIX_CB: {
		const uint8_t cb_instr = read_u8(cpu);

		switch (cb_instr) {
			OP(CPU_OP_RLC_B, alu_rlc, cpu->reg.b);
			OP(CPU_OP_RLC_C, alu_rlc, cpu->reg.c);
			OP(CPU_OP_RLC_D, alu_rlc, cpu->reg.d);
			OP(CPU_OP_RLC_E, alu_rlc, cpu->reg.e);
			OP(CPU_OP_RLC_H, alu_rlc, cpu->reg.h);
			OP(CPU_OP_RLC_L, alu_rlc, cpu->reg.l);
			OP_MEM_HL(CPU_OP_RLC_MEM_HL, alu_rlc);
			OP(CPU_OP_RLC_A, alu_rlc, cpu->reg.a);
			OP(CPU_OP_RRC_B, alu_rrc, cpu->reg.b);
			OP(CPU_OP_RRC_C, alu_rrc, cpu->reg.c);
			OP(CPU_OP_RRC_D, alu_rrc, cpu->reg.d);
			OP(CPU_OP_RRC_E, alu_rrc, cpu->reg.e);
			OP(CPU_OP_RRC_H, alu_rrc, cpu->reg.h);
			OP(CPU_OP_RRC_L, alu_rrc, cpu->reg.l);
			OP_MEM_HL(CPU_OP_RRC_MEM_HL, alu_rrc);
			OP(CPU_OP_RRC_A, alu_rrc, cpu->reg.a);
			OP(CPU_OP_RL_B, alu_rl, cpu->reg.b);
			OP(CPU_OP_RL_C, alu_rl, cpu->reg.c);
			OP(CPU_OP_RL_D, alu_rl, cpu->reg.d);
			OP(CPU_OP_RL_E, alu_rl, cpu->reg.e);
			OP(CPU_OP_RL_H, alu_rl, cpu->reg.h);
			OP(CPU_OP_RL_L, alu_rl, cpu->reg.l);
			OP_MEM_HL(CPU_OP_RL_MEM_HL, alu_rl);
			OP(CPU_OP_RL_A, alu_rl, cpu->reg.a);
			OP(CPU_OP_RR_B, alu_rr, cpu->reg.b);
			OP(CPU_OP_RR_C, alu_rr, cpu->reg.c);
			OP(CPU_OP_RR_D, alu_rr, cpu->reg.d);
			OP(CPU_OP_RR_E, alu_rr, cpu->reg.e);
			OP(CPU_OP_RR_H, alu_rr, cpu->reg.h);
			OP(CPU_OP_RR_L, alu_rr, cpu->reg.l);
			OP_MEM_HL(CPU_OP_RR_MEM_HL, alu_rr);
			OP(CPU_OP_RR_A, alu_rr, cpu->reg.a);
			OP(CPU_OP_SLA_B, alu_sla, cpu->reg.b);
			OP(CPU_OP_SLA_C, alu_sla, cpu->reg.c);
			OP(CPU_OP_SLA_D, alu_sla, cpu->reg.d);
			OP(CPU_OP_SLA_E, alu_sla, cpu->reg.e);
			OP(CPU_OP_SLA_H, alu_sla, cpu->reg.h);
			OP(CPU_OP_SLA_L, alu_sla, cpu->reg.l);
			OP_MEM_HL(CPU_OP_SLA_MEM_HL, alu_sla);
			OP(CPU_OP_SLA_A, alu_sla, cpu->reg.a);
			OP(CPU_OP_SRA_B, alu_sra, cpu->reg.b);
			OP(CPU_OP_SRA_C, alu_sra, cpu->reg.c);
			OP(CPU_OP_SRA_D, alu_sra, cpu->reg.d);
			OP(CPU_OP_SRA_E, alu_sra, cpu->reg.e);
			OP(CPU_OP_SRA_H, alu_sra, cpu->reg.h);
			OP(CPU_OP_SRA_L, alu_sra, cpu->reg.l);
			OP_MEM_HL(CPU_OP_SRA_MEM_HL, alu_sra);
			OP(CPU_OP_SRA_A, alu_sra, cpu->reg.a);
			OP(CPU_OP_SWAP_B, alu_swap, cpu->reg.b);
			OP(CPU_OP_SWAP_C, alu_swap, cpu->reg.c);
			OP(CPU_OP_SWAP_D, alu_swap, cpu->reg.d);
			OP(CPU_OP_SWAP_E, alu_swap, cpu->reg.e);
			OP(CPU_OP_SWAP_H, alu_swap, cpu->reg.h);
			OP(CPU_OP_SWAP_L, alu_swap, cpu->reg.l);
			OP_MEM_HL(CPU_OP_SWAP_MEM_HL, alu_swap);
			OP(CPU_OP_SWAP_A, alu_swap, cpu->reg.a);
			OP(CPU_OP_SRL_B, alu_srl, cpu->reg.b);
			OP(CPU_OP_SRL_C, alu_srl, cpu->reg.c);
			OP(CPU_OP_SRL_D, alu_srl, cpu->reg.d);
			OP(CPU_OP_SRL_E, alu_srl, cpu->reg.e);
			OP(CPU_OP_SRL_H, alu_srl, cpu->reg.h);
			OP(CPU_OP_SRL_L, alu_srl, cpu->reg.l);
			OP_MEM_HL(CPU_OP_SRL_MEM_HL, alu_srl);
			OP(CPU_OP_SRL_A, alu_srl, cpu->reg.a);
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
			BITOP(CPU_OP_RES_0_B, alu_res, 0, cpu->reg.b);
			BITOP(CPU_OP_RES_0_C, alu_res, 0, cpu->reg.c);
			BITOP(CPU_OP_RES_0_D, alu_res, 0, cpu->reg.d);
			BITOP(CPU_OP_RES_0_E, alu_res, 0, cpu->reg.e);
			BITOP(CPU_OP_RES_0_H, alu_res, 0, cpu->reg.h);
			BITOP(CPU_OP_RES_0_L, alu_res, 0, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_0_MEM_HL, alu_res, 0);
			BITOP(CPU_OP_RES_0_A, alu_res, 0, cpu->reg.a);
			BITOP(CPU_OP_RES_1_B, alu_res, 1, cpu->reg.b);
			BITOP(CPU_OP_RES_1_C, alu_res, 1, cpu->reg.c);
			BITOP(CPU_OP_RES_1_D, alu_res, 1, cpu->reg.d);
			BITOP(CPU_OP_RES_1_E, alu_res, 1, cpu->reg.e);
			BITOP(CPU_OP_RES_1_H, alu_res, 1, cpu->reg.h);
			BITOP(CPU_OP_RES_1_L, alu_res, 1, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_1_MEM_HL, alu_res, 1);
			BITOP(CPU_OP_RES_1_A, alu_res, 1, cpu->reg.a);
			BITOP(CPU_OP_RES_2_B, alu_res, 2, cpu->reg.b);
			BITOP(CPU_OP_RES_2_C, alu_res, 2, cpu->reg.c);
			BITOP(CPU_OP_RES_2_D, alu_res, 2, cpu->reg.d);
			BITOP(CPU_OP_RES_2_E, alu_res, 2, cpu->reg.e);
			BITOP(CPU_OP_RES_2_H, alu_res, 2, cpu->reg.h);
			BITOP(CPU_OP_RES_2_L, alu_res, 2, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_2_MEM_HL, alu_res, 2);
			BITOP(CPU_OP_RES_2_A, alu_res, 2, cpu->reg.a);
			BITOP(CPU_OP_RES_3_B, alu_res, 3, cpu->reg.b);
			BITOP(CPU_OP_RES_3_C, alu_res, 3, cpu->reg.c);
			BITOP(CPU_OP_RES_3_D, alu_res, 3, cpu->reg.d);
			BITOP(CPU_OP_RES_3_E, alu_res, 3, cpu->reg.e);
			BITOP(CPU_OP_RES_3_H, alu_res, 3, cpu->reg.h);
			BITOP(CPU_OP_RES_3_L, alu_res, 3, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_3_MEM_HL, alu_res, 3);
			BITOP(CPU_OP_RES_3_A, alu_res, 3, cpu->reg.a);
			BITOP(CPU_OP_RES_4_B, alu_res, 4, cpu->reg.b);
			BITOP(CPU_OP_RES_4_C, alu_res, 4, cpu->reg.c);
			BITOP(CPU_OP_RES_4_D, alu_res, 4, cpu->reg.d);
			BITOP(CPU_OP_RES_4_E, alu_res, 4, cpu->reg.e);
			BITOP(CPU_OP_RES_4_H, alu_res, 4, cpu->reg.h);
			BITOP(CPU_OP_RES_4_L, alu_res, 4, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_4_MEM_HL, alu_res, 4);
			BITOP(CPU_OP_RES_4_A, alu_res, 4, cpu->reg.a);
			BITOP(CPU_OP_RES_5_B, alu_res, 5, cpu->reg.b);
			BITOP(CPU_OP_RES_5_C, alu_res, 5, cpu->reg.c);
			BITOP(CPU_OP_RES_5_D, alu_res, 5, cpu->reg.d);
			BITOP(CPU_OP_RES_5_E, alu_res, 5, cpu->reg.e);
			BITOP(CPU_OP_RES_5_H, alu_res, 5, cpu->reg.h);
			BITOP(CPU_OP_RES_5_L, alu_res, 5, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_5_MEM_HL, alu_res, 5);
			BITOP(CPU_OP_RES_5_A, alu_res, 5, cpu->reg.a);
			BITOP(CPU_OP_RES_6_B, alu_res, 6, cpu->reg.b);
			BITOP(CPU_OP_RES_6_C, alu_res, 6, cpu->reg.c);
			BITOP(CPU_OP_RES_6_D, alu_res, 6, cpu->reg.d);
			BITOP(CPU_OP_RES_6_E, alu_res, 6, cpu->reg.e);
			BITOP(CPU_OP_RES_6_H, alu_res, 6, cpu->reg.h);
			BITOP(CPU_OP_RES_6_L, alu_res, 6, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_6_MEM_HL, alu_res, 6);
			BITOP(CPU_OP_RES_6_A, alu_res, 6, cpu->reg.a);
			BITOP(CPU_OP_RES_7_B, alu_res, 7, cpu->reg.b);
			BITOP(CPU_OP_RES_7_C, alu_res, 7, cpu->reg.c);
			BITOP(CPU_OP_RES_7_D, alu_res, 7, cpu->reg.d);
			BITOP(CPU_OP_RES_7_E, alu_res, 7, cpu->reg.e);
			BITOP(CPU_OP_RES_7_H, alu_res, 7, cpu->reg.h);
			BITOP(CPU_OP_RES_7_L, alu_res, 7, cpu->reg.l);
			BITOP_HL(CPU_OP_RES_7_MEM_HL, alu_res, 7);
			BITOP(CPU_OP_RES_7_A, alu_res, 7, cpu->reg.a);
			BITOP(CPU_OP_SET_0_B, alu_set, 0, cpu->reg.b);
			BITOP(CPU_OP_SET_0_C, alu_set, 0, cpu->reg.c);
			BITOP(CPU_OP_SET_0_D, alu_set, 0, cpu->reg.d);
			BITOP(CPU_OP_SET_0_E, alu_set, 0, cpu->reg.e);
			BITOP(CPU_OP_SET_0_H, alu_set, 0, cpu->reg.h);
			BITOP(CPU_OP_SET_0_L, alu_set, 0, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_0_MEM_HL, alu_set, 0);
			BITOP(CPU_OP_SET_0_A, alu_set, 0, cpu->reg.a);
			BITOP(CPU_OP_SET_1_B, alu_set, 1, cpu->reg.b);
			BITOP(CPU_OP_SET_1_C, alu_set, 1, cpu->reg.c);
			BITOP(CPU_OP_SET_1_D, alu_set, 1, cpu->reg.d);
			BITOP(CPU_OP_SET_1_E, alu_set, 1, cpu->reg.e);
			BITOP(CPU_OP_SET_1_H, alu_set, 1, cpu->reg.h);
			BITOP(CPU_OP_SET_1_L, alu_set, 1, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_1_MEM_HL, alu_set, 1);
			BITOP(CPU_OP_SET_1_A, alu_set, 1, cpu->reg.a);
			BITOP(CPU_OP_SET_2_B, alu_set, 2, cpu->reg.b);
			BITOP(CPU_OP_SET_2_C, alu_set, 2, cpu->reg.c);
			BITOP(CPU_OP_SET_2_D, alu_set, 2, cpu->reg.d);
			BITOP(CPU_OP_SET_2_E, alu_set, 2, cpu->reg.e);
			BITOP(CPU_OP_SET_2_H, alu_set, 2, cpu->reg.h);
			BITOP(CPU_OP_SET_2_L, alu_set, 2, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_2_MEM_HL, alu_set, 2);
			BITOP(CPU_OP_SET_2_A, alu_set, 2, cpu->reg.a);
			BITOP(CPU_OP_SET_3_B, alu_set, 3, cpu->reg.b);
			BITOP(CPU_OP_SET_3_C, alu_set, 3, cpu->reg.c);
			BITOP(CPU_OP_SET_3_D, alu_set, 3, cpu->reg.d);
			BITOP(CPU_OP_SET_3_E, alu_set, 3, cpu->reg.e);
			BITOP(CPU_OP_SET_3_H, alu_set, 3, cpu->reg.h);
			BITOP(CPU_OP_SET_3_L, alu_set, 3, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_3_MEM_HL, alu_set, 3);
			BITOP(CPU_OP_SET_3_A, alu_set, 3, cpu->reg.a);
			BITOP(CPU_OP_SET_4_B, alu_set, 4, cpu->reg.b);
			BITOP(CPU_OP_SET_4_C, alu_set, 4, cpu->reg.c);
			BITOP(CPU_OP_SET_4_D, alu_set, 4, cpu->reg.d);
			BITOP(CPU_OP_SET_4_E, alu_set, 4, cpu->reg.e);
			BITOP(CPU_OP_SET_4_H, alu_set, 4, cpu->reg.h);
			BITOP(CPU_OP_SET_4_L, alu_set, 4, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_4_MEM_HL, alu_set, 4);
			BITOP(CPU_OP_SET_4_A, alu_set, 4, cpu->reg.a);
			BITOP(CPU_OP_SET_5_B, alu_set, 5, cpu->reg.b);
			BITOP(CPU_OP_SET_5_C, alu_set, 5, cpu->reg.c);
			BITOP(CPU_OP_SET_5_D, alu_set, 5, cpu->reg.d);
			BITOP(CPU_OP_SET_5_E, alu_set, 5, cpu->reg.e);
			BITOP(CPU_OP_SET_5_H, alu_set, 5, cpu->reg.h);
			BITOP(CPU_OP_SET_5_L, alu_set, 5, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_5_MEM_HL, alu_set, 5);
			BITOP(CPU_OP_SET_5_A, alu_set, 5, cpu->reg.a);
			BITOP(CPU_OP_SET_6_B, alu_set, 6, cpu->reg.b);
			BITOP(CPU_OP_SET_6_C, alu_set, 6, cpu->reg.c);
			BITOP(CPU_OP_SET_6_D, alu_set, 6, cpu->reg.d);
			BITOP(CPU_OP_SET_6_E, alu_set, 6, cpu->reg.e);
			BITOP(CPU_OP_SET_6_H, alu_set, 6, cpu->reg.h);
			BITOP(CPU_OP_SET_6_L, alu_set, 6, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_6_MEM_HL, alu_set, 6);
			BITOP(CPU_OP_SET_6_A, alu_set, 6, cpu->reg.a);
			BITOP(CPU_OP_SET_7_B, alu_set, 7, cpu->reg.b);
			BITOP(CPU_OP_SET_7_C, alu_set, 7, cpu->reg.c);
			BITOP(CPU_OP_SET_7_D, alu_set, 7, cpu->reg.d);
			BITOP(CPU_OP_SET_7_E, alu_set, 7, cpu->reg.e);
			BITOP(CPU_OP_SET_7_H, alu_set, 7, cpu->reg.h);
			BITOP(CPU_OP_SET_7_L, alu_set, 7, cpu->reg.l);
			BITOP_HL(CPU_OP_SET_7_MEM_HL, alu_set, 7);
			BITOP(CPU_OP_SET_7_A, alu_set, 7, cpu->reg.a);
			default: UNREACHABLE;
		}
	}

	BRANCH(CPU_OP_CALL_Z_U16, call_if, cpu->reg.f & CPU_ZERO_FLAG);
	BRANCH(CPU_OP_CALL_U16, call_if, true);
	OP_U8(CPU_OP_ADC_A_U8, alu_adc);
	RST(CPU_OP_RST_08, 0x0008);
	BRANCH(CPU_OP_RET_NC, ret_if, !(cpu->reg.f & CPU_CARRY_FLAG));
	POP(CPU_OP_POP_DE, cpu->reg.de);
	BRANCH(CPU_OP_JP_NC_U16, jp_if, !(cpu->reg.f & CPU_CARRY_FLAG));
	BRANCH(CPU_OP_CALL_NC_U16, call_if, !(cpu->reg.f & CPU_CARRY_FLAG));
	PUSH(CPU_OP_PUSH_DE, cpu->reg.de);
	OP_U8(CPU_OP_SUB_A_U8, alu_sub);
	RST(CPU_OP_RST_10, 0x0010);
	BRANCH(CPU_OP_RET_C, ret_if, cpu->reg.f & CPU_CARRY_FLAG);
	BRANCH(CPU_OP_RETI, ret_if, true); // FIX!
	BRANCH(CPU_OP_JP_C_U16, jp_if, cpu->reg.f & CPU_CARRY_FLAG);
	BRANCH(CPU_OP_CALL_C_U16, call_if, cpu->reg.f & CPU_CARRY_FLAG);
	OP_U8(CPU_OP_SBC_A_U8, alu_sbc);
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
	OP_U8(CPU_OP_AND_A_U8, alu_and);
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

	OP_U8(CPU_OP_XOR_A_U8, alu_xor);
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
	OP_U8(CPU_OP_OR_A_U8, alu_or);
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

	OP_U8(CPU_OP_CP_A_U8, alu_cp);
	RST(CPU_OP_RST_38, 0x0038);

	default:
		LOG_ERR(cpu->bus->log,
			"Illegal instruction $%02X trapped at program counter "
			"$%04X.",
			instr, cpu->reg.pc);
	}
}
