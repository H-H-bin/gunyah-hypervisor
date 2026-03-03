// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <doorbell.h>
#include <scheduler.h>
#include <spinlock.h>
#include <vic.h>
#include <virq.h>

#include "doorbell_internal.h"
#include "event_handlers.h"

doorbell_flags_result_t
doorbell_send(doorbell_t *doorbell, doorbell_flags_t new_flags)
{
	assert(doorbell != NULL);

	spinlock_acquire(&doorbell->lock);

	doorbell_flags_t	flags = atomic_load_relaxed(&doorbell->flags);
	doorbell_flags_result_t ret   = doorbell_flags_result_ok(flags);

	flags |= new_flags;

	// Level-triggered assert if there are flags enabled; else edge-only.
	bool edge_only =
		(flags & atomic_load_relaxed(&doorbell->enable_mask)) == 0U;
	// Automatically clear ack_mask flags before a level assertion.
	if (!edge_only) {
		flags &= ~doorbell->ack_mask;
	}
	atomic_store_relaxed(&doorbell->flags, flags);

	(void)virq_assert(&doorbell->source, edge_only);

	spinlock_release(&doorbell->lock);

	return ret;
}

doorbell_flags_result_t
doorbell_receive(doorbell_t *doorbell, doorbell_flags_t clear_flags)
{
	doorbell_flags_result_t ret;

	assert(doorbell != NULL);

	if (clear_flags == 0U) {
		ret = doorbell_flags_result_error(ERROR_ARGUMENT_INVALID);
		goto out;
	}

	spinlock_acquire(&doorbell->lock);

	doorbell_flags_t flags = atomic_load_relaxed(&doorbell->flags);
	ret		       = doorbell_flags_result_ok(flags);

	flags &= ~clear_flags;

	atomic_store_relaxed(&doorbell->flags, flags);
	spinlock_release(&doorbell->lock);

out:
	return ret;
}

error_t
doorbell_reset(doorbell_t *doorbell)
{
	assert(doorbell != NULL);

	spinlock_acquire(&doorbell->lock);

	atomic_store_relaxed(&doorbell->flags, 0U);
	doorbell->ack_mask = 0U;
	atomic_store_relaxed(&doorbell->enable_mask, ~doorbell->ack_mask);

	(void)virq_clear(&doorbell->source);

	spinlock_release(&doorbell->lock);

	return OK;
}

error_t
doorbell_mask(doorbell_t *doorbell, doorbell_flags_t new_enable_mask,
	      doorbell_flags_t new_ack_mask)
{
	assert(doorbell != NULL);

	spinlock_acquire(&doorbell->lock);

	doorbell_flags_t flags = atomic_load_relaxed(&doorbell->flags);
	bool		 was_asserted =
		(flags & atomic_load_relaxed(&doorbell->enable_mask)) != 0U;
	bool now_asserted = (flags & new_enable_mask) != 0U;

	atomic_store_relaxed(&doorbell->enable_mask, new_enable_mask);
	doorbell->ack_mask = new_ack_mask;

	if (was_asserted && !now_asserted) {
		// Deassert if new mask disables all currently asserted flags
		(void)virq_clear(&doorbell->source);

	} else if (!was_asserted && now_asserted) {
		// Assert if new mask enables flags that are already set
		atomic_store_relaxed(&doorbell->flags,
				     flags & ~doorbell->ack_mask);
		(void)virq_assert(&doorbell->source, false);
	} else if (was_asserted && now_asserted) {
		atomic_store_relaxed(&doorbell->flags,
				     flags & ~doorbell->ack_mask);
	} else {
		// Nothing to do.
	}

	spinlock_release(&doorbell->lock);

	return OK;
}

bool
doorbell_handle_virq_check_pending(virq_source_t *source, bool reasserted)
{
	bool ret;

	assert(source != NULL);

	doorbell_t *doorbell = doorbell_container_of_source(source);

	if (reasserted) {
		// Previous VIRQ wasn't delivered yet. If we return false in
		// this case, we can't be sure that we won't race with a
		// doorbell_send() or doorbell_mask() on another CPU.
		ret = true;
	} else {
		// These two variable accesses are not ordered with respect to
		// each other, but they are ordered after the load that
		// determined that the reasserted argument should be false.
		// If we race with a function that sets flag bits or clears mask
		// bits, such that we incorrectly return false, then that other
		// function's virq_assert() will override the false result.
		ret = ((atomic_load_relaxed(&doorbell->flags) &
			atomic_load_relaxed(&doorbell->enable_mask)) != 0U);
	}

	return ret;
}

error_t
doorbell_bind(doorbell_t *doorbell, vic_t *vic, virq_t virq)
{
	assert(doorbell != NULL);
	assert(vic != NULL);

	return vic_bind_shared(&doorbell->source, vic, virq,
			       VIRQ_TRIGGER_DOORBELL);
}

void
doorbell_unbind(doorbell_t *doorbell)
{
	assert(doorbell != NULL);

	vic_unbind_sync(&doorbell->source);
}

error_t
doorbell_handle_object_create_doorbell(doorbell_create_t doorbell_create)
{
	assert(doorbell_create.doorbell != NULL);

	spinlock_init(&doorbell_create.doorbell->lock);

	spinlock_acquire(&doorbell_create.doorbell->lock);

	atomic_init(&doorbell_create.doorbell->flags, 0U);
	doorbell_create.doorbell->ack_mask = 0U;
	atomic_init(&doorbell_create.doorbell->enable_mask,
		    ~doorbell_create.doorbell->ack_mask);

	spinlock_release(&doorbell_create.doorbell->lock);

	return OK;
}

void
doorbell_handle_object_deactivate_doorbell(doorbell_t *doorbell)
{
	assert(doorbell != NULL);

	spinlock_acquire(&doorbell->lock);

	vic_unbind(&doorbell->source);

	spinlock_release(&doorbell->lock);
}
