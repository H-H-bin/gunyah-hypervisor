// Automatically generated. Do not modify.

// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(HYPERCALLS)
#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>
#include <hypconstants.h>
#include <hyprights.h>

#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition_alloc.h>
#include <thread.h>

#include <events/object.h>

hypercall_partition_create_addrspace_result_t
hypercall_partition_create_addrspace(cap_id_t src_partition_cap,
				     cap_id_t cspace_cap)
{
	cspace_t *cspace				  = cspace_get_self();
	hypercall_partition_create_addrspace_result_t ret = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	addrspace_create_t params = { 0 };
	trigger_object_get_defaults_addrspace_event(&params);

	addrspace_ptr_result_t result =
		partition_allocate_addrspace(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.addrspace	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_ADDRSPACE);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_addrspace(obj_ptr.addrspace);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_cspace_result_t
hypercall_partition_create_cspace(cap_id_t src_partition_cap,
				  cap_id_t cspace_cap)
{
	cspace_t				  *cspace = cspace_get_self();
	hypercall_partition_create_cspace_result_t ret	  = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	cspace_create_t params = { 0 };
	trigger_object_get_defaults_cspace_event(&params);

	cspace_ptr_result_t result =
		partition_allocate_cspace(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.cspace		  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_CSPACE);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_cspace(obj_ptr.cspace);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_doorbell_result_t
hypercall_partition_create_doorbell(cap_id_t src_partition_cap,
				    cap_id_t cspace_cap)
{
	cspace_t				    *cspace = cspace_get_self();
	hypercall_partition_create_doorbell_result_t ret    = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	doorbell_create_t params = { 0 };
	trigger_object_get_defaults_doorbell_event(&params);

	doorbell_ptr_result_t result =
		partition_allocate_doorbell(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.doorbell	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_DOORBELL);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_doorbell(obj_ptr.doorbell);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_memextent_result_t
hypercall_partition_create_memextent(cap_id_t src_partition_cap,
				     cap_id_t cspace_cap)
{
	cspace_t *cspace				  = cspace_get_self();
	hypercall_partition_create_memextent_result_t ret = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	memextent_create_t params = { 0 };
	trigger_object_get_defaults_memextent_event(&params);

	memextent_ptr_result_t result =
		partition_allocate_memextent(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.memextent	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_MEMEXTENT);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_memextent(obj_ptr.memextent);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_msgqueue_result_t
hypercall_partition_create_msgqueue(cap_id_t src_partition_cap,
				    cap_id_t cspace_cap)
{
	cspace_t				    *cspace = cspace_get_self();
	hypercall_partition_create_msgqueue_result_t ret    = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	msgqueue_create_t params = { 0 };
	trigger_object_get_defaults_msgqueue_event(&params);

	msgqueue_ptr_result_t result =
		partition_allocate_msgqueue(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.msgqueue	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_MSGQUEUE);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_msgqueue(obj_ptr.msgqueue);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_thread_result_t
hypercall_partition_create_thread(cap_id_t src_partition_cap,
				  cap_id_t cspace_cap)
{
	cspace_t				  *cspace = cspace_get_self();
	hypercall_partition_create_thread_result_t ret	  = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	thread_create_t params = { 0 };
	trigger_object_get_defaults_thread_event(&params);

	thread_ptr_result_t result =
		partition_allocate_thread(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.thread		  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_THREAD);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_thread(obj_ptr.thread);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_vic_result_t
hypercall_partition_create_vic(cap_id_t src_partition_cap, cap_id_t cspace_cap)
{
	cspace_t			       *cspace = cspace_get_self();
	hypercall_partition_create_vic_result_t ret    = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	vic_create_t params = { 0 };
	trigger_object_get_defaults_vic_event(&params);

	vic_ptr_result_t result = partition_allocate_vic(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.vic = result.r;
	cap_id_result_t capid_ret =
		cspace_create_master_cap(dest_cspace, obj_ptr, OBJECT_TYPE_VIC);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_vic(obj_ptr.vic);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_virtio_backend_result_t
hypercall_partition_create_virtio_backend(cap_id_t src_partition_cap,
					  cap_id_t cspace_cap)
{
	cspace_t *cspace = cspace_get_self();
	hypercall_partition_create_virtio_backend_result_t ret = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	virtio_backend_create_t params = { 0 };
	trigger_object_get_defaults_virtio_backend_event(&params);

	virtio_backend_ptr_result_t result =
		partition_allocate_virtio_backend(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.virtio_backend	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_VIRTIO_BACKEND);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_virtio_backend(obj_ptr.virtio_backend);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_vpm_group_result_t
hypercall_partition_create_vpm_group(cap_id_t src_partition_cap,
				     cap_id_t cspace_cap)
{
	cspace_t *cspace				  = cspace_get_self();
	hypercall_partition_create_vpm_group_result_t ret = { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	vpm_group_create_t params = { 0 };
	trigger_object_get_defaults_vpm_group_event(&params);

	vpm_group_ptr_result_t result =
		partition_allocate_vpm_group(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.vpm_group	  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_VPM_GROUP);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_vpm_group(obj_ptr.vpm_group);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}

hypercall_partition_create_vrtc_result_t
hypercall_partition_create_vrtc(cap_id_t src_partition_cap, cap_id_t cspace_cap)
{
	cspace_t				*cspace = cspace_get_self();
	hypercall_partition_create_vrtc_result_t ret	= { 0 };

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, src_partition_cap, CAP_RIGHTS_PARTITION_OBJECT_CREATE);
	if (compiler_unexpected(p.e != OK)) {
		ret.error = p.e;
		goto out;
	}
	partition_t *src_partition = p.r;

	cspace_ptr_result_t c;
	c = cspace_lookup_cspace(cspace, cspace_cap,
				 CAP_RIGHTS_CSPACE_CAP_CREATE);
	if (compiler_unexpected(c.e != OK)) {
		ret.error = c.e;
		goto out_partition_release;
	}
	cspace_t *dest_cspace = c.r;

	vrtc_create_t params = { 0 };
	trigger_object_get_defaults_vrtc_event(&params);

	vrtc_ptr_result_t result =
		partition_allocate_vrtc(src_partition, params);
	if (result.e != OK) {
		ret.error = result.e;
		goto out_cspace_release;
	}
	object_ptr_t obj_ptr;

	obj_ptr.vrtc		  = result.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		dest_cspace, obj_ptr, OBJECT_TYPE_VRTC);
	if (capid_ret.e != OK) {
		ret.error = capid_ret.e;
		object_put_vrtc(obj_ptr.vrtc);
		goto out_cspace_release;
	}

	ret.error   = OK;
	ret.new_cap = capid_ret.r;

out_cspace_release:
	object_put_cspace(dest_cspace);
out_partition_release:
	object_put_partition(src_partition);
out:
	return ret;
}
#else
extern int unused;
#endif
