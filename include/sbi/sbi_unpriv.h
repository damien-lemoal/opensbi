/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#ifndef __SBI_UNPRIV_H__
#define __SBI_UNPRIV_H__

#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bits.h>
#include <sbi/sbi_types.h>

#define DECLARE_UNPRIVILEGED_LOAD_FUNCTION(type, insn)			\
static inline type load_##type(const type *addr, ulong mepc)		\
{									\
	register ulong __mepc asm ("a2") = mepc;			\
	register ulong __mstatus asm ("a3");				\
	type val;							\
	asm ("csrrs %0, mstatus, %3\n"					\
		#insn " %1, %2\n"					\
		"csrw mstatus, %0"					\
	: "+&r" (__mstatus), "=&r" (val)				\
	: "m" (*addr), "r" (MSTATUS_MPRV), "r" (__mepc));		\
	return val;							\
}

#define DECLARE_UNPRIVILEGED_STORE_FUNCTION(type, insn)			\
static inline void store_##type(type *addr, type val, ulong mepc)	\
{									\
	register ulong __mepc asm ("a2") = mepc;			\
	register ulong __mstatus asm ("a3");				\
	asm volatile ("csrrs %0, mstatus, %3\n"				\
		#insn " %1, %2\n"					\
		"csrw mstatus, %0"					\
	: "+&r" (__mstatus)						\
	: "r" (val), "m" (*addr), "r" (MSTATUS_MPRV), "r" (__mepc));	\
}

DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u8, lbu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u16, lhu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s8, lb)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s16, lh)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s32, lw)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u8, sb)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u16, sh)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u32, sw)
#if __riscv_xlen == 64
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u32, lwu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u64, ld)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u64, sd)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(ulong, ld)
#else
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u32, lw)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(ulong, lw)

static inline u64 load_u64(const u64 *addr, ulong mepc)
{
	return load_u32((u32 *)addr, mepc)
		+ ((u64)load_u32((u32 *)addr + 1, mepc) << 32);
}

static inline void store_u64(u64 *addr, u64 val, ulong mepc)
{
	store_u32((u32 *)addr, val, mepc);
	store_u32((u32 *)addr + 1, val >> 32, mepc);
}
#endif

static inline ulong get_insn(ulong mepc, ulong *mstatus)
{
	register ulong __mepc asm ("a2") = mepc;
	register ulong __mstatus asm ("a3");
	ulong val;
#ifndef __riscv_compressed
	asm ("csrrs %[mstatus], mstatus, %[mprv]\n"
		STR(LWU) " %[insn], (%[addr])\n"
		"csrw mstatus, %[mstatus]"
		: [mstatus] "+&r" (__mstatus), [insn] "=&r" (val)
		: [mprv] "r" (MSTATUS_MPRV | MSTATUS_MXR), [addr] "r" (__mepc));
#else
	ulong rvc_mask = 3, tmp;
	asm ("csrrs %[mstatus], mstatus, %[mprv]\n"
		"and %[tmp], %[addr], 2\n"
		"bnez %[tmp], 1f\n"
		STR(LWU) " %[insn], (%[addr])\n"
		"and %[tmp], %[insn], %[rvc_mask]\n"
		"beq %[tmp], %[rvc_mask], 2f\n"
		"sll %[insn], %[insn], %[xlen_minus_16]\n"
		"srl %[insn], %[insn], %[xlen_minus_16]\n"
		"j 2f\n"
		"1:\n"
		"lhu %[insn], (%[addr])\n"
		"and %[tmp], %[insn], %[rvc_mask]\n"
		"bne %[tmp], %[rvc_mask], 2f\n"
		"lhu %[tmp], 2(%[addr])\n"
		"sll %[tmp], %[tmp], 16\n"
		"add %[insn], %[insn], %[tmp]\n"
		"2: csrw mstatus, %[mstatus]"
	: [mstatus] "+&r" (__mstatus), [insn] "=&r" (val), [tmp] "=&r" (tmp)
	: [mprv] "r" (MSTATUS_MPRV | MSTATUS_MXR), [addr] "r" (__mepc),
	[rvc_mask] "r" (rvc_mask), [xlen_minus_16] "i" (__riscv_xlen - 16));
#endif
	*mstatus = __mstatus;
	return val;
}

#endif
