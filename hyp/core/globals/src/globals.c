// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <atomic.h>
#include <cpulocal.h>
#include <globals.h>
#include <spinlock.h>

#include "event_handlers.h"

static global_options_t global_options;
static spinlock_t	global_options_lock;

CPULOCAL_DECLARE_STATIC(_Atomic global_cpu_options_t, global_options);

void
globals_handle_boot_cold_init(void)
{
	spinlock_init(&global_options_lock);
}

const global_options_t *
globals_get_options(void)
{
	return &global_options;
}

void
globals_set_options(global_options_t set)
{
	spinlock_acquire(&global_options_lock);
	global_options = global_options_union(global_options, set);
	spinlock_release(&global_options_lock);
}

void
globals_clear_options(global_options_t clear)
{
	spinlock_acquire(&global_options_lock);
	global_options = global_options_difference(global_options, clear);
	spinlock_release(&global_options_lock);
}

global_cpu_options_t
globals_get_cpu_options(void)
{
	return atomic_load_relaxed(&CPULOCAL(global_options));
}

void
globals_set_cpu_options_by_index(global_cpu_options_t set, cpu_index_t cpu)
{
	(void)global_cpu_options_atomic_union(&CPULOCAL_BY_INDEX(global_options,
								 cpu),
					      set, memory_order_relaxed);
}

void
globals_set_cpu_options(global_cpu_options_t set)
{
	globals_set_cpu_options_by_index(set, cpulocal_get_index());
}

void
globals_clear_cpu_options_by_index(global_cpu_options_t clear, cpu_index_t cpu)
{
	(void)global_cpu_options_atomic_difference(
		&CPULOCAL_BY_INDEX(global_options, cpu), clear,
		memory_order_relaxed);
}

void
globals_clear_cpu_options(global_cpu_options_t clear)
{
	globals_clear_cpu_options_by_index(clear, cpulocal_get_index());
}
