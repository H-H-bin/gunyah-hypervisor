// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <platform_memory_layout.h>
#include <util.h>

static phys_range_t platform_device_layouts[] = {
	// FVP expansion AXI,
	// includes PCI MMIO32 @ { 0x70000000, 0x07800000 } with some SMMU TEs
	{ 0x60000000, 0x20000000 },
	// Standard System devices
	{ 0x21000000U, 0x2f000000U },
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
