// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <cpulocal.h>
#include <ipi.h>
#include <preempt.h>
#include <task_queue.h>

#include <events/task_queue.h>

#include "event_handlers.h"

CPULOCAL_DECLARE_STATIC(task_queue_entry_t *, task_queue_head);

void
task_queue_init(task_queue_entry_t *entry)
{
	atomic_init(&entry->bf, task_queue_entry_bf_default());
}

bool
task_queue_schedule(task_queue_entry_t *entry, task_queue_class_t class)
{
	bool ret = false;

	assert(class != TASK_QUEUE_CLASS_NONE);

	// Disable preemption to make CPU-local accesses safe, and to ensure
	// this operation does not race with the IPI received handler below.
	// Scheduled tasks will always be executed on the same CPU, so no
	// locking is required here.
	preempt_disable();

	cpu_index_t	    cpu	 = cpulocal_get_index();
	task_queue_entry_t *head = CPULOCAL_BY_INDEX(task_queue_head, cpu);

	// The entry will be placed at the head of the list.
	task_queue_entry_bf_t new_bf = task_queue_entry_bf_default();
	task_queue_entry_bf_set_class(&new_bf, class);
	task_queue_entry_bf_set_next(&new_bf, head);

	// This fence ensures that the attempt to schedule the task is always
	// ordered after earlier memory accesses, such that a failure to
	// schedule the task will still result in these accesses being observed
	// by the next task execution. This must be a sequentially-consistent
	// fence to order the load in the compare-exchange after earlier stores.
	atomic_thread_fence(memory_order_seq_cst);

	// Use compare-exchange to update the entry. If this fails, the task is
	// already pending.
	task_queue_entry_bf_t old_bf = task_queue_entry_bf_default();
	if (atomic_compare_exchange_strong_explicit(&entry->bf, &old_bf, new_bf,
						    memory_order_relaxed,
						    memory_order_relaxed)) {
		// Entry successfully updated; place the entry at the head of
		// the list, and raise a relaxed IPI for later execution.
		CPULOCAL_BY_INDEX(task_queue_head, cpu) = entry;
		ipi_one_relaxed(IPI_REASON_TASK_QUEUE, cpu);
		ret = true;
	} else {
		// The task must be pending for the same class.
		assert(task_queue_entry_bf_get_class(&old_bf) == class);
	}

	preempt_enable();

	return ret;
}

bool
task_queue_handle_ipi_received(void)
{
	assert_preempt_disabled();

	cpu_index_t	    cpu	  = cpulocal_get_index();
	task_queue_entry_t *entry = CPULOCAL_BY_INDEX(task_queue_head, cpu);

	while (entry != NULL) {
		// Get the class for the current entry.
		task_queue_entry_bf_t bf = atomic_load_relaxed(&entry->bf);
		task_queue_class_t class = task_queue_entry_bf_get_class(&bf);
		assert(class != TASK_QUEUE_CLASS_NONE);

		// Clear out the entry so it can be reused.
		atomic_store_relaxed(&entry->bf, task_queue_entry_bf_default());

		// This sequentially-consistent fence matches the one in
		// task_queue_schedule(), and ensures that either this CPU
		// clears the entry prior to the next attempt to schedule the
		// task, or that the schedule fails and its earlier memory
		// accesses are observed by task execution below.
		atomic_thread_fence(memory_order_seq_cst);

		// Execute the task.
		error_t err = trigger_task_queue_execute_event(class, entry);
		assert(err == OK);

		// Handle the next entry.
		entry = task_queue_entry_bf_get_next(&bf);
	}

	// All pending tasks have been handled; clear the list.
	CPULOCAL_BY_INDEX(task_queue_head, cpu) = NULL;

	return true;
}
