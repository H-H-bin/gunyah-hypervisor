// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <cpulocal.h>
#include <globals.h>
#include <platform_features.h>
#include <platform_security.h>
#include <scheduler.h>
#include <thread.h>
#include <trace.h>
#include <trace_helpers.h>

#include <asm/barrier.h>

#include "event_handlers.h"

#if defined(ARCH_ARM_FEAT_TRF) && ARCH_ARM_FEAT_TRF

void
vetm_null_handle_boot_cold_init(void)
{
	platform_cpu_features_t features = platform_get_cpu_features();

	if (!platform_cpu_features_get_trace_disable(&features)) {
		global_options_t options = global_options_default();
		global_options_set_vetm_null_allow_sysregs(&options, true);
		globals_set_options(options);
	}
}

void
vetm_null_handle_boot_cpu_cold_init(void)
{
	ID_AA64DFR0_EL1_t id_aa64dfr0 = register_ID_AA64DFR0_EL1_read();
	assert(ID_AA64DFR0_EL1_get_TraceFilt(&id_aa64dfr0) == 1U);
}

void
vetm_null_handle_boot_cpu_warm_init(void)
{
	const global_options_t *options = globals_get_options();

	if (global_options_get_vetm_null_allow_sysregs(options)) {
		TRFCR_EL2_t trfcr = TRFCR_EL2_default();
		// prohibit trace of EL2
		TRFCR_EL2_set_E2TRE(&trfcr, 0);
		register_TRFCR_EL2_write_ordered(trfcr, &asm_ordering);
	}
}

void
vetm_null_handle_boot_hypervisor_start(void)
{
	if (!platform_security_state_debug_disabled()) {
		global_options_t options = global_options_default();
		global_options_set_vetm_null_permit_hlos(&options, true);
		globals_set_options(options);
	}
}

bool
vetm_null_handle_vcpu_activate_thread(thread_t		 *thread,
				      vcpu_option_flags_t vcpu_options)
{
	bool ret;

	assert(thread->kind == THREAD_KIND_VCPU);

	const global_options_t *global_options = globals_get_options();
	bool			permit_hlos =
		global_options_get_vetm_null_permit_hlos(global_options);
	bool allow_sysregs =
		global_options_get_vetm_null_allow_sysregs(global_options);

	bool vcpu_hlos	 = vcpu_option_flags_get_hlos_vm(&vcpu_options);
	bool vcpu_traced = vcpu_option_flags_get_trace_allowed(&vcpu_options);

	// TODO: currently we always give HLOS trace access.
	if (vcpu_traced && !allow_sysregs) {
		// Not permitted by TZ
		ret = false;
	} else if (vcpu_traced && !vcpu_hlos) {
		// Not implemented by vetm_null
		ret = false;
	} else if (vcpu_hlos && permit_hlos && allow_sysregs) {
		// Debug device; give HLOS threads trace access, even if not
		// requested by RM
		vcpu_option_flags_set_trace_allowed(&thread->vcpu_options,
						    true);
		ret = true;
	} else {
		ret = true;
	}

	// Don't trap the trace system registers if tracing is allowed
	if (vcpu_option_flags_get_trace_allowed(&thread->vcpu_options)) {
		MDCR_EL2_set_TTRF(&thread->vcpu_regs_el2.mdcr_el2, false);
#if defined(ARCH_ARM_FEAT_VHE)
		CPTR_EL2_E2H1_set_TTA(&thread->vcpu_regs_el2.cptr_el2, false);
#else
		CPTR_EL2_E2H0_set_TTA(&thread->vcpu_regs_el2.cptr_el2, false);
#endif
	}

	return ret;
}

void
vetm_null_handle_thread_load_state(void)
{
	thread_t	       *current = thread_get_self();
	const global_options_t *options = globals_get_options();

	if ((current->kind == THREAD_KIND_VCPU) &&
	    global_options_get_vetm_null_allow_sysregs(options)) {
		const bool enable_trace = vcpu_option_flags_get_trace_allowed(
			&current->vcpu_options);
		TRFCR_EL1_t trfcr = TRFCR_EL1_default();
		TRFCR_EL1_set_TS(&trfcr, TRFCR_EL1_TS_VIRTUAL);
		TRFCR_EL1_set_E1TRE(&trfcr, enable_trace);
		TRFCR_EL1_set_E0TRE(&trfcr, enable_trace);
		register_TRFCR_EL1_write(trfcr);
	}
}

#else // !ARCH_ARM_FEAT_TRF

vcpu_trap_result_t
vetm_null_handle_vdevice_access_fixed_addr(vmaddr_t ipa, size_t access_size,
					   register_t *value, bool is_write)
{
	vcpu_trap_result_t ret = VCPU_TRAP_RESULT_UNHANDLED;
	(void)access_size;

	thread_t *vcpu = thread_get_self();
	assert(vcpu != NULL);

	if (!vcpu_option_flags_get_hlos_vm(&vcpu->vcpu_options)) {
		ret = false;
		goto out;
	}

	if ((ipa >= PLATFORM_ETM_BASE) &&
	    (ipa < (PLATFORM_ETM_BASE +
		    (PLATFORM_ETM_STRIDE * PLATFORM_MAX_CORES)))) {
		// Treat the entire ETM region as RAZ/WI
		if (!is_write) {
			*value = 0U;
		}
		ret = VCPU_TRAP_RESULT_EMULATED;
	}

out:
	return ret;
}

#endif // !ARCH_ARM_FEAT_TRF
