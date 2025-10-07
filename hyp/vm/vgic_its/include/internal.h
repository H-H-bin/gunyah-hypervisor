// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#define VGIC_ITS_TRACE(vgic_its, event, msg, ...)                              \
	TRACE(VGIC_ITS, VGIC_ITS_##event, "vits {:#x} (pits {:d}): " msg,      \
	      (uintptr_t)vgic_its, vgic_its->phys_index, __VA_ARGS__)

// GITS register write handlers; must hold a reference to vgic_its or be in an
// RCU critical section, but the ITS lock is not held.

void
vgic_its_set_enabled(vgic_its_t *vgic_its, bool enabled);

void
vgic_its_set_collection_table_base(vgic_its_t  *vgic_its,
				   GITS_BASER_t collection_table_base);

void
vgic_its_set_device_table_base(vgic_its_t  *vgic_its,
			       GITS_BASER_t device_table_base);

void
vgic_its_set_command_queue_base(vgic_its_t   *vgic_its,
				GITS_CBASER_t command_queue_base);

void
vgic_its_set_command_queue_write(vgic_its_t    *vgic_its,
				 GITS_CWRITER_t command_queue_write);

void
vgic_its_translate(vgic_its_t *vgic_its, platform_msi_event_id_t event);

// ITS command handlers; called with the ITS lock held. Any error result will
// stall the command queue.

error_t
vgic_its_cmd_clear(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		   platform_msi_event_id_t event);

error_t
vgic_its_cmd_discard(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		     platform_msi_event_id_t event);

error_t
vgic_its_cmd_int(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		 platform_msi_event_id_t event);

error_t
vgic_its_cmd_inv(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		 platform_msi_event_id_t event);

error_t
vgic_its_cmd_invall(vgic_its_t *vgic_its, vgic_its_ic_id_t icid);

error_t
vgic_its_cmd_mapc_valid(vgic_its_t *vgic_its, vgic_its_ic_id_t icid,
			vgic_its_rdbase_t rdbase);

error_t
vgic_its_cmd_mapc_invalid(vgic_its_t *vgic_its, vgic_its_ic_id_t icid);

error_t
vgic_its_cmd_mapd_valid(vgic_its_t *vgic_its, platform_msi_device_id_t device,
			vmaddr_t itt_base, count_t event_bits);

error_t
vgic_its_cmd_mapd_invalid(vgic_its_t		  *vgic_its,
			  platform_msi_device_id_t device);

// Also used for mapi, which is an alias of mapti when event==vlpi
error_t
vgic_its_cmd_mapti(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		   platform_msi_event_id_t event, vgic_its_ic_id_t icid,
		   virq_t vlpi);

error_t
vgic_its_cmd_movall(vgic_its_t *vgic_its, vgic_its_rdbase_t rdbase_src,
		    vgic_its_rdbase_t rdbase_dst);

error_t
vgic_its_cmd_movi(vgic_its_t *vgic_its, platform_msi_device_id_t device,
		  platform_msi_event_id_t event, vgic_its_ic_id_t icid);

error_t
vgic_its_cmd_sync(vgic_its_t *vgic_its, vgic_its_rdbase_t rdbase);

// ITS table lookups.
//
// Note that these come from guest-accessible memory, so the results must not be
// trusted in any way that might allow the VM to violate partitioning or cause a
// command failure on the physical ITS.
void
vgic_its_copy_in_ctes(vgic_its_t *vgic_its);

void
vgic_its_copy_out_ctes(vgic_its_t *vgic_its);

vgic_its_dte_result_t
vgic_its_copy_in_dte(vgic_its_t *vgic_its, platform_msi_device_id_t device_id);

error_t
vgic_its_copy_out_dte(vgic_its_t *vgic_its, platform_msi_device_id_t device_id,
		      vgic_its_dte_t dte);

vgic_its_itte_result_t
vgic_its_copy_in_itte(vgic_its_t *vgic_its, vgic_its_dte_t dte,
		      platform_msi_event_id_t event_id);

error_t
vgic_its_copy_out_itte(vgic_its_t *vgic_its, vgic_its_dte_t dte,
		       platform_msi_event_id_t event_id, vgic_its_itte_t itte);
