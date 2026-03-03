// Automatically generated. Do not modify.

// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <spinlock.h>

#include "event_handlers.h"

static addrspace_ptr_result_t
cspace_lookup_addrspace_common(cspace_t *cspace, cap_id_t cap_id,
			       cap_rights_addrspace_t rights, bool active_only)
{
	object_ptr_result_t    ret;
	addrspace_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_ADDRSPACE,
				   cap_rights_addrspace_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.addrspace != NULL);
		result = addrspace_ptr_result_ok(ret.r.addrspace);
	} else {
		result = addrspace_ptr_result_error(ret.e);
	}
	return result;
}

addrspace_ptr_result_t
cspace_lookup_addrspace(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_addrspace_t rights)
{
	return cspace_lookup_addrspace_common(cspace, cap_id, rights, true);
}

addrspace_ptr_result_t
cspace_lookup_addrspace_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_addrspace_t rights)
{
	return cspace_lookup_addrspace_common(cspace, cap_id, rights, false);
}

static cspace_ptr_result_t
cspace_lookup_cspace_common(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_cspace_t rights, bool active_only)
{
	object_ptr_result_t ret;
	cspace_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_CSPACE,
				   cap_rights_cspace_raw(rights), active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.cspace != NULL);
		result = cspace_ptr_result_ok(ret.r.cspace);
	} else {
		result = cspace_ptr_result_error(ret.e);
	}
	return result;
}

cspace_ptr_result_t
cspace_lookup_cspace(cspace_t *cspace, cap_id_t cap_id,
		     cap_rights_cspace_t rights)
{
	return cspace_lookup_cspace_common(cspace, cap_id, rights, true);
}

cspace_ptr_result_t
cspace_lookup_cspace_any(cspace_t *cspace, cap_id_t cap_id,
			 cap_rights_cspace_t rights)
{
	return cspace_lookup_cspace_common(cspace, cap_id, rights, false);
}

static doorbell_ptr_result_t
cspace_lookup_doorbell_common(cspace_t *cspace, cap_id_t cap_id,
			      cap_rights_doorbell_t rights, bool active_only)
{
	object_ptr_result_t   ret;
	doorbell_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_DOORBELL,
				   cap_rights_doorbell_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.doorbell != NULL);
		result = doorbell_ptr_result_ok(ret.r.doorbell);
	} else {
		result = doorbell_ptr_result_error(ret.e);
	}
	return result;
}

doorbell_ptr_result_t
cspace_lookup_doorbell(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_doorbell_t rights)
{
	return cspace_lookup_doorbell_common(cspace, cap_id, rights, true);
}

doorbell_ptr_result_t
cspace_lookup_doorbell_any(cspace_t *cspace, cap_id_t cap_id,
			   cap_rights_doorbell_t rights)
{
	return cspace_lookup_doorbell_common(cspace, cap_id, rights, false);
}

static gicv3_its_ptr_result_t
cspace_lookup_gicv3_its_common(cspace_t *cspace, cap_id_t cap_id,
			       cap_rights_gicv3_its_t rights, bool active_only)
{
	object_ptr_result_t    ret;
	gicv3_its_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_GICV3_ITS,
				   cap_rights_gicv3_its_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.gicv3_its != NULL);
		result = gicv3_its_ptr_result_ok(ret.r.gicv3_its);
	} else {
		result = gicv3_its_ptr_result_error(ret.e);
	}
	return result;
}

gicv3_its_ptr_result_t
cspace_lookup_gicv3_its(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_gicv3_its_t rights)
{
	return cspace_lookup_gicv3_its_common(cspace, cap_id, rights, true);
}

gicv3_its_ptr_result_t
cspace_lookup_gicv3_its_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_gicv3_its_t rights)
{
	return cspace_lookup_gicv3_its_common(cspace, cap_id, rights, false);
}

static hwirq_ptr_result_t
cspace_lookup_hwirq_common(cspace_t *cspace, cap_id_t cap_id,
			   cap_rights_hwirq_t rights, bool active_only)
{
	object_ptr_result_t ret;
	hwirq_ptr_result_t  result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_HWIRQ,
				   cap_rights_hwirq_raw(rights), active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.hwirq != NULL);
		result = hwirq_ptr_result_ok(ret.r.hwirq);
	} else {
		result = hwirq_ptr_result_error(ret.e);
	}
	return result;
}

