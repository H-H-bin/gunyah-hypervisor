// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcontainers.h>
#include <hyprights.h>

#include <atomic.h>
#include <bitmap.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <list.h>
#include <object.h>
#include <panic.h>
#include <partition_alloc.h>
#include <platform_irq.h>
#include <qcbor.h>
#include <rcu.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>
#include <vdevice.h>

#include <asm/barrier.h>

#include "event_handlers.h"
#include "gicv3_its.h"
#include "internal.h"
#include "useraccess.h"

error_t
vgic_its_handle_object_create_vgic_its(vgic_its_create_t vgic_its_create)
{
	error_t	    err	     = OK;
	vgic_its_t *vgic_its = vgic_its_create.vgic_its;

	if (vgic_its_create.phys_index >= PLATFORM_GITS_COUNT) {
		err = ERROR_ARGUMENT_INVALID;
		goto fail;
	}

	vgic_its->phys_index = vgic_its_create.phys_index;
	spinlock_init(&vgic_its->lock);

fail:
	return err;
}

error_t
vgic_its_handle_object_activate_vgic_its(vgic_its_t *vgic_its)
{
	error_t				 err = OK;
	const platform_msi_controller_t *pits =
		platform_irq_msi_devices(vgic_its->phys_index);

	if (pits == NULL) {
		err = ERROR_OBJECT_CONFIG;
		goto fail;
	}

	list_init(&vgic_its->device_list);

	// Claim all of the devices on the physical ITS. In future we may
	// make this finer-grained; none of the code outside of this function
	// assumes that the virtual ITS owns all devices on the physical ITS,
	// but we have no real mechanism for fine-grained access control yet.
	for (index_t i = 0U; i < pits->num_devices; i++) {
		platform_msi_device_t *device = &pits->devices[i];

		vgic_its_t *prev_owner =
			atomic_load_acquire(&device->vgic_its_owner);
		if ((prev_owner != NULL) ||
		    !atomic_compare_exchange_strong_explicit(
			    &device->vgic_its_owner, &prev_owner, vgic_its,
			    memory_order_acquire, memory_order_acquire)) {
			err = ERROR_BUSY;
			goto fail;
		}

		list_insert_at_tail(&vgic_its->device_list,
				    &device->vgic_its_list);

		assert(bitmap_empty(device->vgic_its_mapped_events,
				    util_bit(VGIC_ITS_EVENT_BITS)));
	}

	// Reserve an unused device and allocate a dummy event on the physical
	// ITS. Note that if we allow physical ITSs to be split among more than
	// one virtual ITS, we will need a unique dummy event for each virtual
	// ITS, so we will need to move this code and set up an allocator for
	// the reserved events.
	platform_msi_device_id_result_t device_id_r =
		gicv3_its_reserve_unused_device(vgic_its->phys_index, 0U);
	assert(device_id_r.e == OK);
	platform_msi_id_set_its_index(&vgic_its->dummy_event,
				      vgic_its->phys_index);
	platform_msi_id_set_device_id(&vgic_its->dummy_event, device_id_r.r);
	platform_msi_id_set_event_id(&vgic_its->dummy_event, 0U);
	vgic_its->dummy_event_mapped_lpi = 0U;

	// The ITS is initially quiescent.
	GITS_CTLR_set_Quiescent(&vgic_its->ctlr, true);

fail:
	if (err != OK) {
		vgic_its_handle_object_deactivate_vgic_its(vgic_its);
	}
	return err;
}

static GITS_BASER_t
vgic_its_baser_default(GITS_BASER_Type_t type, size_t entry_size)
{
	GITS_BASER_t baser = GITS_BASER_default();

	// The following fields of GITS_BASER are treated as RO. The others,
	// including the Valid bit, are initialised to 0 and must be set by
	// the VM.
	GITS_BASER_set_Entry_Size(&baser, entry_size - 1U);
	GITS_BASER_set_Type(&baser, type);
	// Page size is fixed at 4KB, regardless of the S1 or S2 page size
	// (this is the most space-efficient, especially for indirect tables)
	GITS_BASER_set_Page_Size(&baser, GITS_BASER_PAGE_SIZE_SIZE_4KB);
	// Shareability == 1: Inner shareable
	GITS_BASER_set_Shareability(&baser, 1U);
	// OuterCache == 0: Inner and outer attributes are the same
	GITS_BASER_set_OuterCache(&baser, 0U);
	// InnerCache == 7: Inner write back, read + write alloc
	GITS_BASER_set_InnerCache(&baser, 7U);
	// Indirect is writable only for the device table
	GITS_BASER_set_Indirect(&baser, false);

	return baser;
}

