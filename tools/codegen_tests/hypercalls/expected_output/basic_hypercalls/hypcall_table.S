// Automatically generated. Do not modify.
//
// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hypconstants.h>
#include <asm/asm_defs.inc>

#if defined(ARCH_ARM_FEAT_BTI)
#define BTI_J	bti j
#define BTI_NOP	nop
#define HYPERCALL_ALIGN	0x10
#else
#define BTI_J
#define BTI_NOP
#define HYPERCALL_ALIGN	0x8
#endif

#if (ARCH_SANITIZE_STACK_SIZE % 16) != 0
#error ARCH_SANITIZE_STACK_SIZE alignment
#endif

function hypercall_table align=16
0:
	BTI_J
	adr	x9, hypercall_test_hypercall_1__c_wrapper
	b	vcpu_hypercall_entry_x1_x1
	BTI_NOP	// Align hypercall
.if . - 0b != HYPERCALL_ALIGN
.err // bad alignment
.endif
1:
	BTI_J
	adr	x9, hypercall_test_hypercall_2__c_wrapper
	b	vcpu_hypercall_entry_x3_x1
	BTI_NOP	// Align hypercall
.if . - 1b != HYPERCALL_ALIGN
.err // bad alignment
.endif
2:
	BTI_J
	adr	x9, hypercall_test_hypercall_3__c_wrapper
	b	vcpu_hypercall_entry_x1_slow
	BTI_NOP	// Align hypercall
.if . - 2b != HYPERCALL_ALIGN
.err // bad alignment
.endif
function_end hypercall_table

function vcpu_hypercall_entry_x0_x0 align=64
	mov	x0, xzr
function_chain vcpu_hypercall_entry_x0_x0, vcpu_hypercall_entry_x1_x0
	mov	x1, xzr
function_chain vcpu_hypercall_entry_x1_x0, vcpu_hypercall_entry_x2_x0
	mov	x2, xzr
function_chain vcpu_hypercall_entry_x2_x0, vcpu_hypercall_entry_x3_x0
	mov	x3, xzr
function_chain vcpu_hypercall_entry_x3_x0, vcpu_hypercall_entry_x4_x0
	mov	x4, xzr
function_chain vcpu_hypercall_entry_x4_x0, vcpu_hypercall_entry_x5_x0
	mov	x5, xzr
function_chain vcpu_hypercall_entry_x5_x0, vcpu_hypercall_entry_x6_x0
	mov	x6, xzr
function_chain vcpu_hypercall_entry_x6_x0, vcpu_hypercall_entry_x7_x0
	mov	x7, xzr
function_chain vcpu_hypercall_entry_x7_x0, vcpu_hypercall_entry_x8_x0
	blr	x9
	b	vcpu_hypercall_return_sanitize_x0
function_end vcpu_hypercall_entry_x8_x0

function vcpu_hypercall_entry_x0_x1 align=64
	mov	x0, xzr
function_chain vcpu_hypercall_entry_x0_x1, vcpu_hypercall_entry_x1_x1
	mov	x1, xzr
function_chain vcpu_hypercall_entry_x1_x1, vcpu_hypercall_entry_x2_x1
	mov	x2, xzr
function_chain vcpu_hypercall_entry_x2_x1, vcpu_hypercall_entry_x3_x1
	mov	x3, xzr
function_chain vcpu_hypercall_entry_x3_x1, vcpu_hypercall_entry_x4_x1
	mov	x4, xzr
function_chain vcpu_hypercall_entry_x4_x1, vcpu_hypercall_entry_x5_x1
	mov	x5, xzr
function_chain vcpu_hypercall_entry_x5_x1, vcpu_hypercall_entry_x6_x1
	mov	x6, xzr
function_chain vcpu_hypercall_entry_x6_x1, vcpu_hypercall_entry_x7_x1
	mov	x7, xzr
function_chain vcpu_hypercall_entry_x7_x1, vcpu_hypercall_entry_x8_x1
	blr	x9
	b	vcpu_hypercall_return_sanitize_x1
function_end vcpu_hypercall_entry_x8_x1

function vcpu_hypercall_entry_x0_x2 align=64
	mov	x0, xzr
function_chain vcpu_hypercall_entry_x0_x2, vcpu_hypercall_entry_x1_x2
	mov	x1, xzr
function_chain vcpu_hypercall_entry_x1_x2, vcpu_hypercall_entry_x2_x2
	mov	x2, xzr
function_chain vcpu_hypercall_entry_x2_x2, vcpu_hypercall_entry_x3_x2
	mov	x3, xzr
function_chain vcpu_hypercall_entry_x3_x2, vcpu_hypercall_entry_x4_x2
	mov	x4, xzr
