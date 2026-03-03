// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
vpm_group_configure(vpm_group_t *vpm_group, vpm_group_option_flags_t flags);

error_t
vpm_attach(vpm_group_t *pg, thread_t *thread, index_t index);

error_t
vpm_bind_virq(vpm_group_t *vpm_group, vic_t *vic, virq_t virq);

void
vpm_unbind_virq(vpm_group_t *vpm_group) EXCLUDE_PREEMPT_DISABLED;

error_t
vpm_bind_power(vpm_group_t *vpm_group, power_t *power);

vpm_state_t
vpm_get_state(vpm_group_t *vpm_group);

error_t
vpm_system_wakeup(vpm_group_t *vpm_group);

void
vpm_set_threshold(vpm_group_t *vpm_group, uint64_t power_state);