static GITS_CBASER_t
vgic_its_cbaser_default(void)
{
	GITS_CBASER_t cbaser = GITS_CBASER_default();

	// The following fields of GITS_CBASER are treated as RO. The others,
	// including the Valid bit, are initialised to 0 and must be set by
	// the VM.
	// Shareability == 1: Inner shareable
	GITS_CBASER_set_Shareability(&cbaser, 1U);
	// OuterCache == 0: Inner and outer attributes are the same
	GITS_CBASER_set_OuterCache(&cbaser, 0U);
	// InnerCache == 7: Inner write back, read + write alloc
	GITS_CBASER_set_InnerCache(&cbaser, 7U);

	return cbaser;
}

static error_t
vgic_its_attach_vic(vgic_its_t *vgic_its, vic_t *vic)
{
	assert(vgic_its != NULL);
	assert(vic != NULL);
	assert(vgic_its->phys_index < util_array_size(vic->vgic_its_ptrs));

	error_t err = OK;

	spinlock_acquire(&vgic_its->lock);

	// Check that we don't already have an owner
	vic_t *prev_owner = atomic_load_acquire(&vgic_its->owner);
	if (prev_owner != NULL) {
		err = ERROR_BUSY;
		goto fail_locked;
	}

	// Check that we already have an address space attachment; this must
	// be done first because the ITS might be enabled as soon as it has a
	// VIC attachment.
	if (vgic_its->addrspace == NULL) {
		err = ERROR_ADDR_INVALID;
		goto fail_locked;
	}

	// A detached ITS should always be disabled and quiescent.
	assert(!GITS_CTLR_get_Enabled(&vgic_its->ctlr) &&
	       GITS_CTLR_get_Quiescent(&vgic_its->ctlr));

	// Clear the rest of the ITS register state
	vgic_its->device_table_base = vgic_its_baser_default(
		GITS_BASER_TYPE_DEVICES, sizeof(vgic_its_dte_t));
	vgic_its->collection_table_base = vgic_its_baser_default(
		GITS_BASER_TYPE_COLLECTIONS, sizeof(vgic_its_cte_t));
	vgic_its->command_queue_base  = vgic_its_cbaser_default();
	vgic_its->command_queue_read  = GITS_CREADR_default();
	vgic_its->command_queue_write = GITS_CWRITER_default();

	errno_t err_mem = memset_s(vgic_its->collection_table,
				   sizeof(vgic_its->collection_table), 0,
				   sizeof(vgic_its->collection_table));
	if (err_mem != 0) {
		panic("Error in memset_s operation!");
	}

	atomic_store_release(&vgic_its->owner, vic);

	// Attempt to change the VIC's ITS pointer from NULL. If it is not
	// already NULL, fail with a busy error. Once this is set, a virtual
	// device access in the ITS range might be routed to this vITS.
	//
	// This pointer is reference-counted, so we take an extra reference.
	vgic_its_t *prev_vgic_its = NULL;
	(void)object_get_vgic_its_additional(vgic_its);
	if (!atomic_compare_exchange_strong_explicit(
		    &vic->vgic_its_ptrs[vgic_its->phys_index], &prev_vgic_its,
		    vgic_its, memory_order_release, memory_order_relaxed)) {
		object_put_vgic_its(vgic_its);

		// Undo the ownership change.
		atomic_store_release(&vgic_its->owner, NULL);
		err = ERROR_BUSY;

		// Drop the lock and wait for an RCU grace period, in case
		// some other thread has seen the owner we temporarily set.
		spinlock_release(&vgic_its->lock);
		rcu_sync();
		goto fail_unlocked;
	}

fail_locked:
	spinlock_release(&vgic_its->lock);
fail_unlocked:
	return err;
}

error_t
vgic_its_handle_vic_bind_msi_source(vic_t *vic, cap_id_t msi_source_cap)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vgic_its_ptr_result_t vgic_its_r = cspace_lookup_vgic_its(
		cspace, msi_source_cap, CAP_RIGHTS_VGIC_ITS_BIND_VIC);
	if (compiler_unexpected(vgic_its_r.e) != OK) {
		err = vgic_its_r.e;
		goto out;
	}

	err = vgic_its_attach_vic(vgic_its_r.r, vic);

	object_put_vgic_its(vgic_its_r.r);

out:
	return err;
}

static error_t
vgic_its_attach_addrspace(vgic_its_t *vgic_its, addrspace_t *addrspace)
{
	assert(vgic_its != NULL);
	assert(addrspace != NULL);

	error_t err = OK;

	spinlock_acquire(&vgic_its->lock);

	// Check that we don't already have an address space
	if (vgic_its->addrspace != NULL) {
		err = ERROR_BUSY;
		goto fail_locked;
	}

	vgic_its->addrspace = object_get_addrspace_additional(addrspace);

fail_locked:
	spinlock_release(&vgic_its->lock);

	return err;
}

