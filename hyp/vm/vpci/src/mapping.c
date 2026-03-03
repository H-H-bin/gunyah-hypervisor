// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <compiler.h>
#include <log.h>
#include <memextent.h>
#include <panic.h>
#include <rcu.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>
#include <vdevice.h>

#include "event_handlers.h"
#include "mapping.h"

static vdevice_type_t
vpci_vdevice_type(index_t bar_index)
{
	vdevice_type_t ret;

	switch (bar_index) {
	case 0:
		ret = VDEVICE_TYPE_VPCI_BAR0;
		break;
	case 1:
		ret = VDEVICE_TYPE_VPCI_BAR1;
		break;
	case 2:
		ret = VDEVICE_TYPE_VPCI_BAR2;
		break;
	case 3:
		ret = VDEVICE_TYPE_VPCI_BAR3;
		break;
	case 4:
		ret = VDEVICE_TYPE_VPCI_BAR4;
		break;
	case 5:
		ret = VDEVICE_TYPE_VPCI_BAR5;
		break;
	default:
		panic("BAR index out of range");
	}

	return ret;
}

error_t
vpci_memextent_claim(vpci_device_t *device)
{
	error_t err = OK;

	for (index_t i = 0U; i < util_array_size(device->functions); i++) {
		vpci_function_t *function = device->functions[i];
		if (function == NULL) {
			continue;
		}
		for (index_t j = 0U; j < util_array_size(function->bars); j++) {
			vpci_bar_data_t *bar = &function->bars[j];
			if (bar->memextent == NULL) {
				continue;
			}

			assert(bar->vdevice.type == VDEVICE_TYPE_NONE);
			bar->vdevice.type = vpci_vdevice_type(j);

			err = vdevice_attach_phys(&bar->vdevice,
						  bar->memextent);
			if (err != OK) {
				bar->vdevice.type = VDEVICE_TYPE_NONE;
				goto out;
			}
		}
	}

out:
	if (err != OK) {
		vpci_memextent_release(device);
	}

	return err;
}

void
vpci_memextent_release(vpci_device_t *device)
{
	for (index_t i = 0U; i < util_array_size(device->functions); i++) {
		vpci_function_t *function = device->functions[i];
		if (function == NULL) {
			continue;
		}
		for (index_t j = 0U; j < util_array_size(function->bars); j++) {
			vpci_bar_data_t *bar = &function->bars[j];
			if (bar->memextent == NULL) {
				continue;
			}

			vdevice_detach_phys(&bar->vdevice, bar->memextent);
			bar->unmap_pending = true;
			rcu_enqueue(&bar->unmap_rcu,
				    RCU_UPDATE_CLASS_VPCI_UNMAP_BAR);
		}
	}
}

bool
vpci_map_allowed_for_bar(vpci_device_t *device, vpci_function_t *function,
			 vpci_bar_data_t *bar)
{
	(void)device; // Only used for the REQUIRE_LOCK assertion
	bool allowed;

	switch (bar->address_type) {
	case VPCI_BAR_ADDRESS_TYPE_IO:
		allowed = pci_function_command_get_io_space(&function->command);
		break;
	case VPCI_BAR_ADDRESS_TYPE_MEM_64:
	case VPCI_BAR_ADDRESS_TYPE_MEM_32:
	case VPCI_BAR_ADDRESS_TYPE_MEM_NP_32:
		allowed =
			pci_function_command_get_mem_space(&function->command);
		break;
	case VPCI_BAR_ADDRESS_TYPE_MEM_64_HIGH:
	default:
		allowed = false;
		break;
	}

	return allowed;
}

bool
vpci_map_bar_check_range(vpci_t *vpci, vpci_bar_data_t *bar, vmaddr_t base)
{
	bool range_ok;

	assert(util_is_p2aligned(base, bar->size_bits));
	vmaddr_t end = base + util_bit(bar->size_bits) - 1U;

	assert((vpci->io_size == 0U) ||
	       !util_add_overflows(vpci->io_base, vpci->io_size - 1U));
	assert((vpci->npmem_size == 0U) ||
	       !util_add_overflows(vpci->npmem_base, vpci->npmem_size - 1U));
	assert((vpci->pmem_size == 0U) ||
	       !util_add_overflows(vpci->pmem_base, vpci->pmem_size - 1U));

	switch (bar->address_type) {
	case VPCI_BAR_ADDRESS_TYPE_IO:
		range_ok = (vpci->io_size > 0U) &&
			   ((base >= vpci->io_base) &&
			    (end <= (vpci->io_base + vpci->io_size - 1U)));
		break;
	case VPCI_BAR_ADDRESS_TYPE_MEM_NP_32:
		range_ok =
			(vpci->npmem_size > 0U) &&
			((base >= vpci->npmem_base) &&
			 (end <= (vpci->npmem_base + vpci->npmem_size - 1U)));
		break;
	case VPCI_BAR_ADDRESS_TYPE_MEM_32:
	case VPCI_BAR_ADDRESS_TYPE_MEM_64:
		range_ok =
			((vpci->npmem_size > 0U) &&
			 ((base >= vpci->npmem_base) &&
			  (end <=
			   (vpci->npmem_base + vpci->npmem_size - 1U)))) ||
			((vpci->pmem_size > 0U) &&
			 ((base >= vpci->pmem_base) &&
			  (end <= (vpci->pmem_base + vpci->pmem_size - 1U))));
		break;
	case VPCI_BAR_ADDRESS_TYPE_MEM_64_HIGH:
	default:
		panic("Invalid BAR type for mapping");
	}

	return range_ok;
}

