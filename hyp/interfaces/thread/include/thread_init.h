// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Thread restore functions used only by the boot code.
//
// These are similar to setjmp/longjmp and switch from the CPU's boot stack to
// the thread's stack and pick up from where thread_freeze() or
// thread_arch_init_context() saved context.

// Start the CPU's idle thread with initial stack and pc.
noreturn void
thread_boot_set_idle(void) REQUIRE_PREEMPT_DISABLED;

// Resume the CPU's frozen current thread stack and call-frame.
noreturn void
thread_boot_restore_frozen(void) REQUIRE_PREEMPT_DISABLED;
