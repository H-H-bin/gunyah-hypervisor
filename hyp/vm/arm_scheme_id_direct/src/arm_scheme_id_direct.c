// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <compiler.h>
#include <log.h>
#include <platform_features.h>
#include <thread.h>
#include <trace.h>
#include <vcpu.h>

#include <asm/barrier.h>
#include <asm/sysregs.h>
#include <asm/system_registers.h>

#include "event_handlers.h"

#if defined(ENABLE_SCHEMEID_MGMT) && ENABLE_SCHEMEID_MGMT

// Current implementation for Cache Scheme ID management support:
// - Hyp will only manage granting access to the registers to VM's
// - PVM will have write access to manage required Scheme ID registers
// - Any/all GVM/SVM's will not have write access to these registers
// - HCR_EL2.TIDCP bit controls any RW access to these cause exception into EL2
// - ACTLR_EL2 bits control whether EL1 will have write access to these set of
//   implementation defined registers.
//     If the bit is 0, then the registers will be RO/WI for EL1
//     If the bit is 1, then the registers will be writable in EL1
//
// So, hyp implements the design as follows:
//  - If the SchemeID feature is enabled, then we clear HCR_EL2.TIDCP
//  - When Activating the threads for the VM,
//      - for PVM, set the ACTLR_EL2 bits for the required registers,
//      - else we set the bits cleared, so the write is ignored from EL1
//  - These control registers HCR_EL2 and ACTLR_EL2 being part of the vCPU
//      thread specific data, these will be restored to physical registers
//      when the vCPU thread is being scheduled to run

bool
arm_scheme_id_direct_handle_vcpu_activate_thread(thread_t	    *thread,
						 vcpu_option_flags_t options)
{
	assert(thread != NULL);

	if (!vcpu_is_vcpu(thread)) {
		goto out;
	}

	vcpu_el2_registers_t *el2_regs = &thread->vcpu_regs_el2;
	HCR_EL2_set_TIDCP(&el2_regs->hcr_el2, false);

	if (vcpu_option_flags_get_hlos_vm(&options)) {
		ACTLR_EL2_set_PWREN(&thread->sch_id.actlr_el2, true);
		ACTLR_EL2_set_TSIDEN(&thread->sch_id.actlr_el2, true);
		ACTLR_EL2_set_SMEN(&thread->sch_id.actlr_el2, true);
		ACTLR_EL2_set_CLUSTERPMUEN(&thread->sch_id.actlr_el2, true);
	} else {
		ACTLR_EL2_set_PWREN(&thread->sch_id.actlr_el2, false);
		ACTLR_EL2_set_TSIDEN(&thread->sch_id.actlr_el2, false);
		ACTLR_EL2_set_SMEN(&thread->sch_id.actlr_el2, false);
		ACTLR_EL2_set_CLUSTERPMUEN(&thread->sch_id.actlr_el2, false);
	}
out:
	return true;
}

void
arm_scheme_id_direct_handle_vcpu_load_state(void)
{
	thread_t *thread = thread_get_self();

	register_ACTLR_EL2_write(thread->sch_id.actlr_el2);
}
#endif
