// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Device memory fences
//
// The atomic_thread_fence() builtin only generates a fence for CPU threads,
// which means the compiler is allowed to use a DMB ISH instruction. For device
// accesses this is not good enough; we need a DMB SY.
//
// Note that the instructions here are the same for AArch64 and ARMv8 AArch32.
#define atomic_device_fence(p)                                                 \
	do {                                                                   \
		switch (p) {                                                   \
		case memory_order_relaxed:                                     \
			atomic_thread_fence(memory_order_relaxed);             \
			break;                                                 \
		case memory_order_acquire:                                     \
		case memory_order_consume:                                     \
			/* Order all reads before DMB with reads and writes */ \
			/* after it. */                                        \
			__asm__ volatile("dmb ld" ::: "memory");               \
			break;                                                 \
		case memory_order_release:                                     \
		case memory_order_acq_rel:                                     \
		case memory_order_seq_cst:                                     \
		default:                                                       \
			/* Order all reads and writes before DMB with reads */ \
			/* and writes after it. */                             \
			__asm__ volatile("dmb sy" ::: "memory");               \
			break;                                                 \
		}                                                              \
	} while (0)

bool
atomic_compare_exchange_uint32_ll_sc_weak(_Atomic uint32_t *obj,
					  uint32_t *expected, uint32_t desired);

bool
atomic_compare_exchange_uint64_ll_sc_weak(_Atomic uint64_t *obj,
					  uint64_t *expected, uint64_t desired);

#define atomic_compare_exchange_ll_sc_weak(obj, expected, desired)              \
	_Generic((obj),                                                         \
		_Atomic uint32_t *: atomic_compare_exchange_uint32_ll_sc_weak,  \
		_Atomic uint64_t *: atomic_compare_exchange_uint64_ll_sc_weak)( \
		(obj), (expected), (desired))
