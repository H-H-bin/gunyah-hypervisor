// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <compiler.h>
#include <list.h>
#include <panic.h>
#include <platform_watchdog.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>
#include <watchdog.h>

#include "event_handlers.h"
#include "watchdog_queue.h"

watchdog_queue_t watchdog_queue;

static bool
is_timeout_a_smaller_than_b(list_node_t *node_a, list_node_t *node_b)
{
	assert(node_a != NULL);
	assert(node_b != NULL);

	bool smaller = false;

	watchdog_abs_ticks_t timeout_a =
		watchdog_container_of_watchdog_queue_list_node(node_a)
			->next_timeout;
	watchdog_abs_ticks_t timeout_b =
		watchdog_container_of_watchdog_queue_list_node(node_b)
			->next_timeout;

	if ((timeout_a - timeout_b) > (uint64_t)INT64_MAX) {
		smaller = true;
	}

	return smaller;
}

void
watchdog_queue_acquire_lock(void) LOCK_IMPL
{
	spinlock_acquire(&watchdog_queue.lock);
}

void
watchdog_queue_release_lock(void) LOCK_IMPL
{
	spinlock_release(&watchdog_queue.lock);
}

// Must be called with the watchdog queue lock held
watchdog_t *
watchdog_queue_head(void)
{
	watchdog_t *wdt = NULL;

	list_node_t *head = list_get_head(&watchdog_queue.list);

	if (head != NULL) {
		wdt = watchdog_container_of_watchdog_queue_list_node(head);
	}

	return wdt;
}

// Must be called with the watchdog queue lock held
void
watchdog_enqueue(watchdog_t *wdt, watchdog_abs_ticks_t timeout,
		 watchdog_action_t action)
{
	if (compiler_unexpected(wdt->next_timeout !=
				WATCHDOG_INVALID_ABS_TIMEOUT)) {
		// This watchdog object is already queued
		panic("Request to queue a watchdog that is already queued");
	}

	wdt->action	  = action;
	wdt->next_timeout = timeout;

	bool new_head = list_insert_in_order(&watchdog_queue.list,
					     &wdt->watchdog_queue_list_node,
					     is_timeout_a_smaller_than_b);

	if (new_head) {
		watchdog_queue.next_timeout = wdt->next_timeout;
	}
}

// Must be called with the watchdog queue lock held
void
watchdog_dequeue(watchdog_t *wdt)
{
	if (compiler_unexpected(wdt->next_timeout ==
				WATCHDOG_INVALID_ABS_TIMEOUT)) {
		// This watchdog object is not queued
		panic("Request to dequeue a watchdog that is not queued");
	}

	bool new_head = list_delete_node(&watchdog_queue.list,
					 &wdt->watchdog_queue_list_node);

	if (new_head) {
		list_node_t	    *head = list_get_head(&watchdog_queue.list);
		watchdog_abs_ticks_t timeout =
			watchdog_container_of_watchdog_queue_list_node(head)
				->next_timeout;

		watchdog_queue.next_timeout = timeout;
	} else if (list_is_empty(&watchdog_queue.list)) {
		watchdog_queue.next_timeout = WATCHDOG_INVALID_ABS_TIMEOUT;
	} else {
		// The deleted node is not the head. Nothing to do.
	}

	wdt->next_timeout = WATCHDOG_INVALID_ABS_TIMEOUT;
}

void
watchdog_queue_handle_boot_cold_init(void)
{
	spinlock_init(&watchdog_queue.lock);
	watchdog_queue.next_timeout = WATCHDOG_INVALID_ABS_TIMEOUT;
	list_init(&watchdog_queue.list);
}
