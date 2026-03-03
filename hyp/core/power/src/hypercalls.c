// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(INTERFACE_VCPU) && INTERFACE_VCPU

#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <log.h>
#include <object.h>
#include <power.h>
#include <thread.h>
#include <trace.h>

error_t
hypercall_power_system_suspend(cap_id_t system_power_cap)
{
	error_t ret;

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	cspace_t *cspace = cspace_get_self();

	power_ptr_result_t p = cspace_lookup_power(
		cspace, system_power_cap, CAP_RIGHTS_POWER_SYSTEM_SUSPEND);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}

	TRACE_AND_LOG(DEBUG, INFO, "power_system_suspend: from VM {:d}",
		      thread_get_self()->addrspace->vmid);

	// Initiate the system suspend to power off all the cores and put the
	// system into suspend state (e.g. PSCI_SYSTEM_SUSPEND / ACPI S2, S3
	// etc).
	ret = power_system_suspend();

	TRACE_AND_LOG(ERROR, WARN, "power_system_suspend: ret {:d}",
		      (register_t)ret);

	object_put_power(p.r);
out:
#else  // PLATFORM_ENABLE_SYSTEM_SUSPEND
	(void)system_power_cap;
	ret = ERROR_UNIMPLEMENTED;
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND
	return ret;
}

error_t
hypercall_power_cpu_suspend(cap_id_t system_power_cap, uint64_t power_state)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	power_ptr_result_t p = cspace_lookup_power(
		cspace, system_power_cap, CAP_RIGHTS_POWER_CPU_SUSPEND);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}

	ret = power_cpu_suspend(
		psci_suspend_powerstate_cast((uint32_t)power_state));

	// FIXME: QC Gunyah issue #271
	if (p.e == OK) {
		object_put_power(p.r);
	}
out:
	return ret;
}

#else  // INTERFACE_VCPU
extern int unused;
#endif // INTERFACE_VCPU
