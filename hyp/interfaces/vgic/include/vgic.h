// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

const platform_mpidr_mapping_t *
vgic_get_mpidr_mapping(const vic_t *vic);

// Sync LRs for all vCPUs attached to a vic and wake up vCPU if necessary.
void
vgic_sync_all(vic_t *vic, bool wakeup);

#if VGIC_HAS_LPI && !GICV3_HAS_VLPI_V4_1
// Interfaces for asserting vLPIs and vLPI<->pLPI mappings on GICv3 with ITS.
//
// LPI support on GICv3 with ITS uses a range_tree for mapping vLPIs to pLPIs
// and another range_tree for storing delivery states for vLPIs. The interface
// functions here allow allocating new ranges in these range_trees and finding
// the same mappings when they are needed for LPI delivery.

// Allocate a dstate for a given vLPI. If necessary, allocates a range in the
// vLPI tree of the given vCPU.
vgic_delivery_state_t _Atomic *
vgic_allocate_vlpi_dstate(thread_t *target_vcpu, virq_t vlpi) REQUIRE_RCU_READ;

// Map the vLPI to an unused pLPI. If necessary, allocates a range in the
// pLPI->vLPI range tree.
// Returns the pLPI.
irq_result_t
vgic_map_vlpi(thread_t *vcpu, virq_t vlpi);

// Try to set the physical LPI affinity of a vCPU if it is uninitialized.
// Returns the old affinity value if it was already initialized or the new
// one otherwise.
cpu_index_t
vgic_try_set_vcpu_lpi_affinity(thread_t *vcpu);

// Get the physical affinity of a pLPI.
cpu_index_result_t
vgic_get_lpi_affinity(irq_t lpi);

// Find the delivery state for a given vLPI.
vgic_delivery_state_t _Atomic *
vgic_find_vlpi_dstate(thread_t *vcpu, virq_t vlpi) REQUIRE_RCU_READ;

// Find the pLPI which a vLPI is mapped to.
irq_result_t
vgic_get_lpi(thread_t *vcpu, virq_t vlpi);

// Assert a vLPI.
error_t
vlpi_assert(thread_t *vcpu, virq_t vlpi);

// Unmap a vLPI from a pLPI. If the corresponding pLPI range becomes unused,
// release it so that it can be used by other vCPUs. Returns the unmapped
// pLPI on success.
irq_result_t
vgic_unmap_vlpi(thread_t *vcpu, virq_t vlpi);

// Set the pending-sync flag for a given vLPI. This flag indicates that the
// state of the pLPI corresponding to the supplied vLPI needs to be
// invalidated.
void
vgic_set_vlpi_pending_sync(thread_t *vcpu, irq_t vlpi) REQUIRE_RCU_READ;

// Clear the pending-sync flag for a given vLPI.
void
vgic_clear_vlpi_pending_sync(thread_t *vcpu, irq_t vlpi) REQUIRE_RCU_READ;

// Get the next vLPI that has the pending-sync flag set.
void
vgic_vlpi_deliver_pending_sync(thread_t *vcpu, size_t max_lpi) REQUIRE_RCU_READ;

// Update the route of a vLPI.
void
vgic_vlpi_update_route(_Atomic vgic_delivery_state_t *dstate, irq_t vlpi,
		       thread_t *vcpu) REQUIRE_RCU_READ;

// Handler for vLPI delivery state update events. If the vLPI is mapped to a
// pLPI, the configuration of the pLPI is updated as well. Returns true if the
// next guest SYNC should refresh the redistributor cache (i.e., the pLPI state
// of the supplied vLPI needs to be invalidated).
bool
vgic_handle_dstate_update(thread_t *vcpu, irq_t vlpi,
			  vgic_delivery_state_t new_dstate,
			  vlpi_updated_flags_t	updated) REQUIRE_RCU_READ;

// Invalidate vLPI. Copies in the configuration value from guest memory and
// updates the corresponding delivery state. If no delivery state is passed, it
// is looked up based on the vLPI. Returns true if the next guest SYNC should
// refresh the redistributor cache.
bool_result_t
vgic_inv_vlpi(vic_t *vic, thread_t *vcpu, irq_t vlpi,
	      _Atomic vgic_delivery_state_t *dstate) REQUIRE_RCU_READ;

// Move a vLPI from one redistributor to another. If the vLPI is bound to a
// pLPI, performs the necessary re-mapping to a new pLPI range which is bound
// to the target vCPU.
irq_result_t
vgic_move_vlpi(thread_t *source_vcpu, thread_t *target_vcpu, irq_t vlpi,
	       bool purely_virtual) REQUIRE_RCU_READ;

// Clear the pending state of a vLPI.
error_t
vgic_clear_vlpi_pending(thread_t *vcpu, virq_t vlpi);

// Determine if an update to a delivery state necessitates a LR sync.
bool
vgic_dstate_is_sync_needed(vgic_delivery_state_t dstate,
			   vlpi_updated_flags_t	 updated);
#endif // VGIC_HAS_LPI && !GICV3_HAS_VLPI_V4_1
