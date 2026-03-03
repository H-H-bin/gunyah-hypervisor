// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

addrspace_ptr_result_t
partition_allocate_addrspace(partition_t *parent, addrspace_create_t create);

cspace_ptr_result_t
partition_allocate_cspace(partition_t *parent, cspace_create_t create);

doorbell_ptr_result_t
partition_allocate_doorbell(partition_t *parent, doorbell_create_t create);

gicv3_its_ptr_result_t
partition_allocate_gicv3_its(partition_t *parent, gicv3_its_create_t create);

hwirq_ptr_result_t
partition_allocate_hwirq(partition_t *parent, hwirq_create_t create);

memextent_ptr_result_t
partition_allocate_memextent(partition_t *parent, memextent_create_t create);

msgqueue_ptr_result_t
partition_allocate_msgqueue(partition_t *parent, msgqueue_create_t create);

partition_ptr_result_t
partition_allocate_partition(partition_t *parent, partition_create_t create);

thread_ptr_result_t
partition_allocate_thread(partition_t *parent, thread_create_t create);

vic_ptr_result_t
partition_allocate_vic(partition_t *parent, vic_create_t create);

virtio_backend_ptr_result_t
partition_allocate_virtio_backend(partition_t		 *parent,
				  virtio_backend_create_t create);

vpm_group_ptr_result_t
partition_allocate_vpm_group(partition_t *parent, vpm_group_create_t create);

vrtc_ptr_result_t
partition_allocate_vrtc(partition_t *parent, vrtc_create_t create);