error_t
vgic_its_handle_addrspace_attach_vdma(addrspace_t *addrspace,
				      cap_id_t vdma_object_cap, index_t index)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	vgic_its_ptr_result_t vgic_its_r = cspace_lookup_vgic_its(
		cspace, vdma_object_cap, CAP_RIGHTS_VGIC_ITS_ATTACH_ADDRSPACE);
	if (compiler_unexpected(vgic_its_r.e) != OK) {
		err = vgic_its_r.e;
		goto out;
	}

	if (index != 0U) {
		err = ERROR_ARGUMENT_INVALID;
		goto out_ref;
	}

	err = vgic_its_attach_addrspace(vgic_its_r.r, addrspace);

out_ref:
	object_put_vgic_its(vgic_its_r.r);
out:
	return err;
}

error_t
vgic_its_handle_addrspace_attach_vdevice(addrspace_t *addrspace,
					 cap_id_t     vdevice_object_cap,
					 index_t index, vmaddr_t vbase,
					 size_t				  size,
					 addrspace_attach_vdevice_flags_t flags)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	if (flags.raw != 0U) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	vgic_its_ptr_result_t vgic_its_r =
		cspace_lookup_vgic_its(cspace, vdevice_object_cap,
				       CAP_RIGHTS_VGIC_ITS_ATTACH_ADDRSPACE);
	if (compiler_unexpected(vgic_its_r.e) != OK) {
		err = vgic_its_r.e;
		goto out;
	}

	if (index != 0U) {
		err = ERROR_ARGUMENT_INVALID;
		goto out_ref;
	}

	spinlock_acquire(&vgic_its_r.r->lock);

	if (vgic_its_r.r->gits_device.type != VDEVICE_TYPE_NONE) {
		err = ERROR_BUSY;
		goto out_locked;
	}
	vgic_its_r.r->gits_device.type = VDEVICE_TYPE_VGIC_ITS;

	err = vdevice_attach_vmaddr(&vgic_its_r.r->gits_device, addrspace,
				    vbase, size);
	if (err != OK) {
		vgic_its_r.r->gits_device.type = VDEVICE_TYPE_NONE;
	}

out_locked:
	spinlock_release(&vgic_its_r.r->lock);
out_ref:
	object_put_vgic_its(vgic_its_r.r);
out:
	return err;
}

void
vgic_its_handle_object_deactivate_vgic_its(vgic_its_t *vgic_its)
{
	// Release all the physical ITS devices that we claimed.
	platform_msi_device_t *device;
	list_foreach_container_maydelete (device, &vgic_its->device_list,
					  platform_msi_device, vgic_its_list) {
		vgic_its_t *prev_owner =
			atomic_load_acquire(&device->vgic_its_owner);
		if (prev_owner != vgic_its) {
			continue;
		}
		(void)list_delete_node(&vgic_its->device_list,
				       &device->vgic_its_list);

		// Mapped event IDs should only be possible while an active VIC
		// is attached, in which case the VIC should hold a reference to
		// this object, so we can't get here.
		assert(bitmap_empty(device->vgic_its_mapped_events,
				    util_bit(VGIC_ITS_EVENT_BITS)));

		atomic_store_release(&device->vgic_its_owner, NULL);
	}

	if (vgic_its->gits_device.type != VDEVICE_TYPE_NONE) {
		vdevice_detach_vmaddr(&vgic_its->gits_device);
	}
}

void
vgic_its_handle_object_deactivate_vic(vic_t *vic)
{
	for (index_t i = 0U; i < util_array_size(vic->vgic_its_ptrs); i++) {
		vgic_its_t *vgic_its =
			atomic_load_acquire(&vic->vgic_its_ptrs[i]);
		if (vgic_its == NULL) {
			continue;
		}
		assert(atomic_load_relaxed(&vgic_its->owner) == vic);

		// Discard all of the virtual LPI mappings.
		vgic_its_set_enabled(vgic_its, false);
		assert(!GITS_CTLR_get_Enabled(&vgic_its->ctlr) &&
		       GITS_CTLR_get_Quiescent(&vgic_its->ctlr));

		atomic_store_release(&vgic_its->owner, NULL);
	}
}

void
vgic_its_handle_object_cleanup_vic(vic_t *vic)
{
	for (index_t i = 0U; i < util_array_size(vic->vgic_its_ptrs); i++) {
		vgic_its_t *vgic_its =
			atomic_load_relaxed(&vic->vgic_its_ptrs[i]);
		if (vgic_its != NULL) {
			object_put_vgic_its(vgic_its);
		}
	}
}

