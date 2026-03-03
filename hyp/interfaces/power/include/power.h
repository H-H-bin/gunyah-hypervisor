// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Obtain a pointer to the array of CPU states.
//
// This array may be modified by a remote CPU at any time. No synchronisation is
// available. Therefore this must only be used for debug purposes, e.g. to
// expose the array in crash dumps.
const cpu_power_state_array_t *
power_get_cpu_states_for_debug(void);

// Vote to keep a CPU powered on.
//
// This may return ERROR_FAILURE if the specified CPU cannot be powered on for
// platform-specific reasons (such as the CPU being disabled by fuses or a
// hardware failure while powering it on), or ERROR_ARGUMENT_INVALID if the CPU
// index is out of range.
error_t
power_vote_cpu_on(cpu_index_t cpu);

// Revoke an earlier vote to keep a CPU powered on.
//
// This must only be called after a successful power_vote_cpu_on(). Calling it
// on the only powered-on CPU may leave the system in an unrecoverable state;
// it is the caller's responsibility to avoid this.
void
power_vote_cpu_off(cpu_index_t cpu);

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
// Initiate System Suspend Sequence
error_t
power_system_suspend(void);
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND

error_t
power_cpu_suspend(psci_suspend_powerstate_t power_state);
