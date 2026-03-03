// Automatically generated. Do not modify.

#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>

#include <compiler.h>
#include <thread.h>
#include <trace.h>

error_code_t
hypercall_test_hypercall_1__c_wrapper(interrupt_control_register_t in0)

	REQUIRE_PREEMPT_DISABLED EXCLUDE_RCU_READ;

error_code_t
hypercall_test_hypercall_1__c_wrapper(interrupt_control_register_t in0)
{
	error_code_t ret_;

	thread_entry_from_user(THREAD_ENTRY_REASON_HYPERCALL);

	TRACE(USER, HYPERCALL, "test_hypercall_1: {:#x}",
	      (register_t)in0.bf[0]);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_1(in0);
	goto out;

out:

	TRACE(USER, HYPERCALL, "test_hypercall_1 ret: {:#x}", (register_t)ret_);

	thread_exit_to_user(THREAD_ENTRY_REASON_HYPERCALL);

	return ret_;
}

error_code_t
hypercall_test_hypercall_2__c_wrapper(uint64_t in0, count_t in1,
				      interrupt_control_register_t in2)

	REQUIRE_PREEMPT_DISABLED EXCLUDE_RCU_READ;

error_code_t
hypercall_test_hypercall_2__c_wrapper(uint64_t in0, count_t in1,
				      interrupt_control_register_t in2)
{
	error_code_t ret_;

	thread_entry_from_user(THREAD_ENTRY_REASON_HYPERCALL);

	TRACE(USER, HYPERCALL, "test_hypercall_2: {:#x} {:#x} {:#x}",
	      (register_t)in0, (register_t)in1, (register_t)in2.bf[0]);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_2(in0, in1, in2);
	goto out;

out:

	TRACE(USER, HYPERCALL, "test_hypercall_2 ret: {:#x}", (register_t)ret_);

	thread_exit_to_user(THREAD_ENTRY_REASON_HYPERCALL);

	return ret_;
}

static_assert(sizeof(hypercall_test_hypercall_3_result_t) <=
		      (8U * sizeof(register_t)),
	      "Return structure must fit in 8 machine registers");

hypercall_test_hypercall_3_result_t
hypercall_test_hypercall_3__c_wrapper(uint64_t in0)

	REQUIRE_PREEMPT_DISABLED EXCLUDE_RCU_READ;

hypercall_test_hypercall_3_result_t
hypercall_test_hypercall_3__c_wrapper(uint64_t in0)
{
	hypercall_test_hypercall_3_result_t ret_;

	thread_entry_from_user(THREAD_ENTRY_REASON_HYPERCALL);

	TRACE(USER, HYPERCALL, "test_hypercall_3: {:#x}", (register_t)in0);

	// FIXME: unchecked reserved inputs

	ret_ = hypercall_test_hypercall_3(in0);
	goto out;

out:

	TRACE(USER, HYPERCALL, "test_hypercall_3 ret: {:#x} {:#x} {:#x}",
	      (register_t)ret_.out0, (register_t)ret_.out1,
	      (register_t)ret_.error);

	thread_exit_to_user(THREAD_ENTRY_REASON_HYPERCALL);

	return ret_;
}
