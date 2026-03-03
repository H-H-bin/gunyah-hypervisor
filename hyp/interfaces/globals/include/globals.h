// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

const global_options_t *
globals_get_options(void);

void
globals_set_options(global_options_t set);

void
globals_clear_options(global_options_t clear);

global_cpu_options_t
globals_get_cpu_options(void) REQUIRE_PREEMPT_DISABLED;

void
globals_set_cpu_options_by_index(global_cpu_options_t set, cpu_index_t cpu);

void
globals_set_cpu_options(global_cpu_options_t set) REQUIRE_PREEMPT_DISABLED;

void
globals_clear_cpu_options_by_index(global_cpu_options_t clear, cpu_index_t cpu);

void
globals_clear_cpu_options(global_cpu_options_t clear) REQUIRE_PREEMPT_DISABLED;
