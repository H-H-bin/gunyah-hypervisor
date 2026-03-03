// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

addrspace_t *
object_get_addrspace_additional(addrspace_t *addrspace);

bool
object_get_addrspace_safe(addrspace_t *addrspace);

void
object_put_addrspace(addrspace_t *addrspace);

error_t
object_activate_addrspace(addrspace_t *addrspace);

cspace_t *
object_get_cspace_additional(cspace_t *cspace);

bool
object_get_cspace_safe(cspace_t *cspace);

void
object_put_cspace(cspace_t *cspace);

error_t
object_activate_cspace(cspace_t *cspace);

doorbell_t *
object_get_doorbell_additional(doorbell_t *doorbell);

bool
object_get_doorbell_safe(doorbell_t *doorbell);

void
object_put_doorbell(doorbell_t *doorbell);

error_t
object_activate_doorbell(doorbell_t *doorbell);

gicv3_its_t *
object_get_gicv3_its_additional(gicv3_its_t *gicv3_its);

bool
object_get_gicv3_its_safe(gicv3_its_t *gicv3_its);

void
object_put_gicv3_its(gicv3_its_t *gicv3_its);

error_t
object_activate_gicv3_its(gicv3_its_t *gicv3_its);

hwirq_t *
object_get_hwirq_additional(hwirq_t *hwirq);

bool
object_get_hwirq_safe(hwirq_t *hwirq);

void
object_put_hwirq(hwirq_t *hwirq);

error_t
object_activate_hwirq(hwirq_t *hwirq);

memextent_t *
object_get_memextent_additional(memextent_t *memextent);

bool
object_get_memextent_safe(memextent_t *memextent);

void
object_put_memextent(memextent_t *memextent);

error_t
object_activate_memextent(memextent_t *memextent);

msgqueue_t *
object_get_msgqueue_additional(msgqueue_t *msgqueue);

bool
object_get_msgqueue_safe(msgqueue_t *msgqueue);

void
object_put_msgqueue(msgqueue_t *msgqueue);

error_t
object_activate_msgqueue(msgqueue_t *msgqueue);

partition_t *
object_get_partition_additional(partition_t *partition);

bool
object_get_partition_safe(partition_t *partition);

void
object_put_partition(partition_t *partition);

error_t
object_activate_partition(partition_t *partition);

thread_t *
object_get_thread_additional(thread_t *thread);

bool
object_get_thread_safe(thread_t *thread);

void
object_put_thread(thread_t *thread);

error_t
object_activate_thread(thread_t *thread);

vic_t *
object_get_vic_additional(vic_t *vic);

bool
object_get_vic_safe(vic_t *vic);

void
object_put_vic(vic_t *vic);

error_t
object_activate_vic(vic_t *vic);

virtio_backend_t *
object_get_virtio_backend_additional(virtio_backend_t *virtio_backend);

bool
object_get_virtio_backend_safe(virtio_backend_t *virtio_backend);

void
object_put_virtio_backend(virtio_backend_t *virtio_backend);

error_t
object_activate_virtio_backend(virtio_backend_t *virtio_backend);

vpm_group_t *
object_get_vpm_group_additional(vpm_group_t *vpm_group);

bool
object_get_vpm_group_safe(vpm_group_t *vpm_group);

void
object_put_vpm_group(vpm_group_t *vpm_group);

error_t
object_activate_vpm_group(vpm_group_t *vpm_group);

vrtc_t *
object_get_vrtc_additional(vrtc_t *vrtc);

bool
object_get_vrtc_safe(vrtc_t *vrtc);

void
object_put_vrtc(vrtc_t *vrtc);

error_t
object_activate_vrtc(vrtc_t *vrtc);

object_ptr_t
object_get_additional(object_type_t type, object_ptr_t object);

bool
object_get_safe(object_type_t type, object_ptr_t object);

void
object_put(object_type_t type, object_ptr_t object);

object_header_t *
object_get_header(object_type_t type, object_ptr_t object);

error_t
object_activate(object_type_t type, object_ptr_t object);
