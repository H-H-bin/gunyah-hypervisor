// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#define timer_t hyp_timer_t
#include <hyptypes.h>
#undef timer_t

#define register_t std_register_t
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#undef register_t

#include "mem_manager.h"
#include "stub.h"

// Assume test is for 64 bit
// Fake physical memory region up to 48 bit, each time, the allocated physical
// address will not return, so when it's used up all 48 bit physical memory,
// allocation pmem will be failed
#define MAX_PMEM_ADDR (0xFFFFFFFFFFFF)

// assume test is for 4K
#define P4K_SHIFT (12)
#define P4K_MASK  ((1 << 12) - 1)

#define CUR_MASK  P4K_MASK
#define CUR_SHIFT P4K_SHIFT
#define CUR_PG_SZ (1 << CUR_SHIFT)

#define ROUND_UP_SZ(sz) ((sz + CUR_PG_SZ - 1) & (~CUR_MASK))

#if !defined(NDEBUG)
noreturn void
assert_failed(const char *file, int line, const char *func, const char *err)
{
	(void)file;
	(void)line;
	(void)func;
	(void)err;
	abort();
}
#endif

void
panic(const char *msg)
{
	printf("PANIC: %s\n", msg);
	exit(-1);
}

void
rcu_read_start(void)
{
}

void
rcu_read_finish(void)
{
}

void
spinlock_init(spinlock_t *unused)
{
	(void)unused;
}

void
spinlock_acquire(spinlock_t *unused)
{
	(void)unused;
}

void
spinlock_acquire_nopreempt(spinlock_t *unused)
{
	(void)unused;
}

void
spinlock_release(spinlock_t *unused)
{
	(void)unused;
}

void
spinlock_release_nopreempt(spinlock_t *unused)
{
	(void)unused;
}
