// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <object.h>
#include <util.h>
#include <vic.h>
#include <virq.h>
#include <virtio.h>

#include <events/virtio_backend.h>

#include "event_handlers.h"

error_t
virtio_backend_handle_object_activate_virtio_backend(
	virtio_backend_t *virtio_backend)
{
	// Read back the stashed configuration.
	virtio_device_type_t device_type = virtio_backend->virtio.device_type;
	count_t		     vqs	 = virtio_backend->virtio.vqs;
	memextent_t	    *memextent = virtio_backend->virtio.config_cache_me;
	virtio_backend->virtio.config_cache_me = NULL;

	// XXX: this should support other transport types.
	error_t err = virtio_startup(
		&virtio_backend->virtio, VIRTIO_BACKEND_TYPE_HYPERCALL,
		VIRTIO_TRANSPORT_TYPE_MMIO, device_type, vqs,
		VIRTIO_MAX_DEVICE_CONFIG_BYTES, memextent,
		offsetof(virtio_mmio_regs_t, device_config));

	// If the above function succeeded, it will have taken a new reference
	// to the memextent. So, we can drop the original reference regardless
	// of the error result.
	if (memextent != NULL) {
		object_put_memextent(memextent);
	}

	return err;
}

void
virtio_backend_handle_object_deactivate_virtio_backend(
	virtio_backend_t *virtio_backend)
{
	assert(virtio_backend != NULL);

	vic_unbind(&virtio_backend->virq_source);

	(void)virtio_shutdown(&virtio_backend->virtio);
}

void
virtio_backend_handle_object_cleanup_virtio_backend(
	virtio_backend_t *virtio_backend)
{
	assert(virtio_backend != NULL);

	(void)trigger_virtio_backend_device_config_cleanup_event(
		virtio_backend->virtio.device_type, virtio_backend);

	(void)virtio_cleanup(&virtio_backend->virtio);

	if (virtio_backend->virtio.config_cache_me != NULL) {
		object_put_memextent(virtio_backend->virtio.config_cache_me);
		virtio_backend->virtio.config_cache_me = NULL;
	}
}

void
virtio_backend_unwind_object_activate_virtio_backend(
	virtio_backend_t *virtio_backend)
{
	virtio_backend_handle_object_deactivate_virtio_backend(virtio_backend);
	virtio_backend_handle_object_cleanup_virtio_backend(virtio_backend);
}

static void
virtio_backend_notify(virtio_backend_t		    *virtio_backend,
		      virtio_backend_notify_reason_t reason)
{
	virtio_backend_notify_reason_t old_reason =
		virtio_backend_notify_reason_atomic_union(
			&virtio_backend->reason, reason, memory_order_release);

	if (!virtio_backend_notify_reason_is_empty(
		    virtio_backend_notify_reason_difference(reason,
							    old_reason))) {
		(void)virq_assert(&virtio_backend->virq_source, false);
	}
}

void
virtio_backend_handle_virtio_queue_notify(virtio_t *virtio, index_t vq)
{
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virtio(virtio);

	if (vq < VIRTIO_MAX_VQS) {
		register_t old_vqs_bitmap = atomic_fetch_or_explicit(
			&virtio_backend->vqs_bitmap, util_bit(vq),
			memory_order_relaxed);

		if ((old_vqs_bitmap & util_bit(vq)) == 0U) {
			virtio_backend_notify_reason_t reason =
				virtio_backend_notify_reason_default();
			virtio_backend_notify_reason_set_new_buffer(&reason,
								    true);

			virtio_backend_notify(virtio_backend, reason);
		}
	}
}

void
virtio_backend_handle_virtio_reset_requested(virtio_t *virtio)
{
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virtio(virtio);

	virtio_backend_notify_reason_t reason =
		virtio_backend_notify_reason_default();
	virtio_backend_notify_reason_set_reset_request(&reason, true);

	virtio_backend_notify(virtio_backend, reason);

	if (!virtio_backend_option_flags_get_sync_reset(
		    &virtio_backend->flags)) {
		// Asynchronous reset; complete immediately
		error_t err = virtio_reset_complete(virtio);
		assert(err == OK);
	}
}

error_t
virtio_backend_handle_virtio_features_ok_check(virtio_t *virtio)
{
	(void)virtio;
	return ERROR_UNIMPLEMENTED;
}

error_t
virtio_backend_handle_virtio_driver_ok(virtio_t *virtio)
{
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virtio(virtio);

	virtio_backend_notify_reason_t reason =
		virtio_backend_notify_reason_default();
	virtio_backend_notify_reason_set_driver_ok(&reason, true);

	virtio_backend_notify(virtio_backend, reason);

	return OK;
}

error_t
virtio_backend_handle_virtio_failed(virtio_t *virtio)
{
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virtio(virtio);

	virtio_backend_notify_reason_t reason =
		virtio_backend_notify_reason_default();
	virtio_backend_notify_reason_set_driver_ok(&reason, true);

	virtio_backend_notify(virtio_backend, reason);

	return OK;
}

error_t
virtio_backend_handle_virtio_device_config_write(virtio_t *virtio,
						 size_t	   offset,
						 size_t	   access_size,
						 uint32_t  value)
{
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virtio(virtio);

	return trigger_virtio_backend_device_config_write_event(
		virtio->device_type, virtio_backend, offset, (register_t)value,
		access_size);
}

bool
virtio_backend_handle_virq_check_pending(virq_source_t *source)
{
	assert(source != NULL);

	// Deassert backend's IRQ when get_notification has been called
	virtio_backend_t *virtio_backend =
		virtio_backend_container_of_virq_source(source);

	virtio_backend_notify_reason_t reason =
		atomic_load_relaxed(&virtio_backend->reason);
	return !virtio_backend_notify_reason_is_empty(reason);
}
