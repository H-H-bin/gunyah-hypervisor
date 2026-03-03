// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <irq.h>
#include <thread.h>
#include <timer_queue.h>
#include <vcpu.h>
#include <vic.h>
#include <virq.h>

#include <asm/barrier.h>

#include "arm_vm_pmu.h"
#include "event_handlers.h"

error_t
arm_vm_pmu_handle_object_activate_thread(thread_t *thread)
{
	error_t ret = OK;

	if (vcpu_is_vcpu(thread)) {
		ret = vic_bind_private_vcpu(&thread->pmu.pmu_virq_src, thread,
					    PLATFORM_VM_PMU_IRQ,
					    VIRQ_TRIGGER_PMU);
	}

	return ret;
}

void
arm_vm_pmu_handle_object_deactivate_thread(thread_t *thread)
{
	if (vcpu_is_vcpu(thread)) {
		vic_unbind(&thread->pmu.pmu_virq_src);
	}
}
