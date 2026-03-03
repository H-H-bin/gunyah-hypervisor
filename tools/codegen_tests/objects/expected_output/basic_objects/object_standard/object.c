// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <atomic.h>
#include <attributes.h>
#include <compiler.h>
#include <object.h>
#include <panic.h>
#include <rcu.h>
#include <refcount.h>
#include <spinlock.h>

#include <events/object.h>

addrspace_t *
object_get_addrspace_additional(addrspace_t *addrspace)
{
	refcount_get_additional(&addrspace->header.refcount);

	return addrspace;
}

bool
object_get_addrspace_safe(addrspace_t *addrspace)
{
	return refcount_get_safe(&addrspace->header.refcount);
}

static void NOINLINE
object_free_addrspace(addrspace_t *addrspace)
{
	object_state_t old_state = atomic_exchange_explicit(
		&addrspace->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_addrspace_event(addrspace);
	}
	rcu_enqueue(&addrspace->header.rcu_entry,
		    RCU_UPDATE_CLASS_ADDRSPACE_DESTROY);
}

void
object_put_addrspace(addrspace_t *addrspace)
{
	if (compiler_unexpected(refcount_put(&addrspace->header.refcount))) {
		object_free_addrspace(addrspace);
	}
}

error_t
object_activate_addrspace(addrspace_t *addrspace)
{
	object_ptr_t o = { .addrspace = addrspace };
	return object_activate(OBJECT_TYPE_ADDRSPACE, o);
}

cspace_t *
object_get_cspace_additional(cspace_t *cspace)
{
	refcount_get_additional(&cspace->header.refcount);

	return cspace;
}

bool
object_get_cspace_safe(cspace_t *cspace)
{
	return refcount_get_safe(&cspace->header.refcount);
}

static void NOINLINE
object_free_cspace(cspace_t *cspace)
{
	object_state_t old_state = atomic_exchange_explicit(
		&cspace->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_cspace_event(cspace);
	}
	rcu_enqueue(&cspace->header.rcu_entry, RCU_UPDATE_CLASS_CSPACE_DESTROY);
}

void
object_put_cspace(cspace_t *cspace)
{
	if (compiler_unexpected(refcount_put(&cspace->header.refcount))) {
		object_free_cspace(cspace);
	}
}

error_t
object_activate_cspace(cspace_t *cspace)
{
	object_ptr_t o = { .cspace = cspace };
	return object_activate(OBJECT_TYPE_CSPACE, o);
}

doorbell_t *
object_get_doorbell_additional(doorbell_t *doorbell)
{
	refcount_get_additional(&doorbell->header.refcount);

	return doorbell;
}

bool
object_get_doorbell_safe(doorbell_t *doorbell)
{
	return refcount_get_safe(&doorbell->header.refcount);
}

static void NOINLINE
object_free_doorbell(doorbell_t *doorbell)
{
	object_state_t old_state = atomic_exchange_explicit(
		&doorbell->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_doorbell_event(doorbell);
	}
	rcu_enqueue(&doorbell->header.rcu_entry,
		    RCU_UPDATE_CLASS_DOORBELL_DESTROY);
}

void
object_put_doorbell(doorbell_t *doorbell)
{
	if (compiler_unexpected(refcount_put(&doorbell->header.refcount))) {
		object_free_doorbell(doorbell);
	}
}

error_t
object_activate_doorbell(doorbell_t *doorbell)
{
	object_ptr_t o = { .doorbell = doorbell };
	return object_activate(OBJECT_TYPE_DOORBELL, o);
}

gicv3_its_t *
object_get_gicv3_its_additional(gicv3_its_t *gicv3_its)
{
	refcount_get_additional(&gicv3_its->header.refcount);

	return gicv3_its;
}

bool
object_get_gicv3_its_safe(gicv3_its_t *gicv3_its)
{
	return refcount_get_safe(&gicv3_its->header.refcount);
}

static void NOINLINE
object_free_gicv3_its(gicv3_its_t *gicv3_its)
{
	object_state_t old_state = atomic_exchange_explicit(
		&gicv3_its->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_gicv3_its_event(gicv3_its);
	}
	rcu_enqueue(&gicv3_its->header.rcu_entry,
		    RCU_UPDATE_CLASS_GICV3_ITS_DESTROY);
}

