// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(ARCH_ARM_FEAT_SVE) || defined(ARCH_ARM_FEAT_SME)
// Disable the SVE / SME traps in CPTR_EL2.
CPTR_EL2_E2H1_t
vfp_enable_hw_access(asm_ordering_dummy_t *ordering_var);

// Write to the CPTR_EL2 register.
void
vfp_restore_cptr(CPTR_EL2_E2H1_t cptr, asm_ordering_dummy_t *ordering_var);
#endif

#if defined(ARCH_ARM_FEAT_SVE)
// Read the current non-streaming vector length.
register_t
vfp_rdvl(asm_ordering_dummy_t *ordering_var);

// Check whether SVE is implemented by reading the ID_AA64PFR0_EL1 register.
bool
vfp_sve_implemented(void);
#endif

#if defined(ARCH_ARM_FEAT_SME)
// Read the current streaming vector length.
register_t
vfp_rdsvl(asm_ordering_dummy_t *ordering_var);

// Check whether SME is implemented by reading the ID_AA64PFR1_EL1 register.
bool
vfp_sme_implemented(void);
#endif
