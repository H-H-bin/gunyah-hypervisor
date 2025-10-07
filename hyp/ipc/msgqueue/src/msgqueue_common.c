// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <scheduler.h>
#include <spinlock.h>
#include <util.h>
#include <vic.h>
#include <virq.h>

#include "event_handlers.h"
#include "msgqueue_common.h"
#include "useraccess.h"

bool_result_t
msgqueue_send_msg(msgqueue_t *msgqueue, size_t size, kernel_or_gvaddr_t msg,
		  bool push, bool from_kernel)
{
	bool_result_t ret;

	assert(msgqueue != NULL);

	spinlock_acquire(&msgqueue->lock);

	count_t count = atomic_load_relaxed(&msgqueue->count);
	if (count == msgqueue->queue_depth) {
		ret.e = ERROR_MSGQUEUE_FULL;
		ret.r = false;
		goto out;
	}
	if (size > msgqueue->max_msg_size) {
		ret.e = ERROR_ARGUMENT_SIZE;
		ret.r = false;
		goto out;
	}

	// Enqueue message at the tail of the queue
	void *hyp_va =
		(void *)(msgqueue->buf + msgqueue->tail + sizeof(size_t));

	if (from_kernel) {
		(void)memscpy(hyp_va, msgqueue->max_msg_size,
			      (void *)msg.kernel_addr, size);
	} else {
		size_result_t ret_val = useraccess_copy_from_guest_va(
			hyp_va, msgqueue->max_msg_size, msg.guest_addr, size);
		if (ret_val.e != OK) {
			ret.e = ret_val.e;
			ret.r = false;
			goto out;
		}
	}

	// Update the message size header
	uintptr_t msg_size_hdr_addr = (uintptr_t)msgqueue->buf + msgqueue->tail;
	assert_debug(util_is_baligned(msg_size_hdr_addr, alignof(size_t)));

	size_t *msg_size_hdr = (size_t *)msg_size_hdr_addr;
	*msg_size_hdr	     = size;

	// Increment number of queued messages
	count++;
	atomic_store_relaxed(&msgqueue->count, count);

	// Update the tail pointer
	msgqueue->tail += (count_t)(msgqueue->max_msg_size + sizeof(size_t));
	assert_debug(msgqueue->tail <= msgqueue->queue_size);

	if (msgqueue->tail == msgqueue->queue_size) {
		msgqueue->tail = 0U;
	}

	// If buffer was previously below the not empty threshold, we must
	// wake up the receiver side by asserting the receiver virq source.
	if (push || (count == atomic_load_relaxed(&msgqueue->notempty_thd))) {
		(void)virq_assert(&msgqueue->rcv_source, false);
	}

	ret = bool_result_ok(count != msgqueue->queue_depth);

out:
	spinlock_release(&msgqueue->lock);

	return ret;
}

receive_info_result_t
msgqueue_receive_msg(msgqueue_t *msgqueue, kernel_or_gvaddr_t buffer,
		     size_t max_size, bool to_kernel)
{
	receive_info_result_t ret = { .e = OK };
	size_t		      size;

	ret.r.notempty = true;

	assert(msgqueue != NULL);
	assert(msgqueue->buf != NULL);

	spinlock_acquire(&msgqueue->lock);

	count_t count = atomic_load_relaxed(&msgqueue->count);
	if (count == 0U) {
		ret.e	       = ERROR_MSGQUEUE_EMPTY;
		ret.r.notempty = false;
		goto out;
	}

	// Read the message size header
	uintptr_t msg_size_hdr_addr = (uintptr_t)msgqueue->buf + msgqueue->head;
	assert_debug(util_is_baligned(msg_size_hdr_addr, alignof(size_t)));

	size_t *msg_size_hdr = (size_t *)msg_size_hdr_addr;
	size		     = *msg_size_hdr;
	assert_debug((size > 0U) && (size <= msgqueue->max_msg_size));

	// Dequeue message from the head of the queue
	void *hyp_va =
		(void *)(msgqueue->buf + msgqueue->head + sizeof(size_t));

	if (to_kernel) {
		if (size > max_size) {
			ret.e	       = ERROR_ARGUMENT_SIZE;
			ret.r.notempty = false;
			goto out;
		}

		(void)memscpy((void *)buffer.kernel_addr, max_size, hyp_va,
			      size);
	} else {
		size_result_t ret_val = useraccess_copy_to_guest_va(
			buffer.guest_addr, max_size, hyp_va, size, false);
		if (ret_val.e != OK) {
			ret.e	       = ret_val.e;
			ret.r.notempty = false;
			goto out;
		}
	}

	// Decrement number of queued messages
	count--;
	atomic_store_relaxed(&msgqueue->count, count);

	// Update head value
	msgqueue->head += (count_t)(msgqueue->max_msg_size + sizeof(size_t));
	assert_debug(msgqueue->head <= msgqueue->queue_size);

	if (msgqueue->head == msgqueue->queue_size) {
		msgqueue->head = 0U;
	}

	// If buffer was previously above the not full threshold, we must let
	// the sender side know that it can send more messages.
	if (count == atomic_load_relaxed(&msgqueue->notfull_thd)) {
		// We wake up the sender side by asserting the sender virq
		// source.
		(void)virq_assert(&msgqueue->send_source, false);
	}

	ret.r.size     = size;
	ret.r.notempty = count != 0U;

out:
	spinlock_release(&msgqueue->lock);

	return ret;
}

