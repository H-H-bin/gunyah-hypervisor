// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition.h>
#include <spinlock.h>
#include <watchdog.h>

#include "watchdog.h"

error_t
hypercall_watchdog_attach_vcpu(cap_id_t watchdog_cap, cap_id_t vcpu_cap)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	watchdog_ptr_result_t watchdog_r = cspace_lookup_watchdog(
		cspace, watchdog_cap, CAP_RIGHTS_WATCHDOG_ATTACH_VCPU);
	if (compiler_unexpected(watchdog_r.e != OK)) {
		err = watchdog_r.e;
		goto out;
	}
	watchdog_t *wdog = watchdog_r.r;

	thread_ptr_result_t thread_r = cspace_lookup_thread_any(
		cspace, vcpu_cap, CAP_RIGHTS_THREAD_OBJECT_ACTIVATE);
	if (compiler_unexpected(thread_r.e != OK)) {
		err = thread_r.e;
		goto out_release_wdt;
	}
	thread_t *vcpu = thread_r.r;

	if (compiler_unexpected(vcpu->kind != THREAD_KIND_VCPU)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out_release_vcpu;
	}

	spinlock_acquire(&vcpu->header.lock);
	if (atomic_load_relaxed(&vcpu->header.state) == OBJECT_STATE_INIT) {
		err = watchdog_attach(wdog, vcpu);
	} else {
		err = ERROR_OBJECT_STATE;
	}
	spinlock_release(&vcpu->header.lock);

out_release_vcpu:
	object_put_thread(vcpu);
out_release_wdt:
	object_put_watchdog(wdog);
out:
	return err;
}

error_t
hypercall_watchdog_bind_virq(cap_id_t watchdog_cap, cap_id_t vic_cap,
			     virq_t			  virq,
			     watchdog_bind_option_flags_t bind_options)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	// Check for unknown bind option flags
	if (!watchdog_bind_option_flags_is_clean(bind_options)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	watchdog_ptr_result_t p = cspace_lookup_watchdog(
		cspace, watchdog_cap, CAP_RIGHTS_WATCHDOG_BIND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	watchdog_t *watchdog = p.r;

	vic_ptr_result_t v =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(v.e != OK)) {
		err = v.e;
		goto out_release_wdt;
	}
	vic_t *vic = v.r;

	watchdog_virq_type_t virq_type;

	// For backwards compatibility, we assume that WATCHDOG_VIRQ_TYPE_BARK
	// is set as virq_type when the bite_virq flag is 0.
	if (!watchdog_bind_option_flags_get_bite_virq(&bind_options)) {
		virq_type = WATCHDOG_VIRQ_TYPE_BARK;
	} else {
		virq_type = WATCHDOG_VIRQ_TYPE_BITE;
	}

	err = watchdog_bind_virq(watchdog, vic, virq, virq_type);

	object_put_vic(vic);
out_release_wdt:
	object_put_watchdog(watchdog);
out:
	return err;
}

error_t
hypercall_watchdog_unbind_virq(cap_id_t			    watchdog_cap,
			       watchdog_bind_option_flags_t unbind_options)
{
	error_t	  err	 = OK;
	cspace_t *cspace = cspace_get_self();

	// Check for unknown bind option flags
	if (!watchdog_bind_option_flags_is_clean(unbind_options)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	watchdog_ptr_result_t p = cspace_lookup_watchdog(
		cspace, watchdog_cap, CAP_RIGHTS_WATCHDOG_BIND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	watchdog_t *watchdog = p.r;

	watchdog_virq_type_t virq_type;

	// For backwards compatibility, we assume that WATCHDOG_VIRQ_TYPE_BARK
	// is set as virq_type when the bite_virq flag is 0.
	if (!watchdog_bind_option_flags_get_bite_virq(&unbind_options)) {
		virq_type = WATCHDOG_VIRQ_TYPE_BARK;
	} else {
		virq_type = WATCHDOG_VIRQ_TYPE_BITE;
	}

	err = watchdog_unbind_virq(watchdog, virq_type);

	object_put_watchdog(watchdog);
out:
	return err;
}

error_t
hypercall_watchdog_configure(cap_id_t		     watchdog_cap,
			     watchdog_option_flags_t watchdog_options)
{
	error_t	      err;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, watchdog_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE,
		&type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out;
	}
	if (type != OBJECT_TYPE_WATCHDOG) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto out_watchdog_release;
	}

	watchdog_t *watchdog = o.r.watchdog;

	spinlock_acquire(&watchdog->header.lock);

	if (atomic_load_relaxed(&watchdog->header.state) == OBJECT_STATE_INIT) {
		err = watchdog_configure(watchdog, watchdog_options);
	} else {
		err = ERROR_OBJECT_STATE;
	}

	spinlock_release(&watchdog->header.lock);
out_watchdog_release:
	object_put(type, o.r);
out:
	return err;
}

error_t
hypercall_watchdog_manage(cap_id_t watchdog_cap, watchdog_manage_op_t operation)
{
	error_t	  err	 = OK;
	cspace_t *cspace = cspace_get_self();

	watchdog_ptr_result_t p = cspace_lookup_watchdog(
		cspace, watchdog_cap, CAP_RIGHTS_WATCHDOG_MANAGE);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	watchdog_t *watchdog = p.r;

	switch (operation) {
	case WATCHDOG_MANAGE_OP_FREEZE:
		err = watchdog_freeze(watchdog, false);
		break;
	case WATCHDOG_MANAGE_OP_FREEZE_AND_RESET:
		err = watchdog_freeze(watchdog, true);
		break;
	case WATCHDOG_MANAGE_OP_UNFREEZE:
		err = watchdog_unfreeze(watchdog);
		break;
	default:
		err = ERROR_ARGUMENT_INVALID;
		break;
	}

	object_put_watchdog(watchdog);
out:
	return err;
}