void
vgic_its_handle_rootvm_init(partition_t *root_partition, cspace_t *root_cspace,
			    hyp_env_data_t   *hyp_env,
			    qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	assert(qcbor_enc_ctxt != NULL);

	QCBOREncode_OpenArrayInMap(qcbor_enc_ctxt, "vic_msi_source");

	// Create a virtual ITS object for every physical ITS
	index_t i;
	for (i = 0U; i < PLATFORM_GITS_COUNT; i++) {
		vgic_its_create_t vgic_its_params = {
			.phys_index = i,
		};
		vgic_its_ptr_result_t vgic_its_r = partition_allocate_vgic_its(
			root_partition, vgic_its_params);
		if (vgic_its_r.e != OK) {
			panic("Unable to create virtual ITS object");
		}

		error_t err = object_activate_vgic_its(vgic_its_r.r);
		if (err != OK) {
			if ((err == ERROR_DENIED) ||
			    (err == ERROR_ARGUMENT_INVALID) ||
			    (err == ERROR_BUSY)) {
				QCBOREncode_AddUInt64(qcbor_enc_ctxt,
						      CSPACE_CAP_INVALID);
				object_put_vgic_its(vgic_its_r.r);
				continue;
			} else {
				panic("Failed to activate virtual ITS object");
			}
		}

		// Create a master cap for the vgic_its
		object_ptr_t	vgic_its_optr = { .vgic_its = vgic_its_r.r };
		cap_id_result_t cid_r	      = cspace_create_master_cap(
			root_cspace, vgic_its_optr, OBJECT_TYPE_VGIC_ITS);
		if (cid_r.e != OK) {
			panic("Unable to create cap to vgic_its");
		}
		QCBOREncode_AddUInt64(qcbor_enc_ctxt, cid_r.r);
	}
	hyp_env->gits_base   = PLATFORM_GITS_BASE;
	hyp_env->gits_stride = (size_t)util_bit(GITS_STRIDE_SHIFT);
	QCBOREncode_CloseArray(qcbor_enc_ctxt);

	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "gits_stride",
				   hyp_env->gits_stride);
	// Array of tuples of base address and number of ITSs for each
	// contiguous ITS range. Currently only one range is supported.
	QCBOREncode_OpenArrayInMap(qcbor_enc_ctxt, "gits_ranges");
	QCBOREncode_OpenArray(qcbor_enc_ctxt);
	QCBOREncode_AddUInt64(qcbor_enc_ctxt, PLATFORM_GITS_BASE);
	QCBOREncode_AddUInt64(qcbor_enc_ctxt, PLATFORM_GITS_COUNT);
	QCBOREncode_CloseArray(qcbor_enc_ctxt);
	QCBOREncode_CloseArray(qcbor_enc_ctxt);
}

static error_t
vgic_its_process_id_mapd_command(vgic_its_t *vgic_its, gic_its_cmd_t cmd)
{
	error_t err;
	if (gic_its_cmd_mapd_get_valid(&cmd.mapd)) {
		err = vgic_its_cmd_mapd_valid(
			vgic_its, gic_its_cmd_mapd_get_device_id(&cmd.mapd),
			gic_its_cmd_mapd_get_itt_addr(&cmd.mapd),
			gic_its_cmd_mapd_get_size(&cmd.mapd) + 1U);
	} else {
		err = vgic_its_cmd_mapd_invalid(
			vgic_its, gic_its_cmd_mapd_get_device_id(&cmd.mapd));
	}
	return err;
}

static error_t
vgic_its_process_id_mapc_command(vgic_its_t *vgic_its, gic_its_cmd_t cmd)
{
	error_t err;
	if (gic_its_cmd_mapc_get_valid(&cmd.mapc)) {
		err = vgic_its_cmd_mapc_valid(
			vgic_its, gic_its_cmd_mapc_get_icid(&cmd.mapc),
			(vgic_its_rdbase_t)gic_its_cmd_mapc_get_rdbase(
				&cmd.mapc));
	} else {
		err = vgic_its_cmd_mapc_invalid(
			vgic_its, gic_its_cmd_mapc_get_icid(&cmd.mapc));
	}

	return err;
}