function_chain vcpu_hypercall_entry_x4_x2, vcpu_hypercall_entry_x5_x2
	mov	x5, xzr
function_chain vcpu_hypercall_entry_x5_x2, vcpu_hypercall_entry_x6_x2
	mov	x6, xzr
function_chain vcpu_hypercall_entry_x6_x2, vcpu_hypercall_entry_x7_x2
	mov	x7, xzr
function_chain vcpu_hypercall_entry_x7_x2, vcpu_hypercall_entry_x8_x2
	blr	x9
	b	vcpu_hypercall_return_sanitize_x2
function_end vcpu_hypercall_entry_x8_x2

function vcpu_hypercall_entry_x0_slow align=64
	mov	x0, xzr
function_chain vcpu_hypercall_entry_x0_slow, vcpu_hypercall_entry_x1_slow
	mov	x1, xzr
function_chain vcpu_hypercall_entry_x1_slow, vcpu_hypercall_entry_x2_slow
	mov	x2, xzr
function_chain vcpu_hypercall_entry_x2_slow, vcpu_hypercall_entry_x3_slow
	mov	x3, xzr
function_chain vcpu_hypercall_entry_x3_slow, vcpu_hypercall_entry_x4_slow
	mov	x4, xzr
function_chain vcpu_hypercall_entry_x4_slow, vcpu_hypercall_entry_x5_slow
	mov	x5, xzr
function_chain vcpu_hypercall_entry_x5_slow, vcpu_hypercall_entry_x6_slow
	mov	x6, xzr
function_chain vcpu_hypercall_entry_x6_slow, vcpu_hypercall_entry_x7_slow
	mov	x7, xzr
function_chain vcpu_hypercall_entry_x7_slow, vcpu_hypercall_entry_x8_slow
	// Allocate stack space for the maximum allowed return size
	sub	sp, sp, 64
	mov	x8, sp

	// Zero-initialise the allocated stack space, since any unused returns
	// in the struct may be left uninitialised by the compiler and returned
	// to the caller.
	stp	xzr, xzr, [sp, #0]
	stp	xzr, xzr, [sp, #16]
	stp	xzr, xzr, [sp, #32]
	stp	xzr, xzr, [sp, #48]

	// Jump to the C handler
	blr	x9

	// Load x0..x7 from the return structure on the stack
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	add	sp, sp, 64

	b	vcpu_hypercall_return_sanitize_x8
function_end vcpu_hypercall_entry_x8_slow

function vcpu_hypercall_entry_x0_sensitive align=64
	mov	x0, xzr
function_chain vcpu_hypercall_entry_x0_sensitive, vcpu_hypercall_entry_x1_sensitive
	mov	x1, xzr
function_chain vcpu_hypercall_entry_x1_sensitive, vcpu_hypercall_entry_x2_sensitive
	mov	x2, xzr
function_chain vcpu_hypercall_entry_x2_sensitive, vcpu_hypercall_entry_x3_sensitive
	mov	x3, xzr
function_chain vcpu_hypercall_entry_x3_sensitive, vcpu_hypercall_entry_x4_sensitive
	mov	x4, xzr
function_chain vcpu_hypercall_entry_x4_sensitive, vcpu_hypercall_entry_x5_sensitive
	mov	x5, xzr
function_chain vcpu_hypercall_entry_x5_sensitive, vcpu_hypercall_entry_x6_sensitive
	mov	x6, xzr
function_chain vcpu_hypercall_entry_x6_sensitive, vcpu_hypercall_entry_x7_sensitive
	mov	x7, xzr
function_chain vcpu_hypercall_entry_x7_sensitive, vcpu_hypercall_entry_x8_sensitive
	// Allocate stack space for the maximum allowed return size
	sub	sp, sp, 64
	mov	x8, sp

	// Zero-initialise the allocated stack space, since any unused returns
	// in the struct may be left uninitialised by the compiler and returned
	// to the caller.
	stp	xzr, xzr, [sp, #0]
	stp	xzr, xzr, [sp, #16]
	stp	xzr, xzr, [sp, #32]
	stp	xzr, xzr, [sp, #48]

	// Jump to the C handler
	blr	x9

	// Load x0..x7 from the return structure on the stack
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	add	sp, sp, 64

	// Zero any sensitive stack values
	sub	x9, sp, ARCH_SANITIZE_STACK_SIZE
	mov	x10, sp
local zero_stack:
	stp	xzr, xzr, [x9], 16
	cmp	x9, x10
	bne	LOCAL(zero_stack)

	b	vcpu_hypercall_return_sanitize_x8
function_end vcpu_hypercall_entry_x8_sensitive
