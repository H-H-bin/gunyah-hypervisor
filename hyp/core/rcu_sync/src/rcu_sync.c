// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <compiler.h>
#include <log.h>
#include <object.h>
#include <preempt.h>
#include <rcu.h>
#include <scheduler.h>
#include <thread.h>
#include <trace.h>
#include <util.h>

#include <asm/barrier.h>
#include <asm/event.h>

#include "event_handlers.h"

void
rcu_sync(void)
{
	rcu_sync_state_t state = {
		.waiting = true,
	};
	rcu_enqueue(&state.rcu_entry, RCU_UPDATE_CLASS_SYNC_COMPLETE);
	register_t iterations = 0U;

	rcu_expedite();

	while (atomic_load_acquire(&state.waiting)) {
		iterations++;
		(void)scheduler_schedule();
	}

	TRACE(DEBUG, INFO, "rcu_sync: {:d} iterations", iterations);
}

bool
rcu_sync_killable(void)
{
	bool killed = false;

	// We use a thread-local sync state here, so it remains valid if we
	// return early and doesn't need to be cancelled (which the RCU API
	// currently does not allow).
	static _Thread_local rcu_sync_state_t state;

	// If the state struct's flag is already set, then an earlier killable
	// sync on this thread has not yet completed. We can't reuse it as that
	// may complete too early, so just fail immediately.
	if (compiler_unexpected(atomic_load_acquire(&state.waiting))) {
		killed = true;
		goto out;
	}

	atomic_init(&state.waiting, true);
	rcu_enqueue(&state.rcu_entry, RCU_UPDATE_CLASS_SYNC_COMPLETE);
	register_t iterations = 0U;

	rcu_expedite();

	thread_t *thread = thread_get_self();
	while (atomic_load_acquire(&state.waiting)) {
		if (thread_is_dying(thread)) {
			killed = true;
			goto out;
		}
		iterations++;
		(void)scheduler_schedule();
	}

	TRACE(DEBUG, INFO, "rcu_sync_killable: {:d} iterations", iterations);
out:
	return !killed;
}

rcu_update_status_t
rcu_sync_handle_update(rcu_entry_t *entry)
{
	rcu_update_status_t ret = rcu_update_status_default();

	rcu_sync_state_t *state = rcu_sync_state_container_of_rcu_entry(entry);

	atomic_store_release(&state->waiting, false);

	return ret;
}

#if defined(UNIT_TESTS) && UNIT_TESTS
static _Atomic count_t rcu_sync_test_ready_count;
static _Atomic bool    rcu_sync_test_start_flag;
static _Atomic bool    rcu_sync_test_success_flag;

void
rcu_sync_handle_tests_init(void)
{
	atomic_init(&rcu_sync_test_ready_count, 0U);
	atomic_init(&rcu_sync_test_start_flag, false);
	atomic_init(&rcu_sync_test_success_flag, false);
}

bool
rcu_sync_handle_tests_start(void)
{
	bool failed = false;

	count_t my_order = atomic_fetch_add_explicit(&rcu_sync_test_ready_count,
						     1U, memory_order_acquire);

	if ((my_order + 1U) == PLATFORM_MAX_CORES) {
		// We're the last core to be ready; trigger the test
		asm_event_store_and_wake(&rcu_sync_test_start_flag, true);

		rcu_sync();

		// Success (unless any other CPU sees this)
		atomic_store_release(&rcu_sync_test_success_flag, true);

		// Wait until we're the last core running
		while (asm_event_load_before_wait(&rcu_sync_test_ready_count) !=
		       1U) {
			asm_event_wait(&rcu_sync_test_ready_count);
		}

		LOG(DEBUG, INFO, "rcu_sync test complete");
	} else {
		rcu_read_start();

		// Wait for the last core to trigger the test
		while (!asm_event_load_before_wait(&rcu_sync_test_start_flag)) {
			asm_event_wait(&rcu_sync_test_start_flag);
		}

		// Spin for a while to give rcu_sync() time to return early
		for (count_t i = 0;
		     i < util_bit((24 * (my_order + 1)) / PLATFORM_MAX_CORES);
		     i++) {
			asm_yield();
		}

		// Make sure the test hasn't succeeded yet; that would indicate
		// that rcu_sync() returned early
		if (atomic_load_acquire(&rcu_sync_test_success_flag)) {
			LOG(DEBUG, WARN, "rcu_sync() returned too early!");
			failed = true;
		}

		rcu_read_finish();

		// Tell the main CPU we've finished
		(void)atomic_fetch_sub_explicit(&rcu_sync_test_ready_count, 1U,
						memory_order_release);
		asm_event_wake_updated();

		// Wait for the test to finish and permit quiescent states
		while (!atomic_load_acquire(&rcu_sync_test_success_flag)) {
			scheduler_yield();
		}
	}

	return failed;
}
#endif