hwirq_ptr_result_t
cspace_lookup_hwirq(cspace_t *cspace, cap_id_t cap_id,
		    cap_rights_hwirq_t rights)
{
	return cspace_lookup_hwirq_common(cspace, cap_id, rights, true);
}

hwirq_ptr_result_t
cspace_lookup_hwirq_any(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_hwirq_t rights)
{
	return cspace_lookup_hwirq_common(cspace, cap_id, rights, false);
}

static memextent_ptr_result_t
cspace_lookup_memextent_common(cspace_t *cspace, cap_id_t cap_id,
			       cap_rights_memextent_t rights, bool active_only)
{
	object_ptr_result_t    ret;
	memextent_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_MEMEXTENT,
				   cap_rights_memextent_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.memextent != NULL);
		result = memextent_ptr_result_ok(ret.r.memextent);
	} else {
		result = memextent_ptr_result_error(ret.e);
	}
	return result;
}

memextent_ptr_result_t
cspace_lookup_memextent(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_memextent_t rights)
{
	return cspace_lookup_memextent_common(cspace, cap_id, rights, true);
}

memextent_ptr_result_t
cspace_lookup_memextent_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_memextent_t rights)
{
	return cspace_lookup_memextent_common(cspace, cap_id, rights, false);
}

static msgqueue_ptr_result_t
cspace_lookup_msgqueue_common(cspace_t *cspace, cap_id_t cap_id,
			      cap_rights_msgqueue_t rights, bool active_only)
{
	object_ptr_result_t   ret;
	msgqueue_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_MSGQUEUE,
				   cap_rights_msgqueue_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.msgqueue != NULL);
		result = msgqueue_ptr_result_ok(ret.r.msgqueue);
	} else {
		result = msgqueue_ptr_result_error(ret.e);
	}
	return result;
}

msgqueue_ptr_result_t
cspace_lookup_msgqueue(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_msgqueue_t rights)
{
	return cspace_lookup_msgqueue_common(cspace, cap_id, rights, true);
}

msgqueue_ptr_result_t
cspace_lookup_msgqueue_any(cspace_t *cspace, cap_id_t cap_id,
			   cap_rights_msgqueue_t rights)
{
	return cspace_lookup_msgqueue_common(cspace, cap_id, rights, false);
}

static partition_ptr_result_t
cspace_lookup_partition_common(cspace_t *cspace, cap_id_t cap_id,
			       cap_rights_partition_t rights, bool active_only)
{
	object_ptr_result_t    ret;
	partition_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_PARTITION,
				   cap_rights_partition_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.partition != NULL);
		result = partition_ptr_result_ok(ret.r.partition);
	} else {
		result = partition_ptr_result_error(ret.e);
	}
	return result;
}

partition_ptr_result_t
cspace_lookup_partition(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_partition_t rights)
{
	return cspace_lookup_partition_common(cspace, cap_id, rights, true);
}

partition_ptr_result_t
cspace_lookup_partition_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_partition_t rights)
{
	return cspace_lookup_partition_common(cspace, cap_id, rights, false);
}

static thread_ptr_result_t
cspace_lookup_thread_common(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_thread_t rights, bool active_only)
{
	object_ptr_result_t ret;
	thread_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_THREAD,
				   cap_rights_thread_raw(rights), active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.thread != NULL);
		result = thread_ptr_result_ok(ret.r.thread);
	} else {
		result = thread_ptr_result_error(ret.e);
	}
	return result;
}

thread_ptr_result_t
cspace_lookup_thread(cspace_t *cspace, cap_id_t cap_id,
		     cap_rights_thread_t rights)
{
	return cspace_lookup_thread_common(cspace, cap_id, rights, true);
}

thread_ptr_result_t
cspace_lookup_thread_any(cspace_t *cspace, cap_id_t cap_id,
			 cap_rights_thread_t rights)
{
	return cspace_lookup_thread_common(cspace, cap_id, rights, false);
}

