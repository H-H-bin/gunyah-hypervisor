// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier; BSD-3-Clause

// Issue a TLBI that tries to clear a TLB conflict for S1 or S1+S2 entries at
// the specified VA. If the s1ptw flag is not set, only clears leaf entries.
// This does not synchronise the TLBI; the caller must execute a DSB and
// possibly an ISB.
void
addrspace_clear_s1_tlb_conflict(gvaddr_t guest_va, bool s1ptw,
				asm_ordering_dummy_t *ordering);

// Issue a TLBI that tries to clear a TLB conflict for S2 entries at the
// specified IPA. This does not synchronise the TLBI; the caller must execute a
// DSB and possibly an ISB.
void
addrspace_clear_s2_tlb_conflict(vmaddr_t	      guest_ipa,
				asm_ordering_dummy_t *ordering);
