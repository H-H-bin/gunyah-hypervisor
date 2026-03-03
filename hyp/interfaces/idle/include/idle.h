// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Idle thread and related APIs.

// Get the current CPU's idle thread. Must only be called in a cpulocal
// critical section.
thread_t *
idle_thread(void) REQUIRE_PREEMPT_DISABLED;

// Get the specified CPU's idle thread.
thread_t *
idle_thread_for(cpu_index_t cpu_index);

// True when running in the current CPU's idle thread.
bool
idle_is_current(void) REQUIRE_PREEMPT_DISABLED;

bool
idle_yield(void) REQUIRE_PREEMPT_DISABLED;

extern opaque_lock_t idle_blocked;

// Prepare to block EL2 execution on the calling CPU.
//
// The block may take the form of a halting instruction such as WFI or WFE, or a
// call into a higher exception level that is sensitive to wakeup events, such
// as an SMCCC interruptible call.
void
idle_block_start(void) REQUIRE_PREEMPT_DISABLED ACQUIRE_LOCK(idle_blocked)
EXCLUDE_RCU_READ;

// Clean up after resuming execution after blocking on the calling CPU.
void
idle_block_finish(void) REQUIRE_PREEMPT_DISABLED RELEASE_LOCK(idle_blocked)
EXCLUDE_RCU_READ;

// Handle a wakeup event received during idle.
idle_state_t
idle_wakeup(void) REQUIRE_PREEMPT_DISABLED;
