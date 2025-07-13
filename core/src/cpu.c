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

#include <assert.h>
#include <stdbool.h>

#include "comp.h"
#include "cpu.h"
#include "cpu-defs.h"
#include "bus.h"
#include "log.h"

LOG_CHANNEL(AGOGE_CORE_LOG_CH_CPU);

NODISCARD static uint8_t read_u8(struct agoge_core_ctx *const ctx)
{
	return agoge_core_bus_read(ctx, ctx->cpu.reg.pc++);
}

NODISCARD static uint16_t read_u16(struct agoge_core_ctx *const ctx)
{
	const uint8_t lo = read_u8(ctx);
	const uint8_t hi = read_u8(ctx);

	return (hi << 8) | lo;
}

static void flag_upd(struct agoge_core_ctx *const ctx, const uint8_t flag,
		     const bool cond_met)
{
	if (cond_met) {
		ctx->cpu.reg.f |= flag;
	} else {
		ctx->cpu.reg.f &= ~flag;
	}
}

static void flag_zero_upd(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	flag_upd(ctx, CPU_FLAG_ZERO, !val);
}

NODISCARD static uint8_t alu_inc(struct agoge_core_ctx *const ctx, uint8_t val)
{
	ctx->cpu.reg.f &= ~CPU_FLAG_SUBTRACT;
	flag_upd(ctx, CPU_FLAG_HALF_CARRY, (val & 0x0F) == 0x0F);
	flag_zero_upd(ctx, ++val);

	return val;
}

