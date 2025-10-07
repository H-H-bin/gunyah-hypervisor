// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Low-level interface to the GICv3 ITS driver.

#if GICV3_HAS_ITS

// ITS init, called by the base GIC driver's boot_cold_init handler.
void
gicv3_its_init(gits_t *(*regs)[PLATFORM_GITS_COUNT],
	       count_t common_lpi_affinity);

// ITS per-CPU init, called by the base GIC driver's boot_cpu_cold_init handler.
void
gicv3_its_init_cpu(cpu_index_t cpu, gicr_t *gicr, paddr_t gicr_phys,
		   index_t gicr_pn);

// Reserve an unused device ID and allocate a specified number of events for it.
// This is used by the virtual ITS code to temporarily map LPIs so it can
// operate on them directly. It can only be called once per physical ITS.
platform_msi_device_id_result_t
gicv3_its_reserve_unused_device(platform_msi_controller_id_t its,
				platform_msi_event_id_t	     max_event);

// Query whether an ITS command has completed.
//
// Given a sequence number returned by a command submission, this routine
// returns true if that command is known to have completed in the ITS.
bool_result_t
gicv3_its_is_complete(platform_msi_controller_id_t its, count_t cmd_seq);

// Wait for an ITS command to complete.
//
// Given a sequence number returned by a command submission, this routine waits
// for the command to complete. This is a busy-wait, so it should be deferred
// for as long as possible after submitting the command.
//
// Only commands that return a count_result_t can be waited for; other commands
// must be followed by an explicit sync command to be guaranteed to be
// observable on any specific redistributor or VPE.
error_t
gicv3_its_wait(platform_msi_controller_id_t its, count_t cmd_seq);

// Command submission functions.
//
// Note that the size of the command buffer is limited, so any of these
// functions may spin waiting for the ITS to process a command.

// Physical MSI to physical LPI mapping.
//
// The specified device must be listed in platform_irq_msi_devices, with an
// event limit greater than the specified event.
//
// The effects might not be observed until a subsequent SYNC executes for the
// affected CPU.
error_t
gicv3_its_map(platform_msi_id_t msi_id, cpu_index_t cpu, irq_t lpi);

error_t
gicv3_its_move(platform_msi_id_t msi_id, cpu_index_t cpu);

// LPI cache invalidation.
//
// This can be done either by mapped event ID, or for all physical LPIs targeted
// to a specified CPU. The former works for virtual LPIs. There is a separate
// function to invalidate all virtual LPIs on a vPE, below.
//
// In most cases it is more useful to invalidate by LPI number; there are GICR
// registers to do this, but they only work on GICv4.1, or GICv3 with no ITS.
// Functions to access those registers are declared in gicv3.h.
error_t
gicv3_its_invalidate(platform_msi_id_t msi_id);

error_t
gicv3_its_invalidate_all(platform_msi_controller_id_t its, cpu_index_t cpu);

// Physical MSI manual assertion and unmapping (note: these work for any MSI
// that is mapped, regardless of whether the mapped LPI is physical or virtual)
//
// The effects might not be observed until a subsequent SYNC executes for the
// affected CPU, or a subsequent VSYNC executes for the affected VCPU.
error_t
gicv3_its_assert(platform_msi_id_t msi_id);

error_t
gicv3_its_clear(platform_msi_id_t msi_id);

error_t
gicv3_its_discard(platform_msi_id_t msi_id);

// Physical IRQ mapping synchronisation.
//
// This must be called, and its completion waited on, to guarantee that any map,
// move, or discard command previously submitted to the specified ITS has taken
// effect on the specified physical CPU's redistributor.
count_result_t
gicv3_its_sync(platform_msi_controller_id_t its, cpu_index_t cpu);

#if GICV3_HAS_VLPI

