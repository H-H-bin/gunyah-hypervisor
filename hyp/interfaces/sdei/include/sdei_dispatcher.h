// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Trigger the error event for a specific VM.
void
sdei_trigger_error_event(vic_t *vic, sdei_error_flags_t flags);

// Trigger the error event for all non-PVM VMs.
void
sdei_trigger_error_event_all_non_pvm(sdei_error_flags_t flags);

// Trigger the error event for the PVM.
void
sdei_trigger_error_event_pvm(sdei_error_flags_t flags);

// Check whether we are in a system-wide grace period.
bool
sdei_in_system_grace_period(void);

// Start the system-wide grace period.
bool
sdei_start_system_grace_period(const char *msg, abort_reason_t reason,
			       sdei_error_flags_t flags);

// Start a VM-specific grace period.
bool
sdei_start_vm_grace_period(vic_t *vic, sdei_error_flags_t flags);

// Get the current busy state
sdei_busy_state_t
sdei_get_busy_state(void);

// Check whether there are any busy VMs.
bool
sdei_is_busy(sdei_busy_state_t state, sdei_busy_select_t which);

error_t
sdei_dispatcher_virq_bind_shared(virq_source_t *source, vic_t *vic, virq_t virq,
				 virq_trigger_t trigger);

bool_result_t
sdei_dispatcher_virq_assert(const virq_source_t *source, vic_t *vic)
	REQUIRE_RCU_READ;
