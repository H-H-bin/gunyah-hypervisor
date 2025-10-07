// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Initialise a task queue entry.
void
task_queue_init(task_queue_entry_t *entry);

// Schedule future execution of a task queue entry for the given class.
//
// All calls to this function for the same entry must be serialised by the
// caller.
//
// The caller also must ensure that the entry is not freed until the task has
// executed. This can be done by taking a reference to the object containing the
// entry before calling this function, and releasing it in the
// task_queue_execute handler or if this function fails.
//
// In implementations that share task queues between CPUs or allow cross-CPU
// execution of tasks, this call implies a release memory barrier that matches
// an acquire memory barrier before the task_queue_execute handler starts.
//
// If the task was already queued, this function returns false. Otherwise, the
// task_queue_execute handler will be executed once per successful call.
bool
task_queue_schedule(task_queue_entry_t *entry, task_queue_class_t class);
