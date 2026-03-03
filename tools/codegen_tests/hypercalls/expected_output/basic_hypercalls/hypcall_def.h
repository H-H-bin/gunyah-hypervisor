// Automatically generated. Do not modify.
//
// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Hypercall numbers
// 0x6000 : test_hypercall_1
// 0x6001 : test_hypercall_2
// 0x6002 : test_hypercall_3

#if !defined(__ASSEMBLER__)

error_code_t
hypercall_test_hypercall_1(interrupt_control_register_t in0)
	EXCLUDE_PREEMPT_DISABLED;

error_code_t
hypercall_test_hypercall_2(uint64_t in0, count_t in1,
			   interrupt_control_register_t in2)
	EXCLUDE_PREEMPT_DISABLED;

typedef struct hypercall_test_hypercall_3_result {
	uint64_t _Alignas(register_t) out0;
	count_t _Alignas(register_t) out1;
	uint8_t _pad0[4]; // Pad for struct static zero initialization
	error_code_t _Alignas(register_t) error;
	uint8_t _pad1[4]; // Pad for struct static zero initialization
} hypercall_test_hypercall_3_result_t;

hypercall_test_hypercall_3_result_t
hypercall_test_hypercall_3(uint64_t in0) EXCLUDE_PREEMPT_DISABLED;

#else
#define HYPERCALL_BASE 0x6000U
#define HYPERCALL_NUM  3U
#endif
