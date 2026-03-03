// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// ITS functions.

// Map a vLPI to physical event and target it to the redistributor of gicr_vcpu.
error_t
vgic_its_map(vgic_its_t *vgic_its, platform_msi_device_t *device,
	     platform_msi_event_id_t event, thread_t *gicr_vcpu, virq_t vlpi,
	     bool sync) REQUIRE_RCU_READ;

// Sync ITS for all attached vCPUs.
void
vgic_its_sync_all(vgic_its_t *vgic_its);

// Sync ITS for a particular vCPU.
void
vgic_its_sync(vgic_its_t *vgic_its, cpu_index_t cpu_index);
