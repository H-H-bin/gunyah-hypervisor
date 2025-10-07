// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hypconstants.h>

#include <asm/asm_defs.inc>

	.section .text.boot
function __entry_el2, section=nosection
	// Enable I-cache
	abs64	x10, 0x30401800
	msr	SCTLR_EL2, x10

	// Get a 192-bit random seed into x0..x2 from the TRNG if possible
	abs64	x0, 0xC4000053 // SMCCC ID of TRNG_RND
	mov	x1, #192 // Max entropy from one 64-bit call
	smc	#0
	cbnz	x0, 1f
	mov	x0, x3
	mov	x3, #0
	b	2f

1:
	// As a last resort in case TRNG is not implemented, read the cycle
	// counter and the physical counter, which may be somewhat random in
	// the low bits.
	mrs	x0, PMCCNTR_EL0
	mrs	x1, CNTPCT_EL0
2:

	b	aarch64_init
function_end __entry_el2

function soc_fvp_entry_cold_secondary global nosection
	b	aarch64_secondary_init
function_end soc_fvp_entry_cold_secondary

function soc_fvp_entry_warm
	b	aarch64_warm_init
function_end soc_fvp_entry_warm