static error_t
vgic_its_process_one_command(vgic_its_t *vgic_its, gic_its_cmd_t cmd)
{
	error_t err;

	TRACE(VGIC, INFO, "VITS {:#x}: cmd {:#x} {:#x} {:#x} {:#x}",
	      (uintptr_t)vgic_its, cmd.base.bf[0], cmd.base.bf[1],
	      cmd.base.bf[2], cmd.base.bf[3]);

	switch (gic_its_cmd_base_get_cmd(&cmd.base)) {
	case GIC_ITS_CMD_ID_CLEAR:
		err = vgic_its_cmd_clear(
			vgic_its, gic_its_cmd_clear_get_device_id(&cmd.clear),
			gic_its_cmd_clear_get_event_id(&cmd.clear));
		break;
	case GIC_ITS_CMD_ID_DISCARD:
		err = vgic_its_cmd_discard(
			vgic_its,
			gic_its_cmd_discard_get_device_id(&cmd.discard),
			gic_its_cmd_discard_get_event_id(&cmd.discard));
		break;
	case GIC_ITS_CMD_ID_INT:
		err = vgic_its_cmd_int(vgic_its,
				       gic_its_cmd_int_get_device_id(&cmd.int_),
				       gic_its_cmd_int_get_event_id(&cmd.int_));
		break;
	case GIC_ITS_CMD_ID_INV:
		err = vgic_its_cmd_inv(vgic_its,
				       gic_its_cmd_inv_get_device_id(&cmd.inv),
				       gic_its_cmd_inv_get_event_id(&cmd.inv));
		break;
	case GIC_ITS_CMD_ID_INVALL:
		err = vgic_its_cmd_invall(
			vgic_its, gic_its_cmd_invall_get_icid(&cmd.invall));
		break;
	case GIC_ITS_CMD_ID_MAPC:
		err = vgic_its_process_id_mapc_command(vgic_its, cmd);
		break;
	case GIC_ITS_CMD_ID_MAPD:
		err = vgic_its_process_id_mapd_command(vgic_its, cmd);
		break;
	case GIC_ITS_CMD_ID_MAPI:
		err = vgic_its_cmd_mapti(
			vgic_its, gic_its_cmd_mapi_get_device_id(&cmd.mapi),
			gic_its_cmd_mapi_get_event_id(&cmd.mapi),
			gic_its_cmd_mapi_get_icid(&cmd.mapi),
			gic_its_cmd_mapi_get_event_id(&cmd.mapi));
		break;
	case GIC_ITS_CMD_ID_MAPTI:
		err = vgic_its_cmd_mapti(
			vgic_its, gic_its_cmd_mapti_get_device_id(&cmd.mapti),
			gic_its_cmd_mapti_get_event_id(&cmd.mapti),
			gic_its_cmd_mapti_get_icid(&cmd.mapti),
			gic_its_cmd_mapti_get_lpi(&cmd.mapti));
		break;
	case GIC_ITS_CMD_ID_MOVALL:
		err = vgic_its_cmd_movall(
			vgic_its,
			(vgic_its_rdbase_t)gic_its_cmd_movall_get_rdbase1(
				&cmd.movall),
			(vgic_its_rdbase_t)gic_its_cmd_movall_get_rdbase2(
				&cmd.movall));
		break;
	case GIC_ITS_CMD_ID_MOVI:
		err = vgic_its_cmd_movi(
			vgic_its, gic_its_cmd_movi_get_device_id(&cmd.movi),
			gic_its_cmd_movi_get_event_id(&cmd.movi),
			gic_its_cmd_movi_get_icid(&cmd.movi));
		break;
	case GIC_ITS_CMD_ID_SYNC:
		err = vgic_its_cmd_sync(
			vgic_its,
			(vgic_its_rdbase_t)gic_its_cmd_sync_get_rdbase(
				&cmd.sync));
		break;
	case GIC_ITS_CMD_ID_INVDB:
	case GIC_ITS_CMD_ID_VINVALL:
	case GIC_ITS_CMD_ID_VMAPI:
	case GIC_ITS_CMD_ID_VMAPP:
	case GIC_ITS_CMD_ID_VMAPTI:
	case GIC_ITS_CMD_ID_VMOVI:
	case GIC_ITS_CMD_ID_VMOVP:
	case GIC_ITS_CMD_ID_VSGI:
	default:
		// GICv4-only or invalid command ID
		TRACE(VGIC, WARN,
		      "VGIC ITS: bad cmd id {:#x} ({:#x} {:#x} {:#x} {:#x})",
		      gic_its_cmd_base_get_cmd(&cmd.base), cmd.base.bf[0],
		      cmd.base.bf[1], cmd.base.bf[2], cmd.base.bf[3]);
		err = ERROR_ARGUMENT_INVALID;
		break;
	}

	if (err != OK) {
		TRACE(VGIC, WARN, "VITS {:#x}: cmd failed: {:d}",
		      (uintptr_t)vgic_its, (register_t)err);
	}
	return err;
}

