// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>
#include <hypconstants.h>
#include <hyprights.h>

#include <atomic.h>
#include <bitmap.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <memextent.h>
#include <object.h>
#include <spinlock.h>
#include <util.h>
#include <vic.h>
#include <virq.h>
#include <virtio.h>

#include <events/virtio_backend.h>

error_t
hypercall_virtio_backend_configure(cap_id_t virtio_backend_cap,
				   cap_id_t memextent_cap, count_t vqs_num,
				   virtio_backend_option_flags_t     flags,
				   virtio_backend_interface_type_t   type,
				   virtio_backend_memextent_layout_t me_layout)
{
	error_t	      err;
	cspace_t     *cspace = cspace_get_self();
	object_type_t obj_type;

	if (!virtio_backend_option_flags_is_clean(flags)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	virtio_device_type_t	checked_device_type;
	virtio_transport_type_t transport_type;
	if (virtio_backend_option_flags_get_valid_type(&flags)) {
		if (!virtio_backend_interface_type_is_clean(type)) {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		virtio_device_type_t device_type =
			virtio_backend_interface_type_get_device(&type);
		if (trigger_virtio_backend_valid_device_type_event(
			    device_type)) {
			checked_device_type = device_type;
		} else {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		transport_type =
			virtio_backend_interface_type_get_transport(&type);
	} else {
#if defined(INTERFACE_VIRTIO_MMIO)
		checked_device_type = VIRTIO_DEVICE_TYPE_INVALID;
		transport_type	    = VIRTIO_TRANSPORT_TYPE_MMIO;
#else
		err = ERROR_ARGUMENT_INVALID;
		goto out;
#endif
	}

	size_t checked_config_size, checked_config_offset;
	if (virtio_backend_option_flags_get_valid_me_layout(&flags)) {
		if (!virtio_backend_memextent_layout_is_clean(me_layout)) {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}

		size_t config_size =
			virtio_backend_memextent_layout_get_devcfg_size(
				&me_layout);
		size_t config_offset =
			virtio_backend_memextent_layout_get_devcfg_offset(
				&me_layout);

		if (config_size > VIRTIO_MAX_DEVICE_CONFIG_BYTES) {
			err = ERROR_ARGUMENT_SIZE;
			goto out;
		}
		if (util_add_overflows(config_size, config_offset)) {
			err = ERROR_ARGUMENT_SIZE;
			goto out;
		}

		checked_config_size   = config_size;
		checked_config_offset = config_offset;
	}
#if defined(INTERFACE_VIRTIO_MMIO)
	else if (transport_type == VIRTIO_TRANSPORT_TYPE_MMIO) {
		checked_config_size =
			util_sizeof_member(virtio_mmio_regs_t, device_config);
		checked_config_offset =
			offsetof(virtio_mmio_regs_t, device_config);
	}
#endif
	else {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	memextent_t *memextent;
	if (memextent_cap == CSPACE_CAP_INVALID) {
		memextent = NULL;
	} else {
		memextent_ptr_result_t m = cspace_lookup_memextent(
			cspace, memextent_cap, CAP_RIGHTS_MEMEXTENT_ATTACH);
		if (compiler_unexpected(m.e != OK)) {
			err = m.e;
			goto out;
		}
		memextent = m.r;

		size_result_t me_size_r = memextent_get_mapped_size(memextent);
		if (me_size_r.e != OK) {
			err = me_size_r.e;
			goto out_memextent_release;
		}
		if ((checked_config_size + checked_config_offset) >
		    me_size_r.r) {
			err = ERROR_ARGUMENT_SIZE;
			goto out_memextent_release;
		}
	}

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, virtio_backend_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE,
		&obj_type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out_memextent_release;
	}
	if (obj_type != OBJECT_TYPE_VIRTIO_BACKEND) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto out_virtio_backend_release;
	}

	virtio_backend_t *virtio_backend = o.r.virtio_backend;

	spinlock_acquire(&virtio_backend->header.lock);

	if (atomic_load_relaxed(&virtio_backend->header.state) !=
	    OBJECT_STATE_INIT) {
		err = ERROR_OBJECT_STATE;
		goto out_locked;
	}

	virtio_backend->flags = flags;

	err = virtio_configure(
		&virtio_backend->virtio, virtio_backend->header.partition,
		VIRTIO_BACKEND_TYPE_HYPERCALL, transport_type,
		checked_device_type, vqs_num, checked_config_size, memextent,
		offsetof(virtio_mmio_regs_t, device_config),
		virtio_backend_option_flags_get_per_queue_irqs(&flags));

out_locked:
	spinlock_release(&virtio_backend->header.lock);
out_virtio_backend_release:
	object_put(obj_type, o.r);
out_memextent_release:
	if (memextent != NULL) {
		object_put_memextent(memextent);
	}
out:
	return err;
}

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

static error_t
hypercall_virtio_backend_notify_unified(
	virtio_backend_t	      *virtio_backend,
	virtio_backend_notify_status_t interrupt_status)
	REQUIRE_SPINLOCK(virtio_backend -> virtio.status_lock)
{
	error_t ret;

	if (virtio_backend_option_flags_get_per_queue_irqs(
		    &virtio_backend->flags) &&
	    (virtio_backend->virtio.vqs > 1U)) {
		ret = ERROR_DENIED;
		goto out;
	}
	if (!virtio_interrupt_is_clean(interrupt_status.unified)) {
		ret = ERROR_UNIMPLEMENTED;
		goto out;
	}

	if (virtio_interrupt_get_config_update(&interrupt_status.unified)) {
		error_t err = virtio_config_update_end(&virtio_backend->virtio);
		if (err != OK) {
			ret = err;
			goto out;
		}
	}

	if (virtio_interrupt_get_queue_ready(&interrupt_status.unified)) {
		error_t err = virtio_queue_ready(&virtio_backend->virtio,
						 VIRTIO_QUEUE_UNSPECIFIED);
		if (err != OK) {
			ret = err;
			goto out;
		}
	}

	ret = OK;

out:
	return ret;
}

static error_t
hypercall_virtio_backend_notify_per_queue(
	virtio_backend_t	      *virtio_backend,
	virtio_backend_notify_status_t interrupt_status)
	REQUIRE_SPINLOCK(virtio_backend -> virtio.status_lock)
{
	error_t ret;
	BITMAP_DECLARE(VIRTIO_MAX_VQS, vqs_ready) = { 0U };
	virtio_backend_interrupt_perqueue_t new_interrupts =
		interrupt_status.per_queue;

	if (virtio_backend_interrupt_perqueue_get_config_update(
		    &interrupt_status.per_queue)) {
		error_t err = virtio_config_update_end(&virtio_backend->virtio);
		if (err != OK) {
			ret = err;
			goto out;
		}
	}

	static_assert(
		VIRTIO_MAX_VQS ==
			VIRTIO_BACKEND_INTERRUPT_PERQUEUE_QUEUES_READY_BITS,
		"Queue bits don't fit in register argument");
	vqs_ready[0] = virtio_backend_interrupt_perqueue_get_queues_ready(
		&new_interrupts);

	BITMAP_FOREACH_SET_BEGIN(i, vqs_ready, VIRTIO_MAX_VQS)
		error_t err = virtio_queue_ready(&virtio_backend->virtio, i);
		if (err != OK) {
			ret = err;
			goto out;
		}
	BITMAP_FOREACH_SET_END

	ret = OK;

out:
	return ret;
}

error_t
hypercall_virtio_backend_notify(cap_id_t virtio_backend_cap,
				virtio_backend_notify_status_t interrupt_status,
				virtio_backend_notify_flags_t  flags)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	if (!virtio_backend_notify_flags_is_clean(flags)) {
		ret = ERROR_UNIMPLEMENTED;
		goto out;
	}

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap,
		CAP_RIGHTS_VIRTIO_BACKEND_ASSERT_VIRQ);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;
	virtio_status_lock(&virtio_backend->virtio);

	if (virtio_backend->virtio.reset_request) {
		ret = ERROR_BUSY;
		goto out_virtio_backend_release;
	}

	if (virtio_backend_notify_flags_get_config_update(&flags)) {
		error_t err =
			virtio_config_update_begin(&virtio_backend->virtio);
		if (err != OK) {
			ret = err;
			goto out_virtio_backend_release;
		}
	}

	if (!virtio_backend_notify_flags_get_per_queue(&flags)) {
		ret = hypercall_virtio_backend_notify_unified(virtio_backend,
							      interrupt_status);
	} else {
		ret = hypercall_virtio_backend_notify_per_queue(
			virtio_backend, interrupt_status);
	}

out_virtio_backend_release:
	virtio_status_unlock(&virtio_backend->virtio);
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

	// Check features enforced by the backend API
	if (feature_sel == 1U) {
		uint32_t allow =
			(uint32_t)util_bit(
				((index_t)VIRTIO_F_VERSION_1 - 32U)) |
			(uint32_t)util_bit(
				((index_t)VIRTIO_F_ACCESS_PLATFORM - 32U)) |
			dev_feat;
		uint32_t forbid =
			~(uint32_t)util_bit(
				((index_t)VIRTIO_F_NOTIFICATION_DATA - 32U)) &
			dev_feat;

		if ((allow != dev_feat) || (forbid != dev_feat)) {
			ret = ERROR_DENIED;
			goto out_virtio_backend_release;
		}
	}

	// Check features enforced by the protocol
	uint32_result_t dev_feat_r =
		trigger_virtio_backend_device_set_features_event(
			virtio_backend->virtio.device_type, virtio_backend,
			feature_sel, dev_feat);
	if (dev_feat_r.e != OK) {
		ret = dev_feat_r.e;
		goto out_virtio_backend_release;
	}

	virtio_status_lock(&virtio_backend->virtio);
	ret = virtio_set_dev_features(&virtio_backend->virtio, feature_sel,
				      dev_feat_r.r);
	virtio_status_unlock(&virtio_backend->virtio);

out_virtio_backend_release:
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
	uint32_result_t res = virtio_get_drv_features(&virtio_backend->virtio,
						      feature_sel, true);
	ret.error	    = res.e;
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

	virtio_backend_notify_reason_t reason = atomic_exchange_explicit(
		&virtio_backend->reason, virtio_backend_notify_reason_default(),
		memory_order_acquire);
	uint64_t vqs_bitmap =
		virtio_backend_notify_reason_get_new_buffer(&reason)
			? atomic_exchange_explicit(&virtio_backend->vqs_bitmap,
						   0U, memory_order_relaxed)
			: 0U;

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
		virtio_needs_reset(&virtio_backend->virtio);
		ret = OK;
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
