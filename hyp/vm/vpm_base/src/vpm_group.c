// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <object.h>
#include <spinlock.h>

#include "event_handlers.h"
#include "vpm_base.h"

error_t
vpm_bind_power(vpm_group_t *vpm_group, power_t *power)
{
	error_t ret;

	assert_debug(vpm_group != NULL);
	assert_debug(power != NULL);

	spinlock_acquire(&vpm_group->header.lock);

	if (vpm_group->power != NULL) {
		ret = ERROR_OBJECT_CONFIGURED;
		goto out;
	}
	vpm_group->power = object_get_power_additional(power);

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	vpm_group->power_system_suspend = true;
#endif

	ret = OK;
out:
	spinlock_release(&vpm_group->header.lock);

	return ret;
}

void
vpm_base_handle_object_deactivate_vpm_group(vpm_group_t *vpm_group)
{
	if (vpm_group->power != NULL) {
		object_put_power(vpm_group->power);
		vpm_group->power = NULL;
	}
}