static void
vgic_its_process_commands(vgic_its_t *vgic_its)
{
	enum { disabled, invalid, empty, stalled, head_range } exit_reason;

	count_t cmds_done = 0U;

	TRACE(VGIC, INFO, "VITS {:#x}: starting command processing",
	      (uintptr_t)vgic_its);

	while (1) {
		spinlock_acquire(&vgic_its->lock);

		if (!GITS_CTLR_get_Enabled(&vgic_its->ctlr) ||
		    GITS_CTLR_get_Quiescent(&vgic_its->ctlr)) {
			// The ITS is disabled; we can't process commands.
			spinlock_release(&vgic_its->lock);
			exit_reason = disabled;
			break;
		}

		if (!GITS_CBASER_get_Valid(&vgic_its->command_queue_base)) {
			// The queue has not been configured by the VM yet
			spinlock_release(&vgic_its->lock);
			exit_reason = invalid;
			break;
		}

		index_t read_index =
			GITS_CREADR_get_Index(&vgic_its->command_queue_read);
		if (GITS_CWRITER_get_Index(&vgic_its->command_queue_write) ==
		    read_index) {
			// No commands waiting in the queue
			spinlock_release(&vgic_its->lock);
			exit_reason = empty;
			break;
		}

		if (GITS_CREADR_get_Stalled(&vgic_its->command_queue_read)) {
			// Stalled after a command failure, until CWRITER is
			// written with the Retry bit set
			spinlock_release(&vgic_its->lock);
			exit_reason = stalled;
			break;
		}

		size_t	queue_size = (((size_t)GITS_CBASER_get_Size(
					       &vgic_its->command_queue_base) +
				       1U) *
				      4096U);
		count_t queue_entries =
			(count_t)(queue_size / sizeof(gic_its_cmd_t));

		if (GITS_CWRITER_get_Index(&vgic_its->command_queue_write) >=
		    queue_entries) {
			// Write pointer out of range; treat queue as invalid
			spinlock_release(&vgic_its->lock);
			exit_reason = head_range;
			break;
		}

		// Read the next command out of the queue
		gic_its_cmd_t cmd;
		paddr_t	      queue_base = GITS_CBASER_get_Physical_Address(
			      &vgic_its->command_queue_base);
		size_t cmd_offset = read_index * sizeof(cmd);

		assert(cmd_offset < queue_size);

		error_t err = useraccess_copy_from_guest_ipa(
				      vgic_its->addrspace, &cmd, sizeof(cmd),
				      queue_base + cmd_offset, sizeof(cmd),
				      false, false)
				      .e;
		if (err != OK) {
			TRACE(VGIC, WARN,
			      "VITS {:#x}: cmd read @ {:#x} failed: {:d}",
			      (uintptr_t)vgic_its,
			      (uintptr_t)(queue_base + cmd_offset),
			      (register_t)err);
		}

		// If we read the command successfully, try to execute it
		if (err == OK) {
			err = vgic_its_process_one_command(vgic_its, cmd);
			cmds_done++;
		}

		if (err == OK) {
			// Success; advance the read pointer
			GITS_CREADR_set_Index(&vgic_its->command_queue_read,
					      (read_index + 1U) %
						      queue_entries);
		} else {
			// Failure; stall the queue
			// FIXME:
			GITS_CREADR_set_Stalled(&vgic_its->command_queue_read,
						true);
		}

		spinlock_release(&vgic_its->lock);
	}

	TRACE(VGIC, INFO,
	      "VITS {:#x}: processed {:d} commands (exit reason {:d})",
	      (uintptr_t)vgic_its, cmds_done, (register_t)exit_reason);
}

static void
vgic_its_map(platform_msi_controller_id_t its, platform_msi_device_t *device,
	     platform_msi_event_id_t event, thread_t *gicr_vcpu, virq_t lpi)
{
	bitmap_set(device->vgic_its_mapped_events, event);

	platform_msi_id_t msi_id = platform_msi_id_default();
	platform_msi_id_set_event_id(&msi_id, event);
	platform_msi_id_set_device_id(&msi_id, device->device_id);
	platform_msi_id_set_its_index(&msi_id, its);

	error_t err = gicv3_its_vmap(msi_id, gicr_vcpu, lpi);
	assert(err == OK);
}

static void
vgic_its_enable(vgic_its_t *vgic_its)
{
	// This is a potentially very slow operation, since it must issue many
	// physical VMAPTI commands if the virtual ITS was previously disabled
	// with mapped LPIs. Therefore we frequently drop and re-acquire the
	// lock, and then re-check the enabled bit to ensure that we haven't
	// been overtaken by a vgic_its_disable() on a different CPU.
	spinlock_acquire(&vgic_its->lock);
	if (!GITS_CTLR_get_Enabled(&vgic_its->ctlr)) {
		goto out_locked;
	}

	vgic_its_copy_in_ctes(vgic_its);

	// Go through all the attached real devices and check their DTEs,
	// in case we need to restore a physical mapping.
	platform_msi_device_t *device;
	list_foreach_container (device, &vgic_its->device_list,
				platform_msi_device, vgic_its_list) {
		spinlock_release(&vgic_its->lock);
		asm_yield();
		spinlock_acquire(&vgic_its->lock);
		if (!GITS_CTLR_get_Enabled(&vgic_its->ctlr)) {
			goto out_locked;
		}

		vgic_its_dte_result_t dte_r =
			vgic_its_copy_in_dte(vgic_its, device->device_id);

		if ((dte_r.e != OK) || !vgic_its_dte_get_valid(&dte_r.r)) {
			// Device is not mapped; skip it
			continue;
		}

		// DTE is valid; iterate through the ITT
		count_t itt_entries = (count_t)util_bit(
			vgic_its_dte_get_itt_size(&dte_r.r) + 1U);
		for (index_t i = 0U; i < itt_entries; i++) {
			vgic_its_itte_result_t itte_r =
				vgic_its_copy_in_itte(vgic_its, dte_r.r, i);

			if ((itte_r.e != OK) ||
			    !vgic_its_itte_get_valid(&itte_r.r)) {
				// Event is not mapped; skip it
				continue;
			}

			vgic_its_cte_t *cte_ptr =
				&vgic_its->collection_table
					 [vgic_its_itte_get_icid(&itte_r.r)];
			vgic_its_cte_set_known_empty(cte_ptr, false);

			if (!vgic_its_cte_get_valid(cte_ptr)) {
				continue;
			}

			vgic_its_rdbase_t rd_index =
				vgic_its_cte_get_rd_index(cte_ptr);

			rcu_read_start();
			vic_t *vic = atomic_load_consume(&vgic_its->owner);
			if (rd_index < vic->gicr_count) {
				thread_t *vcpu = atomic_load_consume(
					&vic->gicr_vcpus[rd_index]);
				if (vcpu != NULL) {
					vgic_its_map(vgic_its->phys_index,
						     device, i, vcpu,
						     vgic_its_itte_get_lpi(
							     &itte_r.r));
				}
			}
			rcu_read_finish();

			spinlock_release(&vgic_its->lock);
			asm_yield();
			spinlock_acquire(&vgic_its->lock);
			if (!GITS_CTLR_get_Enabled(&vgic_its->ctlr)) {
				goto out_locked;
			}
		}
	}

	// We've successfully finished enabling; clear the Quiescent bit.
	GITS_CTLR_set_Quiescent(&vgic_its->ctlr, false);
out_locked:
	spinlock_release(&vgic_its->lock);

	// If the command queue was initialised to not be empty, we might now
	// need to start processing commands.
	vgic_its_process_commands(vgic_its);
}