error_t
vpci_try_map_bar(vpci_t *vpci, vpci_device_t *device, vpci_bar_data_t *bar,
		 index_t bar_index)
{
	error_t ret;

	spinlock_acquire(&device->mapping_lock);

	assert(vpci != NULL);
	assert(vpci->addrspace != NULL);
	assert(util_is_p2aligned(bar->ipa_addr, bar->size_bits));

	if (bar->is_mapped) {
		ret = ERROR_RETRY;
	} else if (!vpci_map_bar_check_range(vpci, bar, bar->ipa_addr)) {
		TRACE(DEBUG, INFO,
		      "vPCI {:#x} device {:d}: BAR {:#x} can't map at {:#x}",
		      (uintptr_t)vpci, device->slot, (uintptr_t)bar,
		      bar->ipa_addr);
		ret = OK; // Nothing to do
	} else if (bar->memextent != NULL) {
		TRACE(DEBUG, INFO,
		      "vPCI {:#x} device {:d}: BAR {:#x} mapping at {:#x}",
		      (uintptr_t)vpci, device->slot, (uintptr_t)bar,
		      bar->ipa_addr);
		memextent_mapping_attrs_t map_attrs =
			memextent_mapping_attrs_default();
		pgtable_access_t access =
			(bar->access_type == VPCI_BAR_ACCESS_TYPE_UNTRAPPED)
				? PGTABLE_ACCESS_RW
				: PGTABLE_ACCESS_R;
		memextent_mapping_attrs_set_user_access(&map_attrs, access);
		memextent_mapping_attrs_set_kernel_access(&map_attrs, access);
		memextent_mapping_attrs_set_memtype(
			&map_attrs, PGTABLE_VM_MEMTYPE_DEVICE_NGNRE);

		error_t err = memextent_map(bar->memextent, vpci->addrspace,
					    bar->ipa_addr, map_attrs,
					    addrspace_map_flags_default());
		if (err == OK) {
			bar->is_mapped = true;
		} else {
			TRACE_AND_LOG(
				ERROR, WARN,
				"vPCI {:#x} device {:d}: BAR memextent {:#x} map failure: {:d}",
				(uintptr_t)vpci, device->slot,
				(uintptr_t)bar->memextent, (register_t)err);
		}
		ret = err;
	} else {
		assert(bar->vdevice.type == VDEVICE_TYPE_NONE);
		error_t err = vdevice_attach_vmaddr(
			vpci_vdevice_type(bar_index), &bar->vdevice,
			vpci->addrspace, bar->ipa_addr,
			util_bit(bar->size_bits));
		if (err == OK) {
			bar->is_mapped = true;
		} else {
			TRACE_AND_LOG(
				ERROR, WARN,
				"vPCI {:#x} device {:d}: BAR vdevice attach failure: {:d}",
				(uintptr_t)vpci, device->slot, (register_t)err);
		}
		ret = err;
	}

	spinlock_release(&device->mapping_lock);
	return ret;
}

error_t
vpci_try_map_function(vpci_t *vpci, vpci_device_t *device,
		      vpci_function_t *function, bool io_bars, bool mem_bars)
{
	error_t ret;

	for (index_t i = 0U; i < util_array_size(function->bars); i++) {
		vpci_bar_data_t *bar = &function->bars[i];

		bool affected;
		switch (bar->address_type) {
		case VPCI_BAR_ADDRESS_TYPE_IO:
			affected = io_bars;
			break;
		case VPCI_BAR_ADDRESS_TYPE_MEM_64:
		case VPCI_BAR_ADDRESS_TYPE_MEM_32:
		case VPCI_BAR_ADDRESS_TYPE_MEM_NP_32:
			affected = mem_bars;
			break;
		case VPCI_BAR_ADDRESS_TYPE_MEM_64_HIGH:
		default:
			affected = false;
			break;
		}
		if (!affected) {
			continue;
		}

		error_t err = vpci_try_map_bar(vpci, device, bar, i);
		if (err != OK) {
			ret = err;
			goto out;
		}
	}
	ret = OK;
out:
	return ret;
}

