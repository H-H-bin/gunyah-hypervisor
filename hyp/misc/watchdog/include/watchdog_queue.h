// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

extern watchdog_queue_t watchdog_queue;

void
watchdog_enqueue(watchdog_t *wdt, watchdog_abs_ticks_t timeout,
		 watchdog_action_t action);

void
watchdog_dequeue(watchdog_t *wdt);

watchdog_t *
watchdog_queue_head(void);

void
watchdog_queue_acquire_lock(void) ACQUIRE_LOCK(watchdog_queue.lock)
	ACQUIRE_PREEMPT_DISABLED;

void
watchdog_queue_release_lock(void) RELEASE_LOCK(watchdog_queue.lock)
	RELEASE_PREEMPT_DISABLED;
