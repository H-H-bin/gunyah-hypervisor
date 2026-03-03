// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Barrier and wait operations.
//
// These functions should not be used unless the event interface and other
// compiler barriers are unsuitable.

// Yield the CPU to another hardware thread (SMT) / or let a simulator give CPU
// time to something else.
static inline void
asm_yield(void)
{
	__asm__ volatile("yield");
}

// Put the CPU into low power state until at most the system counter reaches
// timeout ticks. This may wake-up early due to other CPU events.
static inline void
asm_wait_timeout(ticks_t timeout)
{
#if defined(ARCH_ARM_FEAT_WFxT)
	__asm__ volatile("wfet %0" ::"r"(timeout));
#else
	(void)timeout;
	asm_yield();
#endif
}

// Ensure that writes to CPU configuration registers and other similar events
// are visible to code executing on the CPU. For example, use this between
// enabling access to floating point registers and actually using those
// registers.
static inline void
asm_context_sync_fence(void)
{
	__asm__ volatile("isb" ::: "memory");
}

static inline void
asm_context_sync_ordered(asm_ordering_dummy_t *order)
{
	__asm__ volatile("isb" : "+m"(*(order)));
}

// Ensure that all memory accesses, all TLB, branch predictor and cache
// maintenance operations are completed. For example, this can be used for MMIO
// between a write to a non-early-completion device register and a subsequent
// memory writes.
// Note, these are full-system barriers and are stronger than what is needed
// for TLB or cache maintenance sync requirements.
static inline void
asm_data_sync_fence(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}

static inline void
asm_data_sync_ordered(asm_ordering_dummy_t *order)
{
	__asm__ volatile("dsb" : "+m"(*(order)));
}

static inline void
asm_data_store_sync_fence(void)
{
	__asm__ volatile("dsb st" ::: "memory");
}

// Outer Shareable data store. Usefull for devices in Outer Domain, like SMMUs.
// It doesn't need to sync accesses to non-coherent IO devices
static inline void
asm_data_outer_shareable_store_sync_fence(void)
{
	__asm__ volatile("dsb oshst" ::: "memory");
}

// The asm_ordering variable is used as an artificial dependency to order
// different individual asm statements with respect to each other in a way that
// is lighter weight than a full "memory" clobber.
extern asm_ordering_dummy_t asm_ordering;
