// Automatically generated. Do not modify.

// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <list.h>
#include <object.h>
#include <partition.h>
#include <spinlock.h>

#include "event_handlers.h"

extern list_t	  partition_list;
extern spinlock_t partition_list_lock;

list_t	   partition_list;
spinlock_t partition_list_lock;

void
object_lists_handle_boot_cold_init(void)
{
	spinlock_init(&partition_list_lock);
	list_init(&partition_list);

	partition_t *hyp_partition = partition_get_private();

	// Add hyp_partition manually
	spinlock_acquire(&partition_list_lock);
	list_insert_at_tail_release(&partition_list,
				    &hyp_partition->partition_list_node);
	spinlock_release(&partition_list_lock);

	spinlock_init(&hyp_partition->addrspace_list_lock);
	list_init(&hyp_partition->addrspace_list);
	spinlock_init(&hyp_partition->cspace_list_lock);
	list_init(&hyp_partition->cspace_list);
	spinlock_init(&hyp_partition->doorbell_list_lock);
	list_init(&hyp_partition->doorbell_list);
	spinlock_init(&hyp_partition->gicv3_its_list_lock);
	list_init(&hyp_partition->gicv3_its_list);
	spinlock_init(&hyp_partition->memextent_list_lock);
	list_init(&hyp_partition->memextent_list);
	spinlock_init(&hyp_partition->msgqueue_list_lock);
	list_init(&hyp_partition->msgqueue_list);
	spinlock_init(&hyp_partition->thread_list_lock);
	list_init(&hyp_partition->thread_list);
	spinlock_init(&hyp_partition->vic_list_lock);
	list_init(&hyp_partition->vic_list);
	spinlock_init(&hyp_partition->virtio_backend_list_lock);
	list_init(&hyp_partition->virtio_backend_list);
	spinlock_init(&hyp_partition->vpm_group_list_lock);
	list_init(&hyp_partition->vpm_group_list);
	spinlock_init(&hyp_partition->vrtc_list_lock);
	list_init(&hyp_partition->vrtc_list);
}

error_t
object_lists_handle_object_create_partition(partition_create_t partition_create)
{
	partition_t *partition = partition_create.partition;

	spinlock_acquire(&partition_list_lock);
	list_insert_at_tail_release(&partition_list,
				    &partition->partition_list_node);
	spinlock_release(&partition_list_lock);

	spinlock_init(&partition->addrspace_list_lock);
	list_init(&partition->addrspace_list);
	spinlock_init(&partition->cspace_list_lock);
	list_init(&partition->cspace_list);
	spinlock_init(&partition->doorbell_list_lock);
	list_init(&partition->doorbell_list);
	spinlock_init(&partition->gicv3_its_list_lock);
	list_init(&partition->gicv3_its_list);
	spinlock_init(&partition->memextent_list_lock);
	list_init(&partition->memextent_list);
	spinlock_init(&partition->msgqueue_list_lock);
	list_init(&partition->msgqueue_list);
	spinlock_init(&partition->thread_list_lock);
	list_init(&partition->thread_list);
	spinlock_init(&partition->vic_list_lock);
	list_init(&partition->vic_list);
	spinlock_init(&partition->virtio_backend_list_lock);
	list_init(&partition->virtio_backend_list);
	spinlock_init(&partition->vpm_group_list_lock);
	list_init(&partition->vpm_group_list);
	spinlock_init(&partition->vrtc_list_lock);
	list_init(&partition->vrtc_list);

	return OK;
}

void
object_lists_handle_object_cleanup_partition(partition_t *partition)
{
	spinlock_acquire(&partition_list_lock);
	(void)list_delete_node(&partition_list,
			       &partition->partition_list_node);
	spinlock_release(&partition_list_lock);

	spinlock_acquire(&partition->addrspace_list_lock);
	assert(list_is_empty(&partition->addrspace_list));
	spinlock_release(&partition->addrspace_list_lock);
	spinlock_acquire(&partition->cspace_list_lock);
	assert(list_is_empty(&partition->cspace_list));
	spinlock_release(&partition->cspace_list_lock);
	spinlock_acquire(&partition->doorbell_list_lock);
	assert(list_is_empty(&partition->doorbell_list));
	spinlock_release(&partition->doorbell_list_lock);
	spinlock_acquire(&partition->gicv3_its_list_lock);
	assert(list_is_empty(&partition->gicv3_its_list));
	spinlock_release(&partition->gicv3_its_list_lock);
	spinlock_acquire(&partition->memextent_list_lock);
	assert(list_is_empty(&partition->memextent_list));
	spinlock_release(&partition->memextent_list_lock);
	spinlock_acquire(&partition->msgqueue_list_lock);
	assert(list_is_empty(&partition->msgqueue_list));
	spinlock_release(&partition->msgqueue_list_lock);
	spinlock_acquire(&partition->thread_list_lock);
	assert(list_is_empty(&partition->thread_list));
	spinlock_release(&partition->thread_list_lock);
	spinlock_acquire(&partition->vic_list_lock);
	assert(list_is_empty(&partition->vic_list));
	spinlock_release(&partition->vic_list_lock);
	spinlock_acquire(&partition->virtio_backend_list_lock);
	assert(list_is_empty(&partition->virtio_backend_list));
	spinlock_release(&partition->virtio_backend_list_lock);
	spinlock_acquire(&partition->vpm_group_list_lock);
	assert(list_is_empty(&partition->vpm_group_list));
	spinlock_release(&partition->vpm_group_list_lock);
	spinlock_acquire(&partition->vrtc_list_lock);
	assert(list_is_empty(&partition->vrtc_list));
	spinlock_release(&partition->vrtc_list_lock);
}

