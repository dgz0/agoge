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
#define CPU_OP_DEC_C (UINT8_C(0x0D))
#define CPU_OP_LD_C_U8 (UINT8_C(0x0E))
#define CPU_OP_LD_DE_U16 (UINT8_C(0x11))
#define CPU_OP_LD_MEM_DE_A (UINT8_C(0x12))
#define CPU_OP_INC_D (UINT8_C(0x14))
#define CPU_OP_JR_S8 (UINT8_C(0x18))
#define CPU_OP_INC_E (UINT8_C(0x1C))
#define CPU_OP_JR_NZ_S8 (UINT8_C(0x20))
#define CPU_OP_LD_HL_U16 (UINT8_C(0x21))
#define CPU_OP_LDI_A_MEM_HL (UINT8_C(0x2A))
#define CPU_OP_LD_SP_U16 (UINT8_C(0x31))
#define CPU_OP_LD_A_U8 (UINT8_C(0x3E))
#define CPU_OP_LD_B_A (UINT8_C(0x47))
#define CPU_OP_LD_A_B (UINT8_C(0x78))
#define CPU_OP_LD_A_H (UINT8_C(0x7C))
#define CPU_OP_LD_A_L (UINT8_C(0x7D))
#define CPU_OP_JP_U16 (UINT8_C(0xC3))
#define CPU_OP_RET (UINT8_C(0xC9))
#define CPU_OP_CALL_U16 (UINT8_C(0xCD))
#define CPU_OP_LD_MEM_FF00_U8_A (UINT8_C(0xE0))
#define CPU_OP_LD_MEM_U16_A (UINT8_C(0xEA))
#define CPU_OP_DI (UINT8_C(0xF3))

#define CPU_PWRUP_REG_PC (UINT16_C(0x0100))

#define CPU_FLAG_ZERO (BIT_7)
#define CPU_FLAG_SUBTRACT (BIT_6)
#define CPU_FLAG_HALF_CARRY (BIT_5)
#define CPU_FLAG_CARRY (BIT_4)