static void alu_inc_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_inc(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_dec(struct agoge_core_ctx *const ctx, uint8_t val)
{
	ctx->cpu.reg.f |= CPU_FLAG_SUBTRACT;
	flag_upd(ctx, CPU_FLAG_HALF_CARRY, !(val & 0x0F));
	flag_zero_upd(ctx, --val);

	return val;
}

static void stack_push(struct agoge_core_ctx *const ctx, const uint16_t val)
{
	agoge_core_bus_write(ctx, --ctx->cpu.reg.sp, val >> 8);
	agoge_core_bus_write(ctx, --ctx->cpu.reg.sp, val & UINT8_MAX);
}

NODISCARD static uint16_t stack_pop(struct agoge_core_ctx *const ctx)
{
	const uint8_t lo = agoge_core_bus_read(ctx, ctx->cpu.reg.sp++);
	const uint8_t hi = agoge_core_bus_read(ctx, ctx->cpu.reg.sp++);

	return (hi << 8) | lo;
}

NODISCARD static uint8_t alu_rlc_op(struct agoge_core_ctx *const ctx,
				    const uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	flag_upd(ctx, CPU_FLAG_CARRY, val & BIT_7);

	return (val << 1) | (val >> 7);
}

NODISCARD static uint8_t alu_rl_op(struct agoge_core_ctx *const ctx,
				   const uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	const bool carry = ctx->cpu.reg.f & CPU_FLAG_CARRY;

	flag_upd(ctx, CPU_FLAG_CARRY, val & BIT_7);
	return (val << 1) | carry;
}

NODISCARD static uint8_t alu_rl(struct agoge_core_ctx *const ctx, uint8_t val)
{
	val = alu_rl_op(ctx, val);
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_rl_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_rl(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_rlc(struct agoge_core_ctx *const ctx, uint8_t val)
{
	val = alu_rlc_op(ctx, val);
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_rlc_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_rlc(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_rrc_op(struct agoge_core_ctx *const ctx,
				    const uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	flag_upd(ctx, CPU_FLAG_CARRY, val & 1);

	return (val >> 1) | (val << 7);
}

NODISCARD static uint8_t alu_rrc(struct agoge_core_ctx *const ctx, uint8_t val)
{
	val = alu_rrc_op(ctx, val);
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_rrc_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_rrc(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_rr_op(struct agoge_core_ctx *const ctx,
				   uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);

	const bool old_carry = val & BIT_0;
	const bool carry = ctx->cpu.reg.f & CPU_FLAG_CARRY;

	val = (val >> 1) | (carry << 7);
	flag_upd(ctx, CPU_FLAG_CARRY, old_carry);

	return val;
}

NODISCARD static uint8_t alu_rr(struct agoge_core_ctx *const ctx, uint8_t val)
{
	val = alu_rr_op(ctx, val);
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_rr_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_rr(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_sla(struct agoge_core_ctx *const ctx, uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);

	flag_upd(ctx, CPU_FLAG_CARRY, val & BIT_7);
	val <<= 1;
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_sla_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_sla(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_sra(struct agoge_core_ctx *const ctx, uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);

	flag_upd(ctx, CPU_FLAG_CARRY, val & BIT_0);
	val = (val >> 1) | (val & BIT_7);
	flag_zero_upd(ctx, val);

	return val;
}

static void alu_sra_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_sra(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_swap(struct agoge_core_ctx *const ctx, uint8_t val)
{
	val = ((val & 0x0F) << 4) | (val >> 4);
	ctx->cpu.reg.f = (!val) ? CPU_FLAG_ZERO : 0x00;

	return val;
}

static void alu_swap_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_swap(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

NODISCARD static uint8_t alu_srl(struct agoge_core_ctx *const ctx, uint8_t val)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	const bool carry = val & BIT_0;

	val >>= 1;

	flag_zero_upd(ctx, val);
	flag_upd(ctx, CPU_FLAG_CARRY, carry);

	return val;
}

static void alu_srl_mem_hl(struct agoge_core_ctx *const ctx)
{
	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val = alu_srl(ctx, val);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

static void alu_bit(struct agoge_core_ctx *const ctx, const unsigned int b,
		    const uint8_t val)
{
	// There are only 8 bits in a byte, indexed from 0 to 7, so, passing any
	// other value doesn't make sense and is always a bug. Should be a very
	// trivial fix for you.
	assert(b <= 7);

	ctx->cpu.reg.f &= ~CPU_FLAG_SUBTRACT;
	ctx->cpu.reg.f |= CPU_FLAG_HALF_CARRY;

	flag_zero_upd(ctx, val & (UINT8_C(1) << b));
}

NODISCARD static uint16_t alu_add_sp(struct agoge_core_ctx *const ctx)
{
	ctx->cpu.reg.f &= ~(CPU_FLAG_ZERO | CPU_FLAG_SUBTRACT);
	const int8_t s8 = (int8_t)read_u8(ctx);

	const int sum = ctx->cpu.reg.sp + s8;

	flag_upd(ctx, CPU_FLAG_HALF_CARRY, (ctx->cpu.reg.sp ^ s8 ^ sum) & 0x10);
	flag_upd(ctx, CPU_FLAG_CARRY, (ctx->cpu.reg.sp ^ s8 ^ sum) & 0x100);

	return sum;
}

static void alu_add_hl(struct agoge_core_ctx *const ctx, const uint16_t val)
{
	ctx->cpu.reg.f &= ~CPU_FLAG_SUBTRACT;

	const unsigned int sum = ctx->cpu.reg.hl + val;

	flag_upd(ctx, CPU_FLAG_HALF_CARRY,
		 (ctx->cpu.reg.hl ^ val ^ sum) & 0x1000);

	flag_upd(ctx, CPU_FLAG_CARRY, sum > UINT16_MAX);

	ctx->cpu.reg.hl = sum;
}

static void alu_add_op(struct agoge_core_ctx *const ctx, const uint8_t val,
		       const bool carry)
{
	ctx->cpu.reg.f &= ~CPU_FLAG_SUBTRACT;

	const unsigned int res = ctx->cpu.reg.a + val + carry;
	const uint8_t sum = res;

	flag_zero_upd(ctx, sum);
	flag_upd(ctx, CPU_FLAG_HALF_CARRY, (ctx->cpu.reg.a ^ val ^ res) & 0x10);
	flag_upd(ctx, CPU_FLAG_CARRY, res > UINT8_MAX);

	ctx->cpu.reg.a = sum;
}

static uint8_t alu_sub_op(struct agoge_core_ctx *const ctx, const uint8_t val,
			  const bool carry)
{
	ctx->cpu.reg.f |= CPU_FLAG_SUBTRACT;

	const int res = ctx->cpu.reg.a - val - carry;
	const uint8_t diff = res;

	flag_zero_upd(ctx, diff);
	flag_upd(ctx, CPU_FLAG_HALF_CARRY, (ctx->cpu.reg.a ^ val ^ res) & 0x10);
	flag_upd(ctx, CPU_FLAG_CARRY, res < 0);

	return diff;
}

static void alu_add(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	alu_add_op(ctx, val, 0);
}

static void alu_adc(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	alu_add_op(ctx, val, ctx->cpu.reg.f & CPU_FLAG_CARRY);
}

static void alu_sub(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	ctx->cpu.reg.a = alu_sub_op(ctx, val, 0);
}

static void alu_sbc(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	ctx->cpu.reg.a = alu_sub_op(ctx, val, ctx->cpu.reg.f & CPU_FLAG_CARRY);
}

static void alu_cp(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	(void)alu_sub_op(ctx, val, 0);
}

static void alu_res_hl(struct agoge_core_ctx *const ctx, const unsigned int b)
{
	// There are only 8 bits in a byte, indexed from 0 to 7, so, passing any
	// other value doesn't make sense and is always a bug. Should be a very
	// trivial fix for you.
	assert(b <= 7);

	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val &= ~(UINT8_C(1) << b);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

static void alu_set_hl(struct agoge_core_ctx *const ctx, const unsigned int b)
{
	// There are only 8 bits in a byte, indexed from 0 to 7, so, passing any
	// other value doesn't make sense and is always a bug. Should be a very
	// trivial fix for you.
	assert(b <= 7);

	uint8_t val = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	val |= (UINT8_C(1) << b);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, val);
}

static void jp_if(struct agoge_core_ctx *const ctx, const bool cond_met)
{
	const uint16_t addr = read_u16(ctx);

	if (cond_met) {
		ctx->cpu.reg.pc = addr;
	}
}

static void ret_if(struct agoge_core_ctx *const ctx, const bool cond_met)
{
	if (cond_met) {
		ctx->cpu.reg.pc = stack_pop(ctx);
	}
}

static void call_if(struct agoge_core_ctx *const ctx, const bool cond_met)
{
	const uint16_t addr = read_u16(ctx);

	if (cond_met) {
		stack_push(ctx, ctx->cpu.reg.pc);
		ctx->cpu.reg.pc = addr;
	}
}

static void jr_if(struct agoge_core_ctx *const ctx, const bool cond_met)
{
	const int8_t off = (int8_t)read_u8(ctx);

	if (cond_met) {
		ctx->cpu.reg.pc += off;
	}
}

static void alu_or(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	ctx->cpu.reg.a |= val;
	ctx->cpu.reg.f = (!ctx->cpu.reg.a) ? CPU_FLAG_ZERO : 0x00;
}

static void alu_and(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	ctx->cpu.reg.a &= val;
	ctx->cpu.reg.f = (!ctx->cpu.reg.a) ?
				 (CPU_FLAG_ZERO | CPU_FLAG_HALF_CARRY) :
				 CPU_FLAG_HALF_CARRY;
}

static void alu_xor(struct agoge_core_ctx *const ctx, const uint8_t val)
{
	ctx->cpu.reg.a ^= val;
	ctx->cpu.reg.f = (!ctx->cpu.reg.a) ? CPU_FLAG_ZERO : 0x00;
}

static void op_daa(struct agoge_core_ctx *const ctx)
{
	uint8_t val = 0;

	if (ctx->cpu.reg.f & CPU_FLAG_HALF_CARRY) {
		val |= 0x06;
	}

	if (ctx->cpu.reg.f & CPU_FLAG_CARRY) {
		val |= 0x60;
	}

	if (!(ctx->cpu.reg.f & CPU_FLAG_SUBTRACT)) {
		if ((ctx->cpu.reg.a & 0x0F) > 0x09) {
			val |= 0x06;
		}

		if (ctx->cpu.reg.a > 0x99) {
			val |= 0x60;
		}
		ctx->cpu.reg.a += val;
	} else {
		ctx->cpu.reg.a -= val;
	}

	flag_zero_upd(ctx, ctx->cpu.reg.a);
	flag_upd(ctx, CPU_FLAG_CARRY, val & 0x60);

	ctx->cpu.reg.f &= ~CPU_FLAG_HALF_CARRY;
}

static void rst(struct agoge_core_ctx *const ctx, const uint16_t vec)
{
	stack_push(ctx, ctx->cpu.reg.pc);
	ctx->cpu.reg.pc = vec;
}

void agoge_core_cpu_reset(struct agoge_core_ctx *const ctx)
{
	ctx->cpu.reg.pc = CPU_PWRUP_REG_PC;
}

void agoge_core_cpu_run(struct agoge_core_ctx *const ctx,
			unsigned int run_cycles)
{
	// The CPU was requested to run for zero cycles; this is nonsense.
	assert(run_cycles != 0);

#define DISPATCH()                                 \
	({                                         \
		if (unlikely(run_cycles-- == 0)) { \
			return;                    \
		}                                  \
		instr = read_u8(ctx);              \
		goto *op_tbl[instr];               \
	})

	static const void *const op_tbl[] = {
		// clang-format off

		[CPU_OP_NOP]			= &&nop,
		[CPU_OP_LD_BC_U16]		= &&ld_bc_u16,
		[CPU_OP_LD_MEM_BC_A]		= &&ld_mem_bc_a,
		[CPU_OP_INC_BC]			= &&inc_bc,
		[CPU_OP_INC_B]			= &&inc_b,
		[CPU_OP_DEC_B]			= &&dec_b,
		[CPU_OP_LD_B_U8]		= &&ld_b_u8,
		[CPU_OP_RLCA]			= &&rlca,
		[CPU_OP_LD_MEM_U16_SP]		= &&ld_mem_u16_sp,
		[CPU_OP_ADD_HL_BC]		= &&add_hl_bc,
		[CPU_OP_LD_A_MEM_BC]		= &&ld_a_mem_bc,
		[CPU_OP_DEC_BC]			= &&dec_bc,
		[CPU_OP_INC_C]			= &&inc_c,
		[CPU_OP_DEC_C]			= &&dec_c,
		[CPU_OP_LD_C_U8]		= &&ld_c_u8,
		[CPU_OP_RRCA]			= &&rrca,
		[CPU_OP_LD_DE_U16]		= &&ld_de_u16,
		[CPU_OP_LD_MEM_DE_A]		= &&ld_mem_de_a,
		[CPU_OP_INC_DE]			= &&inc_de,
		[CPU_OP_INC_D]			= &&inc_d,
		[CPU_OP_DEC_D]			= &&dec_d,
		[CPU_OP_LD_D_U8]		= &&ld_d_u8,
		[CPU_OP_RLA]			= &&rla,
		[CPU_OP_JR_S8]			= &&jr_s8,
		[CPU_OP_ADD_HL_DE]		= &&add_hl_de,
		[CPU_OP_LD_A_MEM_DE]		= &&ld_a_mem_de,
		[CPU_OP_DEC_DE]			= &&dec_de,
		[CPU_OP_INC_E]			= &&inc_e,
		[CPU_OP_DEC_E]			= &&dec_e,
		[CPU_OP_LD_E_U8]		= &&ld_e_u8,
		[CPU_OP_RRA]			= &&rra,
		[CPU_OP_JR_NZ_S8]		= &&jr_nz_s8,
		[CPU_OP_LD_HL_U16]		= &&ld_hl_u16,
		[CPU_OP_LDI_MEM_HL_A]		= &&ldi_mem_hl_a,
		[CPU_OP_INC_HL]			= &&inc_hl,
		[CPU_OP_INC_H]			= &&inc_h,
		[CPU_OP_DEC_H]			= &&dec_h,
		[CPU_OP_LD_H_U8]		= &&ld_h_u8,
		[CPU_OP_DAA]			= &&daa,
		[CPU_OP_JR_Z_S8]		= &&jr_z_s8,
		[CPU_OP_ADD_HL_HL]		= &&add_hl_hl,
		[CPU_OP_LDI_A_MEM_HL]		= &&ldi_a_mem_hl,
		[CPU_OP_DEC_HL]			= &&dec_hl,
		[CPU_OP_INC_L]			= &&inc_l,
		[CPU_OP_DEC_L]			= &&dec_l,
		[CPU_OP_LD_L_U8]		= &&ld_l_u8,
		[CPU_OP_CPL]			= &&cpl,
		[CPU_OP_JR_NC_S8]		= &&jr_nc_s8,
		[CPU_OP_LD_SP_U16]		= &&ld_sp_u16,
		[CPU_OP_LDD_MEM_HL_A]		= &&ldd_mem_hl_a,
		[CPU_OP_INC_SP]			= &&inc_sp,
		[CPU_OP_INC_MEM_HL]		= &&inc_mem_hl,
		[CPU_OP_DEC_MEM_HL]		= &&dec_mem_hl,
		[CPU_OP_LD_MEM_HL_U8]		= &&ld_mem_hl_u8,
		[CPU_OP_SCF]			= &&scf,
		[CPU_OP_JR_C_S8]		= &&jr_c_s8,
		[CPU_OP_ADD_HL_SP]		= &&add_hl_sp,
		[CPU_OP_LDD_A_MEM_HL]		= &&ldd_a_mem_hl,
		[CPU_OP_DEC_SP]			= &&dec_sp,
		[CPU_OP_INC_A]			= &&inc_a,
		[CPU_OP_DEC_A]			= &&dec_a,
		[CPU_OP_LD_A_U8]		= &&ld_a_u8,
		[CPU_OP_CCF]			= &&ccf,
		[CPU_OP_LD_B_B]			= &&ld_b_b,
		[CPU_OP_LD_B_C]			= &&ld_b_c,
		[CPU_OP_LD_B_D]			= &&ld_b_d,
		[CPU_OP_LD_B_E]			= &&ld_b_e,
		[CPU_OP_LD_B_H]			= &&ld_b_h,
		[CPU_OP_LD_B_L]			= &&ld_b_l,
		[CPU_OP_LD_B_MEM_HL]		= &&ld_b_mem_hl,
		[CPU_OP_LD_B_A]			= &&ld_b_a,
		[CPU_OP_LD_C_B]			= &&ld_c_b,
		[CPU_OP_LD_C_C]			= &&ld_c_c,
		[CPU_OP_LD_C_D]			= &&ld_c_d,
		[CPU_OP_LD_C_E]			= &&ld_c_e,
		[CPU_OP_LD_C_H]			= &&ld_c_h,
		[CPU_OP_LD_C_L]			= &&ld_c_l,
		[CPU_OP_LD_C_MEM_HL]		= &&ld_c_mem_hl,
		[CPU_OP_LD_C_A]			= &&ld_c_a,
		[CPU_OP_LD_D_B]			= &&ld_d_b,
		[CPU_OP_LD_D_C]			= &&ld_d_c,
		[CPU_OP_LD_D_D]			= &&ld_d_d,
		[CPU_OP_LD_D_E]			= &&ld_d_e,
		[CPU_OP_LD_D_H]			= &&ld_d_h,
		[CPU_OP_LD_D_L]			= &&ld_d_l,
		[CPU_OP_LD_D_MEM_HL]		= &&ld_d_mem_hl,
		[CPU_OP_LD_D_A]			= &&ld_d_a,
		[CPU_OP_LD_E_B]			= &&ld_e_b,
		[CPU_OP_LD_E_C]			= &&ld_e_c,
		[CPU_OP_LD_E_D]			= &&ld_e_d,
		[CPU_OP_LD_E_E]			= &&ld_e_e,
		[CPU_OP_LD_E_H]			= &&ld_e_h,
		[CPU_OP_LD_E_L]			= &&ld_e_l,
		[CPU_OP_LD_E_MEM_HL]		= &&ld_e_mem_hl,
		[CPU_OP_LD_E_A]			= &&ld_e_a,
		[CPU_OP_LD_H_B]			= &&ld_h_b,
		[CPU_OP_LD_H_C]			= &&ld_h_c,
		[CPU_OP_LD_H_D]			= &&ld_h_d,
		[CPU_OP_LD_H_E]			= &&ld_h_e,
		[CPU_OP_LD_H_H]			= &&ld_h_h,
		[CPU_OP_LD_H_L]			= &&ld_h_l,
		[CPU_OP_LD_H_MEM_HL]		= &&ld_h_mem_hl,
		[CPU_OP_LD_H_A]			= &&ld_h_a,
		[CPU_OP_LD_L_B]			= &&ld_l_b,
		[CPU_OP_LD_L_C]			= &&ld_l_c,
		[CPU_OP_LD_L_D]			= &&ld_l_d,
		[CPU_OP_LD_L_E]			= &&ld_l_e,
		[CPU_OP_LD_L_H]			= &&ld_l_h,
		[CPU_OP_LD_L_L]			= &&ld_l_l,
		[CPU_OP_LD_L_MEM_HL]		= &&ld_l_mem_hl,
		[CPU_OP_LD_L_A]			= &&ld_l_a,
		[CPU_OP_LD_MEM_HL_B]		= &&ld_mem_hl_b,
		[CPU_OP_LD_MEM_HL_C]		= &&ld_mem_hl_c,
		[CPU_OP_LD_MEM_HL_D]		= &&ld_mem_hl_d,
		[CPU_OP_LD_MEM_HL_E]		= &&ld_mem_hl_e,
		[CPU_OP_LD_MEM_HL_H]		= &&ld_mem_hl_h,
		[CPU_OP_LD_MEM_HL_L]		= &&ld_mem_hl_l,
		[CPU_OP_LD_MEM_HL_A]		= &&ld_mem_hl_a,
		[CPU_OP_LD_A_B]			= &&ld_a_b,
		[CPU_OP_LD_A_C]			= &&ld_a_c,
		[CPU_OP_LD_A_D]			= &&ld_a_d,
		[CPU_OP_LD_A_E]			= &&ld_a_e,
		[CPU_OP_LD_A_H]			= &&ld_a_h,
		[CPU_OP_LD_A_L]			= &&ld_a_l,
		[CPU_OP_LD_A_MEM_HL]		= &&ld_a_mem_hl,
		[CPU_OP_LD_A_A]			= &&ld_a_a,
		[CPU_OP_ADD_A_B]		= &&add_a_b,
		[CPU_OP_ADD_A_C]		= &&add_a_c,
		[CPU_OP_ADD_A_D]		= &&add_a_d,
		[CPU_OP_ADD_A_E]		= &&add_a_e,
		[CPU_OP_ADD_A_H]		= &&add_a_h,
		[CPU_OP_ADD_A_L]		= &&add_a_l,
		[CPU_OP_ADD_A_MEM_HL]		= &&add_a_mem_hl,
		[CPU_OP_ADD_A_A]		= &&add_a_a,
		[CPU_OP_ADC_A_B]		= &&adc_a_b,
		[CPU_OP_ADC_A_C]		= &&adc_a_c,
		[CPU_OP_ADC_A_D]		= &&adc_a_d,
		[CPU_OP_ADC_A_E]		= &&adc_a_e,
		[CPU_OP_ADC_A_H]		= &&adc_a_h,
		[CPU_OP_ADC_A_L]		= &&adc_a_l,
		[CPU_OP_ADC_A_MEM_HL]		= &&adc_a_mem_hl,
		[CPU_OP_ADC_A_A]		= &&adc_a_a,
		[CPU_OP_SUB_A_B]		= &&sub_a_b,
		[CPU_OP_SUB_A_C]		= &&sub_a_c,
		[CPU_OP_SUB_A_D]		= &&sub_a_d,
		[CPU_OP_SUB_A_E]		= &&sub_a_e,
		[CPU_OP_SUB_A_H]		= &&sub_a_h,
		[CPU_OP_SUB_A_L]		= &&sub_a_l,
		[CPU_OP_SUB_A_MEM_HL]		= &&sub_a_mem_hl,
		[CPU_OP_SUB_A_A]		= &&sub_a_a,
		[CPU_OP_SBC_A_B]		= &&sbc_a_b,
		[CPU_OP_SBC_A_C]		= &&sbc_a_c,
		[CPU_OP_SBC_A_D]		= &&sbc_a_d,
		[CPU_OP_SBC_A_E]		= &&sbc_a_e,
		[CPU_OP_SBC_A_H]		= &&sbc_a_h,
		[CPU_OP_SBC_A_L]		= &&sbc_a_l,
		[CPU_OP_SBC_A_MEM_HL]		= &&sbc_a_mem_hl,
		[CPU_OP_SBC_A_A]		= &&sbc_a_a,
		[CPU_OP_AND_A_B]		= &&and_a_b,
		[CPU_OP_AND_A_C]		= &&and_a_c,
		[CPU_OP_AND_A_D]		= &&and_a_d,
		[CPU_OP_AND_A_E]		= &&and_a_e,
		[CPU_OP_AND_A_H]		= &&and_a_h,
		[CPU_OP_AND_A_L]		= &&and_a_l,
		[CPU_OP_AND_A_MEM_HL]		= &&and_a_mem_hl,
		[CPU_OP_AND_A_A]		= &&and_a_a,
		[CPU_OP_XOR_A_B]		= &&xor_a_b,
		[CPU_OP_XOR_A_C]		= &&xor_a_c,
		[CPU_OP_XOR_A_D]		= &&xor_a_d,
		[CPU_OP_XOR_A_E]		= &&xor_a_e,
		[CPU_OP_XOR_A_H]		= &&xor_a_h,
		[CPU_OP_XOR_A_L]		= &&xor_a_l,
		[CPU_OP_XOR_A_MEM_HL]		= &&xor_a_mem_hl,
		[CPU_OP_XOR_A_A]		= &&xor_a_a,
		[CPU_OP_OR_A_B]			= &&or_a_b,
		[CPU_OP_OR_A_C]			= &&or_a_c,
		[CPU_OP_OR_A_D]			= &&or_a_d,
		[CPU_OP_OR_A_E]			= &&or_a_e,
		[CPU_OP_OR_A_H]			= &&or_a_h,
		[CPU_OP_OR_A_L]			= &&or_a_l,
		[CPU_OP_OR_A_MEM_HL]		= &&or_a_mem_hl,
		[CPU_OP_OR_A_A]			= &&or_a_a,
		[CPU_OP_CP_A_B]			= &&cp_a_b,
		[CPU_OP_CP_A_C]			= &&cp_a_c,
		[CPU_OP_CP_A_D]			= &&cp_a_d,
		[CPU_OP_CP_A_E]			= &&cp_a_e,
		[CPU_OP_CP_A_H]			= &&cp_a_h,
		[CPU_OP_CP_A_L]			= &&cp_a_l,
		[CPU_OP_CP_A_MEM_HL]		= &&cp_a_mem_hl,
		[CPU_OP_CP_A_A]			= &&cp_a_a,
		[CPU_OP_RET_NZ]			= &&ret_nz,
		[CPU_OP_POP_BC]			= &&pop_bc,
		[CPU_OP_JP_NZ_U16]		= &&jp_nz_u16,
		[CPU_OP_JP_U16]			= &&jp_u16,
		[CPU_OP_CALL_NZ_U16]		= &&call_nz_u16,
		[CPU_OP_PUSH_BC]		= &&push_bc,
		[CPU_OP_ADD_A_U8]		= &&add_a_u8,
		[CPU_OP_RST_00]			= &&rst_00,
		[CPU_OP_RET_Z]			= &&ret_z,
		[CPU_OP_RET]			= &&ret,
		[CPU_OP_JP_Z_U16]		= &&jp_z_u16,
		[CPU_OP_PREFIX_CB]		= &&prefix_cb,
		[CPU_OP_CALL_Z_U16]		= &&call_z_u16,
		[CPU_OP_CALL_U16]		= &&call_u16,
		[CPU_OP_ADC_A_U8]		= &&adc_a_u8,
		[CPU_OP_RST_08]			= &&rst_08,
		[CPU_OP_RET_NC]			= &&ret_nc,
		[CPU_OP_POP_DE]			= &&pop_de,
		[CPU_OP_JP_NC_U16]		= &&jp_nc_u16,
		[CPU_OP_CALL_NC_U16]		= &&call_nc_u16,
		[CPU_OP_PUSH_DE]		= &&push_de,
		[CPU_OP_SUB_A_U8]		= &&sub_a_u8,
		[CPU_OP_RST_10]			= &&rst_10,
		[CPU_OP_RET_C]			= &&ret_c,
		[CPU_OP_RETI]			= &&reti,
		[CPU_OP_JP_C_U16]		= &&jp_c_u16,
		[CPU_OP_CALL_C_U16]		= &&call_c_u16,
		[CPU_OP_SBC_A_U8]		= &&sbc_a_u8,
		[CPU_OP_RST_18]			= &&rst_18,
		[CPU_OP_LD_MEM_FF00_U8_A]	= &&ld_mem_ff00_u8_a,
		[CPU_OP_POP_HL]			= &&pop_hl,
		[CPU_OP_LD_MEM_FF00_C_A]	= &&ld_mem_ff00_c_a,
		[CPU_OP_PUSH_HL]		= &&push_hl,
		[CPU_OP_AND_A_U8]		= &&and_a_u8,
		[CPU_OP_RST_20]			= &&rst_20,
		[CPU_OP_ADD_SP_S8]		= &&add_sp_s8,
		[CPU_OP_JP_HL]			= &&jp_hl,
		[CPU_OP_LD_MEM_U16_A]		= &&ld_mem_u16_a,
		[CPU_OP_XOR_A_U8]		= &&xor_a_u8,
		[CPU_OP_RST_28]			= &&rst_28,
		[CPU_OP_LD_A_MEM_FF00_U8]	= &&ld_a_mem_ff00_u8,
		[CPU_OP_LD_A_MEM_FF00_C]	= &&ld_a_mem_ff00_c,
		[CPU_OP_POP_AF]			= &&pop_af,
		[CPU_OP_DI]			= &&di,
		[CPU_OP_PUSH_AF]		= &&push_af,
		[CPU_OP_OR_A_U8]		= &&or_a_u8,
		[CPU_OP_RST_30]			= &&rst_30,
		[CPU_OP_LD_HL_SP_S8]		= &&ld_hl_sp_s8,
		[CPU_OP_LD_SP_HL]		= &&ld_sp_hl,
		[CPU_OP_LD_A_MEM_U16]		= &&ld_a_mem_u16,
		[CPU_OP_EI]			= &&ei,
		[CPU_OP_CP_A_U8]		= &&cp_a_u8,
		[CPU_OP_RST_38]			= &&rst_38

		// clang-format on
	};

	static const void *const cb_tbl[] = {
		// clang-format off

		[CPU_OP_RLC_B]		= &&rlc_b,
		[CPU_OP_RLC_C]		= &&rlc_c,
		[CPU_OP_RLC_D]		= &&rlc_d,
		[CPU_OP_RLC_E]		= &&rlc_e,
		[CPU_OP_RLC_H]		= &&rlc_h,
		[CPU_OP_RLC_L]		= &&rlc_l,
		[CPU_OP_RLC_MEM_HL]	= &&rlc_mem_hl,
		[CPU_OP_RLC_A]		= &&rlc_a,
		[CPU_OP_RRC_B]		= &&rrc_b,
		[CPU_OP_RRC_C]		= &&rrc_c,
		[CPU_OP_RRC_D]		= &&rrc_d,
		[CPU_OP_RRC_E]		= &&rrc_e,
		[CPU_OP_RRC_H]		= &&rrc_h,
		[CPU_OP_RRC_L]		= &&rrc_l,
		[CPU_OP_RRC_MEM_HL]	= &&rrc_mem_hl,
		[CPU_OP_RRC_A]		= &&rrc_a,
		[CPU_OP_RL_B]		= &&rl_b,
		[CPU_OP_RL_C]		= &&rl_c,
		[CPU_OP_RL_D]		= &&rl_d,
		[CPU_OP_RL_E]		= &&rl_e,
		[CPU_OP_RL_H]		= &&rl_h,
		[CPU_OP_RL_L]		= &&rl_l,
		[CPU_OP_RL_MEM_HL]	= &&rl_mem_hl,
		[CPU_OP_RL_A]		= &&rl_a,
		[CPU_OP_RR_B]		= &&rr_b,
		[CPU_OP_RR_C]		= &&rr_c,
		[CPU_OP_RR_D]		= &&rr_d,
		[CPU_OP_RR_E]		= &&rr_e,
		[CPU_OP_RR_H]		= &&rr_h,
		[CPU_OP_RR_L]		= &&rr_l,
		[CPU_OP_RR_MEM_HL]	= &&rr_mem_hl,
		[CPU_OP_RR_A]		= &&rr_a,
		[CPU_OP_SLA_B]		= &&sla_b,
		[CPU_OP_SLA_C]		= &&sla_c,
		[CPU_OP_SLA_D]		= &&sla_d,
		[CPU_OP_SLA_E]		= &&sla_e,
		[CPU_OP_SLA_H]		= &&sla_h,
		[CPU_OP_SLA_L]		= &&sla_l,
		[CPU_OP_SLA_MEM_HL]	= &&sla_mem_hl,
		[CPU_OP_SLA_A]		= &&sla_a,
		[CPU_OP_SRA_B]		= &&sra_b,
		[CPU_OP_SRA_C]		= &&sra_c,
		[CPU_OP_SRA_D]		= &&sra_d,
		[CPU_OP_SRA_E]		= &&sra_e,
		[CPU_OP_SRA_H]		= &&sra_h,
		[CPU_OP_SRA_L]		= &&sra_l,
		[CPU_OP_SRA_MEM_HL]	= &&sra_mem_hl,
		[CPU_OP_SRA_A]		= &&sra_a,
		[CPU_OP_SWAP_B]		= &&swap_b,
		[CPU_OP_SWAP_C]		= &&swap_c,
		[CPU_OP_SWAP_D]		= &&swap_d,
		[CPU_OP_SWAP_E]		= &&swap_e,
		[CPU_OP_SWAP_H]		= &&swap_h,
		[CPU_OP_SWAP_L]		= &&swap_l,
		[CPU_OP_SWAP_MEM_HL]	= &&swap_mem_hl,
		[CPU_OP_SWAP_A]		= &&swap_a,
		[CPU_OP_SRL_B]		= &&srl_b,
		[CPU_OP_SRL_C]		= &&srl_c,
		[CPU_OP_SRL_D]		= &&srl_d,
		[CPU_OP_SRL_E]		= &&srl_e,
		[CPU_OP_SRL_H]		= &&srl_h,
		[CPU_OP_SRL_L]		= &&srl_l,
		[CPU_OP_SRL_MEM_HL]	= &&srl_mem_hl,
		[CPU_OP_SRL_A]		= &&srl_a,
		[CPU_OP_BIT_0_B]	= &&bit_0_b,
		[CPU_OP_BIT_0_C]	= &&bit_0_c,
		[CPU_OP_BIT_0_D]	= &&bit_0_d,
		[CPU_OP_BIT_0_E]	= &&bit_0_e,
		[CPU_OP_BIT_0_H]	= &&bit_0_h,
		[CPU_OP_BIT_0_L]	= &&bit_0_l,
		[CPU_OP_BIT_0_MEM_HL]	= &&bit_0_mem_hl,
		[CPU_OP_BIT_0_A]	= &&bit_0_a,
		[CPU_OP_BIT_1_B]	= &&bit_1_b,
		[CPU_OP_BIT_1_C]	= &&bit_1_c,
		[CPU_OP_BIT_1_D]	= &&bit_1_d,
		[CPU_OP_BIT_1_E]	= &&bit_1_e,
		[CPU_OP_BIT_1_H]	= &&bit_1_h,
		[CPU_OP_BIT_1_L]	= &&bit_1_l,
		[CPU_OP_BIT_1_MEM_HL]	= &&bit_1_mem_hl,
		[CPU_OP_BIT_1_A]	= &&bit_1_a,
		[CPU_OP_BIT_2_B]	= &&bit_2_b,
		[CPU_OP_BIT_2_C]	= &&bit_2_c,
		[CPU_OP_BIT_2_D]	= &&bit_2_d,
		[CPU_OP_BIT_2_E]	= &&bit_2_e,
		[CPU_OP_BIT_2_H]	= &&bit_2_h,
		[CPU_OP_BIT_2_L]	= &&bit_2_l,
		[CPU_OP_BIT_2_MEM_HL]	= &&bit_2_mem_hl,
		[CPU_OP_BIT_2_A]	= &&bit_2_a,
		[CPU_OP_BIT_3_B]	= &&bit_3_b,
		[CPU_OP_BIT_3_C]	= &&bit_3_c,
		[CPU_OP_BIT_3_D]	= &&bit_3_d,
		[CPU_OP_BIT_3_E]	= &&bit_3_e,
		[CPU_OP_BIT_3_H]	= &&bit_3_h,
		[CPU_OP_BIT_3_L]	= &&bit_3_l,
		[CPU_OP_BIT_3_MEM_HL]	= &&bit_3_mem_hl,
		[CPU_OP_BIT_3_A]	= &&bit_3_a,
		[CPU_OP_BIT_4_B]	= &&bit_4_b,
		[CPU_OP_BIT_4_C]	= &&bit_4_c,
		[CPU_OP_BIT_4_D]	= &&bit_4_d,
		[CPU_OP_BIT_4_E]	= &&bit_4_e,
		[CPU_OP_BIT_4_H]	= &&bit_4_h,
		[CPU_OP_BIT_4_L]	= &&bit_4_l,
		[CPU_OP_BIT_4_MEM_HL]	= &&bit_4_mem_hl,
		[CPU_OP_BIT_4_A]	= &&bit_4_a,
		[CPU_OP_BIT_5_B]	= &&bit_5_b,
		[CPU_OP_BIT_5_C]	= &&bit_5_c,
		[CPU_OP_BIT_5_D]	= &&bit_5_d,
		[CPU_OP_BIT_5_E]	= &&bit_5_e,
		[CPU_OP_BIT_5_H]	= &&bit_5_h,
		[CPU_OP_BIT_5_L]	= &&bit_5_l,
		[CPU_OP_BIT_5_MEM_HL]	= &&bit_5_mem_hl,
		[CPU_OP_BIT_5_A]	= &&bit_5_a,
		[CPU_OP_BIT_6_B]	= &&bit_6_b,
		[CPU_OP_BIT_6_C]	= &&bit_6_c,
		[CPU_OP_BIT_6_D]	= &&bit_6_d,
		[CPU_OP_BIT_6_E]	= &&bit_6_e,
		[CPU_OP_BIT_6_H]	= &&bit_6_h,
		[CPU_OP_BIT_6_L]	= &&bit_6_l,
		[CPU_OP_BIT_6_MEM_HL]	= &&bit_6_mem_hl,
		[CPU_OP_BIT_6_A]	= &&bit_6_a,
		[CPU_OP_BIT_7_B]	= &&bit_7_b,
		[CPU_OP_BIT_7_C]	= &&bit_7_c,
		[CPU_OP_BIT_7_D]	= &&bit_7_d,
		[CPU_OP_BIT_7_E]	= &&bit_7_e,
		[CPU_OP_BIT_7_H]	= &&bit_7_h,
		[CPU_OP_BIT_7_L]	= &&bit_7_l,
		[CPU_OP_BIT_7_MEM_HL]	= &&bit_7_mem_hl,
		[CPU_OP_BIT_7_A]	= &&bit_7_a,
		[CPU_OP_RES_0_B]	= &&res_0_b,
		[CPU_OP_RES_0_C]	= &&res_0_c,
		[CPU_OP_RES_0_D]	= &&res_0_d,
		[CPU_OP_RES_0_E]	= &&res_0_e,
		[CPU_OP_RES_0_H]	= &&res_0_h,
		[CPU_OP_RES_0_L]	= &&res_0_l,
		[CPU_OP_RES_0_MEM_HL]	= &&res_0_mem_hl,
		[CPU_OP_RES_0_A]	= &&res_0_a,
		[CPU_OP_RES_1_B]	= &&res_1_b,
		[CPU_OP_RES_1_C]	= &&res_1_c,
		[CPU_OP_RES_1_D]	= &&res_1_d,
		[CPU_OP_RES_1_E]	= &&res_1_e,
		[CPU_OP_RES_1_H]	= &&res_1_h,
		[CPU_OP_RES_1_L]	= &&res_1_l,
		[CPU_OP_RES_1_MEM_HL]	= &&res_1_mem_hl,
		[CPU_OP_RES_1_A]	= &&res_1_a,
		[CPU_OP_RES_2_B]	= &&res_2_b,
		[CPU_OP_RES_2_C]	= &&res_2_c,
		[CPU_OP_RES_2_D]	= &&res_2_d,
		[CPU_OP_RES_2_E]	= &&res_2_e,
		[CPU_OP_RES_2_H]	= &&res_2_h,
		[CPU_OP_RES_2_L]	= &&res_2_l,
		[CPU_OP_RES_2_MEM_HL]	= &&res_2_mem_hl,
		[CPU_OP_RES_2_A]	= &&res_2_a,
		[CPU_OP_RES_3_B]	= &&res_3_b,
		[CPU_OP_RES_3_C]	= &&res_3_c,
		[CPU_OP_RES_3_D]	= &&res_3_d,
		[CPU_OP_RES_3_E]	= &&res_3_e,
		[CPU_OP_RES_3_H]	= &&res_3_h,
		[CPU_OP_RES_3_L]	= &&res_3_l,
		[CPU_OP_RES_3_MEM_HL]	= &&res_3_mem_hl,
		[CPU_OP_RES_3_A]	= &&res_3_a,
		[CPU_OP_RES_4_B]	= &&res_4_b,
		[CPU_OP_RES_4_C]	= &&res_4_c,
		[CPU_OP_RES_4_D]	= &&res_4_d,
		[CPU_OP_RES_4_E]	= &&res_4_e,
		[CPU_OP_RES_4_H]	= &&res_4_h,
		[CPU_OP_RES_4_L]	= &&res_4_l,
		[CPU_OP_RES_4_MEM_HL]	= &&res_4_mem_hl,
		[CPU_OP_RES_4_A]	= &&res_4_a,
		[CPU_OP_RES_5_B]	= &&res_5_b,
		[CPU_OP_RES_5_C]	= &&res_5_c,
		[CPU_OP_RES_5_D]	= &&res_5_d,
		[CPU_OP_RES_5_E]	= &&res_5_e,
		[CPU_OP_RES_5_H]	= &&res_5_h,
		[CPU_OP_RES_5_L]	= &&res_5_l,
		[CPU_OP_RES_5_MEM_HL]	= &&res_5_mem_hl,
		[CPU_OP_RES_5_A]	= &&res_5_a,
		[CPU_OP_RES_6_B]	= &&res_6_b,
		[CPU_OP_RES_6_C]	= &&res_6_c,
		[CPU_OP_RES_6_D]	= &&res_6_d,
		[CPU_OP_RES_6_E]	= &&res_6_e,
		[CPU_OP_RES_6_H]	= &&res_6_h,
		[CPU_OP_RES_6_L]	= &&res_6_l,
		[CPU_OP_RES_6_MEM_HL]	= &&res_6_mem_hl,
		[CPU_OP_RES_6_A]	= &&res_6_a,
		[CPU_OP_RES_7_B]	= &&res_7_b,
		[CPU_OP_RES_7_C]	= &&res_7_c,
		[CPU_OP_RES_7_D]	= &&res_7_d,
		[CPU_OP_RES_7_E]	= &&res_7_e,
		[CPU_OP_RES_7_H]	= &&res_7_h,
		[CPU_OP_RES_7_L]	= &&res_7_l,
		[CPU_OP_RES_7_MEM_HL]	= &&res_7_mem_hl,
		[CPU_OP_RES_7_A]	= &&res_7_a,
		[CPU_OP_SET_0_B]	= &&set_0_b,
		[CPU_OP_SET_0_C]	= &&set_0_c,
		[CPU_OP_SET_0_D]	= &&set_0_d,
		[CPU_OP_SET_0_E]	= &&set_0_e,
		[CPU_OP_SET_0_H]	= &&set_0_h,
		[CPU_OP_SET_0_L]	= &&set_0_l,
		[CPU_OP_SET_0_MEM_HL]	= &&set_0_mem_hl,
		[CPU_OP_SET_0_A]	= &&set_0_a,
		[CPU_OP_SET_1_B]	= &&set_1_b,
		[CPU_OP_SET_1_C]	= &&set_1_c,
		[CPU_OP_SET_1_D]	= &&set_1_d,
		[CPU_OP_SET_1_E]	= &&set_1_e,
		[CPU_OP_SET_1_H]	= &&set_1_h,
		[CPU_OP_SET_1_L]	= &&set_1_l,
		[CPU_OP_SET_1_MEM_HL]	= &&set_1_mem_hl,
		[CPU_OP_SET_1_A]	= &&set_1_a,
		[CPU_OP_SET_2_B]	= &&set_2_b,
		[CPU_OP_SET_2_C]	= &&set_2_c,
		[CPU_OP_SET_2_D]	= &&set_2_d,
		[CPU_OP_SET_2_E]	= &&set_2_e,
		[CPU_OP_SET_2_H]	= &&set_2_h,
		[CPU_OP_SET_2_L]	= &&set_2_l,
		[CPU_OP_SET_2_MEM_HL]	= &&set_2_mem_hl,
		[CPU_OP_SET_2_A]	= &&set_2_a,
		[CPU_OP_SET_3_B]	= &&set_3_b,
		[CPU_OP_SET_3_C]	= &&set_3_c,
		[CPU_OP_SET_3_D]	= &&set_3_d,
		[CPU_OP_SET_3_E]	= &&set_3_e,
		[CPU_OP_SET_3_H]	= &&set_3_h,
		[CPU_OP_SET_3_L]	= &&set_3_l,
		[CPU_OP_SET_3_MEM_HL]	= &&set_3_mem_hl,
		[CPU_OP_SET_3_A]	= &&set_3_a,
		[CPU_OP_SET_4_B]	= &&set_4_b,
		[CPU_OP_SET_4_C]	= &&set_4_c,
		[CPU_OP_SET_4_D]	= &&set_4_d,
		[CPU_OP_SET_4_E]	= &&set_4_e,
		[CPU_OP_SET_4_H]	= &&set_4_h,
		[CPU_OP_SET_4_L]	= &&set_4_l,
		[CPU_OP_SET_4_MEM_HL]	= &&set_4_mem_hl,
		[CPU_OP_SET_4_A]	= &&set_4_a,
		[CPU_OP_SET_5_B]	= &&set_5_b,
		[CPU_OP_SET_5_C]	= &&set_5_c,
		[CPU_OP_SET_5_D]	= &&set_5_d,
		[CPU_OP_SET_5_E]	= &&set_5_e,
		[CPU_OP_SET_5_H]	= &&set_5_h,
		[CPU_OP_SET_5_L]	= &&set_5_l,
		[CPU_OP_SET_5_MEM_HL]	= &&set_5_mem_hl,
		[CPU_OP_SET_5_A]	= &&set_5_a,
		[CPU_OP_SET_6_B]	= &&set_6_b,
		[CPU_OP_SET_6_C]	= &&set_6_c,
		[CPU_OP_SET_6_D]	= &&set_6_d,
		[CPU_OP_SET_6_E]	= &&set_6_e,
		[CPU_OP_SET_6_H]	= &&set_6_h,
		[CPU_OP_SET_6_L]	= &&set_6_l,
		[CPU_OP_SET_6_MEM_HL]	= &&set_6_mem_hl,
		[CPU_OP_SET_6_A]	= &&set_6_a,
		[CPU_OP_SET_7_B]	= &&set_7_b,
		[CPU_OP_SET_7_C]	= &&set_7_c,
		[CPU_OP_SET_7_D]	= &&set_7_d,
		[CPU_OP_SET_7_E]	= &&set_7_e,
		[CPU_OP_SET_7_H]	= &&set_7_h,
		[CPU_OP_SET_7_L]	= &&set_7_l,
		[CPU_OP_SET_7_MEM_HL]	= &&set_7_mem_hl,
		[CPU_OP_SET_7_A]	= &&set_7_a

		// clang-format on
	};

	uint8_t instr, u8;
	uint16_t u16;

nop:
	DISPATCH();

ld_bc_u16:
	ctx->cpu.reg.bc = read_u16(ctx);
	DISPATCH();

ld_mem_bc_a:
	agoge_core_bus_write(ctx, ctx->cpu.reg.bc, ctx->cpu.reg.a);
	DISPATCH();

inc_bc:
	ctx->cpu.reg.bc++;
	DISPATCH();

inc_b:
	ctx->cpu.reg.b = alu_inc(ctx, ctx->cpu.reg.b);
	DISPATCH();

dec_b:
	ctx->cpu.reg.b = alu_dec(ctx, ctx->cpu.reg.b);
	DISPATCH();

ld_b_u8:
	ctx->cpu.reg.b = read_u8(ctx);
	DISPATCH();

rlca:
	ctx->cpu.reg.a = alu_rlc_op(ctx, ctx->cpu.reg.a);
	ctx->cpu.reg.f &= ~CPU_FLAG_ZERO;

	DISPATCH();

ld_mem_u16_sp:
	u16 = read_u16(ctx);

	agoge_core_bus_write(ctx, u16 + 0, ctx->cpu.reg.sp & UINT8_MAX);
	agoge_core_bus_write(ctx, u16 + 1, ctx->cpu.reg.sp >> 8);

	DISPATCH();

add_hl_bc:
	alu_add_hl(ctx, ctx->cpu.reg.bc);
	DISPATCH();

ld_a_mem_bc:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, ctx->cpu.reg.bc);
	DISPATCH();

dec_bc:
	ctx->cpu.reg.bc--;
	DISPATCH();

inc_c:
	ctx->cpu.reg.c = alu_inc(ctx, ctx->cpu.reg.c);
	DISPATCH();

dec_c:
	ctx->cpu.reg.c = alu_dec(ctx, ctx->cpu.reg.c);
	DISPATCH();

ld_c_u8:
	ctx->cpu.reg.c = read_u8(ctx);
	DISPATCH();

rrca:
	ctx->cpu.reg.a = alu_rrc_op(ctx, ctx->cpu.reg.a);
	ctx->cpu.reg.f &= ~CPU_FLAG_ZERO;

	DISPATCH();

ld_de_u16:
	ctx->cpu.reg.de = read_u16(ctx);
	DISPATCH();

ld_mem_de_a:
	agoge_core_bus_write(ctx, ctx->cpu.reg.de, ctx->cpu.reg.a);
	DISPATCH();

inc_de:
	ctx->cpu.reg.de++;
	DISPATCH();

inc_d:
	ctx->cpu.reg.d = alu_inc(ctx, ctx->cpu.reg.d);
	DISPATCH();

dec_d:
	ctx->cpu.reg.d = alu_dec(ctx, ctx->cpu.reg.d);
	DISPATCH();

ld_d_u8:
	ctx->cpu.reg.d = read_u8(ctx);
	DISPATCH();

rla:
	ctx->cpu.reg.a = alu_rl_op(ctx, ctx->cpu.reg.a);
	ctx->cpu.reg.f &= ~CPU_FLAG_ZERO;

	DISPATCH();

jr_s8:
	jr_if(ctx, true);
	DISPATCH();

add_hl_de:
	alu_add_hl(ctx, ctx->cpu.reg.de);
	DISPATCH();

ld_a_mem_de:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, ctx->cpu.reg.de);
	DISPATCH();

dec_de:
	ctx->cpu.reg.de--;
	DISPATCH();

inc_e:
	ctx->cpu.reg.e = alu_inc(ctx, ctx->cpu.reg.e);
	DISPATCH();

dec_e:
	ctx->cpu.reg.e = alu_dec(ctx, ctx->cpu.reg.e);
	DISPATCH();

ld_e_u8:
	ctx->cpu.reg.e = read_u8(ctx);
	DISPATCH();

rra:
	ctx->cpu.reg.a = alu_rr(ctx, ctx->cpu.reg.a);
	ctx->cpu.reg.f &= ~CPU_FLAG_ZERO;

	DISPATCH();

jr_nz_s8:
	jr_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_ZERO));
	DISPATCH();

ld_hl_u16:
	ctx->cpu.reg.hl = read_u16(ctx);
	DISPATCH();

ldi_mem_hl_a:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl++, ctx->cpu.reg.a);
	DISPATCH();

inc_hl:
	ctx->cpu.reg.hl++;
	DISPATCH();

inc_h:
	ctx->cpu.reg.h = alu_inc(ctx, ctx->cpu.reg.h);
	DISPATCH();

dec_h:
	ctx->cpu.reg.h = alu_dec(ctx, ctx->cpu.reg.h);
	DISPATCH();

ld_h_u8:
	ctx->cpu.reg.h = read_u8(ctx);
	DISPATCH();

daa:
	op_daa(ctx);
	DISPATCH();

jr_z_s8:
	jr_if(ctx, ctx->cpu.reg.f & CPU_FLAG_ZERO);
	DISPATCH();

add_hl_hl:
	alu_add_hl(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ldi_a_mem_hl:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, ctx->cpu.reg.hl++);
	DISPATCH();

dec_hl:
	ctx->cpu.reg.hl--;
	DISPATCH();

inc_l:
	ctx->cpu.reg.l = alu_inc(ctx, ctx->cpu.reg.l);
	DISPATCH();

dec_l:
	ctx->cpu.reg.l = alu_dec(ctx, ctx->cpu.reg.l);
	DISPATCH();

ld_l_u8:
	ctx->cpu.reg.l = read_u8(ctx);
	DISPATCH();

cpl:
	ctx->cpu.reg.a = ~ctx->cpu.reg.a;
	ctx->cpu.reg.f |= (CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);

	DISPATCH();

jr_nc_s8:
	jr_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_CARRY));
	DISPATCH();

ld_sp_u16:
	ctx->cpu.reg.sp = read_u16(ctx);
	DISPATCH();

ldd_mem_hl_a:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl--, ctx->cpu.reg.a);
	DISPATCH();

inc_sp:
	ctx->cpu.reg.sp++;
	DISPATCH();

inc_mem_hl:
	alu_inc_mem_hl(ctx);
	DISPATCH();

dec_mem_hl:
	u8 = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	u8 = alu_dec(ctx, u8);
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, u8);

	DISPATCH();

ld_mem_hl_u8:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, read_u8(ctx));
	DISPATCH();

scf:
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	ctx->cpu.reg.f |= CPU_FLAG_CARRY;

	DISPATCH();

jr_c_s8:
	jr_if(ctx, ctx->cpu.reg.f & CPU_FLAG_CARRY);
	DISPATCH();

add_hl_sp:
	alu_add_hl(ctx, ctx->cpu.reg.sp);
	DISPATCH();

ldd_a_mem_hl:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, ctx->cpu.reg.hl--);
	DISPATCH();

