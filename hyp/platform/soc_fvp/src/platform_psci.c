// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#if !defined(UNIT_TESTS)
#include <platform_psci.h>
#include <qcbor.h>
#include <util.h>

#include "event_handlers.h"

bool
platform_psci_is_cpu_active(psci_cpu_state_t cpu_state)
{
	return (cpu_state == PSCI_CPU_STATE_ACTIVE);
}

bool
platform_psci_is_cpu_poweroff(psci_cpu_state_t cpu_state)
{
	(void)cpu_state;
	return false;
}

psci_cpu_state_t
platform_psci_get_cpu_state(psci_suspend_powerstate_t suspend_state)
{
	psci_cpu_state_t stateid =
		psci_suspend_powerstate_get_StateID(&suspend_state);

	return stateid;
}

void
platform_psci_set_cpu_state(psci_suspend_powerstate_t *suspend_state,
			    psci_cpu_state_t	       cpu_state)
{
	psci_suspend_powerstate_set_StateID(suspend_state, cpu_state);
}

psci_cpu_state_t
platform_psci_shallowest_cpu_state(psci_cpu_state_t state1,
				   psci_cpu_state_t state2)
{
	return (psci_cpu_state_t)(util_min(state1, state2));
}

psci_cpu_state_t
platform_psci_deepest_cpu_state(cpu_index_t cpu)
{
	(void)cpu;
	return PSCI_CPU_STATE_WFI;
}

psci_suspend_powerstate_stateid_t
platform_psci_deepest_cpu_level_stateid(cpu_index_t cpu)
{
	return platform_psci_deepest_cpu_state(cpu);
}

psci_suspend_powerstate_stateid_t
platform_psci_deepest_cluster_level_stateid(cpu_index_t cpu, uint8_t max_depth)
{
	(void)max_depth;
	return platform_psci_deepest_cpu_level_stateid(cpu);
}

psci_ret_t
platform_psci_suspend_state_validation(psci_suspend_powerstate_t suspend_state,
				       cpu_index_t cpu, psci_mode_t psci_mode)
{
	(void)suspend_state;
	(void)cpu;
	(void)psci_mode;

	// XXX
	return PSCI_RET_SUCCESS;
}

error_t
platform_psci_get_index_by_level(cpu_index_t cpu, uint32_t *start_idx,
				 uint32_t *children_counts, uint32_t level)
{
	error_t ret;

	if (level == 0) {
		(void)cpu;
		*start_idx	 = 0U;
		*children_counts = PLATFORM_MAX_CORES;
		ret		 = OK;
	} else {
		ret = ERROR_UNIMPLEMENTED;
	}

	return ret;
}

#endif
