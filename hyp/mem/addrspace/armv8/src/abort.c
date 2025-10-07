// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(INTERFACE_VCPU)
#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <addrspace.h>
#include <atomic.h>
#include <qcbor.h>
#include <rcu.h>
#include <thread.h>

#include <asm/barrier.h>

#include "addrspace_lookup.h"
#include "event_handlers.h"

static void
addrspace_handle_guest_tlb_conflict(vmaddr_result_t ipa, FAR_EL2_t far,
				    bool s1ptw)
{
	// If this fault was not on a stage 1 PT walk, the ipa argument is not
	// valid, because the architecture allows the TLB to avoid caching it.
	// We can do a lookup on the VA to try to find it. This may fail if the
	// CPU caches S1-only translations and the conflict is in that cache.
	//
	// For a fault on a stage 1 PT walk, the ipa argument is always valid.
	if (!s1ptw) {
		ipa = addrspace_va_to_ipa_read(
			FAR_EL2_get_VirtualAddress(&far));
	} else {
		assert(ipa.e == OK);
	}

	asm_ordering_dummy_t tlbi_s2_ordering;
	if (ipa.e == OK) {
		// If the IPA is valid, the conflict may have been between S2
		// TLB entries, so flush the IPA from the S2 TLB. Note that if
		// our IPA lookup above failed, the conflict must be in S1+S2 or
		// S1-only entries, so no S2 flush is needed.
		addrspace_clear_s2_tlb_conflict(ipa.r, &tlbi_s2_ordering);
	}

	// Regardless of whether the IPA is valid, there is always a possibility
	// that the conflict was on S1+S2 or S1-only entries. So we always flush
	// by VA. If the fault was on a stage 1 page table walk, the fault may
	// have been on a cached next-level entry, so we flush those too.
	asm_ordering_dummy_t tlbi_s1_ordering;
	addrspace_clear_s1_tlb_conflict(FAR_EL2_get_VirtualAddress(&far), s1ptw,
					&tlbi_s1_ordering);

	// Ensure the invalidation is complete
	__asm__ volatile("dsb nsh" ::"m"(tlbi_s1_ordering),
			 "m"(tlbi_s2_ordering));
}

// Retry faults if they may have been caused by break before make during block
// splits in the direct physical access region
static vcpu_trap_result_t
addrspace_handle_guest_translation_fault(FAR_EL2_t far)
{
	vcpu_trap_result_t ret;

	uintptr_t addr = FAR_EL2_get_VirtualAddress(&far);

	thread_t *current = thread_get_self();
	assert(current != NULL);

	addrspace_t *addrspace = current->addrspace;
	assert(addrspace != NULL);

	rcu_read_start();
	// Look up the faulting address. If there may have been a transient
	// fault due to BBM, this will return either ERROR_RETRY (if the fault
	// is still present) or OK. A result of ERROR_ADDR_INVALID indicates
	// that the S2 translation fault is still present and can't have been
	// caused by BBM.
	ret = (addrspace_va_to_pa_noretry(addr, false).e == ERROR_ADDR_INVALID)
		      ? VCPU_TRAP_RESULT_UNHANDLED
		      : VCPU_TRAP_RESULT_RETRY;
	rcu_read_finish();

	return ret;
}

vcpu_trap_result_t
addrspace_handle_vcpu_trap_data_abort_guest(ESR_EL2_t esr, vmaddr_result_t ipa,
					    FAR_EL2_t far)
{
	vcpu_trap_result_t ret;

	ESR_EL2_ISS_DATA_ABORT_t iss =
		ESR_EL2_ISS_DATA_ABORT_cast(ESR_EL2_get_ISS(&esr));
	iss_da_ia_fsc_t fsc = ESR_EL2_ISS_DATA_ABORT_get_DFSC(&iss);

	// Only translation faults can be caused by BBM
	if ((fsc == ISS_DA_IA_FSC_TRANSLATION_L0) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L1) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L2) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L3)) {
		ret = addrspace_handle_guest_translation_fault(far);
	} else if (fsc == ISS_DA_IA_FSC_TLB_CONFLICT) {
		addrspace_handle_guest_tlb_conflict(
			ipa, far, ESR_EL2_ISS_DATA_ABORT_get_S1PTW(&iss));
		ret = VCPU_TRAP_RESULT_RETRY;
	} else {
		ret = VCPU_TRAP_RESULT_UNHANDLED;
	}

	return ret;
}

vcpu_trap_result_t
addrspace_handle_vcpu_trap_pf_abort_guest(ESR_EL2_t esr, vmaddr_result_t ipa,
					  FAR_EL2_t far)
{
	vcpu_trap_result_t ret;

	ESR_EL2_ISS_INST_ABORT_t iss =
		ESR_EL2_ISS_INST_ABORT_cast(ESR_EL2_get_ISS(&esr));
	iss_da_ia_fsc_t fsc = ESR_EL2_ISS_INST_ABORT_get_IFSC(&iss);

	// Only translation faults can be caused by BBM
	if ((fsc == ISS_DA_IA_FSC_TRANSLATION_L0) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L1) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L2) ||
	    (fsc == ISS_DA_IA_FSC_TRANSLATION_L3)) {
		ret = addrspace_handle_guest_translation_fault(far);
	} else if (fsc == ISS_DA_IA_FSC_TLB_CONFLICT) {
		addrspace_handle_guest_tlb_conflict(
			ipa, far, ESR_EL2_ISS_INST_ABORT_get_S1PTW(&iss));
		ret = VCPU_TRAP_RESULT_RETRY;
	} else {
		ret = VCPU_TRAP_RESULT_UNHANDLED;
	}

	return ret;
}
#else
extern char unused;
#endif