// Virtual PE management.
//
// These functions are called when activating a VCPU, enabling VLPIs, disabling
// VLPIs, or deactivating a VCPU, respectively. They must be called for all
// VCPUs that have support for VLPIs and are attached to a VGIC with at least
// one VGITS, or else which require hardware VSGI delivery (on GICv4.1).
//
// The caller must serialise and balance calls to these four functions. That is,
// for each VCPU, there must be exactly one call to _activate, followed by an
// optional pair of calls to _map (when LPIs are enabled) and _unmap (when the
// VCPU object is deactivated with LPIs enabled), followed by a call to _cleanup
// (when the VCPU object is freed). Also, the call to _unmap will return an ITS
// command sequence number; the caller must wait until both an RCU grace period
// has elapsed and the returned command number has completed on the ITS with
// index 0 prior to calling _cleanup.
//
// Currently, no more than one pair of _map / _unmap calls is permitted for a
// VCPU object, which means that the virtual GICR_CTLR.EnableLPIs bit must
// become RES1 after it is set to 1 (as permitted by the GICv3 specification).
//
// Doorbell interrupts are registered by this driver, but must be handled by
// the caller. VCPU migration is also handled by this driver if necessary; i.e.
// if migration is supported by the scheduler, GICv4.1 is implemented, and the
// origin and destination GICRs are in different CommonLPIAff groups.
error_t
gicv3_its_vpe_activate(thread_t *vcpu);

error_t
gicv3_its_vpe_map(thread_t *vcpu, count_t virq_bits, paddr_t config_table,
		  size_t config_table_size, paddr_t pending_table,
		  size_t pending_table_size, bool pending_zeroed);

count_result_t
gicv3_its_vpe_unmap(thread_t *vcpu);

void
gicv3_its_vpe_cleanup(thread_t *vcpu);

// Physical MSI to virtual LPI mapping.
//
// The specified device must have previously been provisioned by calling
// gicv3_its_enable_device(). Also, the specified VCPU must have been mapped
// by calling gicv3_its_vpe_map().
//
// The effects might not be observed until a subsequent VSYNC executes for the
// affected VCPU.
error_t
gicv3_its_vmap(platform_msi_id_t msi_id, thread_t *vcpu, irq_t lpi);

error_t
gicv3_its_vmove(platform_msi_id_t msi_id, thread_t *vcpu);

error_t
gicv3_its_vinvalidate_all(platform_msi_controller_id_t its, thread_t *vcpu);

// Virtual IRQ mapping synchronisation.
//
// This must be called, and its completion waited on, to guarantee that any map,
// move, discard, or SGI configuration command previously submitted to the
// specified ITS has taken effect on the redistributor for the physical CPU that
// the specified VCPU is currently executing on (if any).
count_result_t
gicv3_its_vsync(platform_msi_controller_id_t its, thread_t *vcpu);

#if GICV3_HAS_VLPI_V4_1

// SGI configuration and assertion.
//
// The effects of config and clear commands might not be observed until a
// subsequent VSYNC executes for the affected VCPU.
//
// The _config and _clear functions are no-ops if gicv3_its_vpe_map() has not
// been called for the specified VCPU. The _assert function will fail an
// assertion if the specified VCPU has not been mapped.
error_t
gicv3_its_vsgi_config(thread_t *vcpu, virq_t vsgi, bool enabled, bool group1,
		      uint8_t priority);

error_t
gicv3_its_vsgi_assert(thread_t *vcpu, virq_t vsgi);

error_t
gicv3_its_vsgi_clear(thread_t *vcpu, virq_t vsgi);

// Issue a VSYNC command for the ITS that processes VSGI commands.
count_result_t
gicv3_its_vsgi_sync(thread_t *vcpu);

// Check whether a VSYNC command is complete for the ITS that processes VSGI
// commands.
bool_result_t
gicv3_its_vsgi_is_complete(count_t cmd_seq);

#endif // GICV3_HAS_VLPI_V4_1

#endif // GICV3_HAS_VLPI

size_t
gicv3_its_page_size(GITS_BASER_Page_Size_t page_size);

void
gicv3_its_disable_all(gits_t *const (*regs)[PLATFORM_GITS_COUNT]);

#endif // GICV3_HAS_ITS
