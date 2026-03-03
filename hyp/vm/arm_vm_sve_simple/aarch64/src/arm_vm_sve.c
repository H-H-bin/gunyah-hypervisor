// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#if defined(ARCH_ARM_FEAT_SVE)

#include <hypregisters.h>

#include <compiler.h>
#include <log.h>
#include <platform_features.h>
#include <qcbor.h>
#include <thread.h>
#include <trace.h>
#include <vcpu.h>

#include <asm/barrier.h>
#include <asm/system_registers.h>

#include "event_handlers.h"

// A simple SVE module that allows SVE access to HLOS only.

static bool sve_disabled = false;

// Ensure the value of SVE_Z_REG_SIZE (PLATFORM_SVE_REG_SIZE) is sane
static_assert(SVE_Z_REG_SIZE >= SVE_Z_MIN_REG_SIZE,
	      "SVE register size should be minimum 16 bytes");

void
arm_vm_sve_simple_handle_boot_runtime_init(void)
{
	// Before writing ZCR_EL2, make sure EL2 has access to SVE subsystem.
	// Unfortunately the same bit that controls EL1/EL0 access also controls
	// EL2 access.
	CPTR_EL2_E2H1_t cptr =
		register_CPTR_EL2_E2H1_read_ordered(&asm_ordering);
	CPTR_EL2_E2H1_set_ZEN(&cptr, CPTR_ZEN_TRAP_NONE);
	register_CPTR_EL2_E2H1_write_ordered(cptr, &asm_ordering);
}

void
arm_vm_sve_simple_handle_boot_cold_init(void)
{
	platform_cpu_features_t features = platform_get_cpu_features();

	sve_disabled = platform_cpu_features_get_sve_disable(&features);
}

void
arm_vm_sve_simple_handle_boot_cpu_warm_init(void)
{
	if (!sve_disabled) {
		// Initialise ZCR_EL2 as its reset value is architecturally
		// UNKNOWN. SVE register size is (ZCR_EL2.LEN + 1) * 128 bits.
		// SVE_Z_REG_SIZE is in bytes.
		ZCR_EL2_t zcr = ZCR_EL2_default();
		ZCR_EL2_set_LEN(&zcr,
				(uint8_t)(((SVE_Z_REG_SIZE << 3) >> 7) - 1U));

		register_ZCR_EL2_write(zcr);
		// No need to disable SVE access, the context-switch code will
		// do it if necessary (if we are switching to a non-HLOS VM)
	}
}

void
arm_vm_sve_simple_handle_rootvm_init(qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	assert(qcbor_enc_ctxt != NULL);

	QCBOREncode_AddBoolToMap(qcbor_enc_ctxt, "sve_supported",
				 !sve_disabled);
}

bool
arm_vm_sve_simple_handle_vcpu_activate_thread(thread_t		 *thread,
					      vcpu_option_flags_t options)
{
	assert(thread != NULL);
	bool ret;

	if (vcpu_is_vcpu(thread)) {
		bool hlos	 = vcpu_option_flags_get_hlos_vm(&options);
		bool sve_allowed = vcpu_option_flags_get_sve_allowed(&options);

		if (sve_allowed && sve_disabled) {
			// Not permitted
			ret = false;
		} else if (hlos && sve_allowed) {
			// Give HLOS threads SVE access
			CPTR_EL2_E2H1_set_ZEN(&thread->vcpu_regs_el2.cptr_el2,
					      CPTR_ZEN_TRAP_NONE);
			vcpu_option_flags_set_sve_allowed(&thread->vcpu_options,
							  true);
			ret = true;
		} else if (!hlos && sve_allowed) {
			// Not supported
			ret = false;
		} else {
			CPTR_EL2_E2H1_set_ZEN(&thread->vcpu_regs_el2.cptr_el2,
					      CPTR_ZEN_TRAP_ALL);
			ret = true;
		}
	} else {
		ret = true;
	}

	return ret;
}

#endif