dec_sp:
	ctx->cpu.reg.sp--;
	DISPATCH();

inc_a:
	ctx->cpu.reg.a = alu_inc(ctx, ctx->cpu.reg.a);
	DISPATCH();

dec_a:
	ctx->cpu.reg.a = alu_dec(ctx, ctx->cpu.reg.a);
	DISPATCH();

ld_a_u8:
	ctx->cpu.reg.a = read_u8(ctx);
	DISPATCH();

ccf:
	ctx->cpu.reg.f &= ~(CPU_FLAG_SUBTRACT | CPU_FLAG_HALF_CARRY);
	ctx->cpu.reg.f ^= CPU_FLAG_CARRY;

	DISPATCH();

ld_b_b:
	ctx->cpu.reg.b = ctx->cpu.reg.b;
	DISPATCH();

ld_b_c:
	ctx->cpu.reg.b = ctx->cpu.reg.c;
	DISPATCH();

ld_b_d:
	ctx->cpu.reg.b = ctx->cpu.reg.d;
	DISPATCH();

ld_b_e:
	ctx->cpu.reg.b = ctx->cpu.reg.e;
	DISPATCH();

ld_b_h:
	ctx->cpu.reg.b = ctx->cpu.reg.h;
	DISPATCH();

ld_b_l:
	ctx->cpu.reg.b = ctx->cpu.reg.l;
	DISPATCH();

