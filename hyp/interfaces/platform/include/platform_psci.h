// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause
// Checks that the suspend state is valid
//
// Gets the cluster mask for the respective cpu
register_t
platform_cluster_mask(cpu_index_t cpu);
// This function checks that the pcpu supports the cpu and cluster level
// state specified. If the specified state is not valid, then it returns
// PSCI_RET_INVALID_PARAMETERS.
psci_ret_t
platform_psci_suspend_state_validation(psci_suspend_powerstate_t suspend_state,
				       cpu_index_t cpu, psci_mode_t psci_mode);

// Gets the lpm state from the suspend power state
index_t
platform_psci_get_lpm_state(psci_suspend_powerstate_t suspend_state,
			    index_t		      level);

// Sets lpm state to the stateid of the suspend power state
void
platform_psci_set_lpm_state(psci_suspend_powerstate_t *suspend_state,
			    index_t lpm_state, index_t level);

// Returns true if node state is in active state
bool
platform_psci_is_node_active(index_t lpm_state, index_t level);

// Returns true if node is in power collapse state
bool
platform_psci_is_node_poweroff(index_t lpm_state, index_t level);

// Returns the nodes indices
uint32_t
platform_psci_get_level_index(cpu_index_t cpu, index_t level);

// Returns the start index of children in hierarchy/counts based on level and
// cpu
error_t
platform_psci_get_index_by_level(cpu_index_t cpu, uint32_t *start_idx,
				 uint32_t *children_counts, uint32_t level);

// Returns the deepest suspend state at particular level supported by the system
index_t
platform_psci_deepest_lpm_state(index_t cpu, index_t level);

// Returns that shallowest state between two lpm states
index_t
platform_psci_shallowest_lpm_state(index_t state1, index_t state2);

// Returns the deepest respective-level suspend state id supported by a cpu
psci_suspend_powerstate_stateid_t
platform_psci_deepest_level_stateid(cpu_index_t cpu, index_t level);

#if !defined(PSCI_AFFINITY_LEVELS_NOT_SUPPORTED) ||                            \
	!PSCI_AFFINITY_LEVELS_NOT_SUPPORTED

// Returns the suspend level of the last cpu
index_t
platform_psci_get_last_cpu_level(psci_suspend_powerstate_t suspend_state);

// Sets the suspend level of the last cpu
void
platform_psci_set_last_cpu_level(psci_suspend_powerstate_t *suspend_state,
				 index_t		    last_cpu);

#endif

bool
platform_psci_check_vpm_threshold(psci_suspend_powerstate_t suspend_state,
				  psci_suspend_powerstate_t suspend_state_th);
