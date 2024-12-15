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

#include <stdint.h>

// clang-format off

#define CPU_OP_NOP          (UINT8_C(0x00))
#define CPU_OP_LD_BC_U16	(UINT8_C(0x01))
#define CPU_OP_LD_MEM_BC_A  (UINT8_C(0x02))
#define CPU_OP_INC_BC		(UINT8_C(0x03))
#define CPU_OP_INC_B            (UINT8_C(0x04))
#define CPU_OP_DEC_B		(UINT8_C(0x05))
#define CPU_OP_LD_B_U8		(UINT8_C(0x06))
#define CPU_OP_RLCA         (UINT8_C(0x07))
#define CPU_OP_LD_MEM_U16_SP    (UINT8_C(0x08))
#define CPU_OP_ADD_HL_BC        (UINT8_C(0x09))
#define CPU_OP_LD_A_MEM_BC  (UINT8_C(0x0A))
#define CPU_OP_DEC_BC           (UINT8_C(0x0B))
#define CPU_OP_INC_C            (UINT8_C(0x0C))
#define CPU_OP_DEC_C		(UINT8_C(0x0D))
#define CPU_OP_LD_C_U8		(UINT8_C(0x0E))
#define CPU_OP_RRCA         (UINT8_C(0x0F))
#define CPU_OP_LD_DE_U16	(UINT8_C(0x11))
#define CPU_OP_LD_MEM_DE_A	(UINT8_C(0x12))
#define CPU_OP_INC_DE		(UINT8_C(0x13))
#define CPU_OP_INC_D		(UINT8_C(0x14))
#define CPU_OP_DEC_D        (UINT8_C(0x15))
#define CPU_OP_LD_D_U8          (UINT8_C(0x16))
#define CPU_OP_RLA (UINT8_C(0x17))
#define CPU_OP_JR_S8		(UINT8_C(0x18))
#define CPU_OP_ADD_HL_DE	(UINT8_C(0x19))
#define CPU_OP_LD_A_MEM_DE	(UINT8_C(0x1A))
#define CPU_OP_DEC_DE           (UINT8_C(0x1B))
#define CPU_OP_INC_E		(UINT8_C(0x1C))
#define CPU_OP_DEC_E            (UINT8_C(0x1D))
#define CPU_OP_LD_E_U8          (UINT8_C(0x1E))
#define CPU_OP_RRA		(UINT8_C(0x1F))
#define CPU_OP_JR_NZ_S8		(UINT8_C(0x20))
#define CPU_OP_LD_HL_U16	(UINT8_C(0x21))
#define CPU_OP_LDI_MEM_HL_A	(UINT8_C(0x22))
#define CPU_OP_INC_HL		(UINT8_C(0x23))
#define CPU_OP_INC_H		(UINT8_C(0x24))
#define CPU_OP_DEC_H            (UINT8_C(0x25))
#define CPU_OP_LD_H_U8		(UINT8_C(0x26))
#define CPU_OP_DAA              (UINT8_C(0x27))
#define CPU_OP_JR_Z_S8		(UINT8_C(0x28))
#define CPU_OP_ADD_HL_HL        (UINT8_C(0x29))
#define CPU_OP_LDI_A_MEM_HL	(UINT8_C(0x2A))
#define CPU_OP_DEC_HL           (UINT8_C(0x2B))
#define CPU_OP_INC_L		(UINT8_C(0x2C))
#define CPU_OP_DEC_L		(UINT8_C(0x2D))
#define CPU_OP_LD_L_U8          (UINT8_C(0x2E))
#define CPU_OP_CPL              (UINT8_C(0x2F))
#define CPU_OP_JR_NC_S8         (UINT8_C(0x30))
#define CPU_OP_LD_SP_U16	(UINT8_C(0x31))
#define CPU_OP_LDD_HL_A		(UINT8_C(0x32))
#define CPU_OP_INC_SP           (UINT8_C(0x33))
#define CPU_OP_INC_MEM_HL   (UINT8_C(0x34))
#define CPU_OP_DEC_MEM_HL   (UINT8_C(0x35))
#define CPU_OP_LD_MEM_HL_U8     (UINT8_C(0x36))
#define CPU_OP_SCF              (UINT8_C(0x37))
#define CPU_OP_JR_C_S8          (UINT8_C(0x38))
#define CPU_OP_ADD_HL_SP        (UINT8_C(0x39))
#define CPU_OP_LDD_A_HL         (UINT8_C(0x3A))
#define CPU_OP_DEC_SP           (UINT8_C(0x3B))
#define CPU_OP_INC_A            (UINT8_C(0x3C))
#define CPU_OP_DEC_A            (UINT8_C(0x3D))
#define CPU_OP_LD_A_U8		(UINT8_C(0x3E))
#define CPU_OP_CCF            (UINT8_C(0x3F))
#define CPU_OP_LD_B_B           (UINT8_C(0x40))
#define CPU_OP_LD_B_C           (UINT8_C(0x41))
#define CPU_OP_LD_B_D           (UINT8_C(0x42))
#define CPU_OP_LD_B_E           (UINT8_C(0x43))
#define CPU_OP_LD_B_H           (UINT8_C(0x44))
#define CPU_OP_LD_B_L           (UINT8_C(0x45))
#define CPU_OP_LD_B_MEM_HL	(UINT8_C(0x46))
#define CPU_OP_LD_B_A		(UINT8_C(0x47))
#define CPU_OP_LD_C_B           (UINT8_C(0x48))
#define CPU_OP_LD_C_C           (UINT8_C(0x49))
#define CPU_OP_LD_C_D           (UINT8_C(0x4A))
#define CPU_OP_LD_C_E           (UINT8_C(0x4B))
#define CPU_OP_LD_C_H           (UINT8_C(0x4C))
#define CPU_OP_LD_C_L           (UINT8_C(0x4D))
#define CPU_OP_LD_C_MEM_HL	(UINT8_C(0x4E))
#define CPU_OP_LD_C_A           (UINT8_C(0x4F))
#define CPU_OP_LD_D_B           (UINT8_C(0x50))
#define CPU_OP_LD_D_C           (UINT8_C(0x51))
#define CPU_OP_LD_D_D           (UINT8_C(0x52))
#define CPU_OP_LD_D_E           (UINT8_C(0x53))
#define CPU_OP_LD_D_H           (UINT8_C(0x54))
#define CPU_OP_LD_D_L           (UINT8_C(0x55))
#define CPU_OP_LD_D_MEM_HL	(UINT8_C(0x56))
#define CPU_OP_LD_D_A           (UINT8_C(0x57))
#define CPU_OP_LD_E_B           (UINT8_C(0x58))
#define CPU_OP_LD_E_C           (UINT8_C(0x59))
#define CPU_OP_LD_E_D           (UINT8_C(0x5A))
#define CPU_OP_LD_E_E           (UINT8_C(0x5B))
#define CPU_OP_LD_E_H           (UINT8_C(0x5C))
#define CPU_OP_LD_E_L           (UINT8_C(0x5D))
#define CPU_OP_LD_E_MEM_HL      (UINT8_C(0x5E))
#define CPU_OP_LD_E_A           (UINT8_C(0x5F))
#define CPU_OP_LD_H_B           (UINT8_C(0x60))
#define CPU_OP_LD_H_C           (UINT8_C(0x61))
#define CPU_OP_LD_H_D           (UINT8_C(0x62))
#define CPU_OP_LD_H_E           (UINT8_C(0x63))
#define CPU_OP_LD_H_H           (UINT8_C(0x64))
#define CPU_OP_LD_H_L           (UINT8_C(0x65))
#define CPU_OP_LD_H_MEM_HL      (UINT8_C(0x66))
#define CPU_OP_LD_H_A           (UINT8_C(0x67))
#define CPU_OP_LD_L_B           (UINT8_C(0x68))
#define CPU_OP_LD_L_C           (UINT8_C(0x69))
#define CPU_OP_LD_L_D           (UINT8_C(0x6A))
#define CPU_OP_LD_L_E           (UINT8_C(0x6B))
#define CPU_OP_LD_L_H           (UINT8_C(0x6C))
#define CPU_OP_LD_L_L           (UINT8_C(0x6D))
#define CPU_OP_LD_L_MEM_HL      (UINT8_C(0x6E))
#define CPU_OP_LD_L_A           (UINT8_C(0x6F))
#define CPU_OP_LD_MEM_HL_B      (UINT8_C(0x70))
#define CPU_OP_LD_MEM_HL_C      (UINT8_C(0x71))
#define CPU_OP_LD_MEM_HL_D      (UINT8_C(0x72))
#define CPU_OP_LD_MEM_HL_E      (UINT8_C(0x73))
#define CPU_OP_LD_MEM_HL_H      (UINT8_C(0x74))
#define CPU_OP_LD_MEM_HL_L      (UINT8_C(0x75))
#define CPU_OP_HALT             (UINT8_C(0x76))
#define CPU_OP_LD_MEM_HL_A	(UINT8_C(0x77))
#define CPU_OP_LD_A_B		(UINT8_C(0x78))
#define CPU_OP_LD_A_C           (UINT8_C(0x79))
#define CPU_OP_LD_A_D           (UINT8_C(0x7A))
#define CPU_OP_LD_A_E           (UINT8_C(0x7B))
#define CPU_OP_LD_A_H		(UINT8_C(0x7C))
#define CPU_OP_LD_A_L		(UINT8_C(0x7D))
#define CPU_OP_LD_A_MEM_HL      (UINT8_C(0x7E))
#define CPU_OP_LD_A_A           (UINT8_C(0x7F))
#define CPU_OP_ADD_A_B        (UINT8_C(0x80))
#define CPU_OP_ADD_A_C      (UINT8_C(0x81))
#define CPU_OP_ADD_A_D        (UINT8_C(0x82))
#define CPU_OP_ADD_A_E        (UINT8_C(0x83))
#define CPU_OP_ADD_A_H        (UINT8_C(0x84))
#define CPU_OP_ADD_A_L      (UINT8_C(0x85))
#define CPU_OP_ADD_A_MEM_HL (UINT8_C(0x86))
#define CPU_OP_ADD_A_A      (UINT8_C(0x87))
#define CPU_OP_ADC_A_B      (UINT8_C(0x88))
#define CPU_OP_ADC_A_C      (UINT8_C(0x89))
#define CPU_OP_ADC_A_D      (UINT8_C(0x8A))
#define CPU_OP_ADC_A_E      (UINT8_C(0x8B))
#define CPU_OP_ADC_A_H      (UINT8_C(0x8C))
#define CPU_OP_ADC_A_L      (UINT8_C(0x8D))
#define CPU_OP_ADC_A_MEM_HL (UINT8_C(0x8E))
#define CPU_OP_ADC_A_A      (UINT8_C(0x8F))
#define CPU_OP_SUB_A_B      (UINT8_C(0x90))
#define CPU_OP_SUB_A_C      (UINT8_C(0x91))
#define CPU_OP_SUB_A_D      (UINT8_C(0x92))
#define CPU_OP_SUB_A_E      (UINT8_C(0x93))
#define CPU_OP_SUB_A_H      (UINT8_C(0x94))
#define CPU_OP_SUB_A_L      (UINT8_C(0x95))
#define CPU_OP_SUB_A_MEM_HL (UINT8_C(0x96))
#define CPU_OP_SUB_A_A      (UINT8_C(0x97))
#define CPU_OP_SBC_A_B      (UINT8_C(0x98))
#define CPU_OP_SBC_A_C      (UINT8_C(0x99))
#define CPU_OP_SBC_A_D      (UINT8_C(0x9A))
#define CPU_OP_SBC_A_E      (UINT8_C(0x9B))
#define CPU_OP_SBC_A_H      (UINT8_C(0x9C))
#define CPU_OP_SBC_A_L      (UINT8_C(0x9D))
#define CPU_OP_SBC_A_MEM_HL (UINT8_C(0x9E))
#define CPU_OP_SBC_A_A      (UINT8_C(0x9F))
#define CPU_OP_AND_A_B      (UINT8_C(0xA0))
#define CPU_OP_AND_A_C      (UINT8_C(0xA1))
#define CPU_OP_AND_A_D      (UINT8_C(0xA2))
#define CPU_OP_AND_A_E      (UINT8_C(0xA3))
#define CPU_OP_AND_A_H      (UINT8_C(0xA4))
#define CPU_OP_AND_A_L      (UINT8_C(0xA5))
#define CPU_OP_AND_A_MEM_HL (UINT8_C(0xA6))
#define CPU_OP_AND_A_A      (UINT8_C(0xA7))
#define CPU_OP_XOR_A_B      (UINT8_C(0xA8))
#define CPU_OP_XOR_A_C		(UINT8_C(0xA9))
#define CPU_OP_XOR_A_D      (UINT8_C(0xAA))
#define CPU_OP_XOR_A_E      (UINT8_C(0xAB))
#define CPU_OP_XOR_A_H      (UINT8_C(0xAC))
#define CPU_OP_XOR_A_L          (UINT8_C(0xAD))
#define CPU_OP_XOR_A_MEM_HL	(UINT8_C(0xAE))
#define CPU_OP_XOR_A_A          (UINT8_C(0xAF))
#define CPU_OP_OR_A_B           (UINT8_C(0xB0))
#define CPU_OP_OR_A_C		(UINT8_C(0xB1))
#define CPU_OP_OR_A_D       (UINT8_C(0xB2))
#define CPU_OP_OR_A_E       (UINT8_C(0xB3))
#define CPU_OP_OR_A_H         (UINT8_C(0xB4))
#define CPU_OP_OR_A_L         (UINT8_C(0xB5))
#define CPU_OP_OR_A_MEM_HL      (UINT8_C(0xB6))
#define CPU_OP_OR_A_A		(UINT8_C(0xB7))
#define CPU_OP_CP_A_B       (UINT8_C(0xB8))
#define CPU_OP_CP_A_C         (UINT8_C(0xB9))
#define CPU_OP_CP_A_D         (UINT8_C(0xBA))
#define CPU_OP_CP_A_E           (UINT8_C(0xBB))
#define CPU_OP_CP_A_H         (UINT8_C(0xBC))
#define CPU_OP_CP_A_L         (UINT8_C(0xBD))
#define CPU_OP_CP_A_MEM_HL  (UINT8_C(0xBE))
#define CPU_OP_CP_A_A         (UINT8_C(0xBF))
#define CPU_OP_RET_NZ       (UINT8_C(0xC0))
#define CPU_OP_POP_BC		(UINT8_C(0xC1))
#define CPU_OP_JP_NZ_U16        (UINT8_C(0xC2))
#define CPU_OP_JP_U16		(UINT8_C(0xC3))
#define CPU_OP_CALL_NZ_U16	(UINT8_C(0xC4))
#define CPU_OP_PUSH_BC		(UINT8_C(0xC5))
#define CPU_OP_ADD_A_U8		(UINT8_C(0xC6))
#define CPU_OP_RST_00       (UINT8_C(0xC7))
#define CPU_OP_RET_Z            (UINT8_C(0xC8))
#define CPU_OP_RET		(UINT8_C(0xC9))
#define CPU_OP_JP_Z_U16 (UINT8_C(0xCA))
#define CPU_OP_PREFIX_CB        (UINT8_C(0xCB))
#define CPU_OP_CALL_Z_U16       (UINT8_C(0xCC))
#define CPU_OP_CALL_U16		(UINT8_C(0xCD))
#define CPU_OP_ADC_A_U8         (UINT8_C(0xCE))
#define CPU_OP_RST_08       (UINT8_C(0xCF))
#define CPU_OP_RET_NC           (UINT8_C(0xD0))
#define CPU_OP_POP_DE           (UINT8_C(0xD1))
#define CPU_OP_JP_NC_U16        (UINT8_C(0xD2))
#define CPU_OP_CALL_NC_U16      (UINT8_C(0xD4))
#define CPU_OP_PUSH_DE          (UINT8_C(0xD5))
#define CPU_OP_SUB_A_U8         (UINT8_C(0xD6))
#define CPU_OP_RST_10           (UINT8_C(0xD7))
#define CPU_OP_RET_C            (UINT8_C(0xD8))
#define CPU_OP_RETI             (UINT8_C(0xD9))
#define CPU_OP_JP_C_U16         (UINT8_C(0xDA))
#define CPU_OP_CALL_C_U16       (UINT8_C(0xDC))
#define CPU_OP_SBC_A_U8         (UINT8_C(0xDE))
#define CPU_OP_RST_18           (UINT8_C(0xDF))
#define LD_MEM_FF00_U8_A        (UINT8_C(0xE0))
#define CPU_OP_POP_HL           (UINT8_C(0xE1))
#define CPU_OP_LD_MEM_FF00_C_A  (UINT8_C(0xE2))
#define CPU_OP_PUSH_HL          (UINT8_C(0xE5))
#define CPU_OP_AND_A_U8         (UINT8_C(0xE6))
#define CPU_OP_RST_20           (UINT8_C(0xE7))
#define CPU_OP_ADD_SP_S8        (UINT8_C(0xE8))
#define CPU_OP_JP_HL            (UINT8_C(0xE9))
#define CPU_OP_LD_MEM_U16_A     (UINT8_C(0xEA))
#define CPU_OP_XOR_A_U8         (UINT8_C(0xEE))
#define CPU_OP_RST_28           (UINT8_C(0xEF))
#define CPU_OP_LD_A_MEM_FF00_U8 (UINT8_C(0xF0))
#define CPU_OP_POP_AF           (UINT8_C(0xF1))
#define CPU_OP_LD_A_MEM_FF00_C  (UINT8_C(0xF2))
#define CPU_OP_DI               (UINT8_C(0xF3))
#define CPU_OP_PUSH_AF          (UINT8_C(0xF5))
#define CPU_OP_OR_A_U8          (UINT8_C(0xF6))
#define CPU_OP_RST_30           (UINT8_C(0xF7))
#define CPU_OP_LD_HL_SP_S8      (UINT8_C(0xF8))
#define CPU_OP_LD_SP_HL         (UINT8_C(0xF9))
#define CPU_OP_LD_A_MEM_U16     (UINT8_C(0xFA))
#define CPU_OP_EI               (UINT8_C(0xFB))
#define CPU_OP_CP_A_U8          (UINT8_C(0xFE))
#define CPU_OP_RST_38           (UINT8_C(0xFF))

