// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcall_def.h>
#include <hypcontainers.h>
#include <hyprights.h>

#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <spinlock.h>

#include "event_handlers.h"

error_t
arm_vm_smmuv2_handle_addrspace_attach_vdma(addrspace_t *addrspace,
					   cap_id_t	vdma_device_cap,
					   index_t	index)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vsmmuv2_ptr_result_t vsmmu_r = cspace_lookup_vsmmuv2(
		cspace, vdma_device_cap, CAP_RIGHTS_VSMMUV2_ATTACH_ADDRSPACE);
	if (vsmmu_r.e != OK) {
		err = vsmmu_r.e;
		goto out;
	}
	vsmmuv2_t *vsmmu = vsmmu_r.r;

	if (index != 0U) {
		err = ERROR_ARGUMENT_INVALID;
		goto out_ref;
	}

	spinlock_acquire(&vsmmu->lock);

	if (vsmmu->addrspace != NULL) {
		err = ERROR_BUSY;
		goto out_locked;
	}

	vsmmu->addrspace = object_get_addrspace_additional(addrspace);
	err		 = OK;
out_locked:
	spinlock_release(&vsmmu->lock);
out_ref:
	object_put_vsmmuv2(vsmmu);
out:
	return err;
}

void
arm_vm_smmuv2_handle_object_cleanup_vsmmuv2(vsmmuv2_t *vsmmuv2)
{
	if (vsmmuv2->addrspace != NULL) {
		object_put_addrspace(vsmmuv2->addrspace);
	}
}
