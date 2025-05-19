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

#pragma once

#include "defs.h"

#define CPU_OP_NOP (UINT8_C(0x00))
#define CPU_OP_LD_BC_U16 (UINT8_C(0x01))
#define CPU_OP_INC_BC (UINT8_C(0x03))
#define CPU_OP_INC_B (UINT8_C(0x04))
#define CPU_OP_DEC_B (UINT8_C(0x05))
#define CPU_OP_LD_B_U8 (UINT8_C(0x06))
#define CPU_OP_DEC_C (UINT8_C(0x0D))
#define CPU_OP_LD_C_U8 (UINT8_C(0x0E))
#define CPU_OP_LD_DE_U16 (UINT8_C(0x11))
#define CPU_OP_LD_MEM_DE_A (UINT8_C(0x12))
#define CPU_OP_INC_DE (UINT8_C(0x13))
#define CPU_OP_INC_D (UINT8_C(0x14))
#define CPU_OP_JR_S8 (UINT8_C(0x18))
#define CPU_OP_LD_A_MEM_DE (UINT8_C(0x1A))
#define CPU_OP_INC_E (UINT8_C(0x1C))
#define CPU_OP_DEC_E (UINT8_C(0x1D))
#define CPU_OP_RRA (UINT8_C(0x1F))
#define CPU_OP_JR_NZ_S8 (UINT8_C(0x20))
#define CPU_OP_LD_HL_U16 (UINT8_C(0x21))
#define CPU_OP_LDI_MEM_HL_A (UINT8_C(0x22))
#define CPU_OP_INC_HL (UINT8_C(0x23))
#define CPU_OP_INC_H (UINT8_C(0x24))
#define CPU_OP_DEC_H (UINT8_C(0x25))
#define CPU_OP_LD_H_U8 (UINT8_C(0x26))
#define CPU_OP_JR_Z_S8 (UINT8_C(0x28))
#define CPU_OP_ADD_HL_HL (UINT8_C(0x29))
#define CPU_OP_LDI_A_MEM_HL (UINT8_C(0x2A))
#define CPU_OP_INC_L (UINT8_C(0x2C))
#define CPU_OP_DEC_L (UINT8_C(0x2D))
#define CPU_OP_JR_NC_S8 (UINT8_C(0x30))
#define CPU_OP_LD_SP_U16 (UINT8_C(0x31))
#define CPU_OP_LDD_MEM_HL_A (UINT8_C(0x32))
#define CPU_OP_DEC_MEM_HL (UINT8_C(0x35))
#define CPU_OP_INC_A (UINT8_C(0x3C))
#define CPU_OP_DEC_A (UINT8_C(0x3D))
#define CPU_OP_LD_A_U8 (UINT8_C(0x3E))
#define CPU_OP_LD_B_MEM_HL (UINT8_C(0x46))
#define CPU_OP_LD_B_A (UINT8_C(0x47))
#define CPU_OP_LD_C_MEM_HL (UINT8_C(0x4E))
#define CPU_OP_LD_C_A (UINT8_C(0x4F))
#define CPU_OP_LD_D_MEM_HL (UINT8_C(0x56))
#define CPU_OP_LD_D_A (UINT8_C(0x57))
#define CPU_OP_LD_E_A (UINT8_C(0x5F))
#define CPU_OP_LD_L_MEM_HL (UINT8_C(0x6E))
#define CPU_OP_LD_L_A (UINT8_C(0x6F))
#define CPU_OP_LD_MEM_HL_B (UINT8_C(0x70))
#define CPU_OP_LD_MEM_HL_C (UINT8_C(0x71))
#define CPU_OP_LD_MEM_HL_D (UINT8_C(0x72))
#define CPU_OP_LD_MEM_HL_A (UINT8_C(0x77))
#define CPU_OP_LD_A_B (UINT8_C(0x78))
#define CPU_OP_LD_A_C (UINT8_C(0x79))
#define CPU_OP_LD_A_D (UINT8_C(0x7A))
#define CPU_OP_LD_A_E (UINT8_C(0x7B))
#define CPU_OP_LD_A_H (UINT8_C(0x7C))
#define CPU_OP_LD_A_L (UINT8_C(0x7D))
#define CPU_OP_XOR_A_C (UINT8_C(0xA9))
#define CPU_OP_XOR_A_MEM_HL (UINT8_C(0xAE))
#define CPU_OP_OR_A_C (UINT8_C(0xB1))
#define CPU_OP_OR_A_MEM_HL (UINT8_C(0xB6))
#define CPU_OP_OR_A_A (UINT8_C(0xB7))
#define CPU_OP_CP_A_E (UINT8_C(0xBB))
#define CPU_OP_POP_BC (UINT8_C(0xC1))
#define CPU_OP_JP_NZ_U16 (UINT8_C(0xC2))
#define CPU_OP_JP_U16 (UINT8_C(0xC3))
#define CPU_OP_CALL_NZ_U16 (UINT8_C(0xC4))
#define CPU_OP_PUSH_BC (UINT8_C(0xC5))
#define CPU_OP_ADD_A_U8 (UINT8_C(0xC6))
#define CPU_OP_RET_Z (UINT8_C(0xC8))
#define CPU_OP_RET (UINT8_C(0xC9))
#define CPU_OP_PREFIX_CB (UINT8_C(0xCB))
#define CPU_OP_CALL_U16 (UINT8_C(0xCD))
#define CPU_OP_ADC_A_U8 (UINT8_C(0xCE))
#define CPU_OP_RET_NC (UINT8_C(0xD0))
#define CPU_OP_POP_DE (UINT8_C(0xD1))
#define CPU_OP_PUSH_DE (UINT8_C(0xD5))
#define CPU_OP_SUB_A_U8 (UINT8_C(0xD6))
#define CPU_OP_LD_MEM_FF00_U8_A (UINT8_C(0xE0))
#define CPU_OP_POP_HL (UINT8_C(0xE1))
#define CPU_OP_PUSH_HL (UINT8_C(0xE5))
#define CPU_OP_AND_A_U8 (UINT8_C(0xE6))
#define CPU_OP_JP_HL (UINT8_C(0xE9))
#define CPU_OP_LD_MEM_U16_A (UINT8_C(0xEA))
#define CPU_OP_XOR_A_U8 (UINT8_C(0xEE))
#define CPU_OP_LD_A_MEM_FF00_U8 (UINT8_C(0xF0))
#define CPU_OP_POP_AF (UINT8_C(0xF1))
#define CPU_OP_DI (UINT8_C(0xF3))
#define CPU_OP_PUSH_AF (UINT8_C(0xF5))
#define CPU_OP_LD_A_MEM_U16 (UINT8_C(0xFA))
#define CPU_OP_CP_A_U8 (UINT8_C(0xFE))

#define CPU_OP_RR_C (UINT8_C(0x19))
#define CPU_OP_RR_D (UINT8_C(0x1A))
#define CPU_OP_SRL_B (UINT8_C(0x38))

#define CPU_PWRUP_REG_PC (UINT16_C(0x0100))

#define CPU_FLAG_ZERO (BIT_7)
#define CPU_FLAG_SUBTRACT (BIT_6)
#define CPU_FLAG_HALF_CARRY (BIT_5)
#define CPU_FLAG_CARRY (BIT_4)
