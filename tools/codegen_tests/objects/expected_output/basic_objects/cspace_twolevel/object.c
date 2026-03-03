// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hyprights.h>

#include <list.h>
#include <panic.h>
#include <spinlock.h>

#include "cspace_object.h"
#include "event_handlers.h"

error_t
cspace_init_addrspace_cap_list(addrspace_create_t addrspace_create)
{
	addrspace_t *obj = addrspace_create.addrspace;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_cspace_cap_list(cspace_create_t cspace_create)
{
	cspace_t *obj = cspace_create.cspace;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_doorbell_cap_list(doorbell_create_t doorbell_create)
{
	doorbell_t *obj = doorbell_create.doorbell;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_gicv3_its_cap_list(gicv3_its_create_t gicv3_its_create)
{
	gicv3_its_t *obj = gicv3_its_create.gicv3_its;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_hwirq_cap_list(hwirq_create_t hwirq_create)
{
	hwirq_t *obj = hwirq_create.hwirq;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_memextent_cap_list(memextent_create_t memextent_create)
{
	memextent_t *obj = memextent_create.memextent;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_msgqueue_cap_list(msgqueue_create_t msgqueue_create)
{
	msgqueue_t *obj = msgqueue_create.msgqueue;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_partition_cap_list(partition_create_t partition_create)
{
	partition_t *obj = partition_create.partition;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_thread_cap_list(thread_create_t thread_create)
{
	thread_t *obj = thread_create.thread;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_vic_cap_list(vic_create_t vic_create)
{
	vic_t *obj = vic_create.vic;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_virtio_backend_cap_list(
	virtio_backend_create_t virtio_backend_create)
{
	virtio_backend_t *obj = virtio_backend_create.virtio_backend;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_vpm_group_cap_list(vpm_group_create_t vpm_group_create)
{
	vpm_group_t *obj = vpm_group_create.vpm_group;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

error_t
cspace_init_vrtc_cap_list(vrtc_create_t vrtc_create)
{
	vrtc_t *obj = vrtc_create.vrtc;
	spinlock_init(&obj->header.cap_list_lock);
	list_init(&obj->header.cap_list);
	return OK;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"

cap_rights_t
cspace_get_rights_all(object_type_t type)
{
	cap_rights_t ret;

	switch (type) {
	case OBJECT_TYPE_ADDRSPACE:
		ret = cap_rights_addrspace_raw(CAP_RIGHTS_ADDRSPACE_ALL);
		break;
	case OBJECT_TYPE_CSPACE:
		ret = cap_rights_cspace_raw(CAP_RIGHTS_CSPACE_ALL);
		break;
	case OBJECT_TYPE_DOORBELL:
		ret = cap_rights_doorbell_raw(CAP_RIGHTS_DOORBELL_ALL);
		break;
	case OBJECT_TYPE_GICV3_ITS:
		ret = cap_rights_gicv3_its_raw(CAP_RIGHTS_GICV3_ITS_ALL);
		break;
	case OBJECT_TYPE_HWIRQ:
		ret = cap_rights_hwirq_raw(CAP_RIGHTS_HWIRQ_ALL);
		break;
	case OBJECT_TYPE_MEMEXTENT:
		ret = cap_rights_memextent_raw(CAP_RIGHTS_MEMEXTENT_ALL);
		break;
	case OBJECT_TYPE_MSGQUEUE:
		ret = cap_rights_msgqueue_raw(CAP_RIGHTS_MSGQUEUE_ALL);
		break;
	case OBJECT_TYPE_PARTITION:
		ret = cap_rights_partition_raw(CAP_RIGHTS_PARTITION_ALL);
		break;
	case OBJECT_TYPE_THREAD:
		ret = cap_rights_thread_raw(CAP_RIGHTS_THREAD_ALL);
		break;
	case OBJECT_TYPE_VIC:
		ret = cap_rights_vic_raw(CAP_RIGHTS_VIC_ALL);
		break;
	case OBJECT_TYPE_VIRTIO_BACKEND:
		ret = cap_rights_virtio_backend_raw(
			CAP_RIGHTS_VIRTIO_BACKEND_ALL);
		break;
	case OBJECT_TYPE_VPM_GROUP:
		ret = cap_rights_vpm_group_raw(CAP_RIGHTS_VPM_GROUP_ALL);
		break;
	case OBJECT_TYPE_VRTC:
		ret = cap_rights_vrtc_raw(CAP_RIGHTS_VRTC_ALL);
		break;
	default:
		panic("unknown object type");
	}

	return ret;
}