static void
vpci_do_unmap_bar(vpci_t *vpci, vpci_device_t *device, vpci_bar_data_t *bar,
		  bool deactivate) REQUIRE_SPINLOCK(device -> mapping_lock)
{
	(void)device; // Only used for the REQUIRE_LOCK assertion
	assert(vpci != NULL);
	assert(vpci->addrspace != NULL);

	if (!bar->is_mapped || bar->unmap_pending) {
		// Bar is already unmapped, nothing to do here
	} else if (bar->memextent != NULL) {
		// Write-trapped BAR using a memextent.
		error_t err = memextent_unmap(bar->memextent, vpci->addrspace,
					      bar->ipa_addr,
					      addrspace_map_flags_default());
		TRACE(DEBUG, INFO,
		      "vPCI {:#x} device {:d}: BAR {:#x} unmapped from {:#x}: {:d}",
		      (uintptr_t)vpci, device->slot, (uintptr_t)bar,
		      bar->ipa_addr, (register_t)err);
		assert(err == OK);
		bar->is_mapped = false;
	} else {
		// RW - trapped BAR using an IPA attachment
		vdevice_detach_vmaddr(&bar->vdevice);
		if (!deactivate) {
			bar->unmap_pending = true;
			rcu_enqueue(&bar->unmap_rcu,
				    RCU_UPDATE_CLASS_VPCI_UNMAP_BAR);
		}
	}
}

void
vpci_unmap_bar(vpci_t *vpci, vpci_device_t *device, vpci_bar_data_t *bar)
{
	vpci_do_unmap_bar(vpci, device, bar, false);
}

rcu_update_status_t
vpci_handle_rcu_update(rcu_entry_t *entry)
{
	vpci_bar_data_t *bar = vpci_bar_data_container_of_unmap_rcu(entry);
	assert(bar != NULL);
	assert(bar->index < util_member_array_size(vpci_function_t, bars));

	vpci_function_t *function =
		vpci_function_container_of_bars_element(bar, bar->index);
	assert(function != NULL);

	vpci_device_t *device = function->device;
	assert(device != NULL);

	spinlock_acquire(&device->mapping_lock);
	assert(bar->is_mapped && bar->unmap_pending);
	bar->is_mapped	   = false;
	bar->vdevice	   = (vdevice_t){ 0 };
	bar->unmap_pending = false;
	spinlock_release(&device->mapping_lock);

	return rcu_update_status_default();
}

void
vpci_unmap_function(vpci_t *vpci, vpci_device_t *device,
		    vpci_function_t *function, bool io_bars, bool mem_bars)
{
	for (index_t i = 0U; i < util_array_size(function->bars); i++) {
		vpci_bar_data_t *bar = &function->bars[i];

		bool affected;
		switch (bar->address_type) {
		case VPCI_BAR_ADDRESS_TYPE_IO:
			affected = io_bars;
			break;
		case VPCI_BAR_ADDRESS_TYPE_MEM_64:
		case VPCI_BAR_ADDRESS_TYPE_MEM_32:
		case VPCI_BAR_ADDRESS_TYPE_MEM_NP_32:
			affected = mem_bars;
			break;
		case VPCI_BAR_ADDRESS_TYPE_MEM_64_HIGH:
		default:
			affected = false;
			break;
		}
		if (!affected) {
			continue;
		}

		vpci_do_unmap_bar(vpci, device, bar, false);
	}
}

void
vpci_deactivate_device_mappings(vpci_t *vpci, vpci_device_t *device)
{
	spinlock_acquire(&device->mapping_lock);

	for (index_t i = 0U; i < util_array_size(device->functions); i++) {
		vpci_function_t *function = device->functions[i];
		if (function == NULL) {
			continue;
		}

		for (index_t j = 0U; j < util_array_size(function->bars); j++) {
			vpci_bar_data_t *bar = &function->bars[j];
			vpci_do_unmap_bar(vpci, device, bar, true);
		}
	}

	spinlock_release(&device->mapping_lock);
}

void
vpci_cleanup_device_mappings(vpci_t *vpci, vpci_device_t *device)
{
	(void)vpci;

	// It is possible that an RCU handler to clear is_mapped is still
	// queued, and it might corrupt memory if it runs after this point.
	//
	// Since vpci_deactivate_device() does not queue RCU updates, it should
	// only be possible to panic here if the VM unmaps the BAR very shortly
	// before it is destroyed, which is unlikely to be possible unless it is
	// cooperating with its own manager.
	//
	// To solve this completely we will need to implement rcu_wait().
	// FIXME: QC Gunyah issue #181
	spinlock_acquire(&device->mapping_lock);

	for (index_t i = 0U; i < util_array_size(device->functions); i++) {
		vpci_function_t *function = device->functions[i];
		if (function == NULL) {
			continue;
		}

		for (index_t j = 0U; j < util_array_size(function->bars); j++) {
			vpci_bar_data_t *bar = &function->bars[j];
			if (bar->unmap_pending) {
				panic("vpci_cleanup_device: RCU pending");
			}
		}
	}

	spinlock_release(&device->mapping_lock);
}
