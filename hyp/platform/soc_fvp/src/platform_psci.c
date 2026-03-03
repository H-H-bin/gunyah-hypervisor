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
platform_psci_is_node_active(index_t lpm_state, index_t level)
{
	bool flag = true;
	if (level == 0U) {
		flag = (lpm_state == 0U);
	}

	return flag;
}

bool
platform_psci_is_node_poweroff(index_t lpm_state, index_t level)
{
	(void)level;
	(void)lpm_state;

	// Powerdown not supported in fvp, it always goes into WFI
	return false;
}

index_t
platform_psci_get_lpm_state(psci_suspend_powerstate_t suspend_state,
			    index_t		      level)
{
	(void)level;
	(void)suspend_state;

	return 1U;
}

void
platform_psci_set_lpm_state(psci_suspend_powerstate_t *suspend_state,
			    index_t lpm_state, index_t level)
{
	(void)level;
	(void)lpm_state;
	(void)suspend_state;
}

index_t
platform_psci_shallowest_lpm_state(index_t state1, index_t state2)
{
	return util_min(state1, state2);
}

index_t
platform_psci_deepest_lpm_state(index_t cpu, index_t level)
{
	(void)cpu;
	(void)level;

	// Since FVP does not care about cpu suspend states, we will use 0 as
	// active and non-zero as suspended.
	return PSCI_CPU_STATE_WFI;
}

psci_suspend_powerstate_stateid_t
platform_psci_deepest_level_stateid(cpu_index_t cpu, index_t level)
{
	(void)cpu;
	(void)level;

	// Since FVP does not care about cpu suspend states, we'll use 0 as
	// active and non-zero as suspended.
	return platform_psci_deepest_lpm_state(cpu, level);
}

psci_ret_t
platform_psci_suspend_state_validation(psci_suspend_powerstate_t suspend_state,
				       cpu_index_t cpu, psci_mode_t psci_mode)
{
	(void)suspend_state;
	(void)cpu;
	(void)psci_mode;

	// FVP does not care about suspend states since it only goes to WFI.
	return PSCI_RET_SUCCESS;
}

// Returns the cluster indices
uint32_t
platform_psci_get_level_index(cpu_index_t cpu, index_t level)
{
	(void)cpu;
	(void)level;
	return 0U;
}

error_t
platform_psci_get_index_by_level(cpu_index_t cpu, uint32_t *start_idx,
				 uint32_t *children_counts, uint32_t level)
{
	(void)cpu;
	(void)level;
	*start_idx	 = 0U;
	*children_counts = PLATFORM_MAX_CORES;

	return OK;
}

#endif
