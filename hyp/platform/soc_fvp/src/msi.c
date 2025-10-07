// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <platform_irq.h>
#include <util.h>

static platform_msi_device_t msi_ctrl0_devices[] = {
	// SMMU test engines
	{ .device_id = 0xF0U, .max_event = 32 },
	{ .device_id = 0xF1U, .max_event = 32 },
	{ .device_id = 0x200U, .max_event = 32 },
	{ .device_id = 0x204U, .max_event = 32 },
	{ .device_id = 0x600U, .max_event = 32 },
	{ .device_id = 0x607U, .max_event = 32 },
	{ .device_id = 0x700U, .max_event = 32 },
	{ .device_id = 0x703U, .max_event = 32 },
	{ .device_id = 0x800U, .max_event = 32 },
	{ .device_id = 0x801U, .max_event = 32 },
	// AHCI
	{ .device_id = 0xF8U, .max_event = 32 },
	{ .device_id = 0x100U, .max_event = 32 },
	{ .device_id = 0x500U, .max_event = 32 },
};

static const platform_msi_controller_t msi_controllers[] = {
	[0] = { .num_devices = (count_t)util_array_size(msi_ctrl0_devices),
		.devices     = msi_ctrl0_devices },
	[1] = { .num_devices = 0U, .devices = NULL },
	[2] = { .num_devices = 0U, .devices = NULL },
	[3] = { .num_devices = 0U, .devices = NULL },
};

extern const platform_msi_controller_t *
platform_irq_msi_devices(platform_msi_controller_id_t ctrl_index)
{
	const platform_msi_controller_t *result = NULL;

	if (ctrl_index < util_array_size(msi_controllers)) {
		result = &msi_controllers[ctrl_index];
	}

	return result;
}
