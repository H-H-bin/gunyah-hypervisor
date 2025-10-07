// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypregisters.h>

#include <idle.h>
#include <irq.h>

#include <asm/barrier.h>

#include "idle_arch.h"

bool
idle_arch_wait(void)
{
	bool must_reschedule = false;

	idle_block_start();

	// Execute a WFI, preceded by a DSB to ensure that all outstanding
	// memory operations (e.g. IPI sends) have completed.
	__asm__ volatile("dsb ish; wfi" : "+m"(asm_ordering));

	// Ensure that the ISR_EL1 read is ordered after the WFI.
	asm_context_sync_ordered(&asm_ordering);

	idle_block_finish();

	// Check for interrupts.
	ISR_EL1_t isr = register_ISR_EL1_read_volatile_ordered(&asm_ordering);
	if (ISR_EL1_get_I(&isr)) {
		must_reschedule = irq_interrupt_dispatch();
	}

	return must_reschedule;
}

bool
idle_arch_wait_timeout(ticks_t timeout)
{
	bool must_reschedule = false;

#if defined(ARCH_ARM_FEAT_WFxT) && ARCH_ARM_FEAT_WFxT
	idle_block_start();

	// Execute a WFIT, preceded by a DSB to ensure that all outstanding
	// memory operations (e.g. IPI sends) have completed.
	//
	// Note: WFIT timeouts are based on CNTVCT_EL0, so this assumes that we
	// always set CNTVOFF_EL2 to 0!
	__asm__ volatile("dsb ish; wfit %1"
			 : "+m"(asm_ordering)
			 : "r"(timeout));

	// Ensure that the ISR_EL1 read is ordered after the WFIT.
	asm_context_sync_ordered(&asm_ordering);

	idle_block_finish();
#else
	// Don't wait, since there is no way to set up a timeout without using
	// interrupts. This will cause a busy-loop in the caller.
	(void)timeout;
	asm_context_sync_ordered(&asm_ordering);
#endif

	// Check for interrupts.
	ISR_EL1_t isr = register_ISR_EL1_read_volatile_ordered(&asm_ordering);
	if (ISR_EL1_get_I(&isr)) {
		must_reschedule = irq_interrupt_dispatch();
	}

	return must_reschedule;
}
