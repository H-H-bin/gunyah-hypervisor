// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition.h>
#include <spinlock.h>
#include <util.h>
#include <vic.h>
#include <virq.h>
#include <virtio.h>

error_t
hypercall_virtio_backend_bind_virq(cap_id_t virtio_backend_cap,
				   cap_id_t vic_cap, virq_t virq)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_BIND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	vic_ptr_result_t v =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(v.e != OK)) {
		err = v.e;
		goto out_virtio_backend_release;
	}
	vic_t *vic = v.r;

	err = vic_bind_shared(&virtio_backend->virq_source, vic, virq,
			      VIRQ_TRIGGER_VIRTIO_BACKEND);

	object_put_vic(vic);

	if ((err == OK) &&
	    !virtio_backend_notify_reason_is_empty(
		    atomic_load_relaxed(&virtio_backend->reason))) {
		(void)virq_assert(&virtio_backend->virq_source, false);
	}

out_virtio_backend_release:
	object_put_virtio_backend(virtio_backend);
out:
	return err;
}

error_t
hypercall_virtio_backend_unbind_virq(cap_id_t virtio_backend_cap)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_BIND_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		err = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	vic_unbind_sync(&virtio_backend->virq_source);
	err = OK;

	object_put_virtio_backend(virtio_backend);
out:
	return err;
}

error_t
hypercall_virtio_backend_notify(cap_id_t virtio_backend_cap,
				uint32_t interrupt_status)
{
	error_t	  ret;
	cspace_t *cspace		   = cspace_get_self();
	uint32_t  handled_interrupt_status = 0U;

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_ASSERT_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	if ((interrupt_status & (uint32_t)util_bit(0)) != 0U) {
		// Queue ready (for an unspecified queue).
		error_t err = virtio_queue_ready(&virtio_backend->virtio,
						 VIRTIO_QUEUE_UNSPECIFIED);
		if (err != OK) {
			ret = err;
			goto out_virtio_backend_release;
		}
		handled_interrupt_status |= (uint32_t)util_bit(0);
	}

	if ((interrupt_status & util_bit(1)) != 0U) {
		// Config updated.
		error_t err = virtio_config_update_end(&virtio_backend->virtio);
		if (err != OK) {
			ret = err;
			goto out_virtio_backend_release;
		}
		handled_interrupt_status |= (uint32_t)util_bit(1);
	}

	ret = (interrupt_status == handled_interrupt_status)
		      ? OK
		      : ERROR_UNIMPLEMENTED;

out_virtio_backend_release:
	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

error_t
hypercall_virtio_backend_set_dev_features(cap_id_t virtio_backend_cap,
					  uint32_t feature_sel,
					  uint32_t dev_feat)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	ret = virtio_set_dev_features(&virtio_backend->virtio, feature_sel,
				      dev_feat);
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

error_t
hypercall_virtio_backend_set_queue_size_max(cap_id_t virtio_backend_cap,
					    uint32_t queue_sel,
					    uint32_t queue_size_max)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	ret = virtio_set_queue_size_max(&virtio_backend->virtio, queue_sel,
					queue_size_max);
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

hypercall_virtio_backend_get_drv_features_result_t
hypercall_virtio_backend_get_drv_features(cap_id_t virtio_backend_cap,
					  uint32_t feature_sel)
{
	hypercall_virtio_backend_get_drv_features_result_t ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = (hypercall_virtio_backend_get_drv_features_result_t){
			.error = p.e,
		};
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	uint32_result_t res =
		virtio_get_drv_features(&virtio_backend->virtio, feature_sel);
	ret.error = res.e;
	if (res.e == OK) {
		ret = (hypercall_virtio_backend_get_drv_features_result_t){
			.drv_feat = res.r,
			.error	  = OK,
		};
	} else {
		ret = (hypercall_virtio_backend_get_drv_features_result_t){
			.error = res.e,
		};
	}
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

hypercall_virtio_backend_get_queue_info_result_t
hypercall_virtio_backend_get_queue_info(cap_id_t virtio_backend_cap,
					uint32_t queue_sel)
{
	hypercall_virtio_backend_get_queue_info_result_t ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = (hypercall_virtio_backend_get_queue_info_result_t){
			.error = p.e,
		};
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	virtio_queue_info_result_t res =
		virtio_get_queue_info(&virtio_backend->virtio, queue_sel, true);

	if (res.e == OK) {
		ret = (hypercall_virtio_backend_get_queue_info_result_t){
			.queue_size  = (uint16_t)res.r.size,
			.queue_ready = res.r.ready,
			.queue_desc  = res.r.desc,
			.queue_drv   = res.r.drv,
			.queue_dev   = res.r.dev,
			.error	     = OK,
		};
	} else {
		ret = (hypercall_virtio_backend_get_queue_info_result_t){
			.error = res.e,
		};
	}
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

hypercall_virtio_backend_get_notification_result_t
hypercall_virtio_backend_get_notification(cap_id_t virtio_backend_cap)
{
	hypercall_virtio_backend_get_notification_result_t ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = (hypercall_virtio_backend_get_notification_result_t){
			.error = p.e,
		};
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	spinlock_acquire(&virtio_backend->lock);
	uint64_t vqs_bitmap = atomic_exchange_explicit(
		&virtio_backend->vqs_bitmap, 0U, memory_order_relaxed);
	virtio_backend_notify_reason_t reason =
		atomic_load_relaxed(&virtio_backend->reason);
	atomic_store_relaxed(&virtio_backend->reason,
			     virtio_backend_notify_reason_default());
	spinlock_release(&virtio_backend->lock);

	ret = (hypercall_virtio_backend_get_notification_result_t){
		.reason	    = reason,
		.vqs_bitmap = vqs_bitmap,
		.error	    = OK,
	};

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

error_t
hypercall_virtio_backend_acknowledge_reset(cap_id_t virtio_backend_cap)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	ret = virtio_reset_complete(&virtio_backend->virtio);
	if ((ret == ERROR_BUSY) && !virtio_backend_option_flags_get_sync_reset(
					   &virtio_backend->flags)) {
		// Suppress ERROR_BUSY if synchronous reset is disabled
		ret = OK;
	}
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

error_t
hypercall_virtio_backend_update_status(cap_id_t	       virtio_backend_cap,
				       virtio_status_t status)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	virtio_status_lock(&virtio_backend->virtio);
	if (virtio_status_get_device_needs_reset(&status)) {
		ret = virtio_needs_reset(&virtio_backend->virtio);
	} else if (virtio_status_get_features_ok(&status)) {
		ret = virtio_ack_features_ok(&virtio_backend->virtio);
	} else {
		// Other bits can't be set by the backend and are ignored
		ret = OK;
	}
	virtio_status_unlock(&virtio_backend->virtio);

	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}
