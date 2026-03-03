// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <compiler.h>
#include <vpci.h>

#include "host_bridge.h"

void
vpci_host_bridge_init(vpci_t *vpci)
{
	// Conventional PCI host bridges need a type 1 configuration header,
	// which is not yet implemented. For PCIe, this device is just a dummy
	// and we can use a type 0 configuration header.
	assert(vpci_is_pcie(vpci));

	vpci_device_t *dev = &vpci->bridge;
	*dev		   = (vpci_device_t){ 0 };

	vpci_function_t *fn = &vpci->bridge_fn0;
	*fn		    = (vpci_function_t){ 0 };

	dev->functions[0] = fn;

	fn->type = VPCI_FUNCTION_TYPE_VPCI_HOST_BRIDGE;

	fn->device_class      = PCI_DEVICE_CLASS_BRIDGE;
	fn->device_subclass   = PCI_DEVICE_SUBCLASS_BRIDGE_HOST;
	fn->vendor_id	      = PCI_VENDOR_ID_QUALCOMM;
	fn->product_id	      = VPCI_PRODUCT_ID_GUNYAH_VPCI_HOST_BRIDGE;
	fn->subsys_vendor_id  = fn->vendor_id;
	fn->subsys_product_id = fn->product_id;

	// No IRQ, BARs, or capabilities
}

void
vpci_host_bridge_cleanup(vpci_t *vpci)
{
	// No dynamically allocated memory, so nothing to do here.
	(void)vpci;
}

void
vpci_error(vpci_function_t *function, error_t error)
{
	assert(function != NULL);
	vpci_device_t *device = function->device;
	assert(device != NULL);
	vpci_t *bus = device->bus;
	assert(bus != NULL);

	vpci_error_from_requester(function, &bus->bridge_fn0, error);
}

void
vpci_bus_error(vpci_t *bus)
{
	vpci_error(&bus->bridge_fn0, ERROR_ADDR_INVALID);
}

void
vpci_error_from_requester(vpci_function_t *completer,
			  vpci_function_t *requester, error_t error)
{
	pci_function_status_t error_bits = pci_function_status_default();

	if (compiler_expected(error == OK)) {
		// Nothing to do
	} else if ((error == ERROR_UNIMPLEMENTED) ||
		   (error == ERROR_ADDR_INVALID)) {
		vpci_pcie_error(requester, error);
		// PCI Master-Abort on the requester
		pci_function_status_set_received_master_abort(&error_bits,
							      true);
		(void)pci_function_status_atomic_union(
			&requester->status, error_bits, memory_order_relaxed);
	} else if ((error == ERROR_FAILURE) || (error == ERROR_RETRY)) {
		vpci_pcie_error(completer, error);
		// No PCI abort in this case
	} else {
		vpci_pcie_error(completer, error);
		// PCI Target-Abort sent from the completer to the requester
		pci_function_status_set_signalled_target_abort(&error_bits,
							       true);
		(void)pci_function_status_atomic_union(
			&completer->status, error_bits, memory_order_relaxed);
		error_bits = pci_function_status_default();
		pci_function_status_set_received_target_abort(&error_bits,
							      true);
		(void)pci_function_status_atomic_union(
			&requester->status, error_bits, memory_order_relaxed);
	}
}
