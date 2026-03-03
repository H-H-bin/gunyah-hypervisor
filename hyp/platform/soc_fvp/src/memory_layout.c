// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <platform_memory_layout.h>
#include <util.h>

static phys_range_t platform_device_layouts[] = {
	// Boot Area and expansion AXI, other reserved
	{ 0x0, 0x21000000 },
	// FVP expansion AXI,
	// includes PCI MMIO32 @ { 0x70000000, 0x07800000 } with some SMMU TEs
	{ 0x60000000, 0x20000000 },
	// Standard System devices
	{ 0x21000000U, 0x11000000U },
	// There is a gap at 0x32000000 we can use as free IPA space
	{ 0x44000000U, 0x0C000000U },
	// FVP PCIe space, 12GB
	{ 0x500000000U, 0x300000000U }, // Includes SATA, USB and more SMMU TEs
};

const phys_range_t *
platform_get_device_layouts(count_t *count)
{
	const phys_range_t *layouts;

	layouts = platform_device_layouts;
	*count	= (count_t)util_array_size(platform_device_layouts);

	return layouts;
}