ld_b_mem_hl:
	ctx->cpu.reg.b = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_b_a:
	ctx->cpu.reg.b = ctx->cpu.reg.a;
	DISPATCH();

ld_c_b:
	ctx->cpu.reg.c = ctx->cpu.reg.b;
	DISPATCH();

ld_c_c:
	ctx->cpu.reg.c = ctx->cpu.reg.c;
	DISPATCH();

ld_c_d:
	ctx->cpu.reg.c = ctx->cpu.reg.d;
	DISPATCH();

ld_c_e:
	ctx->cpu.reg.c = ctx->cpu.reg.e;
	DISPATCH();

ld_c_h:
	ctx->cpu.reg.c = ctx->cpu.reg.h;
	DISPATCH();

ld_c_l:
	ctx->cpu.reg.c = ctx->cpu.reg.l;
	DISPATCH();

ld_c_mem_hl:
	ctx->cpu.reg.c = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_c_a:
	ctx->cpu.reg.c = ctx->cpu.reg.a;
	DISPATCH();

ld_d_b:
	ctx->cpu.reg.d = ctx->cpu.reg.b;
	DISPATCH();

ld_d_c:
	ctx->cpu.reg.d = ctx->cpu.reg.c;
	DISPATCH();

ld_d_d:
	ctx->cpu.reg.d = ctx->cpu.reg.d;
	DISPATCH();