static void
vgic_its_disable(vgic_its_t *vgic_its)
{
	// This is a potentially very slow operation, since it must issue many
	// physical DISCARD commands if any LPIs were left mapped, and we must
	// block until they are complete. Therefore we frequently drop and
	// re-acquire the lock to permit preemption to occur.
	//
	// We don't need to re-check the enabled bit after acquiring the lock
	// because that can't be set while the Quiescent bit is clear, and after
	// activation the Quiescent bit is only set at the end of this function.
	// Concurrent calls to this function also should not be possible because
	// it is only called after changing the enable bit to false.
	spinlock_acquire(&vgic_its->lock);
	assert(!GITS_CTLR_get_Quiescent(&vgic_its->ctlr) &&
	       !GITS_CTLR_get_Enabled(&vgic_its->ctlr));

	// Copy the cached collections out to VM memory.
	vgic_its_copy_out_ctes(vgic_its);

	// Go through all the attached real devices and issue DISCARD commands
	// for any LPIs mapped in the physical ITS.
	platform_msi_device_t *device;
	list_foreach_container (device, &vgic_its->device_list,
				platform_msi_device, vgic_its_list) {
		platform_msi_id_t msi_id = platform_msi_id_default();
		platform_msi_id_set_its_index(&msi_id, vgic_its->phys_index);
		platform_msi_id_set_device_id(&msi_id, device->device_id);

		BITMAP_FOREACH_SET_BEGIN(event, device->vgic_its_mapped_events,
					 util_bit(VGIC_ITS_EVENT_BITS))
			platform_msi_id_set_event_id(&msi_id, event);
			error_t err = gicv3_its_discard(msi_id);
			assert(err == OK);
		BITMAP_FOREACH_SET_END

		errno_t err_mem =
			memset_s(device->vgic_its_mapped_events,
				 sizeof(device->vgic_its_mapped_events), 0,
				 sizeof(device->vgic_its_mapped_events));

		if (err_mem != 0) {
			panic("Error in memset_s operation!");
		}

		spinlock_release(&vgic_its->lock);
		asm_yield();
		spinlock_acquire(&vgic_its->lock);
	}
	spinlock_release(&vgic_its->lock);

	// Synchronise the ITS command queue with every attached VCPU.
	rcu_read_start();
	count_result_t sync = count_result_error(ERROR_IDLE);
	vic_t	      *vic  = atomic_load_consume(&vgic_its->owner);
	for (index_t i = 0U; i < vic->gicr_count; i++) {
		thread_t *vcpu = atomic_load_consume(&vic->gicr_vcpus[i]);
		if (vcpu != NULL) {
			sync = gicv3_its_vsync(vgic_its->phys_index, vcpu);
			assert(sync.e == OK);
		}
	}
	rcu_read_finish();

	// Wait for the last queued VSYNC (if any) to complete, which guarantees
	// that the DISCARD commands have taken effect for all attached vPEs.
	if (sync.e == OK) {
		(void)gicv3_its_wait(vgic_its->phys_index, sync.r);
	}

	// Disable is complete; set the Quiescent bit.
	spinlock_acquire(&vgic_its->lock);
	GITS_CTLR_set_Quiescent(&vgic_its->ctlr, true);
	spinlock_release(&vgic_its->lock);
}