void
object_put_gicv3_its(gicv3_its_t *gicv3_its)
{
	if (compiler_unexpected(refcount_put(&gicv3_its->header.refcount))) {
		object_free_gicv3_its(gicv3_its);
	}
}

error_t
object_activate_gicv3_its(gicv3_its_t *gicv3_its)
{
	object_ptr_t o = { .gicv3_its = gicv3_its };
	return object_activate(OBJECT_TYPE_GICV3_ITS, o);
}

hwirq_t *
object_get_hwirq_additional(hwirq_t *hwirq)
{
	refcount_get_additional(&hwirq->header.refcount);

	return hwirq;
}

bool
object_get_hwirq_safe(hwirq_t *hwirq)
{
	return refcount_get_safe(&hwirq->header.refcount);
}

static void NOINLINE
object_free_hwirq(hwirq_t *hwirq)
{
	object_state_t old_state = atomic_exchange_explicit(
		&hwirq->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_hwirq_event(hwirq);
	}
	rcu_enqueue(&hwirq->header.rcu_entry, RCU_UPDATE_CLASS_HWIRQ_DESTROY);
}

void
object_put_hwirq(hwirq_t *hwirq)
{
	if (compiler_unexpected(refcount_put(&hwirq->header.refcount))) {
		object_free_hwirq(hwirq);
	}
}

error_t
object_activate_hwirq(hwirq_t *hwirq)
{
	object_ptr_t o = { .hwirq = hwirq };
	return object_activate(OBJECT_TYPE_HWIRQ, o);
}

memextent_t *
object_get_memextent_additional(memextent_t *memextent)
{
	refcount_get_additional(&memextent->header.refcount);

	return memextent;
}

bool
object_get_memextent_safe(memextent_t *memextent)
{
	return refcount_get_safe(&memextent->header.refcount);
}

static void NOINLINE
object_free_memextent(memextent_t *memextent)
{
	object_state_t old_state = atomic_exchange_explicit(
		&memextent->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_memextent_event(memextent);
	}
	rcu_enqueue(&memextent->header.rcu_entry,
		    RCU_UPDATE_CLASS_MEMEXTENT_DESTROY);
}

void
object_put_memextent(memextent_t *memextent)
{
	if (compiler_unexpected(refcount_put(&memextent->header.refcount))) {
		object_free_memextent(memextent);
	}
}

error_t
object_activate_memextent(memextent_t *memextent)
{
	object_ptr_t o = { .memextent = memextent };
	return object_activate(OBJECT_TYPE_MEMEXTENT, o);
}

msgqueue_t *
object_get_msgqueue_additional(msgqueue_t *msgqueue)
{
	refcount_get_additional(&msgqueue->header.refcount);

	return msgqueue;
}

bool
object_get_msgqueue_safe(msgqueue_t *msgqueue)
{
	return refcount_get_safe(&msgqueue->header.refcount);
}

static void NOINLINE
object_free_msgqueue(msgqueue_t *msgqueue)
{
	object_state_t old_state = atomic_exchange_explicit(
		&msgqueue->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_msgqueue_event(msgqueue);
	}
	rcu_enqueue(&msgqueue->header.rcu_entry,
		    RCU_UPDATE_CLASS_MSGQUEUE_DESTROY);
}

void
object_put_msgqueue(msgqueue_t *msgqueue)
{
	if (compiler_unexpected(refcount_put(&msgqueue->header.refcount))) {
		object_free_msgqueue(msgqueue);
	}
}

error_t
object_activate_msgqueue(msgqueue_t *msgqueue)
{
	object_ptr_t o = { .msgqueue = msgqueue };
	return object_activate(OBJECT_TYPE_MSGQUEUE, o);
}

partition_t *
object_get_partition_additional(partition_t *partition)
{
	refcount_get_additional(&partition->header.refcount);

	return partition;
}

bool
object_get_partition_safe(partition_t *partition)
{
	return refcount_get_safe(&partition->header.refcount);
}

static void NOINLINE
object_free_partition(partition_t *partition)
{
	object_state_t old_state = atomic_exchange_explicit(
		&partition->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_partition_event(partition);
	}
	rcu_enqueue(&partition->header.rcu_entry,
		    RCU_UPDATE_CLASS_PARTITION_DESTROY);
}

