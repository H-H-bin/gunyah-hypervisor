// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <bitmap.h>
#include <rcu.h>
#include <util.h>
#include <vgic_its.h>

#include "gicv3_its.h"

error_t
vgic_its_map(vgic_its_t *vgic_its, platform_msi_device_t *device,
	     platform_msi_event_id_t event, thread_t *gicr_vcpu, virq_t vlpi,
	     bool sync)
{
	(void)sync;
	platform_msi_controller_id_t phys_index =
		atomic_load_consume(&vgic_its->phys_index);

	bitmap_set(device->vgic_its_mapped_events, event);

	platform_msi_id_t msi_id = platform_msi_id_default();
	platform_msi_id_set_event_id(&msi_id, event);
	platform_msi_id_set_device_id(&msi_id, device->device_id);
	platform_msi_id_set_its_index(&msi_id, phys_index);

	error_t err = gicv3_its_vmap(msi_id, gicr_vcpu, vlpi);
	assert(err == OK);

	return err;
}

void
vgic_its_sync_all(vgic_its_t *vgic_its)
{
	platform_msi_controller_id_t phys_index =
		atomic_load_consume(&vgic_its->phys_index);

	// Synchronise the ITS command queue with every attached VCPU.
	rcu_read_start();
	count_result_t sync = count_result_error(ERROR_IDLE);
	vic_t	      *vic  = atomic_load_consume(&vgic_its->owner);
	for (index_t i = 0U; i < vic->gicr_count; i++) {
		thread_t *vcpu = atomic_load_consume(&vic->gicr_vcpus[i]);
		if (vcpu != NULL) {
			sync = gicv3_its_vsync(phys_index, vcpu);
			assert(sync.e == OK);
		}
	}
	rcu_read_finish();

	// Wait for the last queued VSYNC (if any) to complete, which guarantees
	// that the DISCARD commands have taken effect for all attached vPEs.
	if (sync.e == OK) {
		(void)gicv3_its_wait(phys_index, sync.r);
	}
}
