// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
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

#include <asm/nospec_checks.h>

#include "virtio_mmio.h"

error_t
hypercall_virtio_mmio_frontend_bind_virq(cap_id_t virtio_backend_cap,
					 cap_id_t vic_cap, virq_t virq)
{
	error_t	  err;
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
	error_t	  err;
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

	virtio_mmio_frontend_unbind_virq(
		&virtio_backend->virtio.frontend_data.mmio);
	err = OK;

out_ref:
	object_put_virtio_backend(virtio_backend);
out:
	return err;
}
