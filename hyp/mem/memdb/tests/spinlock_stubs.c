// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Stub spinlock implementation for memdb bitmap host testing

#include <hyptypes.h>
#include <stdbool.h>
#include <stdint.h>

void
spinlock_init(spinlock_t *lock)
{
	// Stub implementation - no-op for host testing
	(void)lock;
}

bool
spinlock_acquire(spinlock_t *lock)
{
	// Stub implementation - always succeeds for host testing
	(void)lock;
	return true;
}

bool
spinlock_try_acquire(spinlock_t *lock)
{
	// Stub implementation - always succeeds for host testing
	(void)lock;
	return true;
}

void
spinlock_release(spinlock_t *lock)
{
	// Stub implementation - no-op for host testing
	(void)lock;
}
