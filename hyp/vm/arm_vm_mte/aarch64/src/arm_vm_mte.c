// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#if defined(ARCH_ARM_FEAT_MTE)

#include <hypregisters.h>

#include <arm_mte.h>
#include <atomic.h>
#include <compiler.h>
#include <cpulocal.h>
#include <globals.h>
#include <irq.h>
#include <panic.h>
#include <platform_features.h>
#include <thread.h>
#include <vcpu.h>
#include <virq.h>

#include <asm/barrier.h>
#include <asm/sysregs.h>
#include <asm/system_registers.h>

#include "event_handlers.h"

bool
arm_mte_is_allowed(void)
{
	const global_options_t *global_options = globals_get_options();
	return compiler_unexpected(global_options_get_mte(global_options));
}

void
arm_vm_mte_handle_boot_cold_init(void)
{
	platform_cpu_features_t features = platform_get_cpu_features();

	global_options_t options = global_options_default();
	global_options_set_mte(&options, true);
	if (platform_cpu_features_get_mte_disable(&features)) {
		globals_clear_options(options);
	} else {
		globals_set_options(options);
	}
}

error_t
arm_vm_mte_handle_object_activate_thread(thread_t *thread)
{
	assert(thread != NULL);

	if (vcpu_is_vcpu(thread)) {
		if (arm_mte_is_allowed()) {
			// Give the thread access to the Memory Tagging
			// Extension
			HCR_EL2_set_ATA(&thread->vcpu_regs_el2.hcr_el2, true);
			// Do not trap GMID_EL1 accesses
			HCR_EL2_set_TID5(&thread->vcpu_regs_el2.hcr_el2, false);
		} else {
			HCR_EL2_set_TID5(&thread->vcpu_regs_el2.hcr_el2, true);
		}
		HCR_EL2_set_DCT(&thread->vcpu_regs_el2.hcr_el2, false);
	}

	return OK;
}

void
arm_vm_mte_handle_vcpu_save_state(void)
{
	thread_t *thread = thread_get_self();

	if (arm_mte_is_allowed()) {
		thread->mte.mte_regs.gcr_el1 = register_GCR_EL1_read_volatile();
		thread->mte.mte_regs.rgsr_el1 =
			register_RGSR_EL1_read_volatile();
		thread->mte.mte_regs.tfsr_el1 =
			register_TFSR_EL1_read_volatile();
		thread->mte.mte_regs.tfsre0_el1 =
			register_TFSRE0_EL1_read_volatile();
	}
}

void
arm_vm_mte_handle_vcpu_load_state(void)
{
	if (arm_mte_is_allowed()) {
		thread_t *thread = thread_get_self();

		register_GCR_EL1_write(thread->mte.mte_regs.gcr_el1);
		register_RGSR_EL1_write(thread->mte.mte_regs.rgsr_el1);
		register_TFSR_EL1_write(thread->mte.mte_regs.tfsr_el1);
		register_TFSRE0_EL1_write(thread->mte.mte_regs.tfsre0_el1);
	}
}
#endif