ld_d_e:
	ctx->cpu.reg.d = ctx->cpu.reg.e;
	DISPATCH();

ld_d_h:
	ctx->cpu.reg.d = ctx->cpu.reg.h;
	DISPATCH();

ld_d_l:
	ctx->cpu.reg.d = ctx->cpu.reg.l;
	DISPATCH();

ld_d_mem_hl:
	ctx->cpu.reg.d = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_d_a:
	ctx->cpu.reg.d = ctx->cpu.reg.a;
	DISPATCH();

ld_e_b:
	ctx->cpu.reg.e = ctx->cpu.reg.b;
	DISPATCH();

ld_e_c:
	ctx->cpu.reg.e = ctx->cpu.reg.c;
	DISPATCH();

ld_e_d:
	ctx->cpu.reg.e = ctx->cpu.reg.d;
	DISPATCH();

ld_e_e:
	ctx->cpu.reg.e = ctx->cpu.reg.e;
	DISPATCH();

ld_e_h:
	ctx->cpu.reg.e = ctx->cpu.reg.h;
	DISPATCH();

ld_e_l:
	ctx->cpu.reg.e = ctx->cpu.reg.l;
	DISPATCH();

ld_e_mem_hl:
	ctx->cpu.reg.e = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_e_a:
	ctx->cpu.reg.e = ctx->cpu.reg.a;
	DISPATCH();

ld_h_b:
	ctx->cpu.reg.h = ctx->cpu.reg.b;
	DISPATCH();

ld_h_c:
	ctx->cpu.reg.h = ctx->cpu.reg.c;
	DISPATCH();

ld_h_d:
	ctx->cpu.reg.h = ctx->cpu.reg.d;
	DISPATCH();

ld_h_e:
	ctx->cpu.reg.h = ctx->cpu.reg.e;
	DISPATCH();

ld_h_h:
	ctx->cpu.reg.h = ctx->cpu.reg.h;
	DISPATCH();

ld_h_l:
	ctx->cpu.reg.h = ctx->cpu.reg.l;
	DISPATCH();

ld_h_mem_hl:
	ctx->cpu.reg.h = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_h_a:
	ctx->cpu.reg.h = ctx->cpu.reg.a;
	DISPATCH();

ld_l_b:
	ctx->cpu.reg.l = ctx->cpu.reg.b;
	DISPATCH();

ld_l_c:
	ctx->cpu.reg.l = ctx->cpu.reg.c;
	DISPATCH();

ld_l_d:
	ctx->cpu.reg.l = ctx->cpu.reg.d;
	DISPATCH();

ld_l_e:
	ctx->cpu.reg.l = ctx->cpu.reg.e;
	DISPATCH();

ld_l_h:
	ctx->cpu.reg.l = ctx->cpu.reg.h;
	DISPATCH();

ld_l_l:
	ctx->cpu.reg.l = ctx->cpu.reg.l;
	DISPATCH();

ld_l_mem_hl:
	ctx->cpu.reg.l = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_l_a:
	ctx->cpu.reg.l = ctx->cpu.reg.a;
	DISPATCH();

ld_mem_hl_b:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.b);
	DISPATCH();

ld_mem_hl_c:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.c);
	DISPATCH();

ld_mem_hl_d:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.d);
	DISPATCH();

ld_mem_hl_e:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.e);
	DISPATCH();

ld_mem_hl_h:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.h);
	DISPATCH();

ld_mem_hl_l:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.l);
	DISPATCH();

ld_mem_hl_a:
	agoge_core_bus_write(ctx, ctx->cpu.reg.hl, ctx->cpu.reg.a);
	DISPATCH();

ld_a_b:
	ctx->cpu.reg.a = ctx->cpu.reg.b;
	DISPATCH();

ld_a_c:
	ctx->cpu.reg.a = ctx->cpu.reg.c;
	DISPATCH();

ld_a_d:
	ctx->cpu.reg.a = ctx->cpu.reg.d;
	DISPATCH();

ld_a_e:
	ctx->cpu.reg.a = ctx->cpu.reg.e;
	DISPATCH();

ld_a_h:
	ctx->cpu.reg.a = ctx->cpu.reg.h;
	DISPATCH();

ld_a_l:
	ctx->cpu.reg.a = ctx->cpu.reg.l;
	DISPATCH();

ld_a_mem_hl:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, ctx->cpu.reg.hl);
	DISPATCH();

ld_a_a:
	ctx->cpu.reg.a = ctx->cpu.reg.a;
	DISPATCH();

add_a_b:
	alu_add(ctx, ctx->cpu.reg.b);
	DISPATCH();

add_a_c:
	alu_add(ctx, ctx->cpu.reg.c);
	DISPATCH();

add_a_d:
	alu_add(ctx, ctx->cpu.reg.d);
	DISPATCH();

add_a_e:
	alu_add(ctx, ctx->cpu.reg.e);
	DISPATCH();

add_a_h:
	alu_add(ctx, ctx->cpu.reg.h);
	DISPATCH();

add_a_l:
	alu_add(ctx, ctx->cpu.reg.l);
	DISPATCH();

