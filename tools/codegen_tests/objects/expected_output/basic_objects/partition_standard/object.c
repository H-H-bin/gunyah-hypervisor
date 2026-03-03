// Automatically generated. Do not modify.

// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <object.h>
#include <partition.h>
#include <partition_alloc.h>
#include <rcu.h>
#include <refcount.h>
#include <spinlock.h>

#include <events/object.h>

#include "event_handlers.h"

void
partition_standard_handle_object_init_addrspace(
	addrspace_create_t addrspace_create, partition_t *parent)
{
	addrspace_t *addrspace = addrspace_create.addrspace;

	refcount_init(&addrspace->header.refcount);
	spinlock_init(&addrspace->header.lock);

	addrspace->header.type	    = OBJECT_TYPE_ADDRSPACE;
	addrspace->header.partition = object_get_partition_additional(parent);

	atomic_init(&addrspace->header.state, OBJECT_STATE_INIT);
}

addrspace_ptr_result_t
partition_allocate_addrspace(partition_t *parent, addrspace_create_t create)
{
	void_ptr_result_t      alloc_ret;
	addrspace_ptr_result_t obj_ret;
	addrspace_t	      *addrspace;

	const size_t size  = sizeof(addrspace_t);
	const size_t align = alignof(addrspace_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = addrspace_ptr_result_error(alloc_ret.e);
		goto allocate_addrspace_error;
	}

	addrspace = (addrspace_t *)alloc_ret.r;
	(void)memset_s(addrspace, size, 0, size);

	assert_debug((create.addrspace == NULL) && (addrspace != NULL));
	create.addrspace = addrspace;

	trigger_object_init_addrspace_event(create, parent);

	spinlock_acquire(&create.addrspace->header.lock);
	error_t err = trigger_object_create_addrspace_event(create);
	spinlock_release(&create.addrspace->header.lock);
	if (err != OK) {
		rcu_enqueue(&addrspace->header.rcu_entry,
			    RCU_UPDATE_CLASS_ADDRSPACE_DESTROY);
		obj_ret = addrspace_ptr_result_error(err);
	} else {
		obj_ret = addrspace_ptr_result_ok(addrspace);
	}

allocate_addrspace_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_addrspace(rcu_entry_t *entry)
{
	addrspace_t	   *addrspace;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	  = object_header_container_of_rcu_entry(entry);
	addrspace = addrspace_container_of_header(header);

	trigger_object_cleanup_addrspace_event(&ret, addrspace);

	partition_t *parent = addrspace->header.partition;
	partition_free(parent, addrspace, sizeof(addrspace_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_cspace(cspace_create_t cspace_create,
					     partition_t    *parent)
{
	cspace_t *cspace = cspace_create.cspace;

	refcount_init(&cspace->header.refcount);
	spinlock_init(&cspace->header.lock);

	cspace->header.type	 = OBJECT_TYPE_CSPACE;
	cspace->header.partition = object_get_partition_additional(parent);

	atomic_init(&cspace->header.state, OBJECT_STATE_INIT);
}

cspace_ptr_result_t
partition_allocate_cspace(partition_t *parent, cspace_create_t create)
{
	void_ptr_result_t   alloc_ret;
	cspace_ptr_result_t obj_ret;
	cspace_t	   *cspace;

	const size_t size  = sizeof(cspace_t);
	const size_t align = alignof(cspace_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = cspace_ptr_result_error(alloc_ret.e);
		goto allocate_cspace_error;
	}

	cspace = (cspace_t *)alloc_ret.r;
	(void)memset_s(cspace, size, 0, size);

	assert_debug((create.cspace == NULL) && (cspace != NULL));
	create.cspace = cspace;

	trigger_object_init_cspace_event(create, parent);

	spinlock_acquire(&create.cspace->header.lock);
	error_t err = trigger_object_create_cspace_event(create);
	spinlock_release(&create.cspace->header.lock);
	if (err != OK) {
		rcu_enqueue(&cspace->header.rcu_entry,
			    RCU_UPDATE_CLASS_CSPACE_DESTROY);
		obj_ret = cspace_ptr_result_error(err);
	} else {
		obj_ret = cspace_ptr_result_ok(cspace);
	}

allocate_cspace_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_cspace(rcu_entry_t *entry)
{
	cspace_t	   *cspace;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header = object_header_container_of_rcu_entry(entry);
	cspace = cspace_container_of_header(header);

	trigger_object_cleanup_cspace_event(&ret, cspace);

	partition_t *parent = cspace->header.partition;
	partition_free(parent, cspace, sizeof(cspace_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_doorbell(
	doorbell_create_t doorbell_create, partition_t *parent)
{
	doorbell_t *doorbell = doorbell_create.doorbell;

	refcount_init(&doorbell->header.refcount);
	spinlock_init(&doorbell->header.lock);

	doorbell->header.type	   = OBJECT_TYPE_DOORBELL;
	doorbell->header.partition = object_get_partition_additional(parent);

	atomic_init(&doorbell->header.state, OBJECT_STATE_INIT);
}

doorbell_ptr_result_t
partition_allocate_doorbell(partition_t *parent, doorbell_create_t create)
{
	void_ptr_result_t     alloc_ret;
	doorbell_ptr_result_t obj_ret;
	doorbell_t	     *doorbell;

	const size_t size  = sizeof(doorbell_t);
	const size_t align = alignof(doorbell_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = doorbell_ptr_result_error(alloc_ret.e);
		goto allocate_doorbell_error;
	}

	doorbell = (doorbell_t *)alloc_ret.r;
	(void)memset_s(doorbell, size, 0, size);

	assert_debug((create.doorbell == NULL) && (doorbell != NULL));
	create.doorbell = doorbell;

	trigger_object_init_doorbell_event(create, parent);

	spinlock_acquire(&create.doorbell->header.lock);
	error_t err = trigger_object_create_doorbell_event(create);
	spinlock_release(&create.doorbell->header.lock);
	if (err != OK) {
		rcu_enqueue(&doorbell->header.rcu_entry,
			    RCU_UPDATE_CLASS_DOORBELL_DESTROY);
		obj_ret = doorbell_ptr_result_error(err);
	} else {
		obj_ret = doorbell_ptr_result_ok(doorbell);
	}

allocate_doorbell_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_doorbell(rcu_entry_t *entry)
{
	doorbell_t	   *doorbell;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	 = object_header_container_of_rcu_entry(entry);
	doorbell = doorbell_container_of_header(header);

	trigger_object_cleanup_doorbell_event(&ret, doorbell);

	partition_t *parent = doorbell->header.partition;
	partition_free(parent, doorbell, sizeof(doorbell_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_gicv3_its(
	gicv3_its_create_t gicv3_its_create, partition_t *parent)
{
	gicv3_its_t *gicv3_its = gicv3_its_create.gicv3_its;

	refcount_init(&gicv3_its->header.refcount);
	spinlock_init(&gicv3_its->header.lock);

	gicv3_its->header.type	    = OBJECT_TYPE_GICV3_ITS;
	gicv3_its->header.partition = object_get_partition_additional(parent);

	atomic_init(&gicv3_its->header.state, OBJECT_STATE_INIT);
}

gicv3_its_ptr_result_t
partition_allocate_gicv3_its(partition_t *parent, gicv3_its_create_t create)
{
	void_ptr_result_t      alloc_ret;
	gicv3_its_ptr_result_t obj_ret;
	gicv3_its_t	      *gicv3_its;

	const size_t size  = sizeof(gicv3_its_t);
	const size_t align = alignof(gicv3_its_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = gicv3_its_ptr_result_error(alloc_ret.e);
		goto allocate_gicv3_its_error;
	}

	gicv3_its = (gicv3_its_t *)alloc_ret.r;
	(void)memset_s(gicv3_its, size, 0, size);

	assert_debug((create.gicv3_its == NULL) && (gicv3_its != NULL));
	create.gicv3_its = gicv3_its;

	trigger_object_init_gicv3_its_event(create, parent);

	spinlock_acquire(&create.gicv3_its->header.lock);
	error_t err = trigger_object_create_gicv3_its_event(create);
	spinlock_release(&create.gicv3_its->header.lock);
	if (err != OK) {
		rcu_enqueue(&gicv3_its->header.rcu_entry,
			    RCU_UPDATE_CLASS_GICV3_ITS_DESTROY);
		obj_ret = gicv3_its_ptr_result_error(err);
	} else {
		obj_ret = gicv3_its_ptr_result_ok(gicv3_its);
	}

allocate_gicv3_its_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_gicv3_its(rcu_entry_t *entry)
{
	gicv3_its_t	   *gicv3_its;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	  = object_header_container_of_rcu_entry(entry);
	gicv3_its = gicv3_its_container_of_header(header);

	trigger_object_cleanup_gicv3_its_event(&ret, gicv3_its);

	partition_t *parent = gicv3_its->header.partition;
	partition_free(parent, gicv3_its, sizeof(gicv3_its_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_hwirq(hwirq_create_t hwirq_create,
					    partition_t	  *parent)
{
	hwirq_t *hwirq = hwirq_create.hwirq;

	refcount_init(&hwirq->header.refcount);
	spinlock_init(&hwirq->header.lock);

	hwirq->header.type	= OBJECT_TYPE_HWIRQ;
	hwirq->header.partition = object_get_partition_additional(parent);

	atomic_init(&hwirq->header.state, OBJECT_STATE_INIT);
}

hwirq_ptr_result_t
partition_allocate_hwirq(partition_t *parent, hwirq_create_t create)
{
	void_ptr_result_t  alloc_ret;
	hwirq_ptr_result_t obj_ret;
	hwirq_t		  *hwirq;

	const size_t size  = sizeof(hwirq_t);
	const size_t align = alignof(hwirq_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = hwirq_ptr_result_error(alloc_ret.e);
		goto allocate_hwirq_error;
	}

	hwirq = (hwirq_t *)alloc_ret.r;
	(void)memset_s(hwirq, size, 0, size);

	assert_debug((create.hwirq == NULL) && (hwirq != NULL));
	create.hwirq = hwirq;

	trigger_object_init_hwirq_event(create, parent);

	spinlock_acquire(&create.hwirq->header.lock);
	error_t err = trigger_object_create_hwirq_event(create);
	spinlock_release(&create.hwirq->header.lock);
	if (err != OK) {
		rcu_enqueue(&hwirq->header.rcu_entry,
			    RCU_UPDATE_CLASS_HWIRQ_DESTROY);
		obj_ret = hwirq_ptr_result_error(err);
	} else {
		obj_ret = hwirq_ptr_result_ok(hwirq);
	}

allocate_hwirq_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_hwirq(rcu_entry_t *entry)
{
	hwirq_t		   *hwirq;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header = object_header_container_of_rcu_entry(entry);
	hwirq  = hwirq_container_of_header(header);

	trigger_object_cleanup_hwirq_event(&ret, hwirq);

	partition_t *parent = hwirq->header.partition;
	partition_free(parent, hwirq, sizeof(hwirq_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_memextent(
	memextent_create_t memextent_create, partition_t *parent)
{
	memextent_t *memextent = memextent_create.memextent;

	refcount_init(&memextent->header.refcount);
	spinlock_init(&memextent->header.lock);

	memextent->header.type	    = OBJECT_TYPE_MEMEXTENT;
	memextent->header.partition = object_get_partition_additional(parent);

	atomic_init(&memextent->header.state, OBJECT_STATE_INIT);
}

memextent_ptr_result_t
partition_allocate_memextent(partition_t *parent, memextent_create_t create)
{
	void_ptr_result_t      alloc_ret;
	memextent_ptr_result_t obj_ret;
	memextent_t	      *memextent;

	const size_t size  = sizeof(memextent_t);
	const size_t align = alignof(memextent_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = memextent_ptr_result_error(alloc_ret.e);
		goto allocate_memextent_error;
	}

	memextent = (memextent_t *)alloc_ret.r;
	(void)memset_s(memextent, size, 0, size);

	assert_debug((create.memextent == NULL) && (memextent != NULL));
	create.memextent = memextent;

	trigger_object_init_memextent_event(create, parent);

	spinlock_acquire(&create.memextent->header.lock);
	error_t err = trigger_object_create_memextent_event(create);
	spinlock_release(&create.memextent->header.lock);
	if (err != OK) {
		rcu_enqueue(&memextent->header.rcu_entry,
			    RCU_UPDATE_CLASS_MEMEXTENT_DESTROY);
		obj_ret = memextent_ptr_result_error(err);
	} else {
		obj_ret = memextent_ptr_result_ok(memextent);
	}

allocate_memextent_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_memextent(rcu_entry_t *entry)
{
	memextent_t	   *memextent;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	  = object_header_container_of_rcu_entry(entry);
	memextent = memextent_container_of_header(header);

	trigger_object_cleanup_memextent_event(&ret, memextent);

	partition_t *parent = memextent->header.partition;
	partition_free(parent, memextent, sizeof(memextent_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_msgqueue(
	msgqueue_create_t msgqueue_create, partition_t *parent)
{
	msgqueue_t *msgqueue = msgqueue_create.msgqueue;

	refcount_init(&msgqueue->header.refcount);
	spinlock_init(&msgqueue->header.lock);

	msgqueue->header.type	   = OBJECT_TYPE_MSGQUEUE;
	msgqueue->header.partition = object_get_partition_additional(parent);

	atomic_init(&msgqueue->header.state, OBJECT_STATE_INIT);
}

msgqueue_ptr_result_t
partition_allocate_msgqueue(partition_t *parent, msgqueue_create_t create)
{
	void_ptr_result_t     alloc_ret;
	msgqueue_ptr_result_t obj_ret;
	msgqueue_t	     *msgqueue;

	const size_t size  = sizeof(msgqueue_t);
	const size_t align = alignof(msgqueue_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = msgqueue_ptr_result_error(alloc_ret.e);
		goto allocate_msgqueue_error;
	}

	msgqueue = (msgqueue_t *)alloc_ret.r;
	(void)memset_s(msgqueue, size, 0, size);

	assert_debug((create.msgqueue == NULL) && (msgqueue != NULL));
	create.msgqueue = msgqueue;

	trigger_object_init_msgqueue_event(create, parent);

	spinlock_acquire(&create.msgqueue->header.lock);
	error_t err = trigger_object_create_msgqueue_event(create);
	spinlock_release(&create.msgqueue->header.lock);
	if (err != OK) {
		rcu_enqueue(&msgqueue->header.rcu_entry,
			    RCU_UPDATE_CLASS_MSGQUEUE_DESTROY);
		obj_ret = msgqueue_ptr_result_error(err);
	} else {
		obj_ret = msgqueue_ptr_result_ok(msgqueue);
	}

allocate_msgqueue_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_msgqueue(rcu_entry_t *entry)
{
	msgqueue_t	   *msgqueue;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	 = object_header_container_of_rcu_entry(entry);
	msgqueue = msgqueue_container_of_header(header);

	trigger_object_cleanup_msgqueue_event(&ret, msgqueue);

	partition_t *parent = msgqueue->header.partition;
	partition_free(parent, msgqueue, sizeof(msgqueue_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_partition(
	partition_create_t partition_create, partition_t *parent)
{
	partition_t *partition = partition_create.partition;

	refcount_init(&partition->header.refcount);
	spinlock_init(&partition->header.lock);

	partition->header.type	    = OBJECT_TYPE_PARTITION;
	partition->header.partition = object_get_partition_additional(parent);

	atomic_init(&partition->header.state, OBJECT_STATE_INIT);
}

partition_ptr_result_t
partition_allocate_partition(partition_t *parent, partition_create_t create)
{
	void_ptr_result_t      alloc_ret;
	partition_ptr_result_t obj_ret;
	partition_t	      *partition;

	const size_t size  = sizeof(partition_t);
	const size_t align = alignof(partition_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = partition_ptr_result_error(alloc_ret.e);
		goto allocate_partition_error;
	}

	partition = (partition_t *)alloc_ret.r;
	(void)memset_s(partition, size, 0, size);

	assert_debug((create.partition == NULL) && (partition != NULL));
	create.partition = partition;

	trigger_object_init_partition_event(create, parent);

	spinlock_acquire(&create.partition->header.lock);
	error_t err = trigger_object_create_partition_event(create);
	spinlock_release(&create.partition->header.lock);
	if (err != OK) {
		rcu_enqueue(&partition->header.rcu_entry,
			    RCU_UPDATE_CLASS_PARTITION_DESTROY);
		obj_ret = partition_ptr_result_error(err);
	} else {
		obj_ret = partition_ptr_result_ok(partition);
	}

allocate_partition_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_partition(rcu_entry_t *entry)
{
	partition_t	   *partition;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	  = object_header_container_of_rcu_entry(entry);
	partition = partition_container_of_header(header);

	trigger_object_cleanup_partition_event(&ret, partition);

	partition_t *parent = partition->header.partition;
	partition_free(parent, partition, sizeof(partition_t));
	object_put_partition(parent);

	return ret;
}

extern const size_t thread_size;
extern const size_t thread_align;

void
partition_standard_handle_object_init_thread(thread_create_t thread_create,
					     partition_t    *parent)
{
	thread_t *thread = thread_create.thread;

	refcount_init(&thread->header.refcount);
	spinlock_init(&thread->header.lock);

	thread->header.type	 = OBJECT_TYPE_THREAD;
	thread->header.partition = object_get_partition_additional(parent);

	atomic_init(&thread->header.state, OBJECT_STATE_INIT);
}

thread_ptr_result_t
partition_allocate_thread(partition_t *parent, thread_create_t create)
{
	void_ptr_result_t   alloc_ret;
	thread_ptr_result_t obj_ret;
	thread_t	   *thread;

	const size_t size  = thread_size;
	const size_t align = thread_align;

	assert(size >= sizeof(thread_t));
	alloc_ret = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = thread_ptr_result_error(alloc_ret.e);
		goto allocate_thread_error;
	}

	thread = (thread_t *)alloc_ret.r;
	(void)memset_s(thread, size, 0, size);

	assert_debug((create.thread == NULL) && (thread != NULL));
	create.thread = thread;

	trigger_object_init_thread_event(create, parent);

	spinlock_acquire(&create.thread->header.lock);
	error_t err = trigger_object_create_thread_event(create);
	spinlock_release(&create.thread->header.lock);
	if (err != OK) {
		rcu_enqueue(&thread->header.rcu_entry,
			    RCU_UPDATE_CLASS_THREAD_DESTROY);
		obj_ret = thread_ptr_result_error(err);
	} else {
		obj_ret = thread_ptr_result_ok(thread);
	}

allocate_thread_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_thread(rcu_entry_t *entry)
{
	thread_t	   *thread;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header = object_header_container_of_rcu_entry(entry);
	thread = thread_container_of_header(header);

	trigger_object_cleanup_thread_event(&ret, thread);

	partition_t *parent = thread->header.partition;
	partition_free(parent, thread, thread_size);
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_vic(vic_create_t vic_create,
					  partition_t *parent)
{
	vic_t *vic = vic_create.vic;

	refcount_init(&vic->header.refcount);
	spinlock_init(&vic->header.lock);

	vic->header.type      = OBJECT_TYPE_VIC;
	vic->header.partition = object_get_partition_additional(parent);

	atomic_init(&vic->header.state, OBJECT_STATE_INIT);
}

vic_ptr_result_t
partition_allocate_vic(partition_t *parent, vic_create_t create)
{
	void_ptr_result_t alloc_ret;
	vic_ptr_result_t  obj_ret;
	vic_t		 *vic;

	const size_t size  = sizeof(vic_t);
	const size_t align = alignof(vic_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = vic_ptr_result_error(alloc_ret.e);
		goto allocate_vic_error;
	}

	vic = (vic_t *)alloc_ret.r;
	(void)memset_s(vic, size, 0, size);

	assert_debug((create.vic == NULL) && (vic != NULL));
	create.vic = vic;

	trigger_object_init_vic_event(create, parent);

	spinlock_acquire(&create.vic->header.lock);
	error_t err = trigger_object_create_vic_event(create);
	spinlock_release(&create.vic->header.lock);
	if (err != OK) {
		rcu_enqueue(&vic->header.rcu_entry,
			    RCU_UPDATE_CLASS_VIC_DESTROY);
		obj_ret = vic_ptr_result_error(err);
	} else {
		obj_ret = vic_ptr_result_ok(vic);
	}

allocate_vic_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_vic(rcu_entry_t *entry)
{
	vic_t		   *vic;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header = object_header_container_of_rcu_entry(entry);
	vic    = vic_container_of_header(header);

	trigger_object_cleanup_vic_event(&ret, vic);

	partition_t *parent = vic->header.partition;
	partition_free(parent, vic, sizeof(vic_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_virtio_backend(
	virtio_backend_create_t virtio_backend_create, partition_t *parent)
{
	virtio_backend_t *virtio_backend = virtio_backend_create.virtio_backend;

	refcount_init(&virtio_backend->header.refcount);
	spinlock_init(&virtio_backend->header.lock);

	virtio_backend->header.type = OBJECT_TYPE_VIRTIO_BACKEND;
	virtio_backend->header.partition =
		object_get_partition_additional(parent);

	atomic_init(&virtio_backend->header.state, OBJECT_STATE_INIT);
}

virtio_backend_ptr_result_t
partition_allocate_virtio_backend(partition_t		 *parent,
				  virtio_backend_create_t create)
{
	void_ptr_result_t	    alloc_ret;
	virtio_backend_ptr_result_t obj_ret;
	virtio_backend_t	   *virtio_backend;

	const size_t size  = sizeof(virtio_backend_t);
	const size_t align = alignof(virtio_backend_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = virtio_backend_ptr_result_error(alloc_ret.e);
		goto allocate_virtio_backend_error;
	}

	virtio_backend = (virtio_backend_t *)alloc_ret.r;
	(void)memset_s(virtio_backend, size, 0, size);

	assert_debug((create.virtio_backend == NULL) &&
		     (virtio_backend != NULL));
	create.virtio_backend = virtio_backend;

	trigger_object_init_virtio_backend_event(create, parent);

	spinlock_acquire(&create.virtio_backend->header.lock);
	error_t err = trigger_object_create_virtio_backend_event(create);
	spinlock_release(&create.virtio_backend->header.lock);
	if (err != OK) {
		rcu_enqueue(&virtio_backend->header.rcu_entry,
			    RCU_UPDATE_CLASS_VIRTIO_BACKEND_DESTROY);
		obj_ret = virtio_backend_ptr_result_error(err);
	} else {
		obj_ret = virtio_backend_ptr_result_ok(virtio_backend);
	}

allocate_virtio_backend_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_virtio_backend(rcu_entry_t *entry)
{
	virtio_backend_t   *virtio_backend;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	       = object_header_container_of_rcu_entry(entry);
	virtio_backend = virtio_backend_container_of_header(header);

	trigger_object_cleanup_virtio_backend_event(&ret, virtio_backend);

	partition_t *parent = virtio_backend->header.partition;
	partition_free(parent, virtio_backend, sizeof(virtio_backend_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_vpm_group(
	vpm_group_create_t vpm_group_create, partition_t *parent)
{
	vpm_group_t *vpm_group = vpm_group_create.vpm_group;

	refcount_init(&vpm_group->header.refcount);
	spinlock_init(&vpm_group->header.lock);

	vpm_group->header.type	    = OBJECT_TYPE_VPM_GROUP;
	vpm_group->header.partition = object_get_partition_additional(parent);

	atomic_init(&vpm_group->header.state, OBJECT_STATE_INIT);
}

vpm_group_ptr_result_t
partition_allocate_vpm_group(partition_t *parent, vpm_group_create_t create)
{
	void_ptr_result_t      alloc_ret;
	vpm_group_ptr_result_t obj_ret;
	vpm_group_t	      *vpm_group;

	const size_t size  = sizeof(vpm_group_t);
	const size_t align = alignof(vpm_group_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = vpm_group_ptr_result_error(alloc_ret.e);
		goto allocate_vpm_group_error;
	}

	vpm_group = (vpm_group_t *)alloc_ret.r;
	(void)memset_s(vpm_group, size, 0, size);

	assert_debug((create.vpm_group == NULL) && (vpm_group != NULL));
	create.vpm_group = vpm_group;

	trigger_object_init_vpm_group_event(create, parent);

	spinlock_acquire(&create.vpm_group->header.lock);
	error_t err = trigger_object_create_vpm_group_event(create);
	spinlock_release(&create.vpm_group->header.lock);
	if (err != OK) {
		rcu_enqueue(&vpm_group->header.rcu_entry,
			    RCU_UPDATE_CLASS_VPM_GROUP_DESTROY);
		obj_ret = vpm_group_ptr_result_error(err);
	} else {
		obj_ret = vpm_group_ptr_result_ok(vpm_group);
	}

allocate_vpm_group_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_vpm_group(rcu_entry_t *entry)
{
	vpm_group_t	   *vpm_group;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header	  = object_header_container_of_rcu_entry(entry);
	vpm_group = vpm_group_container_of_header(header);

	trigger_object_cleanup_vpm_group_event(&ret, vpm_group);

	partition_t *parent = vpm_group->header.partition;
	partition_free(parent, vpm_group, sizeof(vpm_group_t));
	object_put_partition(parent);

	return ret;
}

void
partition_standard_handle_object_init_vrtc(vrtc_create_t vrtc_create,
					   partition_t	*parent)
{
	vrtc_t *vrtc = vrtc_create.vrtc;

	refcount_init(&vrtc->header.refcount);
	spinlock_init(&vrtc->header.lock);

	vrtc->header.type      = OBJECT_TYPE_VRTC;
	vrtc->header.partition = object_get_partition_additional(parent);

	atomic_init(&vrtc->header.state, OBJECT_STATE_INIT);
}

vrtc_ptr_result_t
partition_allocate_vrtc(partition_t *parent, vrtc_create_t create)
{
	void_ptr_result_t alloc_ret;
	vrtc_ptr_result_t obj_ret;
	vrtc_t		 *vrtc;

	const size_t size  = sizeof(vrtc_t);
	const size_t align = alignof(vrtc_t);
	alloc_ret	   = partition_alloc(parent, size, align);

	if (alloc_ret.e != OK) {
		obj_ret = vrtc_ptr_result_error(alloc_ret.e);
		goto allocate_vrtc_error;
	}

	vrtc = (vrtc_t *)alloc_ret.r;
	(void)memset_s(vrtc, size, 0, size);

	assert_debug((create.vrtc == NULL) && (vrtc != NULL));
	create.vrtc = vrtc;

	trigger_object_init_vrtc_event(create, parent);

	spinlock_acquire(&create.vrtc->header.lock);
	error_t err = trigger_object_create_vrtc_event(create);
	spinlock_release(&create.vrtc->header.lock);
	if (err != OK) {
		rcu_enqueue(&vrtc->header.rcu_entry,
			    RCU_UPDATE_CLASS_VRTC_DESTROY);
		obj_ret = vrtc_ptr_result_error(err);
	} else {
		obj_ret = vrtc_ptr_result_ok(vrtc);
	}

allocate_vrtc_error:
	return obj_ret;
}

rcu_update_status_t
partition_destroy_vrtc(rcu_entry_t *entry)
{
	vrtc_t		   *vrtc;
	object_header_t	   *header;
	rcu_update_status_t ret = rcu_update_status_default();

	header = object_header_container_of_rcu_entry(entry);
	vrtc   = vrtc_container_of_header(header);

	trigger_object_cleanup_vrtc_event(&ret, vrtc);

	partition_t *parent = vrtc->header.partition;
	partition_free(parent, vrtc, sizeof(vrtc_t));
	object_put_partition(parent);

	return ret;
}
