// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

addrspace_ptr_result_t
cspace_lookup_addrspace(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_addrspace_t rights);

addrspace_ptr_result_t
cspace_lookup_addrspace_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_addrspace_t rights);

cspace_ptr_result_t
cspace_lookup_cspace(cspace_t *cspace, cap_id_t cap_id,
		     cap_rights_cspace_t rights);

cspace_ptr_result_t
cspace_lookup_cspace_any(cspace_t *cspace, cap_id_t cap_id,
			 cap_rights_cspace_t rights);

doorbell_ptr_result_t
cspace_lookup_doorbell(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_doorbell_t rights);

doorbell_ptr_result_t
cspace_lookup_doorbell_any(cspace_t *cspace, cap_id_t cap_id,
			   cap_rights_doorbell_t rights);

gicv3_its_ptr_result_t
cspace_lookup_gicv3_its(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_gicv3_its_t rights);

gicv3_its_ptr_result_t
cspace_lookup_gicv3_its_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_gicv3_its_t rights);

hwirq_ptr_result_t
cspace_lookup_hwirq(cspace_t *cspace, cap_id_t cap_id,
		    cap_rights_hwirq_t rights);

hwirq_ptr_result_t
cspace_lookup_hwirq_any(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_hwirq_t rights);

memextent_ptr_result_t
cspace_lookup_memextent(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_memextent_t rights);

memextent_ptr_result_t
cspace_lookup_memextent_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_memextent_t rights);

msgqueue_ptr_result_t
cspace_lookup_msgqueue(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_msgqueue_t rights);

msgqueue_ptr_result_t
cspace_lookup_msgqueue_any(cspace_t *cspace, cap_id_t cap_id,
			   cap_rights_msgqueue_t rights);

partition_ptr_result_t
cspace_lookup_partition(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_partition_t rights);

partition_ptr_result_t
cspace_lookup_partition_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_partition_t rights);

thread_ptr_result_t
cspace_lookup_thread(cspace_t *cspace, cap_id_t cap_id,
		     cap_rights_thread_t rights);

thread_ptr_result_t
cspace_lookup_thread_any(cspace_t *cspace, cap_id_t cap_id,
			 cap_rights_thread_t rights);

vic_ptr_result_t
cspace_lookup_vic(cspace_t *cspace, cap_id_t cap_id, cap_rights_vic_t rights);

vic_ptr_result_t
cspace_lookup_vic_any(cspace_t *cspace, cap_id_t cap_id,
		      cap_rights_vic_t rights);

virtio_backend_ptr_result_t
cspace_lookup_virtio_backend(cspace_t *cspace, cap_id_t cap_id,
			     cap_rights_virtio_backend_t rights);

virtio_backend_ptr_result_t
cspace_lookup_virtio_backend_any(cspace_t *cspace, cap_id_t cap_id,
				 cap_rights_virtio_backend_t rights);

vpm_group_ptr_result_t
cspace_lookup_vpm_group(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_vpm_group_t rights);

vpm_group_ptr_result_t
cspace_lookup_vpm_group_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_vpm_group_t rights);

vrtc_ptr_result_t
cspace_lookup_vrtc(cspace_t *cspace, cap_id_t cap_id, cap_rights_vrtc_t rights);

vrtc_ptr_result_t
cspace_lookup_vrtc_any(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_vrtc_t rights);
