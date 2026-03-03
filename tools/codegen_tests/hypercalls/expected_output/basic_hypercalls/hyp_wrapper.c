// Automatically generated. Do not modify.

#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>

#include <compiler.h>
#include <thread.h>
#include <trace.h>

#include "smccc_hypercall.h"

static void
test_hypercall_1__hyp_wrapper(register_t *args) EXCLUDE_PREEMPT_DISABLED
{
	error_code_t ret_;

	interrupt_control_register_t in0 =
		interrupt_control_register_cast((uint64_t)args[1]);

	TRACE(USER, HYPERCALL, "test_hypercall_1: {:#x}",
	      (register_t)in0.bf[0]);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_1(in0);

	goto out;

out:
	args[0] = (register_t)ret_;

	TRACE(USER, HYPERCALL, "test_hypercall_1 ret: {:#x}", args[0]);
}

static void
test_hypercall_2__hyp_wrapper(register_t *args) EXCLUDE_PREEMPT_DISABLED
{
	error_code_t ret_;

	uint64_t		     in0 = (uint64_t)args[1];
	count_t			     in1 = (count_t)args[2];
	interrupt_control_register_t in2 =
		interrupt_control_register_cast((uint64_t)args[3]);

	TRACE(USER, HYPERCALL, "test_hypercall_2: {:#x} {:#x} {:#x}",
	      (register_t)in0, (register_t)in1, (register_t)in2.bf[0]);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_2(in0, in1, in2);

	goto out;

out:
	args[0] = (register_t)ret_;

	TRACE(USER, HYPERCALL, "test_hypercall_2 ret: {:#x}", args[0]);
}

static void
test_hypercall_3__hyp_wrapper(register_t *args) EXCLUDE_PREEMPT_DISABLED
{
	hypercall_test_hypercall_3_result_t ret_;

	uint64_t in0 = (uint64_t)args[1];

	TRACE(USER, HYPERCALL, "test_hypercall_3: {:#x}", (register_t)in0);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_3(in0);

	goto out;

out:
	args[0] = (register_t)(ret_.out0);
	args[1] = (register_t)(ret_.out1);
	args[2] = (register_t)(ret_.error);

	TRACE(USER, HYPERCALL, "test_hypercall_3 ret: {:#x} {:#x} {:#x}",
	      args[0], args[1], args[2]);
}

void
smccc_hypercall_table_wrapper(count_t hyp_num, register_t *args)
{
	TRACE(USER, HYPERCALL, "SMCCC Hyp");

	switch (hyp_num) {
	case 0:
		test_hypercall_1__hyp_wrapper(args);
		break;
	case 1:
		test_hypercall_2__hyp_wrapper(args);
		break;
	case 2:
		test_hypercall_3__hyp_wrapper(args);
		break;
	default:
		args[0] = (register_t)SMCCC_UNKNOWN_FUNCTION64;
		break;
	}
}