void
object_put_partition(partition_t *partition)
{
	if (compiler_unexpected(refcount_put(&partition->header.refcount))) {
		object_free_partition(partition);
	}
}

error_t
object_activate_partition(partition_t *partition)
{
	object_ptr_t o = { .partition = partition };
	return object_activate(OBJECT_TYPE_PARTITION, o);
}

thread_t *
object_get_thread_additional(thread_t *thread)
{
	refcount_get_additional(&thread->header.refcount);

	return thread;
}

bool
object_get_thread_safe(thread_t *thread)
{
	return refcount_get_safe(&thread->header.refcount);
}

static void NOINLINE
object_free_thread(thread_t *thread)
{
	object_state_t old_state = atomic_exchange_explicit(
		&thread->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_thread_event(thread);
	}
	rcu_enqueue(&thread->header.rcu_entry, RCU_UPDATE_CLASS_THREAD_DESTROY);
}

void
object_put_thread(thread_t *thread)
{
	if (compiler_unexpected(refcount_put(&thread->header.refcount))) {
		object_free_thread(thread);
	}
}

error_t
object_activate_thread(thread_t *thread)
{
	object_ptr_t o = { .thread = thread };
	return object_activate(OBJECT_TYPE_THREAD, o);
}

vic_t *
object_get_vic_additional(vic_t *vic)
{
	refcount_get_additional(&vic->header.refcount);

	return vic;
}

bool
object_get_vic_safe(vic_t *vic)
{
	return refcount_get_safe(&vic->header.refcount);
}

static void NOINLINE
object_free_vic(vic_t *vic)
{
	object_state_t old_state = atomic_exchange_explicit(
		&vic->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_vic_event(vic);
	}
	rcu_enqueue(&vic->header.rcu_entry, RCU_UPDATE_CLASS_VIC_DESTROY);
}

void
object_put_vic(vic_t *vic)
{
	if (compiler_unexpected(refcount_put(&vic->header.refcount))) {
		object_free_vic(vic);
	}
}

error_t
object_activate_vic(vic_t *vic)
{
	object_ptr_t o = { .vic = vic };
	return object_activate(OBJECT_TYPE_VIC, o);
}

virtio_backend_t *
object_get_virtio_backend_additional(virtio_backend_t *virtio_backend)
{
	refcount_get_additional(&virtio_backend->header.refcount);

	return virtio_backend;
}

bool
object_get_virtio_backend_safe(virtio_backend_t *virtio_backend)
{
	return refcount_get_safe(&virtio_backend->header.refcount);
}

static void NOINLINE
object_free_virtio_backend(virtio_backend_t *virtio_backend)
{
	object_state_t old_state = atomic_exchange_explicit(
		&virtio_backend->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_virtio_backend_event(virtio_backend);
	}
	rcu_enqueue(&virtio_backend->header.rcu_entry,
		    RCU_UPDATE_CLASS_VIRTIO_BACKEND_DESTROY);
}

void
object_put_virtio_backend(virtio_backend_t *virtio_backend)
{
	if (compiler_unexpected(
		    refcount_put(&virtio_backend->header.refcount))) {
		object_free_virtio_backend(virtio_backend);
	}
}

error_t
object_activate_virtio_backend(virtio_backend_t *virtio_backend)
{
	object_ptr_t o = { .virtio_backend = virtio_backend };
	return object_activate(OBJECT_TYPE_VIRTIO_BACKEND, o);
}

vpm_group_t *
object_get_vpm_group_additional(vpm_group_t *vpm_group)
{
	refcount_get_additional(&vpm_group->header.refcount);

	return vpm_group;
}

bool
object_get_vpm_group_safe(vpm_group_t *vpm_group)
{
	return refcount_get_safe(&vpm_group->header.refcount);
}

static void NOINLINE
object_free_vpm_group(vpm_group_t *vpm_group)
{
	object_state_t old_state = atomic_exchange_explicit(
		&vpm_group->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_vpm_group_event(vpm_group);
	}
	rcu_enqueue(&vpm_group->header.rcu_entry,
		    RCU_UPDATE_CLASS_VPM_GROUP_DESTROY);
}

void
object_put_vpm_group(vpm_group_t *vpm_group)
{
	if (compiler_unexpected(refcount_put(&vpm_group->header.refcount))) {
		object_free_vpm_group(vpm_group);
	}
}