error_t
object_lists_handle_object_create_addrspace(addrspace_create_t addrspace_create)
{
	addrspace_t *addrspace = addrspace_create.addrspace;

	partition_t *partition = addrspace->header.partition;

	spinlock_acquire(&partition->addrspace_list_lock);
	list_insert_at_tail_release(&partition->addrspace_list,
				    &addrspace->addrspace_list_node);
	spinlock_release(&partition->addrspace_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_addrspace(addrspace_t *addrspace)
{
	partition_t *partition = addrspace->header.partition;

	spinlock_acquire(&partition->addrspace_list_lock);
	(void)list_delete_node(&partition->addrspace_list,
			       &addrspace->addrspace_list_node);
	spinlock_release(&partition->addrspace_list_lock);
}

error_t
object_lists_handle_object_create_cspace(cspace_create_t cspace_create)
{
	cspace_t *cspace = cspace_create.cspace;

	partition_t *partition = cspace->header.partition;

	spinlock_acquire(&partition->cspace_list_lock);
	list_insert_at_tail_release(&partition->cspace_list,
				    &cspace->cspace_list_node);
	spinlock_release(&partition->cspace_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_cspace(cspace_t *cspace)
{
	partition_t *partition = cspace->header.partition;

	spinlock_acquire(&partition->cspace_list_lock);
	(void)list_delete_node(&partition->cspace_list,
			       &cspace->cspace_list_node);
	spinlock_release(&partition->cspace_list_lock);
}

error_t
object_lists_handle_object_create_doorbell(doorbell_create_t doorbell_create)
{
	doorbell_t *doorbell = doorbell_create.doorbell;

	partition_t *partition = doorbell->header.partition;

	spinlock_acquire(&partition->doorbell_list_lock);
	list_insert_at_tail_release(&partition->doorbell_list,
				    &doorbell->doorbell_list_node);
	spinlock_release(&partition->doorbell_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_doorbell(doorbell_t *doorbell)
{
	partition_t *partition = doorbell->header.partition;

	spinlock_acquire(&partition->doorbell_list_lock);
	(void)list_delete_node(&partition->doorbell_list,
			       &doorbell->doorbell_list_node);
	spinlock_release(&partition->doorbell_list_lock);
}

error_t
object_lists_handle_object_create_gicv3_its(gicv3_its_create_t gicv3_its_create)
{
	gicv3_its_t *gicv3_its = gicv3_its_create.gicv3_its;

	partition_t *partition = gicv3_its->header.partition;

	spinlock_acquire(&partition->gicv3_its_list_lock);
	list_insert_at_tail_release(&partition->gicv3_its_list,
				    &gicv3_its->gicv3_its_list_node);
	spinlock_release(&partition->gicv3_its_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_gicv3_its(gicv3_its_t *gicv3_its)
{
	partition_t *partition = gicv3_its->header.partition;

	spinlock_acquire(&partition->gicv3_its_list_lock);
	(void)list_delete_node(&partition->gicv3_its_list,
			       &gicv3_its->gicv3_its_list_node);
	spinlock_release(&partition->gicv3_its_list_lock);
}

error_t
object_lists_handle_object_create_memextent(memextent_create_t memextent_create)
{
	memextent_t *memextent = memextent_create.memextent;

	partition_t *partition = memextent->header.partition;

	spinlock_acquire(&partition->memextent_list_lock);
	list_insert_at_tail_release(&partition->memextent_list,
				    &memextent->memextent_list_node);
	spinlock_release(&partition->memextent_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_memextent(memextent_t *memextent)
{
	partition_t *partition = memextent->header.partition;

	spinlock_acquire(&partition->memextent_list_lock);
	(void)list_delete_node(&partition->memextent_list,
			       &memextent->memextent_list_node);
	spinlock_release(&partition->memextent_list_lock);
}

error_t
object_lists_handle_object_create_msgqueue(msgqueue_create_t msgqueue_create)
{
	msgqueue_t *msgqueue = msgqueue_create.msgqueue;

	partition_t *partition = msgqueue->header.partition;

	spinlock_acquire(&partition->msgqueue_list_lock);
	list_insert_at_tail_release(&partition->msgqueue_list,
				    &msgqueue->msgqueue_list_node);
	spinlock_release(&partition->msgqueue_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_msgqueue(msgqueue_t *msgqueue)
{
	partition_t *partition = msgqueue->header.partition;

	spinlock_acquire(&partition->msgqueue_list_lock);
	(void)list_delete_node(&partition->msgqueue_list,
			       &msgqueue->msgqueue_list_node);
	spinlock_release(&partition->msgqueue_list_lock);
}

error_t
object_lists_handle_object_create_thread(thread_create_t thread_create)
{
	thread_t *thread = thread_create.thread;

	partition_t *partition = thread->header.partition;

	spinlock_acquire(&partition->thread_list_lock);
	list_insert_at_tail_release(&partition->thread_list,
				    &thread->thread_list_node);
	spinlock_release(&partition->thread_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_thread(thread_t *thread)
{
	partition_t *partition = thread->header.partition;

	spinlock_acquire(&partition->thread_list_lock);
	(void)list_delete_node(&partition->thread_list,
			       &thread->thread_list_node);
	spinlock_release(&partition->thread_list_lock);
}

error_t
object_lists_handle_object_create_vic(vic_create_t vic_create)
{
	vic_t *vic = vic_create.vic;

	partition_t *partition = vic->header.partition;

	spinlock_acquire(&partition->vic_list_lock);
	list_insert_at_tail_release(&partition->vic_list, &vic->vic_list_node);
	spinlock_release(&partition->vic_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_vic(vic_t *vic)
{
	partition_t *partition = vic->header.partition;

	spinlock_acquire(&partition->vic_list_lock);
	(void)list_delete_node(&partition->vic_list, &vic->vic_list_node);
	spinlock_release(&partition->vic_list_lock);
}

error_t
object_lists_handle_object_create_virtio_backend(
	virtio_backend_create_t virtio_backend_create)
{
	virtio_backend_t *virtio_backend = virtio_backend_create.virtio_backend;

	partition_t *partition = virtio_backend->header.partition;

	spinlock_acquire(&partition->virtio_backend_list_lock);
	list_insert_at_tail_release(&partition->virtio_backend_list,
				    &virtio_backend->virtio_backend_list_node);
	spinlock_release(&partition->virtio_backend_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_virtio_backend(
	virtio_backend_t *virtio_backend)
{
	partition_t *partition = virtio_backend->header.partition;

	spinlock_acquire(&partition->virtio_backend_list_lock);
	(void)list_delete_node(&partition->virtio_backend_list,
			       &virtio_backend->virtio_backend_list_node);
	spinlock_release(&partition->virtio_backend_list_lock);
}

error_t
object_lists_handle_object_create_vpm_group(vpm_group_create_t vpm_group_create)
{
	vpm_group_t *vpm_group = vpm_group_create.vpm_group;

	partition_t *partition = vpm_group->header.partition;

	spinlock_acquire(&partition->vpm_group_list_lock);
	list_insert_at_tail_release(&partition->vpm_group_list,
				    &vpm_group->vpm_group_list_node);
	spinlock_release(&partition->vpm_group_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_vpm_group(vpm_group_t *vpm_group)
{
	partition_t *partition = vpm_group->header.partition;

	spinlock_acquire(&partition->vpm_group_list_lock);
	(void)list_delete_node(&partition->vpm_group_list,
			       &vpm_group->vpm_group_list_node);
	spinlock_release(&partition->vpm_group_list_lock);
}

error_t
object_lists_handle_object_create_vrtc(vrtc_create_t vrtc_create)
{
	vrtc_t *vrtc = vrtc_create.vrtc;

	partition_t *partition = vrtc->header.partition;

	spinlock_acquire(&partition->vrtc_list_lock);
	list_insert_at_tail_release(&partition->vrtc_list,
				    &vrtc->vrtc_list_node);
	spinlock_release(&partition->vrtc_list_lock);

	return OK;
}

void
object_lists_handle_object_cleanup_vrtc(vrtc_t *vrtc)
{
	partition_t *partition = vrtc->header.partition;

	spinlock_acquire(&partition->vrtc_list_lock);
	(void)list_delete_node(&partition->vrtc_list, &vrtc->vrtc_list_node);
	spinlock_release(&partition->vrtc_list_lock);
}