#define CPU_OP_RLC_B    (UINT8_C(0x00))
#define CPU_OP_RLC_C    (UINT8_C(0x01))
#define CPU_OP_RLC_D    (UINT8_C(0x02))
#define CPU_OP_RLC_E    (UINT8_C(0x03))
#define CPU_OP_RLC_H    (UINT8_C(0x04))
#define CPU_OP_RLC_L    (UINT8_C(0x05))
#define CPU_OP_RLC_MEM_HL (UINT8_C(0x06))
#define CPU_OP_RLC_A    (UINT8_C(0x07))
#define CPU_OP_RRC_B    (UINT8_C(0x08))
#define CPU_OP_RRC_C    (UINT8_C(0x09))
#define CPU_OP_RRC_D    (UINT8_C(0x0A))
#define CPU_OP_RRC_E    (UINT8_C(0x0B))
#define CPU_OP_RRC_H    (UINT8_C(0x0C))
#define CPU_OP_RRC_L    (UINT8_C(0x0D))
#define CPU_OP_RRC_MEM_HL (UINT8_C(0x0E))
#define CPU_OP_RRC_A    (UINT8_C(0x0F))
#define CPU_OP_RL_B     (UINT8_C(0x10))
#define CPU_OP_RL_C     (UINT8_C(0x11))
#define CPU_OP_RL_D     (UINT8_C(0x12))
#define CPU_OP_RL_E     (UINT8_C(0x13))
#define CPU_OP_RL_H     (UINT8_C(0x14))
#define CPU_OP_RL_L     (UINT8_C(0x15))
#define CPU_OP_RL_MEM_HL (UINT8_C(0x16))
#define CPU_OP_RL_A     (UINT8_C(0x17))
#define CPU_OP_RR_B     (UINT8_C(0x18))
#define CPU_OP_RR_C     (UINT8_C(0x19))
#define CPU_OP_RR_D     (UINT8_C(0x1A))
#define CPU_OP_RR_E     (UINT8_C(0x1B))
#define CPU_OP_RR_H     (UINT8_C(0x1C))
#define CPU_OP_RR_L     (UINT8_C(0x1D))
#define CPU_OP_RR_MEM_HL (UINT8_C(0x1E))
#define CPU_OP_RR_A     (UINT8_C(0x1F))
#define CPU_OP_SLA_B    (UINT8_C(0x20))
#define CPU_OP_SLA_C    (UINT8_C(0x21))
#define CPU_OP_SLA_D    (UINT8_C(0x22))
#define CPU_OP_SLA_E    (UINT8_C(0x23))
#define CPU_OP_SLA_H    (UINT8_C(0x24))
#define CPU_OP_SLA_L    (UINT8_C(0x25))
#define CPU_OP_SLA_MEM_HL (UINT8_C(0x26))
#define CPU_OP_SLA_A    (UINT8_C(0x27))
#define CPU_OP_SRA_B    (UINT8_C(0x28))
#define CPU_OP_SRA_C    (UINT8_C(0x29))
#define CPU_OP_SRA_D    (UINT8_C(0x2A))
#define CPU_OP_SRA_E    (UINT8_C(0x2B))
#define CPU_OP_SRA_H    (UINT8_C(0x2C))
#define CPU_OP_SRA_L    (UINT8_C(0x2D))
#define CPU_OP_SRA_MEM_HL (UINT8_C(0x2E))
#define CPU_OP_SRA_A    (UINT8_C(0x2F))
#define CPU_OP_SWAP_B   (UINT8_C(0x30))
#define CPU_OP_SWAP_C   (UINT8_C(0x31))
#define CPU_OP_SWAP_D   (UINT8_C(0x32))
#define CPU_OP_SWAP_E   (UINT8_C(0x33))
#define CPU_OP_SWAP_H   (UINT8_C(0x34))
#define CPU_OP_SWAP_L   (UINT8_C(0x35))
#define CPU_OP_SWAP_MEM_HL  (UINT8_C(0x36))
#define CPU_OP_SWAP_A   (UINT8_C(0x37))
#define CPU_OP_SRL_B    (UINT8_C(0x38))
#define CPU_OP_SRL_C    (UINT8_C(0x39))
#define CPU_OP_SRL_D    (UINT8_C(0x3A))
#define CPU_OP_SRL_E    (UINT8_C(0x3B))
#define CPU_OP_SRL_H    (UINT8_C(0x3C))
#define CPU_OP_SRL_L    (UINT8_C(0x3D))
#define CPU_OP_SRL_MEM_HL (UINT8_C(0x3E))
#define CPU_OP_SRL_A    (UINT8_C(0x3F))
#define CPU_OP_BIT_0_B  (UINT8_C(0x40))
#define CPU_OP_BIT_0_C  (UINT8_C(0x41))
#define CPU_OP_BIT_0_D  (UINT8_C(0x42))
#define CPU_OP_BIT_0_E  (UINT8_C(0x43))
#define CPU_OP_BIT_0_H  (UINT8_C(0x44))
#define CPU_OP_BIT_0_L  (UINT8_C(0x45))
#define CPU_OP_BIT_0_MEM_HL  (UINT8_C(0x46))
#define CPU_OP_BIT_0_A  (UINT8_C(0x47))
#define CPU_OP_BIT_1_B  (UINT8_C(0x48))
#define CPU_OP_BIT_1_C  (UINT8_C(0x49))
#define CPU_OP_BIT_1_D  (UINT8_C(0x4A))
#define CPU_OP_BIT_1_E  (UINT8_C(0x4B))
#define CPU_OP_BIT_1_H  (UINT8_C(0x4C))
#define CPU_OP_BIT_1_L  (UINT8_C(0x4D))
#define CPU_OP_BIT_1_MEM_HL  (UINT8_C(0x4E))
#define CPU_OP_BIT_1_A  (UINT8_C(0x4F))
#define CPU_OP_BIT_2_B  (UINT8_C(0x50))
#define CPU_OP_BIT_2_C  (UINT8_C(0x51))
#define CPU_OP_BIT_2_D  (UINT8_C(0x52))
#define CPU_OP_BIT_2_E  (UINT8_C(0x53))
#define CPU_OP_BIT_2_H  (UINT8_C(0x54))
#define CPU_OP_BIT_2_L  (UINT8_C(0x55))
#define CPU_OP_BIT_2_MEM_HL  (UINT8_C(0x56))
#define CPU_OP_BIT_2_A  (UINT8_C(0x57))
#define CPU_OP_BIT_3_B  (UINT8_C(0x58))
#define CPU_OP_BIT_3_C  (UINT8_C(0x59))
#define CPU_OP_BIT_3_D  (UINT8_C(0x5A))
#define CPU_OP_BIT_3_E  (UINT8_C(0x5B))
#define CPU_OP_BIT_3_H  (UINT8_C(0x5C))
#define CPU_OP_BIT_3_L  (UINT8_C(0x5D))
#define CPU_OP_BIT_3_MEM_HL  (UINT8_C(0x5E))
#define CPU_OP_BIT_3_A  (UINT8_C(0x5F))
#define CPU_OP_BIT_4_B  (UINT8_C(0x60))
#define CPU_OP_BIT_4_C  (UINT8_C(0x61))
#define CPU_OP_BIT_4_D  (UINT8_C(0x62))
#define CPU_OP_BIT_4_E  (UINT8_C(0x63))
#define CPU_OP_BIT_4_H  (UINT8_C(0x64))
#define CPU_OP_BIT_4_L  (UINT8_C(0x65))
#define CPU_OP_BIT_4_MEM_HL  (UINT8_C(0x66))
#define CPU_OP_BIT_4_A  (UINT8_C(0x67))
#define CPU_OP_BIT_5_B  (UINT8_C(0x68))
#define CPU_OP_BIT_5_C  (UINT8_C(0x69))
#define CPU_OP_BIT_5_D  (UINT8_C(0x6A))
#define CPU_OP_BIT_5_E  (UINT8_C(0x6B))
#define CPU_OP_BIT_5_H  (UINT8_C(0x6C))
#define CPU_OP_BIT_5_L  (UINT8_C(0x6D))
#define CPU_OP_BIT_5_MEM_HL  (UINT8_C(0x6E))
#define CPU_OP_BIT_5_A  (UINT8_C(0x6F))
#define CPU_OP_BIT_6_B  (UINT8_C(0x70))
#define CPU_OP_BIT_6_C  (UINT8_C(0x71))
#define CPU_OP_BIT_6_D  (UINT8_C(0x72))
#define CPU_OP_BIT_6_E  (UINT8_C(0x73))
#define CPU_OP_BIT_6_H  (UINT8_C(0x74))
#define CPU_OP_BIT_6_L  (UINT8_C(0x75))
#define CPU_OP_BIT_6_MEM_HL  (UINT8_C(0x76))
#define CPU_OP_BIT_6_A  (UINT8_C(0x77))
#define CPU_OP_BIT_7_B  (UINT8_C(0x78))
#define CPU_OP_BIT_7_C  (UINT8_C(0x79))
#define CPU_OP_BIT_7_D  (UINT8_C(0x7A))
#define CPU_OP_BIT_7_E  (UINT8_C(0x7B))
#define CPU_OP_BIT_7_H  (UINT8_C(0x7C))
#define CPU_OP_BIT_7_L  (UINT8_C(0x7D))
#define CPU_OP_BIT_7_MEM_HL  (UINT8_C(0x7E))
#define CPU_OP_BIT_7_A  (UINT8_C(0x7F))
#define CPU_OP_RES_0_B  (UINT8_C(0x80))
#define CPU_OP_RES_0_C  (UINT8_C(0x81))
#define CPU_OP_RES_0_D  (UINT8_C(0x82))
#define CPU_OP_RES_0_E  (UINT8_C(0x83))
#define CPU_OP_RES_0_H  (UINT8_C(0x84))
#define CPU_OP_RES_0_L  (UINT8_C(0x85))
#define CPU_OP_RES_0_MEM_HL (UINT8_C(0x86))
#define CPU_OP_RES_0_A  (UINT8_C(0x87))
#define CPU_OP_RES_1_B  (UINT8_C(0x88))
#define CPU_OP_RES_1_C  (UINT8_C(0x89))
#define CPU_OP_RES_1_D  (UINT8_C(0x8A))
#define CPU_OP_RES_1_E  (UINT8_C(0x8B))
#define CPU_OP_RES_1_H  (UINT8_C(0x8C))
#define CPU_OP_RES_1_L  (UINT8_C(0x8D))
#define CPU_OP_RES_1_MEM_HL (UINT8_C(0x8E))
#define CPU_OP_RES_1_A  (UINT8_C(0x8F))
#define CPU_OP_RES_2_B  (UINT8_C(0x90))
#define CPU_OP_RES_2_C  (UINT8_C(0x91))
#define CPU_OP_RES_2_D  (UINT8_C(0x92))
#define CPU_OP_RES_2_E  (UINT8_C(0x93))
#define CPU_OP_RES_2_H  (UINT8_C(0x94))
#define CPU_OP_RES_2_L  (UINT8_C(0x95))
#define CPU_OP_RES_2_MEM_HL (UINT8_C(0x96))
#define CPU_OP_RES_2_A  (UINT8_C(0x97))
#define CPU_OP_RES_3_B  (UINT8_C(0x98))
#define CPU_OP_RES_3_C  (UINT8_C(0x99))
#define CPU_OP_RES_3_D  (UINT8_C(0x9A))
#define CPU_OP_RES_3_E  (UINT8_C(0x9B))
#define CPU_OP_RES_3_H  (UINT8_C(0x9C))
#define CPU_OP_RES_3_L  (UINT8_C(0x9D))
#define CPU_OP_RES_3_MEM_HL (UINT8_C(0x9E))
#define CPU_OP_RES_3_A  (UINT8_C(0x9F))
#define CPU_OP_RES_4_B  (UINT8_C(0xA0))
#define CPU_OP_RES_4_C  (UINT8_C(0xA1))
#define CPU_OP_RES_4_D  (UINT8_C(0xA2))
#define CPU_OP_RES_4_E  (UINT8_C(0xA3))
#define CPU_OP_RES_4_H  (UINT8_C(0xA4))
#define CPU_OP_RES_4_L  (UINT8_C(0xA5))
#define CPU_OP_RES_4_MEM_HL (UINT8_C(0xA6))
#define CPU_OP_RES_4_A  (UINT8_C(0xA7))
#define CPU_OP_RES_5_B  (UINT8_C(0xA8))
#define CPU_OP_RES_5_C  (UINT8_C(0xA9))
#define CPU_OP_RES_5_D  (UINT8_C(0xAA))
#define CPU_OP_RES_5_E  (UINT8_C(0xAB))
#define CPU_OP_RES_5_H  (UINT8_C(0xAC))
#define CPU_OP_RES_5_L  (UINT8_C(0xAD))
#define CPU_OP_RES_5_MEM_HL (UINT8_C(0xAE))
#define CPU_OP_RES_5_A  (UINT8_C(0xAF))
#define CPU_OP_RES_6_B  (UINT8_C(0xB0))
#define CPU_OP_RES_6_C  (UINT8_C(0xB1))
#define CPU_OP_RES_6_D  (UINT8_C(0xB2))
#define CPU_OP_RES_6_E  (UINT8_C(0xB3))
#define CPU_OP_RES_6_H  (UINT8_C(0xB4))
#define CPU_OP_RES_6_L  (UINT8_C(0xB5))
#define CPU_OP_RES_6_MEM_HL (UINT8_C(0xB6))
#define CPU_OP_RES_6_A  (UINT8_C(0xB7))
#define CPU_OP_RES_7_B  (UINT8_C(0xB8))
#define CPU_OP_RES_7_C  (UINT8_C(0xB9))
#define CPU_OP_RES_7_D  (UINT8_C(0xBA))
#define CPU_OP_RES_7_E  (UINT8_C(0xBB))
#define CPU_OP_RES_7_H  (UINT8_C(0xBC))
#define CPU_OP_RES_7_L  (UINT8_C(0xBD))
#define CPU_OP_RES_7_MEM_HL (UINT8_C(0xBE))
#define CPU_OP_RES_7_A  (UINT8_C(0xBF))
#define CPU_OP_SET_0_B  (UINT8_C(0xC0))
#define CPU_OP_SET_0_C  (UINT8_C(0xC1))
#define CPU_OP_SET_0_D  (UINT8_C(0xC2))
#define CPU_OP_SET_0_E  (UINT8_C(0xC3))
#define CPU_OP_SET_0_H  (UINT8_C(0xC4))
#define CPU_OP_SET_0_L  (UINT8_C(0xC5))
#define CPU_OP_SET_0_MEM_HL (UINT8_C(0xC6))
#define CPU_OP_SET_0_A (UINT8_C(0xC7))
#define CPU_OP_SET_1_B  (UINT8_C(0xC8))
#define CPU_OP_SET_1_C  (UINT8_C(0xC9))
#define CPU_OP_SET_1_D  (UINT8_C(0xCA))
#define CPU_OP_SET_1_E  (UINT8_C(0xCB))
#define CPU_OP_SET_1_H  (UINT8_C(0xCC))
#define CPU_OP_SET_1_L  (UINT8_C(0xCD))
#define CPU_OP_SET_1_MEM_HL (UINT8_C(0xCE))
#define CPU_OP_SET_1_A  (UINT8_C(0xCF))
#define CPU_OP_SET_2_B  (UINT8_C(0xD0))
#define CPU_OP_SET_2_C  (UINT8_C(0xD1))
#define CPU_OP_SET_2_D  (UINT8_C(0xD2))
#define CPU_OP_SET_2_E  (UINT8_C(0xD3))
#define CPU_OP_SET_2_H  (UINT8_C(0xD4))
#define CPU_OP_SET_2_L  (UINT8_C(0xD5))
#define CPU_OP_SET_2_MEM_HL (UINT8_C(0xD6))
#define CPU_OP_SET_2_A  (UINT8_C(0xD7))
#define CPU_OP_SET_3_B  (UINT8_C(0xD8))
#define CPU_OP_SET_3_C  (UINT8_C(0xD9))
#define CPU_OP_SET_3_D  (UINT8_C(0xDA))
#define CPU_OP_SET_3_E  (UINT8_C(0xDB))
#define CPU_OP_SET_3_H  (UINT8_C(0xDC))
#define CPU_OP_SET_3_L  (UINT8_C(0xDD))
#define CPU_OP_SET_3_MEM_HL (UINT8_C(0xDE))
#define CPU_OP_SET_3_A  (UINT8_C(0xDF))
#define CPU_OP_SET_4_B  (UINT8_C(0xE0))
#define CPU_OP_SET_4_C  (UINT8_C(0xE1))
#define CPU_OP_SET_4_D  (UINT8_C(0xE2))
#define CPU_OP_SET_4_E  (UINT8_C(0xE3))
#define CPU_OP_SET_4_H  (UINT8_C(0xE4))
#define CPU_OP_SET_4_L  (UINT8_C(0xE5))
#define CPU_OP_SET_4_MEM_HL (UINT8_C(0xE6))
#define CPU_OP_SET_4_A  (UINT8_C(0xE7))
#define CPU_OP_SET_5_B  (UINT8_C(0xE8))
#define CPU_OP_SET_5_C  (UINT8_C(0xE9))
#define CPU_OP_SET_5_D  (UINT8_C(0xEA))
#define CPU_OP_SET_5_E  (UINT8_C(0xEB))
#define CPU_OP_SET_5_H  (UINT8_C(0xEC))
#define CPU_OP_SET_5_L  (UINT8_C(0xED))
#define CPU_OP_SET_5_MEM_HL (UINT8_C(0xEE))
#define CPU_OP_SET_5_A  (UINT8_C(0xEF))
#define CPU_OP_SET_6_B  (UINT8_C(0xF0))
#define CPU_OP_SET_6_C  (UINT8_C(0xF1))
#define CPU_OP_SET_6_D  (UINT8_C(0xF2))
#define CPU_OP_SET_6_E  (UINT8_C(0xF3))
#define CPU_OP_SET_6_H  (UINT8_C(0xF4))
#define CPU_OP_SET_6_L  (UINT8_C(0xF5))
#define CPU_OP_SET_6_MEM_HL (UINT8_C(0xF6))
#define CPU_OP_SET_6_A  (UINT8_C(0xF7))
#define CPU_OP_SET_7_B  (UINT8_C(0xF8))
#define CPU_OP_SET_7_C  (UINT8_C(0xF9))
#define CPU_OP_SET_7_D  (UINT8_C(0xFA))
#define CPU_OP_SET_7_E  (UINT8_C(0xFB))
#define CPU_OP_SET_7_H  (UINT8_C(0xFC))
#define CPU_OP_SET_7_L  (UINT8_C(0xFD))
#define CPU_OP_SET_7_MEM_HL (UINT8_C(0xFE))
#define CPU_OP_SET_7_A  (UINT8_C(0xFF))

#define CPU_ZERO_FLAG		(UINT8_C(1) << 7)
#define CPU_SUBTRACT_FLAG	(UINT8_C(1) << 6)
#define CPU_HALF_CARRY_FLAG	(UINT8_C(1) << 5)
#define CPU_CARRY_FLAG		(UINT8_C(1) << 4)

#define CPU_PWRUP_REG_PC (UINT16_C(0x0100))
#define CPU_PWRUP_REG_SP (UINT16_C(0xFFFE))

#define CPU_PWRUP_REG_AF (UINT16_C(0x01B0))
#define CPU_PWRUP_REG_BC (UINT16_C(0x0013))
#define CPU_PWRUP_REG_DE (UINT16_C(0x00D8))
#define CPU_PWRUP_REG_HL (UINT16_C(0x014D))

// clang-format on