error_t
object_activate_vpm_group(vpm_group_t *vpm_group)
{
	object_ptr_t o = { .vpm_group = vpm_group };
	return object_activate(OBJECT_TYPE_VPM_GROUP, o);
}

vrtc_t *
object_get_vrtc_additional(vrtc_t *vrtc)
{
	refcount_get_additional(&vrtc->header.refcount);

	return vrtc;
}

bool
object_get_vrtc_safe(vrtc_t *vrtc)
{
	return refcount_get_safe(&vrtc->header.refcount);
}

static void NOINLINE
object_free_vrtc(vrtc_t *vrtc)
{
	object_state_t old_state = atomic_exchange_explicit(
		&vrtc->header.state, OBJECT_STATE_DESTROYING,
		memory_order_relaxed);
	if (old_state == OBJECT_STATE_ACTIVE) {
		trigger_object_deactivate_vrtc_event(vrtc);
	}
	rcu_enqueue(&vrtc->header.rcu_entry, RCU_UPDATE_CLASS_VRTC_DESTROY);
}

void
object_put_vrtc(vrtc_t *vrtc)
{
	if (compiler_unexpected(refcount_put(&vrtc->header.refcount))) {
		object_free_vrtc(vrtc);
	}
}

error_t
object_activate_vrtc(vrtc_t *vrtc)
{
	object_ptr_t o = { .vrtc = vrtc };
	return object_activate(OBJECT_TYPE_VRTC, o);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"

object_ptr_t
object_get_additional(object_type_t type, object_ptr_t object)
{
	object_ptr_t ret;

	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		ret.addrspace =
			object_get_addrspace_additional(object.addrspace);
		break;
	case OBJECT_TYPE_CSPACE:
		ret.cspace = object_get_cspace_additional(object.cspace);
		break;
	case OBJECT_TYPE_DOORBELL:
		ret.doorbell = object_get_doorbell_additional(object.doorbell);
		break;
	case OBJECT_TYPE_GICV3_ITS:
		ret.gicv3_its =
			object_get_gicv3_its_additional(object.gicv3_its);
		break;
	case OBJECT_TYPE_HWIRQ:
		ret.hwirq = object_get_hwirq_additional(object.hwirq);
		break;
	case OBJECT_TYPE_MEMEXTENT:
		ret.memextent =
			object_get_memextent_additional(object.memextent);
		break;
	case OBJECT_TYPE_MSGQUEUE:
		ret.msgqueue = object_get_msgqueue_additional(object.msgqueue);
		break;
	case OBJECT_TYPE_PARTITION:
		ret.partition =
			object_get_partition_additional(object.partition);
		break;
	case OBJECT_TYPE_THREAD:
		ret.thread = object_get_thread_additional(object.thread);
		break;
	case OBJECT_TYPE_VIC:
		ret.vic = object_get_vic_additional(object.vic);
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		ret.virtio_backend = object_get_virtio_backend_additional(
			object.virtio_backend);
		break;
	case OBJECT_TYPE_VPM_GROUP:
		ret.vpm_group =
			object_get_vpm_group_additional(object.vpm_group);
		break;
	case OBJECT_TYPE_VRTC:
		ret.vrtc = object_get_vrtc_additional(object.vrtc);
		break;
	default:
		panic("unknown object type");
	}

	return ret;
}

bool
object_get_safe(object_type_t type, object_ptr_t object)
{
	bool ret;

	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		ret = object_get_addrspace_safe(object.addrspace);
		break;
	case OBJECT_TYPE_CSPACE:
		ret = object_get_cspace_safe(object.cspace);
		break;
	case OBJECT_TYPE_DOORBELL:
		ret = object_get_doorbell_safe(object.doorbell);
		break;
	case OBJECT_TYPE_GICV3_ITS:
		ret = object_get_gicv3_its_safe(object.gicv3_its);
		break;
	case OBJECT_TYPE_HWIRQ:
		ret = object_get_hwirq_safe(object.hwirq);
		break;
	case OBJECT_TYPE_MEMEXTENT:
		ret = object_get_memextent_safe(object.memextent);
		break;
	case OBJECT_TYPE_MSGQUEUE:
		ret = object_get_msgqueue_safe(object.msgqueue);
		break;
	case OBJECT_TYPE_PARTITION:
		ret = object_get_partition_safe(object.partition);
		break;
	case OBJECT_TYPE_THREAD:
		ret = object_get_thread_safe(object.thread);
		break;
	case OBJECT_TYPE_VIC:
		ret = object_get_vic_safe(object.vic);
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		ret = object_get_virtio_backend_safe(object.virtio_backend);
		break;
	case OBJECT_TYPE_VPM_GROUP:
		ret = object_get_vpm_group_safe(object.vpm_group);
		break;
	case OBJECT_TYPE_VRTC:
		ret = object_get_vrtc_safe(object.vrtc);
		break;
	default:
		panic("unknown object type");
	}

	return ret;
}