add_a_mem_hl:
	alu_add(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

add_a_a:
	alu_add(ctx, ctx->cpu.reg.a);
	DISPATCH();

adc_a_b:
	alu_adc(ctx, ctx->cpu.reg.b);
	DISPATCH();

adc_a_c:
	alu_adc(ctx, ctx->cpu.reg.c);
	DISPATCH();

adc_a_d:
	alu_adc(ctx, ctx->cpu.reg.d);
	DISPATCH();

adc_a_e:
	alu_adc(ctx, ctx->cpu.reg.e);
	DISPATCH();

adc_a_h:
	alu_adc(ctx, ctx->cpu.reg.h);
	DISPATCH();

adc_a_l:
	alu_adc(ctx, ctx->cpu.reg.l);
	DISPATCH();

adc_a_mem_hl:
	alu_adc(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

adc_a_a:
	alu_adc(ctx, ctx->cpu.reg.a);
	DISPATCH();

sub_a_b:
	alu_sub(ctx, ctx->cpu.reg.b);
	DISPATCH();

sub_a_c:
	alu_sub(ctx, ctx->cpu.reg.c);
	DISPATCH();

sub_a_d:
	alu_sub(ctx, ctx->cpu.reg.d);
	DISPATCH();

sub_a_e:
	alu_sub(ctx, ctx->cpu.reg.e);
	DISPATCH();

sub_a_h:
	alu_sub(ctx, ctx->cpu.reg.h);
	DISPATCH();

sub_a_l:
	alu_sub(ctx, ctx->cpu.reg.l);
	DISPATCH();

sub_a_mem_hl:
	alu_sub(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

sub_a_a:
	alu_sub(ctx, ctx->cpu.reg.a);
	DISPATCH();

sbc_a_b:
	alu_sbc(ctx, ctx->cpu.reg.b);
	DISPATCH();

sbc_a_c:
	alu_sbc(ctx, ctx->cpu.reg.c);
	DISPATCH();

sbc_a_d:
	alu_sbc(ctx, ctx->cpu.reg.d);
	DISPATCH();

sbc_a_e:
	alu_sbc(ctx, ctx->cpu.reg.e);
	DISPATCH();

sbc_a_h:
	alu_sbc(ctx, ctx->cpu.reg.h);
	DISPATCH();

sbc_a_l:
	alu_sbc(ctx, ctx->cpu.reg.l);
	DISPATCH();

sbc_a_mem_hl:
	alu_sbc(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

sbc_a_a:
	alu_sbc(ctx, ctx->cpu.reg.a);
	DISPATCH();

and_a_b:
	alu_and(ctx, ctx->cpu.reg.b);
	DISPATCH();

and_a_c:
	alu_and(ctx, ctx->cpu.reg.c);
	DISPATCH();

and_a_d:
	alu_and(ctx, ctx->cpu.reg.d);
	DISPATCH();

and_a_e:
	alu_and(ctx, ctx->cpu.reg.e);
	DISPATCH();

and_a_h:
	alu_and(ctx, ctx->cpu.reg.h);
	DISPATCH();

and_a_l:
	alu_and(ctx, ctx->cpu.reg.l);
	DISPATCH();

and_a_mem_hl:
	alu_and(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

and_a_a:
	alu_and(ctx, ctx->cpu.reg.a);
	DISPATCH();

xor_a_b:
	alu_xor(ctx, ctx->cpu.reg.b);
	DISPATCH();

xor_a_c:
	alu_xor(ctx, ctx->cpu.reg.c);
	DISPATCH();

xor_a_d:
	alu_xor(ctx, ctx->cpu.reg.d);
	DISPATCH();

xor_a_e:
	alu_xor(ctx, ctx->cpu.reg.e);
	DISPATCH();

xor_a_h:
	alu_xor(ctx, ctx->cpu.reg.h);
	DISPATCH();

xor_a_l:
	alu_xor(ctx, ctx->cpu.reg.l);
	DISPATCH();

xor_a_mem_hl:
	alu_xor(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

xor_a_a:
	alu_xor(ctx, ctx->cpu.reg.a);
	DISPATCH();

or_a_b:
	alu_or(ctx, ctx->cpu.reg.b);
	DISPATCH();

or_a_c:
	alu_or(ctx, ctx->cpu.reg.c);
	DISPATCH();

or_a_d:
	alu_or(ctx, ctx->cpu.reg.d);
	DISPATCH();

or_a_e:
	alu_or(ctx, ctx->cpu.reg.e);
	DISPATCH();

or_a_h:
	alu_or(ctx, ctx->cpu.reg.h);
	DISPATCH();

or_a_l:
	alu_or(ctx, ctx->cpu.reg.l);
	DISPATCH();

or_a_mem_hl:
	alu_or(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

or_a_a:
	alu_or(ctx, ctx->cpu.reg.a);
	DISPATCH();

cp_a_b:
	alu_cp(ctx, ctx->cpu.reg.b);
	DISPATCH();

cp_a_c:
	alu_cp(ctx, ctx->cpu.reg.c);
	DISPATCH();

cp_a_d:
	alu_cp(ctx, ctx->cpu.reg.d);
	DISPATCH();

cp_a_e:
	alu_cp(ctx, ctx->cpu.reg.e);
	DISPATCH();

cp_a_h:
	alu_cp(ctx, ctx->cpu.reg.h);
	DISPATCH();

cp_a_l:
	alu_cp(ctx, ctx->cpu.reg.l);
	DISPATCH();

cp_a_mem_hl:
	alu_cp(ctx, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

cp_a_a:
	alu_cp(ctx, ctx->cpu.reg.a);
	DISPATCH();

ret_nz:
	ret_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_ZERO));
	DISPATCH();

pop_bc:
	ctx->cpu.reg.bc = stack_pop(ctx);
	DISPATCH();

jp_nz_u16:
	jp_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_ZERO));
	DISPATCH();

jp_u16:
	jp_if(ctx, true);
	DISPATCH();

call_nz_u16:
	call_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_ZERO));
	DISPATCH();

push_bc:
	stack_push(ctx, ctx->cpu.reg.bc);
	DISPATCH();

add_a_u8:
	alu_add(ctx, read_u8(ctx));
	DISPATCH();

rst_00:
	rst(ctx, 0x0000);
	DISPATCH();

ret_z:
	ret_if(ctx, ctx->cpu.reg.f & CPU_FLAG_ZERO);
	DISPATCH();

ret:
	ret_if(ctx, true);
	DISPATCH();

jp_z_u16:
	jp_if(ctx, ctx->cpu.reg.f & CPU_FLAG_ZERO);
	DISPATCH();

prefix_cb:
	instr = read_u8(ctx);
	goto *cb_tbl[instr];

rlc_b:
	ctx->cpu.reg.b = alu_rlc(ctx, ctx->cpu.reg.b);
	DISPATCH();

rlc_c:
	ctx->cpu.reg.c = alu_rlc(ctx, ctx->cpu.reg.c);
	DISPATCH();

rlc_d:
	ctx->cpu.reg.d = alu_rlc(ctx, ctx->cpu.reg.d);
	DISPATCH();

rlc_e:
	ctx->cpu.reg.e = alu_rlc(ctx, ctx->cpu.reg.e);
	DISPATCH();

rlc_h:
	ctx->cpu.reg.h = alu_rlc(ctx, ctx->cpu.reg.h);
	DISPATCH();

rlc_l:
	ctx->cpu.reg.l = alu_rlc(ctx, ctx->cpu.reg.l);
	DISPATCH();

rlc_mem_hl:
	alu_rlc_mem_hl(ctx);
	DISPATCH();

rlc_a:
	ctx->cpu.reg.a = alu_rlc(ctx, ctx->cpu.reg.a);
	DISPATCH();

rrc_b:
	ctx->cpu.reg.b = alu_rrc(ctx, ctx->cpu.reg.b);
	DISPATCH();

rrc_c:
	ctx->cpu.reg.c = alu_rrc(ctx, ctx->cpu.reg.c);
	DISPATCH();

rrc_d:
	ctx->cpu.reg.d = alu_rrc(ctx, ctx->cpu.reg.d);
	DISPATCH();

rrc_e:
	ctx->cpu.reg.e = alu_rrc(ctx, ctx->cpu.reg.e);
	DISPATCH();

rrc_h:
	ctx->cpu.reg.h = alu_rrc(ctx, ctx->cpu.reg.h);
	DISPATCH();

rrc_l:
	ctx->cpu.reg.l = alu_rrc(ctx, ctx->cpu.reg.l);
	DISPATCH();

rrc_mem_hl:
	alu_rrc_mem_hl(ctx);
	DISPATCH();

rrc_a:
	ctx->cpu.reg.a = alu_rrc(ctx, ctx->cpu.reg.a);
	DISPATCH();

rl_b:
	ctx->cpu.reg.b = alu_rl(ctx, ctx->cpu.reg.b);
	DISPATCH();

rl_c:
	ctx->cpu.reg.c = alu_rl(ctx, ctx->cpu.reg.c);
	DISPATCH();

rl_d:
	ctx->cpu.reg.d = alu_rl(ctx, ctx->cpu.reg.d);
	DISPATCH();

rl_e:
	ctx->cpu.reg.e = alu_rl(ctx, ctx->cpu.reg.e);
	DISPATCH();

rl_h:
	ctx->cpu.reg.h = alu_rl(ctx, ctx->cpu.reg.h);
	DISPATCH();

rl_l:
	ctx->cpu.reg.l = alu_rl(ctx, ctx->cpu.reg.l);
	DISPATCH();

rl_mem_hl:
	alu_rl_mem_hl(ctx);
	DISPATCH();

rl_a:
	ctx->cpu.reg.a = alu_rl(ctx, ctx->cpu.reg.a);
	DISPATCH();

rr_b:
	ctx->cpu.reg.b = alu_rr(ctx, ctx->cpu.reg.b);
	DISPATCH();

rr_c:
	ctx->cpu.reg.c = alu_rr(ctx, ctx->cpu.reg.c);
	DISPATCH();

rr_d:
	ctx->cpu.reg.d = alu_rr(ctx, ctx->cpu.reg.d);
	DISPATCH();

rr_e:
	ctx->cpu.reg.e = alu_rr(ctx, ctx->cpu.reg.e);
	DISPATCH();

rr_h:
	ctx->cpu.reg.h = alu_rr(ctx, ctx->cpu.reg.h);
	DISPATCH();

rr_l:
	ctx->cpu.reg.l = alu_rr(ctx, ctx->cpu.reg.l);
	DISPATCH();

rr_mem_hl:
	alu_rr_mem_hl(ctx);
	DISPATCH();

rr_a:
	ctx->cpu.reg.a = alu_rr(ctx, ctx->cpu.reg.a);
	DISPATCH();

sla_b:
	ctx->cpu.reg.b = alu_sla(ctx, ctx->cpu.reg.b);
	DISPATCH();

sla_c:
	ctx->cpu.reg.c = alu_sla(ctx, ctx->cpu.reg.c);
	DISPATCH();

sla_d:
	ctx->cpu.reg.d = alu_sla(ctx, ctx->cpu.reg.d);
	DISPATCH();

sla_e:
	ctx->cpu.reg.e = alu_sla(ctx, ctx->cpu.reg.e);
	DISPATCH();

sla_h:
	ctx->cpu.reg.h = alu_sla(ctx, ctx->cpu.reg.h);
	DISPATCH();

sla_l:
	ctx->cpu.reg.l = alu_sla(ctx, ctx->cpu.reg.l);
	DISPATCH();

sla_mem_hl:
	alu_sla_mem_hl(ctx);
	DISPATCH();

sla_a:
	ctx->cpu.reg.a = alu_sla(ctx, ctx->cpu.reg.a);
	DISPATCH();

sra_b:
	ctx->cpu.reg.b = alu_sra(ctx, ctx->cpu.reg.b);
	DISPATCH();

sra_c:
	ctx->cpu.reg.c = alu_sra(ctx, ctx->cpu.reg.c);
	DISPATCH();

sra_d:
	ctx->cpu.reg.d = alu_sra(ctx, ctx->cpu.reg.d);
	DISPATCH();

sra_e:
	ctx->cpu.reg.e = alu_sra(ctx, ctx->cpu.reg.e);
	DISPATCH();

sra_h:
	ctx->cpu.reg.h = alu_sra(ctx, ctx->cpu.reg.h);
	DISPATCH();

sra_l:
	ctx->cpu.reg.l = alu_sra(ctx, ctx->cpu.reg.l);
	DISPATCH();

sra_mem_hl:
	alu_sra_mem_hl(ctx);
	DISPATCH();

sra_a:
	ctx->cpu.reg.a = alu_sra(ctx, ctx->cpu.reg.a);
	DISPATCH();

swap_b:
	ctx->cpu.reg.b = alu_swap(ctx, ctx->cpu.reg.b);
	DISPATCH();

swap_c:
	ctx->cpu.reg.c = alu_swap(ctx, ctx->cpu.reg.c);
	DISPATCH();

swap_d:
	ctx->cpu.reg.d = alu_swap(ctx, ctx->cpu.reg.d);
	DISPATCH();

swap_e:
	ctx->cpu.reg.e = alu_swap(ctx, ctx->cpu.reg.e);
	DISPATCH();

swap_h:
	ctx->cpu.reg.h = alu_swap(ctx, ctx->cpu.reg.h);
	DISPATCH();

swap_l:
	ctx->cpu.reg.l = alu_swap(ctx, ctx->cpu.reg.l);
	DISPATCH();

swap_mem_hl:
	alu_swap_mem_hl(ctx);
	DISPATCH();

swap_a:
	ctx->cpu.reg.a = alu_swap(ctx, ctx->cpu.reg.a);
	DISPATCH();

srl_b:
	ctx->cpu.reg.b = alu_srl(ctx, ctx->cpu.reg.b);
	DISPATCH();

srl_c:
	ctx->cpu.reg.c = alu_srl(ctx, ctx->cpu.reg.c);
	DISPATCH();

srl_d:
	ctx->cpu.reg.d = alu_srl(ctx, ctx->cpu.reg.d);
	DISPATCH();

srl_e:
	ctx->cpu.reg.e = alu_srl(ctx, ctx->cpu.reg.e);
	DISPATCH();

srl_h:
	ctx->cpu.reg.h = alu_srl(ctx, ctx->cpu.reg.h);
	DISPATCH();

srl_l:
	ctx->cpu.reg.l = alu_srl(ctx, ctx->cpu.reg.l);
	DISPATCH();

srl_mem_hl:
	alu_srl_mem_hl(ctx);
	DISPATCH();

srl_a:
	ctx->cpu.reg.a = alu_srl(ctx, ctx->cpu.reg.a);
	DISPATCH();

bit_0_b:
	alu_bit(ctx, 0, ctx->cpu.reg.b);
	DISPATCH();

bit_0_c:
	alu_bit(ctx, 0, ctx->cpu.reg.c);
	DISPATCH();

bit_0_d:
	alu_bit(ctx, 0, ctx->cpu.reg.d);
	DISPATCH();

bit_0_e:
	alu_bit(ctx, 0, ctx->cpu.reg.e);
	DISPATCH();

bit_0_h:
	alu_bit(ctx, 0, ctx->cpu.reg.h);
	DISPATCH();

bit_0_l:
	alu_bit(ctx, 0, ctx->cpu.reg.l);
	DISPATCH();

bit_0_mem_hl:
	alu_bit(ctx, 0, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_0_a:
	alu_bit(ctx, 0, ctx->cpu.reg.a);
	DISPATCH();

bit_1_b:
	alu_bit(ctx, 1, ctx->cpu.reg.b);
	DISPATCH();

bit_1_c:
	alu_bit(ctx, 1, ctx->cpu.reg.c);
	DISPATCH();

bit_1_d:
	alu_bit(ctx, 1, ctx->cpu.reg.d);
	DISPATCH();

bit_1_e:
	alu_bit(ctx, 1, ctx->cpu.reg.e);
	DISPATCH();

bit_1_h:
	alu_bit(ctx, 1, ctx->cpu.reg.h);
	DISPATCH();

bit_1_l:
	alu_bit(ctx, 1, ctx->cpu.reg.l);
	DISPATCH();

bit_1_mem_hl:
	alu_bit(ctx, 1, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_1_a:
	alu_bit(ctx, 1, ctx->cpu.reg.a);
	DISPATCH();

bit_2_b:
	alu_bit(ctx, 2, ctx->cpu.reg.b);
	DISPATCH();

bit_2_c:
	alu_bit(ctx, 2, ctx->cpu.reg.c);
	DISPATCH();

bit_2_d:
	alu_bit(ctx, 2, ctx->cpu.reg.d);
	DISPATCH();

bit_2_e:
	alu_bit(ctx, 2, ctx->cpu.reg.e);
	DISPATCH();

bit_2_h:
	alu_bit(ctx, 2, ctx->cpu.reg.h);
	DISPATCH();

bit_2_l:
	alu_bit(ctx, 2, ctx->cpu.reg.l);
	DISPATCH();

bit_2_mem_hl:
	alu_bit(ctx, 2, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_2_a:
	alu_bit(ctx, 2, ctx->cpu.reg.a);
	DISPATCH();

bit_3_b:
	alu_bit(ctx, 3, ctx->cpu.reg.b);
	DISPATCH();

bit_3_c:
	alu_bit(ctx, 3, ctx->cpu.reg.c);
	DISPATCH();

bit_3_d:
	alu_bit(ctx, 3, ctx->cpu.reg.d);
	DISPATCH();

bit_3_e:
	alu_bit(ctx, 3, ctx->cpu.reg.e);
	DISPATCH();

bit_3_h:
	alu_bit(ctx, 3, ctx->cpu.reg.h);
	DISPATCH();

bit_3_l:
	alu_bit(ctx, 3, ctx->cpu.reg.l);
	DISPATCH();

bit_3_mem_hl:
	alu_bit(ctx, 3, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_3_a:
	alu_bit(ctx, 3, ctx->cpu.reg.a);
	DISPATCH();

bit_4_b:
	alu_bit(ctx, 4, ctx->cpu.reg.b);
	DISPATCH();

bit_4_c:
	alu_bit(ctx, 4, ctx->cpu.reg.c);
	DISPATCH();

bit_4_d:
	alu_bit(ctx, 4, ctx->cpu.reg.d);
	DISPATCH();

bit_4_e:
	alu_bit(ctx, 4, ctx->cpu.reg.e);
	DISPATCH();

bit_4_h:
	alu_bit(ctx, 4, ctx->cpu.reg.h);
	DISPATCH();

bit_4_l:
	alu_bit(ctx, 4, ctx->cpu.reg.l);
	DISPATCH();

bit_4_mem_hl:
	alu_bit(ctx, 4, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_4_a:
	alu_bit(ctx, 4, ctx->cpu.reg.a);
	DISPATCH();

bit_5_b:
	alu_bit(ctx, 5, ctx->cpu.reg.b);
	DISPATCH();

bit_5_c:
	alu_bit(ctx, 5, ctx->cpu.reg.c);
	DISPATCH();

bit_5_d:
	alu_bit(ctx, 5, ctx->cpu.reg.d);
	DISPATCH();

bit_5_e:
	alu_bit(ctx, 5, ctx->cpu.reg.e);
	DISPATCH();

bit_5_h:
	alu_bit(ctx, 5, ctx->cpu.reg.h);
	DISPATCH();

bit_5_l:
	alu_bit(ctx, 5, ctx->cpu.reg.l);
	DISPATCH();

bit_5_mem_hl:
	alu_bit(ctx, 5, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_5_a:
	alu_bit(ctx, 5, ctx->cpu.reg.a);
	DISPATCH();

bit_6_b:
	alu_bit(ctx, 6, ctx->cpu.reg.b);
	DISPATCH();

bit_6_c:
	alu_bit(ctx, 6, ctx->cpu.reg.c);
	DISPATCH();

bit_6_d:
	alu_bit(ctx, 6, ctx->cpu.reg.d);
	DISPATCH();

bit_6_e:
	alu_bit(ctx, 6, ctx->cpu.reg.e);
	DISPATCH();

bit_6_h:
	alu_bit(ctx, 6, ctx->cpu.reg.h);
	DISPATCH();

bit_6_l:
	alu_bit(ctx, 6, ctx->cpu.reg.l);
	DISPATCH();

bit_6_mem_hl:
	alu_bit(ctx, 6, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_6_a:
	alu_bit(ctx, 6, ctx->cpu.reg.a);
	DISPATCH();

bit_7_b:
	alu_bit(ctx, 7, ctx->cpu.reg.b);
	DISPATCH();

bit_7_c:
	alu_bit(ctx, 7, ctx->cpu.reg.c);
	DISPATCH();

bit_7_d:
	alu_bit(ctx, 7, ctx->cpu.reg.d);
	DISPATCH();

bit_7_e:
	alu_bit(ctx, 7, ctx->cpu.reg.e);
	DISPATCH();

bit_7_h:
	alu_bit(ctx, 7, ctx->cpu.reg.h);
	DISPATCH();

bit_7_l:
	alu_bit(ctx, 7, ctx->cpu.reg.l);
	DISPATCH();

bit_7_mem_hl:
	alu_bit(ctx, 7, agoge_core_bus_read(ctx, ctx->cpu.reg.hl));
	DISPATCH();

bit_7_a:
	alu_bit(ctx, 7, ctx->cpu.reg.a);
	DISPATCH();

res_0_b:
	ctx->cpu.reg.b &= ~BIT_0;
	DISPATCH();

res_0_c:
	ctx->cpu.reg.c &= ~BIT_0;
	DISPATCH();

res_0_d:
	ctx->cpu.reg.d &= ~BIT_0;
	DISPATCH();

res_0_e:
	ctx->cpu.reg.e &= ~BIT_0;
	DISPATCH();

res_0_h:
	ctx->cpu.reg.h &= ~BIT_0;
	DISPATCH();

res_0_l:
	ctx->cpu.reg.l &= ~BIT_0;
	DISPATCH();

res_0_mem_hl:
	alu_res_hl(ctx, 0);
	DISPATCH();

res_0_a:
	ctx->cpu.reg.a &= ~BIT_0;
	DISPATCH();

res_1_b:
	ctx->cpu.reg.b &= ~BIT_1;
	DISPATCH();

res_1_c:
	ctx->cpu.reg.c &= ~BIT_1;
	DISPATCH();

res_1_d:
	ctx->cpu.reg.d &= ~BIT_1;
	DISPATCH();

res_1_e:
	ctx->cpu.reg.e &= ~BIT_1;
	DISPATCH();

res_1_h:
	ctx->cpu.reg.h &= ~BIT_1;
	DISPATCH();

res_1_l:
	ctx->cpu.reg.l &= ~BIT_1;
	DISPATCH();

res_1_mem_hl:
	alu_res_hl(ctx, 1);
	DISPATCH();

res_1_a:
	ctx->cpu.reg.a &= ~BIT_1;
	DISPATCH();

res_2_b:
	ctx->cpu.reg.b &= ~BIT_2;
	DISPATCH();

res_2_c:
	ctx->cpu.reg.c &= ~BIT_2;
	DISPATCH();

res_2_d:
	ctx->cpu.reg.d &= ~BIT_2;
	DISPATCH();

res_2_e:
	ctx->cpu.reg.e &= ~BIT_2;
	DISPATCH();

res_2_h:
	ctx->cpu.reg.h &= ~BIT_2;
	DISPATCH();

res_2_l:
	ctx->cpu.reg.l &= ~BIT_2;
	DISPATCH();

res_2_mem_hl:
	alu_res_hl(ctx, 2);
	DISPATCH();

res_2_a:
	ctx->cpu.reg.a &= ~BIT_2;
	DISPATCH();

res_3_b:
	ctx->cpu.reg.b &= ~BIT_3;
	DISPATCH();

res_3_c:
	ctx->cpu.reg.c &= ~BIT_3;
	DISPATCH();

res_3_d:
	ctx->cpu.reg.d &= ~BIT_3;
	DISPATCH();

res_3_e:
	ctx->cpu.reg.e &= ~BIT_3;
	DISPATCH();

res_3_h:
	ctx->cpu.reg.h &= ~BIT_3;
	DISPATCH();

res_3_l:
	ctx->cpu.reg.l &= ~BIT_3;
	DISPATCH();

res_3_mem_hl:
	alu_res_hl(ctx, 3);
	DISPATCH();

res_3_a:
	ctx->cpu.reg.a &= ~BIT_3;
	DISPATCH();

res_4_b:
	ctx->cpu.reg.b &= ~BIT_4;
	DISPATCH();

res_4_c:
	ctx->cpu.reg.c &= ~BIT_4;
	DISPATCH();

res_4_d:
	ctx->cpu.reg.d &= ~BIT_4;
	DISPATCH();

res_4_e:
	ctx->cpu.reg.e &= ~BIT_4;
	DISPATCH();

res_4_h:
	ctx->cpu.reg.h &= ~BIT_4;
	DISPATCH();

res_4_l:
	ctx->cpu.reg.l &= ~BIT_4;
	DISPATCH();

res_4_mem_hl:
	alu_res_hl(ctx, 4);
	DISPATCH();

res_4_a:
	ctx->cpu.reg.a &= ~BIT_4;
	DISPATCH();

res_5_b:
	ctx->cpu.reg.b &= ~BIT_5;
	DISPATCH();

res_5_c:
	ctx->cpu.reg.c &= ~BIT_5;
	DISPATCH();

res_5_d:
	ctx->cpu.reg.d &= ~BIT_5;
	DISPATCH();

res_5_e:
	ctx->cpu.reg.e &= ~BIT_5;
	DISPATCH();

res_5_h:
	ctx->cpu.reg.h &= ~BIT_5;
	DISPATCH();

res_5_l:
	ctx->cpu.reg.l &= ~BIT_5;
	DISPATCH();

res_5_mem_hl:
	alu_res_hl(ctx, 5);
	DISPATCH();

res_5_a:
	ctx->cpu.reg.a &= ~BIT_5;
	DISPATCH();

res_6_b:
	ctx->cpu.reg.b &= ~BIT_6;
	DISPATCH();

res_6_c:
	ctx->cpu.reg.c &= ~BIT_6;
	DISPATCH();

res_6_d:
	ctx->cpu.reg.d &= ~BIT_6;
	DISPATCH();

res_6_e:
	ctx->cpu.reg.e &= ~BIT_6;
	DISPATCH();

res_6_h:
	ctx->cpu.reg.h &= ~BIT_6;
	DISPATCH();

res_6_l:
	ctx->cpu.reg.l &= ~BIT_6;
	DISPATCH();

res_6_mem_hl:
	alu_res_hl(ctx, 6);
	DISPATCH();

res_6_a:
	ctx->cpu.reg.a &= ~BIT_6;
	DISPATCH();

res_7_b:
	ctx->cpu.reg.b &= ~BIT_7;
	DISPATCH();

res_7_c:
	ctx->cpu.reg.c &= ~BIT_7;
	DISPATCH();

res_7_d:
	ctx->cpu.reg.d &= ~BIT_7;
	DISPATCH();

res_7_e:
	ctx->cpu.reg.e &= ~BIT_7;
	DISPATCH();

res_7_h:
	ctx->cpu.reg.h &= ~BIT_7;
	DISPATCH();

res_7_l:
	ctx->cpu.reg.l &= ~BIT_7;
	DISPATCH();

res_7_mem_hl:
	alu_res_hl(ctx, 7);
	DISPATCH();

res_7_a:
	ctx->cpu.reg.a &= ~BIT_7;
	DISPATCH();

set_0_b:
	ctx->cpu.reg.b |= BIT_0;
	DISPATCH();

set_0_c:
	ctx->cpu.reg.c |= BIT_0;
	DISPATCH();

set_0_d:
	ctx->cpu.reg.d |= BIT_0;
	DISPATCH();

set_0_e:
	ctx->cpu.reg.e |= BIT_0;
	DISPATCH();

set_0_h:
	ctx->cpu.reg.h |= BIT_0;
	DISPATCH();

set_0_l:
	ctx->cpu.reg.l |= BIT_0;
	DISPATCH();

set_0_mem_hl:
	alu_set_hl(ctx, 0);
	DISPATCH();

set_0_a:
	ctx->cpu.reg.a |= BIT_0;
	DISPATCH();

set_1_b:
	ctx->cpu.reg.b |= BIT_1;
	DISPATCH();

set_1_c:
	ctx->cpu.reg.c |= BIT_1;
	DISPATCH();

set_1_d:
	ctx->cpu.reg.d |= BIT_1;
	DISPATCH();

set_1_e:
	ctx->cpu.reg.e |= BIT_1;
	DISPATCH();

set_1_h:
	ctx->cpu.reg.h |= BIT_1;
	DISPATCH();

set_1_l:
	ctx->cpu.reg.l |= BIT_1;
	DISPATCH();

set_1_mem_hl:
	alu_set_hl(ctx, 1);
	DISPATCH();

set_1_a:
	ctx->cpu.reg.a |= BIT_1;
	DISPATCH();

set_2_b:
	ctx->cpu.reg.b |= BIT_2;
	DISPATCH();

set_2_c:
	ctx->cpu.reg.c |= BIT_2;
	DISPATCH();

set_2_d:
	ctx->cpu.reg.d |= BIT_2;
	DISPATCH();

set_2_e:
	ctx->cpu.reg.e |= BIT_2;
	DISPATCH();

set_2_h:
	ctx->cpu.reg.h |= BIT_2;
	DISPATCH();

set_2_l:
	ctx->cpu.reg.l |= BIT_2;
	DISPATCH();

set_2_mem_hl:
	alu_set_hl(ctx, 2);
	DISPATCH();

set_2_a:
	ctx->cpu.reg.a |= BIT_2;
	DISPATCH();

set_3_b:
	ctx->cpu.reg.b |= BIT_3;
	DISPATCH();

set_3_c:
	ctx->cpu.reg.c |= BIT_3;
	DISPATCH();

set_3_d:
	ctx->cpu.reg.d |= BIT_3;
	DISPATCH();

set_3_e:
	ctx->cpu.reg.e |= BIT_3;
	DISPATCH();

set_3_h:
	ctx->cpu.reg.h |= BIT_3;
	DISPATCH();

set_3_l:
	ctx->cpu.reg.l |= BIT_3;
	DISPATCH();

set_3_mem_hl:
	alu_set_hl(ctx, 3);
	DISPATCH();

set_3_a:
	ctx->cpu.reg.a |= BIT_3;
	DISPATCH();

set_4_b:
	ctx->cpu.reg.b |= BIT_4;
	DISPATCH();

set_4_c:
	ctx->cpu.reg.c |= BIT_4;
	DISPATCH();

set_4_d:
	ctx->cpu.reg.d |= BIT_4;
	DISPATCH();

set_4_e:
	ctx->cpu.reg.e |= BIT_4;
	DISPATCH();

set_4_h:
	ctx->cpu.reg.h |= BIT_4;
	DISPATCH();

set_4_l:
	ctx->cpu.reg.l |= BIT_4;
	DISPATCH();

set_4_mem_hl:
	alu_set_hl(ctx, 4);
	DISPATCH();

set_4_a:
	ctx->cpu.reg.a |= BIT_4;
	DISPATCH();

set_5_b:
	ctx->cpu.reg.b |= BIT_5;
	DISPATCH();

set_5_c:
	ctx->cpu.reg.c |= BIT_5;
	DISPATCH();

set_5_d:
	ctx->cpu.reg.d |= BIT_5;
	DISPATCH();

set_5_e:
	ctx->cpu.reg.e |= BIT_5;
	DISPATCH();

set_5_h:
	ctx->cpu.reg.h |= BIT_5;
	DISPATCH();

set_5_l:
	ctx->cpu.reg.l |= BIT_5;
	DISPATCH();

set_5_mem_hl:
	alu_set_hl(ctx, 5);
	DISPATCH();

set_5_a:
	ctx->cpu.reg.a |= BIT_5;
	DISPATCH();

set_6_b:
	ctx->cpu.reg.b |= BIT_6;
	DISPATCH();

set_6_c:
	ctx->cpu.reg.c |= BIT_6;
	DISPATCH();

set_6_d:
	ctx->cpu.reg.d |= BIT_6;
	DISPATCH();

set_6_e:
	ctx->cpu.reg.e |= BIT_6;
	DISPATCH();

set_6_h:
	ctx->cpu.reg.h |= BIT_6;
	DISPATCH();

set_6_l:
	ctx->cpu.reg.l |= BIT_6;
	DISPATCH();

set_6_mem_hl:
	alu_set_hl(ctx, 6);
	DISPATCH();

set_6_a:
	ctx->cpu.reg.a |= BIT_6;
	DISPATCH();

set_7_b:
	ctx->cpu.reg.b |= BIT_7;
	DISPATCH();

set_7_c:
	ctx->cpu.reg.c |= BIT_7;
	DISPATCH();

set_7_d:
	ctx->cpu.reg.d |= BIT_7;
	DISPATCH();

set_7_e:
	ctx->cpu.reg.e |= BIT_7;
	DISPATCH();

set_7_h:
	ctx->cpu.reg.h |= BIT_7;
	DISPATCH();

set_7_l:
	ctx->cpu.reg.l |= BIT_7;
	DISPATCH();

set_7_mem_hl:
	alu_set_hl(ctx, 7);
	DISPATCH();

set_7_a:
	ctx->cpu.reg.a |= BIT_7;
	DISPATCH();

call_z_u16:
	call_if(ctx, ctx->cpu.reg.f & CPU_FLAG_ZERO);
	DISPATCH();

call_u16:
	call_if(ctx, true);
	DISPATCH();

adc_a_u8:
	alu_adc(ctx, read_u8(ctx));
	DISPATCH();

rst_08:
	rst(ctx, 0x0008);
	DISPATCH();

ret_nc:
	ret_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_CARRY));
	DISPATCH();

pop_de:
	ctx->cpu.reg.de = stack_pop(ctx);
	DISPATCH();

jp_nc_u16:
	jp_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_CARRY));
	DISPATCH();

call_nc_u16:
	call_if(ctx, !(ctx->cpu.reg.f & CPU_FLAG_CARRY));
	DISPATCH();

push_de:
	stack_push(ctx, ctx->cpu.reg.de);
	DISPATCH();

sub_a_u8:
	alu_sub(ctx, read_u8(ctx));
	DISPATCH();

rst_10:
	rst(ctx, 0x0010);
	DISPATCH();

ret_c:
	ret_if(ctx, ctx->cpu.reg.f & CPU_FLAG_CARRY);
	DISPATCH();

reti:
	// Handle interrupt stuff here
	ret_if(ctx, true);
	DISPATCH();

jp_c_u16:
	jp_if(ctx, ctx->cpu.reg.f & CPU_FLAG_CARRY);
	DISPATCH();

call_c_u16:
	call_if(ctx, ctx->cpu.reg.f & CPU_FLAG_CARRY);
	DISPATCH();

sbc_a_u8:
	alu_sbc(ctx, read_u8(ctx));
	DISPATCH();

rst_18:
	rst(ctx, 0x0018);
	DISPATCH();

ld_mem_ff00_u8_a:
	agoge_core_bus_write(ctx, 0xFF00 + read_u8(ctx), ctx->cpu.reg.a);
	DISPATCH();

pop_hl:
	ctx->cpu.reg.hl = stack_pop(ctx);
	DISPATCH();

ld_mem_ff00_c_a:
	agoge_core_bus_write(ctx, 0xFF00 + ctx->cpu.reg.c, ctx->cpu.reg.a);
	DISPATCH();

push_hl:
	stack_push(ctx, ctx->cpu.reg.hl);
	DISPATCH();

and_a_u8:
	alu_and(ctx, read_u8(ctx));
	DISPATCH();

rst_20:
	rst(ctx, 0x0020);
	DISPATCH();

add_sp_s8:
	ctx->cpu.reg.sp = alu_add_sp(ctx);
	DISPATCH();

jp_hl:
	ctx->cpu.reg.pc = ctx->cpu.reg.hl;
	DISPATCH();

ld_mem_u16_a:
	agoge_core_bus_write(ctx, read_u16(ctx), ctx->cpu.reg.a);
	DISPATCH();

xor_a_u8:
	alu_xor(ctx, read_u8(ctx));
	DISPATCH();

rst_28:
	rst(ctx, 0x0028);
	DISPATCH();

ld_a_mem_ff00_u8:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, 0xFF00 + read_u8(ctx));
	DISPATCH();

ld_a_mem_ff00_c:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, 0xFF00 + ctx->cpu.reg.c);
	DISPATCH();

pop_af:
	ctx->cpu.reg.af = stack_pop(ctx) & ~0x0F;
	DISPATCH();

di:
	DISPATCH();

push_af:
	stack_push(ctx, ctx->cpu.reg.af);
	DISPATCH();

or_a_u8:
	alu_or(ctx, read_u8(ctx));
	DISPATCH();

rst_30:
	rst(ctx, 0x0030);
	DISPATCH();

ld_hl_sp_s8:
	ctx->cpu.reg.hl = alu_add_sp(ctx);
	DISPATCH();

ld_sp_hl:
	ctx->cpu.reg.sp = ctx->cpu.reg.hl;
	DISPATCH();

ld_a_mem_u16:
	ctx->cpu.reg.a = agoge_core_bus_read(ctx, read_u16(ctx));
	DISPATCH();

ei:
	DISPATCH();

cp_a_u8:
	alu_cp(ctx, read_u8(ctx));
	DISPATCH();

rst_38:
	rst(ctx, 0x0038);
	DISPATCH();

#undef DISPATCH
}
