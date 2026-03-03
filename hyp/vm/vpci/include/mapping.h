// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
vpci_memextent_claim(vpci_device_t *device);

void
vpci_memextent_release(vpci_device_t *device);

bool
vpci_map_allowed_for_bar(vpci_device_t *device, vpci_function_t *function,
			 vpci_bar_data_t *bar)
	REQUIRE_LOCK(device -> mapping_lock);

bool
vpci_map_bar_check_range(vpci_t *vpci, vpci_bar_data_t *bar, vmaddr_t base);

error_t
vpci_try_map_bar(vpci_t *vpci, vpci_device_t *device, vpci_bar_data_t *bar,
		 index_t bar_index) EXCLUDE_SPINLOCK(device->mapping_lock);

error_t
vpci_try_map_function(vpci_t *vpci, vpci_device_t *device,
		      vpci_function_t *function, bool io_bars, bool mem_bars)
	EXCLUDE_SPINLOCK(device->mapping_lock);

void
vpci_unmap_bar(vpci_t *vpci, vpci_device_t *device, vpci_bar_data_t *bar)
	REQUIRE_SPINLOCK(device -> mapping_lock);

void
vpci_unmap_function(vpci_t *vpci, vpci_device_t *device,
		    vpci_function_t *function, bool io_bars, bool mem_bars)
	REQUIRE_SPINLOCK(device -> mapping_lock);

void
vpci_deactivate_device_mappings(vpci_t *vpci, vpci_device_t *device)
	EXCLUDE_SPINLOCK(device->mapping_lock);

void
vpci_cleanup_device_mappings(vpci_t *vpci, vpci_device_t *device)
	EXCLUDE_SPINLOCK(device->mapping_lock);
