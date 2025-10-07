// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <atomic.h>

bool
atomic_compare_exchange_uint32_ll_sc_weak(_Atomic uint32_t *obj,
					  uint32_t *expected, uint32_t desired)
{
	bool	 fail = true;
	uint32_t tmp;

	// Word size ldxr / stxr weak atomic exchange operation
	__asm__ volatile(
		"ldxr	%w[tmp], %[obj];"
		"cmp	%w[tmp], %w[exp];"
		"b.ne	1f;"
		"stxr	%w[ret], %w[new], %[obj];"
		"1:;"
		: [obj] "+m"(*(obj)), [ret] "+&r"(fail), [tmp] "=&r"(tmp)
		: [new] "r"(desired), [exp] "r"(*(expected))
		: "cc");
	*(expected) = tmp;

	return !fail;
}

bool
atomic_compare_exchange_uint64_ll_sc_weak(_Atomic uint64_t *obj,
					  uint64_t *expected, uint64_t desired)
{
	bool	 fail = true;
	uint64_t tmp;

	// Register size ldxr / stxr weak atomic exchange operation
	__asm__ volatile(
		"ldxr	%[tmp], %[obj];"
		"cmp	%[tmp], %[exp];"
		"b.ne	1f;"
		"stxr	%w[ret], %[new], %[obj];"
		"1:;"
		: [obj] "+m"(*(obj)), [ret] "+&r"(fail), [tmp] "=&r"(tmp)
		: [new] "r"(desired), [exp] "r"(*(expected))
		: "cc");
	*(expected) = tmp;

	return !fail;
}