void
object_put(object_type_t type, object_ptr_t object)
{
	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		object_put_addrspace(object.addrspace);
		break;
	case OBJECT_TYPE_CSPACE:
		object_put_cspace(object.cspace);
		break;
	case OBJECT_TYPE_DOORBELL:
		object_put_doorbell(object.doorbell);
		break;
	case OBJECT_TYPE_GICV3_ITS:
		object_put_gicv3_its(object.gicv3_its);
		break;
	case OBJECT_TYPE_HWIRQ:
		object_put_hwirq(object.hwirq);
		break;
	case OBJECT_TYPE_MEMEXTENT:
		object_put_memextent(object.memextent);
		break;
	case OBJECT_TYPE_MSGQUEUE:
		object_put_msgqueue(object.msgqueue);
		break;
	case OBJECT_TYPE_PARTITION:
		object_put_partition(object.partition);
		break;
	case OBJECT_TYPE_THREAD:
		object_put_thread(object.thread);
		break;
	case OBJECT_TYPE_VIC:
		object_put_vic(object.vic);
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		object_put_virtio_backend(object.virtio_backend);
		break;
	case OBJECT_TYPE_VPM_GROUP:
		object_put_vpm_group(object.vpm_group);
		break;
	case OBJECT_TYPE_VRTC:
		object_put_vrtc(object.vrtc);
		break;
	default:
		panic("unknown object type");
	}
}

object_header_t *
object_get_header(object_type_t type, object_ptr_t object)
{
	object_header_t *header;

	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		header = &object.addrspace->header;
		break;
	case OBJECT_TYPE_CSPACE:
		header = &object.cspace->header;
		break;
	case OBJECT_TYPE_DOORBELL:
		header = &object.doorbell->header;
		break;
	case OBJECT_TYPE_GICV3_ITS:
		header = &object.gicv3_its->header;
		break;
	case OBJECT_TYPE_HWIRQ:
		header = &object.hwirq->header;
		break;
	case OBJECT_TYPE_MEMEXTENT:
		header = &object.memextent->header;
		break;
	case OBJECT_TYPE_MSGQUEUE:
		header = &object.msgqueue->header;
		break;
	case OBJECT_TYPE_PARTITION:
		header = &object.partition->header;
		break;
	case OBJECT_TYPE_THREAD:
		header = &object.thread->header;
		break;
	case OBJECT_TYPE_VIC:
		header = &object.vic->header;
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		header = &object.virtio_backend->header;
		break;
	case OBJECT_TYPE_VPM_GROUP:
		header = &object.vpm_group->header;
		break;
	case OBJECT_TYPE_VRTC:
		header = &object.vrtc->header;
		break;
	default:
		panic("unknown object type");
	}

	return header;
}