static vic_ptr_result_t
cspace_lookup_vic_common(cspace_t *cspace, cap_id_t cap_id,
			 cap_rights_vic_t rights, bool active_only)
{
	object_ptr_result_t ret;
	vic_ptr_result_t    result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_VIC,
				   cap_rights_vic_raw(rights), active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.vic != NULL);
		result = vic_ptr_result_ok(ret.r.vic);
	} else {
		result = vic_ptr_result_error(ret.e);
	}
	return result;
}

vic_ptr_result_t
cspace_lookup_vic(cspace_t *cspace, cap_id_t cap_id, cap_rights_vic_t rights)
{
	return cspace_lookup_vic_common(cspace, cap_id, rights, true);
}

vic_ptr_result_t
cspace_lookup_vic_any(cspace_t *cspace, cap_id_t cap_id,
		      cap_rights_vic_t rights)
{
	return cspace_lookup_vic_common(cspace, cap_id, rights, false);
}

static virtio_backend_ptr_result_t
cspace_lookup_virtio_backend_common(cspace_t *cspace, cap_id_t cap_id,
				    cap_rights_virtio_backend_t rights,
				    bool			active_only)
{
	object_ptr_result_t	    ret;
	virtio_backend_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_VIRTIO_BACKEND,
				   cap_rights_virtio_backend_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.virtio_backend != NULL);
		result = virtio_backend_ptr_result_ok(ret.r.virtio_backend);
	} else {
		result = virtio_backend_ptr_result_error(ret.e);
	}
	return result;
}

virtio_backend_ptr_result_t
cspace_lookup_virtio_backend(cspace_t *cspace, cap_id_t cap_id,
			     cap_rights_virtio_backend_t rights)
{
	return cspace_lookup_virtio_backend_common(cspace, cap_id, rights,
						   true);
}

virtio_backend_ptr_result_t
cspace_lookup_virtio_backend_any(cspace_t *cspace, cap_id_t cap_id,
				 cap_rights_virtio_backend_t rights)
{
	return cspace_lookup_virtio_backend_common(cspace, cap_id, rights,
						   false);
}

static vpm_group_ptr_result_t
cspace_lookup_vpm_group_common(cspace_t *cspace, cap_id_t cap_id,
			       cap_rights_vpm_group_t rights, bool active_only)
{
	object_ptr_result_t    ret;
	vpm_group_ptr_result_t result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_VPM_GROUP,
				   cap_rights_vpm_group_raw(rights),
				   active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.vpm_group != NULL);
		result = vpm_group_ptr_result_ok(ret.r.vpm_group);
	} else {
		result = vpm_group_ptr_result_error(ret.e);
	}
	return result;
}

vpm_group_ptr_result_t
cspace_lookup_vpm_group(cspace_t *cspace, cap_id_t cap_id,
			cap_rights_vpm_group_t rights)
{
	return cspace_lookup_vpm_group_common(cspace, cap_id, rights, true);
}

vpm_group_ptr_result_t
cspace_lookup_vpm_group_any(cspace_t *cspace, cap_id_t cap_id,
			    cap_rights_vpm_group_t rights)
{
	return cspace_lookup_vpm_group_common(cspace, cap_id, rights, false);
}

static vrtc_ptr_result_t
cspace_lookup_vrtc_common(cspace_t *cspace, cap_id_t cap_id,
			  cap_rights_vrtc_t rights, bool active_only)
{
	object_ptr_result_t ret;
	vrtc_ptr_result_t   result;

	ret = cspace_lookup_object(cspace, cap_id, OBJECT_TYPE_VRTC,
				   cap_rights_vrtc_raw(rights), active_only);
	if (compiler_expected(ret.e == OK)) {
		assert_debug(ret.r.vrtc != NULL);
		result = vrtc_ptr_result_ok(ret.r.vrtc);
	} else {
		result = vrtc_ptr_result_error(ret.e);
	}
	return result;
}

vrtc_ptr_result_t
cspace_lookup_vrtc(cspace_t *cspace, cap_id_t cap_id, cap_rights_vrtc_t rights)
{
	return cspace_lookup_vrtc_common(cspace, cap_id, rights, true);
}

vrtc_ptr_result_t
cspace_lookup_vrtc_any(cspace_t *cspace, cap_id_t cap_id,
		       cap_rights_vrtc_t rights)
{
	return cspace_lookup_vrtc_common(cspace, cap_id, rights, false);
}
