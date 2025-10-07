// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <asm/asm_defs.inc>
#include <asm/cpu.h>
#include <asm/panic.inc>


#define CLEAR_MIN_BITS	10

#if (CPU_L1D_LINE_BITS > CLEAR_MIN_BITS) || (CPU_DCZVA_BITS > CLEAR_MIN_BITS)
#error Unsupported
#endif

#if CPU_L1D_LINE_BITS > CPU_DCZVA_BITS
#define CLEAR_SCALE	(CLEAR_MIN_BITS - CPU_L1D_LINE_BITS)
#define CLEAR_BITS	CPU_L1D_LINE_BITS
#define CLEAR_DELTA	(CPU_L1D_LINE_BITS - CPU_DCZVA_BITS)
#else
#define CLEAR_SCALE	(CLEAR_MIN_BITS - CPU_DCZVA_BITS)
#define CLEAR_BITS	CPU_DCZVA_BITS
#define CLEAR_DELTA	(CPU_DCZVA_BITS - CPU_L1D_LINE_BITS)
#endif

// This attempts to keep the number of repeated DC instructions in the loops to
// approximately four for code size. If the log2 delta between line-size and
// ZVA size is too large however, one (larger) op is used with as many of the
// smaller ops as required to cover the same VA region.
#if CLEAR_DELTA >= 2
#define LOOP_SCALE 0
#elif CLEAR_SCALE > 2
#define LOOP_SCALE 2 - CLEAR_DELTA
#else
#define LOOP_SCALE CLEAR_SCALE
#endif

#define LOOP_BITS (CLEAR_BITS + LOOP_SCALE)

// memory clear
// arguments:
//   x0 - start address, must be CPU_DCZVA_BITS aligned
//   x1 - size, must be 1KiB aligned
function memclear
	cbz	x1, LOCAL(ret_memclear)
	tst	x1, 1023
	b.ne	LOCAL(bad_size)

1:
	subs	x1, x1, 1 << LOOP_BITS
.rept (1 << (LOOP_BITS - CPU_DCZVA_BITS))
	dc	zva, x0
	add	x0, x0, 1 << CPU_DCZVA_BITS
.endr
	b.ne	1b

local ret_memclear:
	ret

function_end memclear

// memory clear and cache clean
// arguments:
//   x0 - start address, must be CPU_DCZVA_BITS aligned
//   x1 - size, must be 1KiB aligned
function memclear_and_clean
	cbz	x1, LOCAL(ret_memclear_and_clean)
	tst	x1, 1023
	b.ne	LOCAL(bad_size)

	mov	x2, x0
1:
.rept (1 << (LOOP_BITS - CPU_DCZVA_BITS))
	dc	zva, x0
	add	x0, x0, 1 << CPU_DCZVA_BITS
.endr
	subs	x1, x1, 1 << LOOP_BITS
.rept (1 << (LOOP_BITS - CPU_L1D_LINE_BITS))
	dc	cvac, x2
	add	x2, x2, 1 << CPU_L1D_LINE_BITS
.endr
	b.ne	1b
	dsb	ish

local ret_memclear_and_clean:
	ret
function_end memclear_and_clean

local bad_size:
	panic "bad size"
