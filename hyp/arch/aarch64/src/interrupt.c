// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypregisters.h>

#include <asm/barrier.h>
#include <asm/interrupt.h>

// Check whether there are currently any interrupts pending.
bool
asm_interrupt_is_pending(void)
{
	ISR_EL1_t isr = register_ISR_EL1_read_volatile_ordered(&asm_ordering);
	return ISR_EL1_get_I(&isr);
}
