// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <addrspace.h>
#include <atomic.h>
#include <compiler.h>
#include <rcu.h>
#include <thread.h>
#include <util.h>
#include <vcpu.h>

#if defined(INTERFACE_TRACE_PROFILE)
#include <trace.h>
#endif

#include "event_handlers.h"
#include "internal.h"

vcpu_trap_result_t
vdevice_handle_vcpu_trap_data_abort_guest(ESR_EL2_t esr, vmaddr_result_t ipa,
					  FAR_EL2_t far)
{
	vcpu_trap_result_t ret	  = VCPU_TRAP_RESULT_UNHANDLED;
	register_t	   val	  = 0U;
	thread_t	  *thread = thread_get_self();

	ESR_EL2_ISS_DATA_ABORT_t iss =
		ESR_EL2_ISS_DATA_ABORT_cast(ESR_EL2_get_ISS(&esr));

	if (ESR_EL2_ISS_DATA_ABORT_get_ISV(&iss)) {
		bool is_write		= ESR_EL2_ISS_DATA_ABORT_get_WnR(&iss);
		bool is_acquire_release = ESR_EL2_ISS_DATA_ABORT_get_AR(&iss);
		iss_da_sas_t	sas	= ESR_EL2_ISS_DATA_ABORT_get_SAS(&iss);
		size_t		size	= (size_t)util_bit((count_t)sas);
		uint8_t		reg_num = ESR_EL2_ISS_DATA_ABORT_get_SRT(&iss);
		iss_da_ia_fsc_t fsc	= ESR_EL2_ISS_DATA_ABORT_get_DFSC(&iss);

		// ISV is not meaningful for a S1 page table walk fault
		assert(!ESR_EL2_ISS_DATA_ABORT_get_S1PTW(&iss));

		if (is_write) {
			val = vcpu_gpr_read(thread, reg_num);

			// Mask register value passed to handlers
			switch (sas) {
			case ISS_DA_SAS_BYTE:
				val = (uint8_t)val;
				break;
			case ISS_DA_SAS_HALFWORD:
				val = (uint16_t)val;
				break;
			case ISS_DA_SAS_WORD:
				val = (uint32_t)val;
				break;
			case ISS_DA_SAS_DOUBLEWORD:
			default:
				// No masking required
				break;
			}

			if (is_acquire_release) {
				atomic_thread_fence(memory_order_release);
			}
		}

		// Only translation and permission faults are considered for
		// vdevice access.
		if ((fsc == ISS_DA_IA_FSC_PERMISSION_L0) ||
		    (fsc == ISS_DA_IA_FSC_PERMISSION_L1) ||
		    (fsc == ISS_DA_IA_FSC_PERMISSION_L2) ||
		    (fsc == ISS_DA_IA_FSC_PERMISSION_L3)) {
			// A permission fault may be a vdevice associated with
			// a physical address with a read-only mapping. Since
			// the IPA is not valid for permission faults, we must
			// look up the physical address from the faulting VA.
			rcu_read_start();
			gvaddr_t va = FAR_EL2_get_VirtualAddress(&far);
			addrspace_va_lookup_result_t lookup_r =
				addrspace_va_to_pa_noretry(va, false);

			// The lookup can fail if the guest unmapped or remapped
			// the faulting VA in stage 1 on another CPU after the
			// stage 2 fault was triggered. In that case, we must
			// retry the faulting instruction.
			if (lookup_r.e != OK) {
				rcu_read_finish();
				ret = VCPU_TRAP_RESULT_RETRY;
			} else {
				ret = vdevice_access_phys(lookup_r.r.phys, size,
							  &val, is_write);
			}
		} else if ((fsc == ISS_DA_IA_FSC_TRANSLATION_L0) ||
			   (fsc == ISS_DA_IA_FSC_TRANSLATION_L1) ||
			   (fsc == ISS_DA_IA_FSC_TRANSLATION_L2) ||
			   (fsc == ISS_DA_IA_FSC_TRANSLATION_L3)) {
			// A translation fault may be a vdevice associated with
			// an IPA, with no underlying physical memory. Note
			// that the IPA is always valid for a translation
			// fault.
			assert(ipa.e == OK);
			ret = vdevice_access_ipa(ipa.r, size, &val, is_write);
		} else {
			// Wrong fault type; not handled by this module
		}
#if defined(INTERFACE_TRACE_PROFILE)
		if (ret == VCPU_TRAP_RESULT_EMULATED) {
			TRACE_PROFILE(
				2, 0U, INFO,
				"vdevice access, FAR {:#x}, is_write {:d}",
				FAR_EL2_get_VirtualAddress(&far),
				is_write ? 1U : 0U);
		}
#endif

		if (!is_write && (ret == VCPU_TRAP_RESULT_EMULATED)) {
			// Mask register value returned by handlers
			switch (sas) {
			case ISS_DA_SAS_BYTE:
				val = (uint8_t)val;
				break;
			case ISS_DA_SAS_HALFWORD:
				val = (uint16_t)val;
				break;
			case ISS_DA_SAS_WORD:
				val = (uint32_t)val;
				break;
			case ISS_DA_SAS_DOUBLEWORD:
			default:
				// No masking required
				break;
			}

			// Do we need to sign-extend the result?
			if (ESR_EL2_ISS_DATA_ABORT_get_SSE(&iss) &&
			    (size != sizeof(uint64_t))) {
				uint64_t mask = util_bit(
					(size * util_width(uint8_t)) - 1U);
				val = (val ^ mask) - mask;
			}

			// Adjust the width if necessary. Note that this is
			// done after sign-extension to implement 32-bit signed
			// load instructions: the W variants of LDRSB and
			// LDRSH.
			if (!ESR_EL2_ISS_DATA_ABORT_get_SF(&iss)) {
				val = (uint32_t)val;
			}

			vcpu_gpr_write(thread, reg_num, val);

			if (is_acquire_release) {
				atomic_thread_fence(memory_order_acquire);
			}
		}
	}

	return ret;
}
