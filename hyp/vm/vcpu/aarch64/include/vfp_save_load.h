// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(ARCH_ARM_FEAT_SVE) || defined(ARCH_ARM_FEAT_SME)
void vfp_save_v_regs(vector_register_t (*v)[32],
		     asm_ordering_dummy_t *ordering_var);

void vfp_load_v_regs(vector_register_t (*v)[32],
		     asm_ordering_dummy_t *ordering_var);
#endif

#if defined(ARCH_ARM_FEAT_SVE)
void vfp_save_sve_z_regs(sve_vector_register_t (*z)[32],
			 asm_ordering_dummy_t *ordering_var);

void vfp_load_sve_z_regs(sve_vector_register_t (*z)[32],
			 asm_ordering_dummy_t *ordering_var);

void vfp_save_sve_pffr_regs(sve_predicate_register_t (*pffr)[17],
			    asm_ordering_dummy_t *ordering_var);

void vfp_load_sve_pffr_regs(sve_predicate_register_t (*pffr)[17],
			    asm_ordering_dummy_t *ordering_var);
#endif

#if defined(ARCH_ARM_FEAT_SME)
void vfp_save_streaming_z_regs(streaming_vector_register_t (*z)[32],
			       asm_ordering_dummy_t *ordering_var);

void vfp_load_streaming_z_regs(streaming_vector_register_t (*z)[32],
			       asm_ordering_dummy_t *ordering_var);

void vfp_save_streaming_p_regs(streaming_predicate_register_t (*p)[16],
			       asm_ordering_dummy_t *ordering_var);

void vfp_load_streaming_p_regs(streaming_predicate_register_t (*p)[16],
			       asm_ordering_dummy_t *ordering_var);

#if defined(ARCH_ARM_FEAT_SME_FA64)
void vfp_save_streaming_pffr_regs(streaming_predicate_register_t (*pffr)[17],
				  asm_ordering_dummy_t *ordering_var);

void vfp_load_streaming_pffr_regs(streaming_predicate_register_t (*pffr)[17],
				  asm_ordering_dummy_t *ordering_var);
#endif

void vfp_save_za_storage(za_array_vectors_t (*za)[PLATFORM_SME_REG_SIZE / 16U],
			 asm_ordering_dummy_t *ordering_var);

void vfp_load_za_storage(za_array_vectors_t (*za)[PLATFORM_SME_REG_SIZE / 16U],
			 asm_ordering_dummy_t *ordering_var);
#endif
