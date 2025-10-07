// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <platform_memory_layout.h>
#include <util.h>

static phys_range_t platform_device_layouts[] = {
	{ 0x0U, 0x40000000U },
};

const phys_range_t *
platform_get_device_layouts(count_t *count)
{
	const phys_range_t *layouts;

	layouts = platform_device_layouts;
	*count	= (count_t)util_array_size(platform_device_layouts);

	return layouts;
}
