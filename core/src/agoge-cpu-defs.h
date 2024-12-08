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

#define CPU_OP_NOP (0x00)
#define CPU_OP_LD_BC_U16 (0x01)
#define CPU_OP_LD_MEM_BC_A (0x02)
#define CPU_OP_INC_BC (0x03)
#define CPU_OP_INC_B (0x04)
#define CPU_OP_DEC_B (0x05)
#define CPU_OP_LD_B_U8 (0x06)
#define CPU_OP_INC_DE (0x13)
#define CPU_OP_XOR_A_C (0xA9)
#define CPU_OP_LDD_HL_A (0x32)
#define CPU_OP_DEC_C (0x0D)
#define CPU_OP_ADD_A_U8 (0xC6)
#define CPU_OP_LD_C_U8 (0x0E)
#define CPU_OP_LDI_MEM_HL_A (0x22)
#define CPU_OP_LD_DE_U16 (0x11)
#define CPU_OP_LD_MEM_DE_A (0x12)
#define CPU_OP_INC_D (0x14)
#define CPU_OP_LD_A_MEM_DE (0x1A)
#define CPU_OP_JR_S8 (0x18)
#define CPU_OP_ADD_HL_DE (0x19)
#define CPU_OP_INC_E (0x1C)
#define CPU_OP_JR_NZ_S8 (0x20)
#define CPU_OP_LD_HL_U16 (0x21)
#define CPU_OP_INC_HL (0x23)
#define CPU_OP_INC_H (0x24)
#define CPU_OP_JR_Z_S8 (0x28)
#define CPU_OP_LDI_A_MEM_HL (0x2A)
#define CPU_OP_INC_L (0x2C)
#define CPU_OP_LD_SP_U16 (0x31)
#define CPU_OP_LD_A_U8 (0x3E)
#define CPU_OP_LD_B_A (0x47)
#define CPU_OP_LD_MEM_HL_A (0x77)
#define CPU_OP_LD_A_B (0x78)
#define CPU_OP_LD_A_H (0x7C)
#define CPU_OP_LD_A_L (0x7D)
#define CPU_OP_OR_A_C (0xB1)
#define CPU_OP_POP_BC (0xC1)
#define CPU_OP_JP_U16 (0xC3)
#define CPU_OP_PUSH_BC (0xC5)
#define CPU_OP_RET (0xC9)
#define CPU_OP_CALL_U16 (0xCD)
#define LD_MEM_FF00_U8_A (0xE0)
#define CPU_OP_POP_HL (0xE1)
#define CPU_OP_PUSH_HL (0xE5)
#define CPU_OP_LD_MEM_U16_A (0xEA)
#define CPU_OP_LD_A_MEM_FF00_U8 (0xF0)
#define CPU_OP_POP_AF (0xF1)
#define CPU_OP_DI (0xF3)
#define CPU_OP_PUSH_AF (0xF5)
#define CPU_OP_LD_A_MEM_U16 (0xFA)
#define CPU_OP_CP_A_U8 (0xFE)
#define CPU_OP_CALL_NZ_U16 (0xC4)
#define CPU_OP_AND_A_U8 (0xE6)
#define CPU_OP_SUB_A_U8 (0xD6)
#define CPU_OP_OR_A_A (0xB7)
#define CPU_OP_DEC_L (0x2D)
#define CPU_OP_PUSH_DE (0xD5)
#define CPU_OP_LD_B_MEM_HL (0x46)
#define CPU_OP_LD_C_MEM_HL (0x4E)
#define CPU_OP_LD_D_MEM_HL (0x56)
#define CPU_OP_XOR_A_MEM_HL (0xAE)
#define CPU_OP_LD_H_U8 (0x26)
#define CPU_OP_RRA (0x1F)
#define CPU_OP_JR_NC_S8 (0x30)
#define CPU_OP_PREFIX_CB (0xCB)

#define CPU_OP_RR_C (0x19)
#define CPU_OP_RR_D (0x1A)
#define CPU_OP_SRL_B (0x38)

#define CPU_ZERO_FLAG (UINT8_C(1) << 7)
#define CPU_SUBTRACT_FLAG (UINT8_C(1) << 6)
#define CPU_HALF_CARRY_FLAG (UINT8_C(1) << 5)
#define CPU_CARRY_FLAG (UINT8_C(1) << 4)

#define CPU_PWRUP_REG_PC (0x0100)
#define CPU_PWRUP_REG_SP (0xFFFE)

#define CPU_PWRUP_REG_AF (0x01B0)
#define CPU_PWRUP_REG_BC (0x0013)
#define CPU_PWRUP_REG_DE (0x00D8)
#define CPU_PWRUP_REG_HL (0x014D)