void
vgic_its_set_enabled(vgic_its_t *vgic_its, bool enabled)
{
	spinlock_acquire(&vgic_its->lock);

	TRACE(VGIC, INFO, "VITS {:#x}: ctrl {:#x} -> enable {:d}",
	      (uintptr_t)vgic_its, GITS_CTLR_raw(vgic_its->ctlr),
	      (register_t)enabled);

	if (enabled && GITS_CTLR_get_Quiescent(&vgic_its->ctlr) &&
	    !GITS_CTLR_get_Enabled(&vgic_its->ctlr)) {
		GITS_CTLR_set_Enabled(&vgic_its->ctlr, true);
		spinlock_release(&vgic_its->lock);
		vgic_its_enable(vgic_its);
	} else if (!enabled && GITS_CTLR_get_Enabled(&vgic_its->ctlr)) {
		// We might be interrupting an enable in progress on another
		// CPU. In that case, the Quiescent bit will currently be true;
		// set it to false to prevent any enable starting until this
		// disable has completed.
		GITS_CTLR_set_Quiescent(&vgic_its->ctlr, false);
		GITS_CTLR_set_Enabled(&vgic_its->ctlr, false);
		spinlock_release(&vgic_its->lock);
		vgic_its_disable(vgic_its);
	} else {
		// Either the Enabled bit has not been changed, or an attempt
		// was made to set Enabled while Quiescent was clear (i.e. a
		// disable was in progress on another CPU). Ignore the write.
		spinlock_release(&vgic_its->lock);
	}
}

void
vgic_its_set_collection_table_base(vgic_its_t  *vgic_its,
				   GITS_BASER_t collection_table_base)
{
	spinlock_acquire(&vgic_its->lock);

	// Copy only the writable fields
	GITS_BASER_copy_Valid(&vgic_its->collection_table_base,
			      &collection_table_base);
	GITS_BASER_copy_Physical_Address(&vgic_its->collection_table_base,
					 &collection_table_base);
	GITS_BASER_copy_Size(&vgic_its->collection_table_base,
			     &collection_table_base);

	// No need to flush or update anything; this register is only used
	// while enabling and disabling the ITS, and the effect of changing it
	// while the ITS is enabled or not quiescent is defined to be
	// UNPREDICTABLE.

	spinlock_release(&vgic_its->lock);
}

void
vgic_its_set_device_table_base(vgic_its_t  *vgic_its,
			       GITS_BASER_t device_table_base)
{
	spinlock_acquire(&vgic_its->lock);

	// Copy only the writable fields
	GITS_BASER_copy_Valid(&vgic_its->device_table_base, &device_table_base);
	GITS_BASER_copy_Physical_Address(&vgic_its->device_table_base,
					 &device_table_base);
	GITS_BASER_copy_Size(&vgic_its->device_table_base, &device_table_base);
	GITS_BASER_copy_Indirect(&vgic_its->device_table_base,
				 &device_table_base);

	// No need to flush or update anything; the effect of changing this
	// register while the ITS is enabled or not quiescent is defined to be
	// UNPREDICTABLE.

	spinlock_release(&vgic_its->lock);
}

void
vgic_its_set_command_queue_base(vgic_its_t   *vgic_its,
				GITS_CBASER_t command_queue_base)
{
	spinlock_acquire(&vgic_its->lock);

	// Copy only the writable fields
	GITS_CBASER_copy_Valid(&vgic_its->command_queue_base,
			       &command_queue_base);
	GITS_CBASER_copy_Physical_Address(&vgic_its->command_queue_base,
					  &command_queue_base);
	GITS_CBASER_copy_Size(&vgic_its->command_queue_base,
			      &command_queue_base);

	// GITS_CREADR is zeroed on writes to this register
	vgic_its->command_queue_read = GITS_CREADR_default();

	spinlock_release(&vgic_its->lock);
}

void
vgic_its_set_command_queue_write(vgic_its_t    *vgic_its,
				 GITS_CWRITER_t command_queue_write)
{
	spinlock_acquire(&vgic_its->lock);

	if (GITS_CWRITER_get_Retry(&command_queue_write)) {
		GITS_CWRITER_set_Retry(&command_queue_write, false);
		GITS_CREADR_set_Stalled(&vgic_its->command_queue_read, false);
	}

	vgic_its->command_queue_write = command_queue_write;

	spinlock_release(&vgic_its->lock);

	vgic_its_process_commands(vgic_its);
}

void
vgic_its_translate(vgic_its_t *vgic_its, platform_msi_event_id_t event)
{
	// The device ID used when the CPU writes to GITS_TRANSLATER is
	// strictly platform-specific, but is typically 0.
	//
	// Note that GITS_TRANSLATER is in a page of its own, and can safely
	// be mapped directly to a VM to allow it to raise device 0 events.
	(void)vgic_its_cmd_int(vgic_its, (platform_msi_device_id_t)0U, event);
}
