// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>
#include <string.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition.h>
#include <spinlock.h>
#include <util.h>
#include <virq.h>
#include <virtio.h>

#include <events/virtio_backend.h>

#include <asm/nospec_checks.h>

#include "virtio_mmio.h"

error_t
hypercall_virtio_mmio_configure(cap_id_t virtio_backend_cap,
				cap_id_t memextent_cap, count_t vqs_num,
				virtio_backend_option_flags_t flags,
				virtio_device_type_t	      device_type)
{
	error_t	      err;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	virtio_backend_option_flags_t allowed_flags =
		virtio_backend_option_flags_default();
	virtio_backend_option_flags_set_valid_device_type(&allowed_flags, true);
	virtio_backend_option_flags_set_sync_reset(&allowed_flags, true);

	if (!virtio_backend_option_flags_is_clean(flags) ||
	    !virtio_backend_option_flags_is_empty(
		    virtio_backend_option_flags_difference(flags,
							   allowed_flags))) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	memextent_ptr_result_t m = cspace_lookup_memextent(
		cspace, memextent_cap, CAP_RIGHTS_MEMEXTENT_ATTACH);
	if (compiler_unexpected(m.e != OK)) {
		err = m.e;
		goto out;
	}

	memextent_t *memextent = m.r;

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, virtio_backend_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE,
		&type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out_memextent_release;
	}
	if (type != OBJECT_TYPE_VIRTIO_BACKEND) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto out_virtio_backend_release;
	}

	virtio_backend_t *virtio_backend = o.r.virtio_backend;

	spinlock_acquire(&virtio_backend->header.lock);

	if (atomic_load_relaxed(&virtio_backend->header.state) !=
	    OBJECT_STATE_INIT) {
		err = ERROR_OBJECT_STATE;
		goto out_locked;
	}

	virtio_device_type_t checked_device_type;

	if (virtio_backend_option_flags_get_valid_device_type(&flags)) {
		if (trigger_virtio_backend_valid_device_type_event(
			    device_type)) {
			checked_device_type = device_type;
		} else {
			err = ERROR_ARGUMENT_INVALID;
			goto out_locked;
		}
	} else {
		checked_device_type = VIRTIO_DEVICE_TYPE_INVALID;
	}

	virtio_backend->flags = flags;

	// Temporarily stash the configuration in the virtio struct. We will
	// read it back and pass it to virtio_startup() in the activate handler.
	virtio_backend->virtio.device_type = checked_device_type;
	virtio_backend->virtio.vqs	   = vqs_num;

	if (virtio_backend->virtio.config_cache_me != NULL) {
		object_put_memextent(virtio_backend->virtio.config_cache_me);
	}
	virtio_backend->virtio.config_cache_me =
		object_get_memextent_additional(memextent);

	err = OK;

out_locked:
	spinlock_release(&virtio_backend->header.lock);
out_virtio_backend_release:
	object_put(type, o.r);
out_memextent_release:
	object_put_memextent(memextent);
out:
	return err;
}

error_t
hypercall_virtio_mmio_frontend_bind_virq(cap_id_t virtio_backend_cap,
					 cap_id_t vic_cap, virq_t virq)
{
	error_t	  err	 = OK;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_BIND_MMIO_FRONTEND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	if (virtio_backend->virtio.transport_type !=
	    VIRTIO_TRANSPORT_TYPE_MMIO) {
		err = ERROR_OBJECT_CONFIG;
		goto out_ref;
	}

	vic_ptr_result_t v =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(v.e != OK)) {
		err = v.e;
		goto out_ref;
	}
	vic_t *vic = v.r;

	err = virtio_mmio_frontend_bind_virq(
		&virtio_backend->virtio.frontend_data.mmio, vic, virq);

	object_put_vic(vic);
out_ref:
	object_put_virtio_backend(virtio_backend);
out:
	return err;
}

error_t
hypercall_virtio_mmio_frontend_unbind_virq(cap_id_t virtio_backend_cap)
{
	error_t	  err	 = OK;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_BIND_MMIO_FRONTEND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_mmio_frontend_unbind_virq(
		&virtio_backend->virtio.frontend_data.mmio);

	object_put_virtio_backend(virtio_backend);
out:
	return err;
}
