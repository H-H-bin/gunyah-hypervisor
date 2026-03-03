// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypregisters.h>

#include <compiler.h>
#include <qcbor.h>
#include <thread.h>
#include <vcpu.h>

#include <asm/sysregs.h>

#include "event_handlers.h"

// Oryon CPUs only implement ACTLR_EL3. There are no additional implementation
// specific EL1 or EL2 registers to context-switch.

void
vcpu_context_switch_cpu_load(void)
{
	// No-op
}

void
vcpu_context_switch_cpu_save(void)
{
	// No-op
}
