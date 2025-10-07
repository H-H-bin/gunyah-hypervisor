// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
#include "event_handlers.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#include "cpptest.h"
extern void
			 T32_Fdx_Waiting(void);
#pragma clang diagnostic pop

void
coverage_handle_boot_runtime_first_init(void)
{
	T32_Fdx_Waiting();
	CppTest_InitializeRuntime();
}

void
coverage_handle_scheduler_stop(void)
{
	CppTest_SendCoverage_fdx();
}

void
coverage_handle_abort_kernel(void)
{
	CppTest_SendCoverage_fdx();
}
