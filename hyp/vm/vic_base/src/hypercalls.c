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

#include <events/vic.h>

#include "vic_base.h"

error_t
hypercall_vic_bind_virq(cap_id_t irq_obj_cap, cap_id_t vic_cap, virq_t virq,
			index_t index)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vic_ptr_result_t vic_r =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(vic_r.e != OK)) {
		err = vic_r.e;
		goto out;
	}

	err = trigger_vic_bind_virq_event(irq_obj_cap, vic_r.r, index, virq);
	object_put_vic(vic_r.r);
out:
	return err;
}

error_t
hypercall_vic_unbind_virq(cap_id_t irq_obj_cap, index_t index)
{
	error_t err = trigger_vic_unbind_virq_event(irq_obj_cap, index);

	return err;
}

error_t
hypercall_vic_configure(cap_id_t vic_cap, count_t max_vcpus, count_t max_virqs,
			vic_option_flags_t vic_options, count_t max_msis)
{
	error_t	      err;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, vic_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE, &type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out_released;
	}
	if (type != OBJECT_TYPE_VIC) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto out_unlocked;
	}
	vic_t *vic = o.r.vic;

	if (vic_option_flags_get_res0_0(&vic_options) != 0U) {
		err = ERROR_ARGUMENT_INVALID;
		goto out_unlocked;
	}

	// For backwards compatibility, treat the max_msis argument as 0 if the
	// caller has not set the flag indicating that it is valid.
	if (!vic_option_flags_get_max_msis_valid(&vic_options)) {
		max_msis = 0U;
	}

	spinlock_acquire(&vic->header.lock);
	if (atomic_load_relaxed(&vic->header.state) == OBJECT_STATE_INIT) {
		err = vic_configure(vic, max_vcpus, max_virqs, max_msis,
				    !vic_option_flags_get_disable_default_addr(
					    &vic_options));
	} else {
		err = ERROR_OBJECT_STATE;
	}
	spinlock_release(&vic->header.lock);

out_unlocked:
	object_put(type, o.r);
out_released:

	return err;
}

error_t
hypercall_vic_attach_vcpu(cap_id_t vic_cap, cap_id_t vcpu_cap, index_t index)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vic_ptr_result_t vic_r =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_ATTACH_VCPU);
	if (compiler_unexpected(vic_r.e != OK)) {
		err = vic_r.e;
		goto out;
	}

	object_type_t	    type;
	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, vcpu_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE, &type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out_release_vic;
	}
	if (type != OBJECT_TYPE_THREAD) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto out_release_vcpu;
	}
	thread_t *thread = o.r.thread;

	spinlock_acquire(&thread->header.lock);
	if (atomic_load_relaxed(&thread->header.state) == OBJECT_STATE_INIT) {
		err = vic_attach_vcpu(vic_r.r, thread, index);
	} else {
		err = ERROR_OBJECT_STATE;
	}
	spinlock_release(&thread->header.lock);

out_release_vcpu:
	object_put(type, o.r);
out_release_vic:
	object_put_vic(vic_r.r);
out:
	return err;
}

error_t
hypercall_vic_bind_msi_source(cap_id_t vic_cap, cap_id_t msi_source_cap)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vic_ptr_result_t vic_r =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(vic_r.e != OK)) {
		err = vic_r.e;
		goto out;
	}

	err = trigger_vic_bind_msi_source_event(vic_r.r, msi_source_cap);

	object_put_vic(vic_r.r);
out:
	return err;
}
