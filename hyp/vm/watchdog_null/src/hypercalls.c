// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypcall_def.h>

error_t
hypercall_watchdog_attach_vcpu(cap_id_t watchdog_cap, cap_id_t vcpu_cap)
{
	(void)watchdog_cap;
	(void)vcpu_cap;
	return ERROR_UNIMPLEMENTED;
}

error_t
hypercall_watchdog_bind_virq(cap_id_t watchdog_cap, cap_id_t vic_cap,
			     virq_t			  virq,
			     watchdog_bind_option_flags_t bind_options)
{
	(void)watchdog_cap;
	(void)vic_cap;
	(void)virq;
	(void)bind_options;
	return ERROR_UNIMPLEMENTED;
}

error_t
hypercall_watchdog_unbind_virq(cap_id_t			    watchdog_cap,
			       watchdog_bind_option_flags_t unbind_options)
{
	(void)watchdog_cap;
	(void)unbind_options;
	return ERROR_UNIMPLEMENTED;
}

error_t
hypercall_watchdog_configure(cap_id_t		     watchdog_cap,
			     watchdog_option_flags_t watchdog_options)
{
	(void)watchdog_cap;
	(void)watchdog_options;
	return ERROR_UNIMPLEMENTED;
}

error_t
hypercall_watchdog_manage(cap_id_t watchdog_cap, watchdog_manage_op_t operation)
{
	(void)watchdog_cap;
	(void)operation;
	return ERROR_UNIMPLEMENTED;
}