error_t
object_activate(object_type_t type, object_ptr_t object)
{
	error_t ret;

	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		spinlock_acquire(&object.addrspace->header.lock);

		if (atomic_load_relaxed(&object.addrspace->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_addrspace_event(
				object.addrspace);

			if (ret == OK) {
				atomic_store_release(
					&object.addrspace->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.addrspace->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.addrspace->header.lock);
		break;
	case OBJECT_TYPE_CSPACE:
		spinlock_acquire(&object.cspace->header.lock);

		if (atomic_load_relaxed(&object.cspace->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_cspace_event(
				object.cspace);

			if (ret == OK) {
				atomic_store_release(
					&object.cspace->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.cspace->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.cspace->header.lock);
		break;
	case OBJECT_TYPE_DOORBELL:
		spinlock_acquire(&object.doorbell->header.lock);

		if (atomic_load_relaxed(&object.doorbell->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_doorbell_event(
				object.doorbell);

			if (ret == OK) {
				atomic_store_release(
					&object.doorbell->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.doorbell->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.doorbell->header.lock);
		break;
	case OBJECT_TYPE_GICV3_ITS:
		spinlock_acquire(&object.gicv3_its->header.lock);

		if (atomic_load_relaxed(&object.gicv3_its->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_gicv3_its_event(
				object.gicv3_its);

			if (ret == OK) {
				atomic_store_release(
					&object.gicv3_its->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.gicv3_its->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.gicv3_its->header.lock);
		break;
	case OBJECT_TYPE_HWIRQ:
		spinlock_acquire(&object.hwirq->header.lock);

		if (atomic_load_relaxed(&object.hwirq->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_hwirq_event(object.hwirq);

			if (ret == OK) {
				atomic_store_release(
					&object.hwirq->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.hwirq->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.hwirq->header.lock);
		break;
	case OBJECT_TYPE_MEMEXTENT:
		spinlock_acquire(&object.memextent->header.lock);

		if (atomic_load_relaxed(&object.memextent->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_memextent_event(
				object.memextent);

			if (ret == OK) {
				atomic_store_release(
					&object.memextent->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.memextent->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.memextent->header.lock);
		break;
	case OBJECT_TYPE_MSGQUEUE:
		spinlock_acquire(&object.msgqueue->header.lock);

		if (atomic_load_relaxed(&object.msgqueue->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_msgqueue_event(
				object.msgqueue);

			if (ret == OK) {
				atomic_store_release(
					&object.msgqueue->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.msgqueue->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.msgqueue->header.lock);
		break;
	case OBJECT_TYPE_PARTITION:
		spinlock_acquire(&object.partition->header.lock);

		if (atomic_load_relaxed(&object.partition->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_partition_event(
				object.partition);

			if (ret == OK) {
				atomic_store_release(
					&object.partition->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.partition->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.partition->header.lock);
		break;
	case OBJECT_TYPE_THREAD:
		spinlock_acquire(&object.thread->header.lock);

		if (atomic_load_relaxed(&object.thread->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_thread_event(
				object.thread);

			if (ret == OK) {
				atomic_store_release(
					&object.thread->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.thread->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.thread->header.lock);
		break;
	case OBJECT_TYPE_VIC:
		spinlock_acquire(&object.vic->header.lock);

		if (atomic_load_relaxed(&object.vic->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_vic_event(object.vic);

			if (ret == OK) {
				atomic_store_release(&object.vic->header.state,
						     OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(&object.vic->header.state,
						     OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.vic->header.lock);
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		spinlock_acquire(&object.virtio_backend->header.lock);

		if (atomic_load_relaxed(&object.virtio_backend->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_virtio_backend_event(
				object.virtio_backend);

			if (ret == OK) {
				atomic_store_release(
					&object.virtio_backend->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.virtio_backend->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.virtio_backend->header.lock);
		break;
	case OBJECT_TYPE_VPM_GROUP:
		spinlock_acquire(&object.vpm_group->header.lock);

		if (atomic_load_relaxed(&object.vpm_group->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_vpm_group_event(
				object.vpm_group);

			if (ret == OK) {
				atomic_store_release(
					&object.vpm_group->header.state,
					OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(
					&object.vpm_group->header.state,
					OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.vpm_group->header.lock);
		break;
	case OBJECT_TYPE_VRTC:
		spinlock_acquire(&object.vrtc->header.lock);

		if (atomic_load_relaxed(&object.vrtc->header.state) ==
		    OBJECT_STATE_INIT) {
			ret = trigger_object_activate_vrtc_event(object.vrtc);

			if (ret == OK) {
				atomic_store_release(&object.vrtc->header.state,
						     OBJECT_STATE_ACTIVE);
			} else {
				atomic_store_relaxed(&object.vrtc->header.state,
						     OBJECT_STATE_FAILED);
			}

		} else {
			ret = ERROR_OBJECT_STATE;
		}

		spinlock_release(&object.vrtc->header.lock);
		break;
	default:
		panic("unknown object type");
	}

	return ret;
}

#pragma clang diagnostic pop
