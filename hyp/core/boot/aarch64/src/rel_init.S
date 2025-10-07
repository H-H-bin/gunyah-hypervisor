// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hypconstants.h>

#include <asm/asm_defs.inc>

// boot_rel_fixup(Elf_Dyn *dyni, Elf_Addr addr_offset, Elf_Addr rel_offset)
// X0: ElfDyn *dyni
// X1: Elf_Addr add_offset
// X2: Elf_addr rel_offset
function boot_rel_fixup, section=nosection
	bti	c

	// x8:	dyn[DT_REL]
	// X9:	dyn[DT_RELA]
	// X10:	dyn[DT_RELSZ]
	// x11:	dyn[DT_RELASZ]
	mov	x8, xzr
	mov	x9, xzr
	mov	x10, xzr
	mov	x11, xzr

	mov	x12, x0
	// Loop through all the dyni->d_tag, find DT_REL, DT_RELA, DT_RELSZ and
	// DT_RELASZ, and assign the corresponding dyn to dyni->d_un.d_ptr
local dyni_loop:
	ldr	x14, [x12, #OFS_ASM_ELF64_DYN_D_UN]
	ldr	x13, [x12, #OFS_ASM_ELF64_DYN_D_TAG]
	cbz	x13, LOCAL(dyni_loop_done)
	cmp	x13, #ASM_ELF_DT_REL
	b.eq	LOCAL(dt_rel)
	cmp	x13, #ASM_ELF_DT_RELA
	b.eq	LOCAL(dt_rela)
	cmp	x13, #ASM_ELF_DT_RELSZ
	b.eq	LOCAL(dt_relsz)
	cmp	x13, #ASM_ELF_DT_RELASZ
	b.ne	LOCAL(dyni_loop_continue)
	// Fall through to dt_relasz

local dt_relasz:
	mov	x11, x14
	b	LOCAL(dyni_loop_continue)
local dt_rel:
	mov	x8, x14
	b	LOCAL(dyni_loop_continue)
local dt_rela:
	mov	x9, x14
	b	LOCAL(dyni_loop_continue)
local dt_relsz:
	mov	x10, x14
	// Fall through

local dyni_loop_continue:
	add	x12, x12, #ASM_ELF64_DYN_SIZE
	b	LOCAL(dyni_loop)

local dyni_loop_done:


        // Elf_Rel  *rel = (Elf_Rel *)(dyn_rel + addr_offset)
	// X8 will become rel
	add	x8, x8, x1		// rel, dyn_rel, addr_offset
        // rel_end = (Elf_Rel *)(dyn_rel + addr_offset + dyn_relsz)
	// X10 will become rel_end
	add	x10, x8, x10		// rel_end, rel, dyn_relsz

local rel_loop:
	// Iterate through rela until rela_end
	cmp	x8, x10
	b.hs	LOCAL(rel_loop_done)	// Exit loop if rel >= rel_end

	// Relocate only if ARCH_CAN_PATCH(rel->r_info)
	ldr	x12, [x8, #OFS_ASM_ELF64_REL_R_INFO]	// rel->r_info
	lsr	x14, x12, 32		// R_SYM(rel->r_info)
	cbnz	x14, LOCAL(rel_loop_continue)
	ands	x13, x12, #0x7fffffff	// R_TYPE(rel->r_info)
	b.eq	LOCAL(rel_loop_relocate)	// == R_AARCH64_NONE
	cmp	x13, #ASM_ELF_R_AARCH64_NULL
	b.eq	LOCAL(rel_loop_relocate)	// == R_AARCH64_NULL
	cmp	x13, #ASM_ELF_R_AARCH64_RELATIVE
	b.ne	LOCAL(rel_loop_continue)	// != R_AARCH64_RELATIVE
	// Fall through

local rel_loop_relocate:
	// Elf_Addr *r = (Elf_Addr *)(rel->r_offset + addr_offset)
	ldr	x12, [x8, #OFS_ASM_ELF64_REL_R_OFFSET] // rel->r_offset
	ldr	x14, [x12, x1, lsl #0]	// *r
	// *r += rel_offset
	add	x14, x14, x2
	str	x14, [x12, x1, lsl #0]
	// Fall through

local rel_loop_continue:
	add	x8, x8, #ASM_ELF64_REL_SIZE
	b	LOCAL(rel_loop)

local rel_loop_done:


        // Elf_Rela  *rela = (Elf_Rela *)(dyn_rela + addr_offset)
	// X9 will become rela
	add	x9, x9, x1		// rel, dyn_rela, addr_offset
        // rela_end = (Elf_Rela *)(dyn_rela + addr_offset + dyn_relasz)
	// X11 will become rela_end
	add	x11, x9, x11		// rela_end, rela, dyn_relasz

local rela_loop:
	// Iterate through rel until rel_end
	cmp	x9, x11
	b.hs	LOCAL(rela_loop_done)		// Exit loop if rela >= rela_end

	// Relocate only if ARCH_CAN_PATCH(rela->r_info)
	ldr	x12, [x9, #OFS_ASM_ELF64_RELA_R_INFO]	// rela->r_info
	lsr	x14, x12, 32		// R_SYM(rela->r_info)
	cbnz	x14, LOCAL(rela_loop_continue)
	ands	x13, x12, #0x7fffffff	// R_TYPE(rela->r_info)
	b.eq	LOCAL(rela_loop_relocate)	// == R_AARCH64_NONE
	cmp	x13, #256
	b.eq	LOCAL(rela_loop_relocate)	// == R_AARCH64_NULL
	cmp	x13, #1027
	b.ne	LOCAL(rela_loop_continue)	// != R_AARCH64_RELATIVE
	// Fall through

local rela_loop_relocate:
	// Elf_Addr *r = (Elf_Addr *)(rela->r_offset + addr_offset)
	ldr	x12, [x9, #OFS_ASM_ELF64_RELA_R_OFFSET]	// rela->r_offset
	// *r = rel_offset + (Elf_Addr)rela->r_addend
	ldr	x14, [x9, #OFS_ASM_ELF64_RELA_R_ADDEND]	// rela->r_addend
	add	x14, x14, x2
	str	x14, [x12, x1, lsl #0]
	// Fall through

local rela_loop_continue:
	add	x9, x9, #ASM_ELF64_RELA_SIZE
	b	LOCAL(rela_loop)

local rela_loop_done:

	ret
function_end boot_rel_fixup