void
msgqueue_flush_queue(msgqueue_t *msgqueue)
{
	assert(msgqueue != NULL);
	assert(msgqueue->buf != NULL);

	spinlock_acquire(&msgqueue->lock);

	// If there is a pending bound interrupt, it will be de-asserted
	if (atomic_load_relaxed(&msgqueue->count) != 0U) {
		atomic_store_relaxed(&msgqueue->count, 0U);
		(void)virq_assert(&msgqueue->send_source, false);
		(void)virq_clear(&msgqueue->rcv_source);
	}

	msgqueue->head = 0U;
	msgqueue->tail = 0U;

	spinlock_release(&msgqueue->lock);
}

error_t
msgqueue_bind(msgqueue_t *msgqueue, vic_t *vic, virq_t virq,
	      virq_source_t *source, virq_trigger_t trigger)
{
	assert(msgqueue != NULL);
	assert(vic != NULL);
	assert(source != NULL);

	error_t ret = vic_bind_shared(source, vic, virq, trigger);

	return ret;
}

void
msgqueue_unbind(virq_source_t *source)
{
	assert(source != NULL);

	vic_unbind_sync(source);
}

bool
msgqueue_rx_handle_virq_check_pending(virq_source_t *source, bool reasserted)
{
	bool ret;

	assert(source != NULL);

	msgqueue_t *msgqueue = msgqueue_container_of_rcv_source(source);

	if (reasserted) {
		// Previous VIRQ wasn't delivered yet. If we return false in
		// this case, we can't be sure that we won't race with a
		// msgqueue_send_msg() on another CPU.
		ret = true;
	} else {
		// These two variable accesses are not ordered with respect to
		// each other, but they are ordered after the load that
		// determined that the reasserted argument should be false.
		// If we race with a function that increases the count or
		// decreases the threshold, such that we incorrectly return
		// false, then that other function's virq_assert() will
		// override the false result.
		ret = (atomic_load_relaxed(&msgqueue->count) >=
		       atomic_load_relaxed(&msgqueue->notempty_thd));
	}

	return ret;
}

bool
msgqueue_tx_handle_virq_check_pending(virq_source_t *source, bool reasserted)
{
	bool ret;

	assert(source != NULL);

	msgqueue_t *msgqueue = msgqueue_container_of_send_source(source);

	if (reasserted) {
		// Previous VIRQ wasn't delivered yet. If we return false in
		// this case, we can't be sure that we won't race with a
		// msgqueue_receive_msg() on another CPU.
		ret = true;
	} else {
		// These two variable accesses are not ordered with respect to
		// each other, but they are ordered after the load that
		// determined that the reasserted argument should be false.
		// If we race with a function that decreases the count or
		// increases the threshold, such that we incorrectly return
		// false, then that other function's virq_assert() will
		// override the false result.
		ret = (atomic_load_relaxed(&msgqueue->count) <=
		       atomic_load_relaxed(&msgqueue->notfull_thd));
	}

	return ret;
}
