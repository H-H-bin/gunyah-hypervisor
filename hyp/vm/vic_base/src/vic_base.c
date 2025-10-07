// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypconstants.h>
#include <hyprights.h>

#include <compiler.h>
#include <cpulocal.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <irq.h>
#include <object.h>
#include <spinlock.h>
#include <thread.h>
#include <util.h>
#include <vic.h>
#include <virq.h>

#include <events/vic.h>
#include <events/virq.h>

#include "event_handlers.h"
#include "vic_base.h"

error_t
vic_base_handle_hwirq_vic_bind_virq(cap_id_t irq_obj_cap, vic_t *vic,
				    index_t index, virq_t virq)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();
	(void)index;

	hwirq_ptr_result_t hwirq_r = cspace_lookup_hwirq(
		cspace, irq_obj_cap, CAP_RIGHTS_HWIRQ_BIND_VIC);

	if (compiler_unexpected(hwirq_r.e != OK)) {
		err = hwirq_r.e;
		goto out;
	}

	err = trigger_vic_bind_hwirq_event(hwirq_r.r->action, vic, hwirq_r.r,
					   virq);

	object_put_hwirq(hwirq_r.r);
out:
	return err;
}

error_t
vic_base_handle_hwirq_vic_unbind_virq(cap_id_t irq_obj_cap, index_t index)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();
	(void)index;

	hwirq_ptr_result_t hwirq_r = cspace_lookup_hwirq(
		cspace, irq_obj_cap, CAP_RIGHTS_HWIRQ_BIND_VIC);

	if (compiler_unexpected(hwirq_r.e != OK)) {
		err = hwirq_r.e;
		goto out;
	}

	assert(hwirq_r.r != NULL);
	err = trigger_vic_unbind_hwirq_event(hwirq_r.r->action, hwirq_r.r);

	object_put_hwirq(hwirq_r.r);
out:
	return err;
}
