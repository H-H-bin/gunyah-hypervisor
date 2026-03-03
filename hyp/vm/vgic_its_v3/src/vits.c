// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <compiler.h>
#include <platform_cpu.h>
#include <rcu.h>
#include <vgic.h>
#include <vgic_its.h>

#include "gicv3_its.h"

error_t
vgic_its_map(vgic_its_t *vgic_its, platform_msi_device_t *device,
	     platform_msi_event_id_t event, thread_t *gicr_vcpu, virq_t vlpi,
	     bool sync)
{
	platform_msi_controller_id_t phys_index;
	error_t			     err;
	cpu_index_t		     lpi_affinity;

	assert(vgic_its != NULL);

	phys_index = atomic_load_consume(&vgic_its->phys_index);

	// If the LPI affinity of the vCPU is not set yet, set it to its
	// current affinity and use it for the entire lifetime of the vCPU.
	lpi_affinity = atomic_load_relaxed(&gicr_vcpu->vgic_lpi_affinity);
	if (lpi_affinity == PLATFORM_MAX_CORES) {
		lpi_affinity = vgic_try_set_vcpu_lpi_affinity(gicr_vcpu);
	}

	// Find the plpi corresponding to vlpi, and if it doesn't
	// exist, map lpi to vlpi in translation table.
	irq_result_t lpi_r = vgic_map_vlpi(gicr_vcpu, vlpi);
	if (lpi_r.e != OK) {
		err = lpi_r.e;
		goto out;
	}

	platform_msi_id_t msi_id = platform_msi_id_default();
	platform_msi_id_set_event_id(&msi_id, event);
	platform_msi_id_set_device_id(&msi_id, device->device_id);
	platform_msi_id_set_its_index(&msi_id, phys_index);

	err = gicv3_its_map(msi_id, lpi_affinity, lpi_r.r);

	if (sync) {
		vgic_its_sync(vgic_its, lpi_affinity);
	}
out:

	return err;
}

void
vgic_its_sync_all(vgic_its_t *vgic_its)
{
	platform_msi_controller_id_t phys_index;
	count_result_t		     sync = count_result_error(ERROR_IDLE);
	vic_t			    *vic;

	assert(vgic_its != NULL);

	phys_index = atomic_load_consume(&vgic_its->phys_index);

	// Synchronise the ITS command queue with every attached VCPU.
	rcu_read_start();

	vic = atomic_load_consume(&vgic_its->owner);
	for (index_t i = 0U; i < vic->gicr_count; i++) {
		thread_t *vcpu = atomic_load_consume(&vic->gicr_vcpus[i]);
		if (vcpu != NULL) {
			cpu_index_t affinity =
				atomic_load_relaxed(&vcpu->vgic_lpi_affinity);

			if (!platform_cpu_functional(affinity)) {
				continue;
			}

			sync = gicv3_its_sync(phys_index, affinity);
			assert(sync.e == OK);
		}
	}

	rcu_read_finish();

	// Wait for the last queued SYNC (if any) to complete, which guarantees
	// that the DISCARD commands have taken effect for all CPUs.
	if (OK == sync.e) {
		(void)gicv3_its_wait(phys_index, sync.r);
	}
}

void
vgic_its_sync(vgic_its_t *vgic_its, cpu_index_t cpu_index)
{
	platform_msi_controller_id_t phys_index;
	count_result_t		     sync;

	assert(vgic_its != NULL);

	phys_index = atomic_load_consume(&vgic_its->phys_index);
	sync	   = gicv3_its_sync(phys_index, cpu_index);

	// Wait for the SYNC to complete.
	if (OK == sync.e) {
		(void)gicv3_its_wait(phys_index, sync.r);
	}
}
