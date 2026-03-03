// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <cpulocal.h>
#include <panic.h>
#include <power.h>
#include <scheduler.h>
#include <thread.h>

#if defined(INTERFACE_VCPU_RUN)
#include <vcpu.h>
#include <vcpu_run.h>
#endif

#include "event_handlers.h"

error_t
vcpu_power_handle_vcpu_poweron(thread_t *vcpu)
{
	assert(!vcpu->vcpu_power_should_vote);
	vcpu->vcpu_power_should_vote = true;

	cpu_index_t cpu	     = scheduler_get_affinity(vcpu);
	bool	    can_vote = cpulocal_index_valid(cpu);

#if defined(INTERFACE_VCPU_RUN)
	if (vcpu_run_is_enabled(vcpu)) {
		can_vote = false;
	}
#endif

	error_t ret;
	if (can_vote) {
		ret = power_vote_cpu_on(cpu);
	} else {
		ret = OK;
	}

	if (ret != OK) {
		// Failed to power on vcpu rollback the vote flag
		vcpu->vcpu_power_should_vote = false;
	}

	return ret;
}

void
vcpu_power_unwind_vcpu_poweron(thread_t *vcpu)
{
	assert_debug(vcpu->vcpu_power_should_vote);

	(void)vcpu_power_handle_vcpu_poweroff(vcpu);
}

error_t
vcpu_power_handle_vcpu_poweroff(thread_t *current)
{
	assert((current != NULL) && current->vcpu_power_should_vote);
	current->vcpu_power_should_vote = false;

	cpu_index_t cpu	     = scheduler_get_affinity(current);
	bool	    can_vote = cpulocal_index_valid(cpu);
#if defined(INTERFACE_VCPU_RUN)
	if (vcpu_run_is_enabled(current)) {
		can_vote = false;
	}
#endif

	if (can_vote) {
		power_vote_cpu_off(cpu);
	}

	return OK;
}

void
vcpu_power_handle_vcpu_stopped(void)
{
	thread_t *vcpu = thread_get_self();
	assert((vcpu != NULL) && (vcpu_is_vcpu(vcpu)));

	scheduler_lock_nopreempt(vcpu);

	// If the current thread is dying, we want to keep the cpu power vote
	// since it will need to be able to be scheduled to run to
	// thread_exit();
	// If the thread is dying, it has been killed but has not yet exited,
	// so don't vote the cpu off. Once the thread_exit is run, the vcpu's
	// state changes to exited, and the vcpu_stopped event is called once
	// again, so we will release the power vote here.
	if (vcpu->vcpu_power_should_vote && !thread_is_dying(vcpu)) {
		vcpu->vcpu_power_should_vote = false;

		cpu_index_t cpu	     = scheduler_get_affinity(vcpu);
		bool	    can_vote = cpulocal_index_valid(cpu);
#if defined(INTERFACE_VCPU_RUN)
		if (vcpu_run_is_enabled(vcpu)) {
			can_vote = false;
		}
#endif

		if (can_vote) {
			power_vote_cpu_off(cpu);
		}
	}

	scheduler_unlock_nopreempt(vcpu);
}

#if defined(INTERFACE_VCPU_RUN)
void
vcpu_power_handle_vcpu_run_enabled(thread_t *vcpu)
{
	cpu_index_t cpu	     = scheduler_get_affinity(vcpu);
	bool	    can_vote = cpulocal_index_valid(cpu);

	if (can_vote && vcpu->vcpu_power_should_vote) {
		power_vote_cpu_off(cpu);
	}
}
#endif

error_t
vcpu_power_handle_scheduler_set_affinity_prepare(thread_t   *thread,
						 cpu_index_t prev_cpu,
						 cpu_index_t next_cpu)
{
	error_t ret = OK;
	assert(prev_cpu != next_cpu);

	if (!vcpu_is_vcpu(thread)) {
		goto out;
	}

#if defined(INTERFACE_VCPU_RUN)
	if (vcpu_run_is_enabled(thread)) {
		goto out;
	}
#endif

	if (thread->vcpu_power_should_vote) {
		if (cpulocal_index_valid(next_cpu)) {
			ret = power_vote_cpu_on(next_cpu);
		}
		if ((ret == OK) && cpulocal_index_valid(prev_cpu)) {
			power_vote_cpu_off(prev_cpu);
		}
	}

out:
	return ret;
}

void
vcpu_power_handle_thread_killed(thread_t *thread)
{
	assert(thread != NULL);

	scheduler_lock(thread);

	if (!vcpu_is_vcpu(thread)) {
		goto out;
	}

	// The vcpu has been killed, and will need to be unblocked and allowed
	// to run to thread_exit(). We need to vote the vcpu's cpu on in case
	// it was already off for this to happen.  Once the thread exits, it
	// will drop the vote again.
	if (!thread->vcpu_power_should_vote) {
		thread->vcpu_power_should_vote = true;

		cpu_index_t cpu	     = scheduler_get_affinity(thread);
		bool	    can_vote = cpulocal_index_valid(cpu);
#if defined(INTERFACE_VCPU_RUN)
		if (vcpu_run_is_enabled(thread)) {
			can_vote = false;
		}
#endif

		if (can_vote) {
			error_t err = power_vote_cpu_on(cpu);
			if (err != OK) {
				panic("Failed to power on cpu when thread killed");
			}
		}
	}

out:
	scheduler_unlock(thread);
}
