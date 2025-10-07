// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

void
smccc_hypercall_table_wrapper(count_t hyp_num, register_t args[7])
	EXCLUDE_PREEMPT_DISABLED;

bool
smccc_handle_hypercall_wrapper(smccc_function_id_t smc_id, bool is_hvc)
	EXCLUDE_PREEMPT_DISABLED;
