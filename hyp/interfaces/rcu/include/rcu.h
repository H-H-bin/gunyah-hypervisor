// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Start a read-side critical section.
//
// An RCU read-side critical section blocks execution of any update functions
// until it is ended by a matching call to rcu_read_finish(). These critical
// sections are permitted to nest; each call to this function must be balanced
// by exactly one matching call to rcu_read_finish().
//
// The caller must not assume that this function disables or does not disable
// preemption.
void
rcu_read_start(void) ACQUIRE_RCU_READ;

// End a read-side critical section.
//
// This reverses the effect of the most recent unmatched rcu_read_start() call.
// If this ends the outermost nested critical section, then RCU update functions
// queued on any CPU after the start of the critical section are permitted to
// run.
void
rcu_read_finish(void) RELEASE_RCU_READ;

// Enqueue a write-side update.
//
// The given update will be processed at the end of the next grace period.
//
// The update is guaranteed not to run until all currently executing RCU
// critical sections have finished. However, there is no guarantee of ordering
// of separately enqueued updates with respect to each other. Nor is there a
// guarantee that they will run on the CPU that enqueued them.
void
rcu_enqueue(rcu_entry_t *rcu_entry, rcu_update_class_t rcu_update_class);

// Block until the current grace period ends.
//
// This is typically implemented using rcu_enqueue(), and therefore provides the
// same ordering guarantee: any currently executing RCU critical section must
// finish before this function can return, but other currently queued updates or
// rcu_sync() calls may not have completed.
void
rcu_sync(void) EXCLUDE_RCU_READ;

// Block until the next grace period ends or the caller is killed.
//
// If this call returns true, it has the same semantics as rcu_sync(). If it
// returns false, the caller has been killed, and there are no ordering
// guarantees provided by RCU.
//
// Note that if this returns false, the caller has preemption disabled and may
// be running while blocked and/or on a CPU it does not have affinity to.
bool
rcu_sync_killable(void) EXCLUDE_RCU_READ;

// Attempt to expedite completion of a recently enqueued update.
//
// This may be called to immediately perform any work on the local CPU that is
// necessary for completion of any updates that have already been queued, if it
// is possible to do so. Note that this does not affect other CPUs, so the grace
// period is not guaranteed to complete before the function returns.
//
// This is used by the rcu_sync() implementation to avoid unnecessary
// rescheduling.
//
// This function:
// - May run RCU update handlers, so it must not be called from an RCU read-side
//   critical section.
// - May call scheduler_yield(), so it must not be called with locks held.
// - May call ipi_clear(), so it must not be called from an IPI handler.
void
rcu_expedite(void) EXCLUDE_RCU_READ;

// Check for pending updates on the calling CPU.
//
// If this call returns true, the CPU should not be allowed to enter a low power
// state or power itself off.
bool
rcu_has_pending_updates(void) REQUIRE_PREEMPT_DISABLED;
