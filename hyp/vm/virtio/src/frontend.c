// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <atomic.h>
#include <compiler.h>
#include <hyp_aspace.h>
#include <memextent.h>
#include <object.h>
#include <panic.h>
#include <rcu.h>
#include <spinlock.h>
#include <thread.h>
#include <trace.h>
#include <util.h>
#include <virtio.h>

#include <events/virtio.h>

#include <asm/cache.h>
#include <asm/cpu.h>
#include <asm/nospec_checks.h>

error_t
virtio_configure(virtio_t *virtio, partition_t *partition,
		 virtio_backend_type_t	 backend_type,
		 virtio_transport_type_t transport_type,
		 virtio_device_type_t device_type, count_t vqs,
		 size_t config_size, memextent_t *config_cache_me,
		 size_t config_offset, bool per_queue_notify)
{
	assert(virtio != NULL);

	error_t err;

	// If the virtio structure already has a backend type set, it may have
	// been initialised already.
	if (virtio->backend_type != VIRTIO_BACKEND_TYPE_INVALID) {
		err = ERROR_BUSY;
		goto out;
	}

	// Basic sanity checks for arguments
	if (vqs > VIRTIO_MAX_VQS) {
		err = ERROR_ARGUMENT_SIZE;
		TRACE(ERROR, INFO, "virtio_startup: vqs {:d} > max {:d}", vqs,
		      VIRTIO_MAX_VQS);
		goto out;
	}

	if (util_add_overflows(config_offset, config_size - 1U) ||
	    ((config_offset + config_size - 1U) >= PGTABLE_HYP_PAGE_SIZE)) {
		err = ERROR_ARGUMENT_SIZE;
		TRACE(ERROR, INFO,
		      "virtio_startup: bad offset {:#x} / size {:#x}",
		      config_offset, config_size);
		goto out;
	}

	static_assert(
		alignof(virtio_config_space_t) <= alignof(uint32_t),
		"Configuration space must have uint32 or smaller alignment");
	if (!util_is_baligned(config_offset, alignof(uint32_t)) ||
	    !util_is_baligned(config_size, alignof(uint32_t))) {
		err = ERROR_ARGUMENT_ALIGNMENT;
		TRACE(ERROR, INFO,
		      "virtio_startup: bad offset {:#x} / size {:#x}",
		      config_offset, config_size);
		goto out;
	}

	if (partition == NULL) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	err = OK;

	// Initialise the struct with the above arguments, and zeros otherwise.
	*virtio = (virtio_t){
		.partition	= object_get_partition_additional(partition),
		.transport_type = transport_type,
		.backend_type	= backend_type,

		.device_type = device_type,

		.config_offset = config_offset,
		.config_size   = config_size,

		.status = virtio_status_default(),

		.vqs = vqs,

		// If there is only one VQ, then notifications are trivially
		// per-VQ, so we can expose that feature to the frontend even if
		// the backend didn't enable it.
		.per_queue_notify = per_queue_notify || (vqs == 1U),
	};

	if (config_cache_me != NULL) {
		virtio->config_cache_me =
			object_get_memextent_additional(config_cache_me);
	}

	spinlock_init(&virtio->status_lock);
out:
	return err;
}

error_t
virtio_activate(virtio_t *virtio)
{
	assert(virtio != NULL);

	error_t err;

	// Map the config cache if it is present
	memextent_t *config_cache_me = virtio->config_cache_me;
	if (config_cache_me != NULL) {
		if ((config_cache_me->type != MEMEXTENT_TYPE_BASIC) ||
		    (config_cache_me->size != PGTABLE_HYP_PAGE_SIZE)) {
			err = ERROR_ARGUMENT_INVALID;
			TRACE(ERROR, INFO,
			      "virtio_startup: bad cache me type {:d} / size {:#x}",
			      config_cache_me->type, config_cache_me->size);
			goto out;
		}

		if (config_cache_me->size <=
		    (virtio->config_offset + virtio->config_size - 1U)) {
			err = ERROR_ARGUMENT_SIZE;
			TRACE(ERROR, INFO,
			      "virtio_startup: bad cache me size {:#x} > {:#x}",
			      config_cache_me->size,
			      virtio->config_offset + virtio->config_size - 1U);
			goto out;
		}

		virt_range_result_t range =
			hyp_aspace_allocate(config_cache_me->size);
		if (range.e != OK) {
			err = range.e;
			TRACE(ERROR, INFO,
			      "virtio_startup: no hyp_aspace: {:d}",
			      (register_t)err);
			goto out_me_ref;
		}
		virtio->config_range = range.r;

		err = memextent_attach(virtio->partition, config_cache_me,
				       range.r.base, config_cache_me->size);
		if (err != OK) {
			TRACE(ERROR, INFO,
			      "virtio_startup: no memextent_attach: {:d}",
			      (register_t)err);
			goto out_range;
		}

		// Flush cache before using the uncached mapping
		cache_clean_range((const void *)virtio->config_range.base,
				  config_cache_me->size);

		virtio->config_cache =
			(virtio_config_space_t *)(virtio->config_range.base +
						  virtio->config_offset);
	}

	// Run transport-specific frontend configuration
	err = trigger_virtio_startup_event(virtio->transport_type, virtio);
	if (err != OK) {
		TRACE(ERROR, INFO, "virtio_startup: event failed: {:d}",
		      (register_t)err);
		goto out_attached;
	}

out_attached:
	if ((err != OK) && (config_cache_me != NULL)) {
		virtio->config_cache = NULL;
		memextent_detach(virtio->partition, config_cache_me);
	}
out_range:
	if ((err != OK) && (config_cache_me != NULL)) {
		hyp_aspace_unmap_and_deallocate(virtio->partition,
						virtio->config_range);
	}
out_me_ref:
	if ((err != OK) && (config_cache_me != NULL)) {
		object_put_memextent(config_cache_me);
		virtio->config_cache_me = NULL;
	}
out:
	return err;
}

void
virtio_shutdown(virtio_t *virtio)
{
	virtio->shutdown = true;

	// TODO: halt vqs, set device_needs_reset, etc
	if (virtio->backend_type != VIRTIO_BACKEND_TYPE_INVALID) {
		trigger_virtio_shutdown_event(virtio->transport_type, virtio);
	}
	virtio->config_cache = NULL;
}

void
virtio_cleanup(virtio_t *virtio)
{
	if (virtio->backend_type == VIRTIO_BACKEND_TYPE_INVALID) {
		goto out;
	}

	assert(virtio->shutdown);

	if (virtio->config_range.size != 0U) {
		assert(virtio->config_cache_me != NULL);
		assert(virtio->partition != NULL);

		memextent_detach(virtio->partition, virtio->config_cache_me);
		hyp_aspace_unmap_and_deallocate(virtio->partition,
						virtio->config_range);
	}

	if (virtio->config_cache_me != NULL) {
		object_put_memextent(virtio->config_cache_me);
		virtio->config_cache_me = NULL;
	}

	if (virtio->partition != NULL) {
		object_put_partition(virtio->partition);
		virtio->partition = NULL;
	}

out:
	return;
}

// Start an access or update that must be synchronised with status changes.
void
virtio_status_lock(virtio_t *virtio)
{
	spinlock_acquire(&virtio->status_lock);
}

void
virtio_status_lock_nopreempt(virtio_t *virtio)
{
	spinlock_acquire_nopreempt(&virtio->status_lock);
}

// Finish an access or update that must be synchronised with status changes.
void
virtio_status_unlock(virtio_t *virtio)
{
	spinlock_release(&virtio->status_lock);
}

void
virtio_status_unlock_nopreempt(virtio_t *virtio)
{
	spinlock_release_nopreempt(&virtio->status_lock);
}

// Handle a frontend write to the device status register.
error_t
virtio_write_status(virtio_t *virtio, virtio_status_t status)
{
	error_t err;

	spinlock_acquire(&virtio->status_lock);

	// Reject any status value with unknown bits set.
	if (!virtio_status_is_clean(status)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	// Is this a reset request?
	if (virtio_status_is_empty(status)) {
		if (virtio_status_is_empty(virtio->status)) {
			// Device is already reset, nothing to do
		} else if (!virtio->reset_request) {
			// Mark reset as requested
			virtio->reset_request = true;

			virtio_status_set_device_needs_reset(&virtio->status,
							     true);
			trigger_virtio_status_updated_event(
				virtio->transport_type, virtio, virtio->status);

			for (index_t i = 0; i < virtio->vqs; i++) {
				virtio->queue_info[i].ready = 0U;
			}

			trigger_virtio_reset_requested_event(
				virtio->backend_type, virtio);
		} else {
			// Reset already requested, nothing to do
		}

		err = OK;
		goto out;
	}

	// Has the device already signalled that it needs a reset?
	if (virtio_status_get_device_needs_reset(&virtio->status)) {
		// Anything other than a reset is ignored.
		err = ERROR_DENIED;
		goto out;
	}

	// Determine the new status after bits are set. Ignore any attempt to
	// clear a bit that is already set.
	virtio_status_t new_status =
		virtio_status_union(virtio->status, status);
	virtio_status_t set_bits =
		virtio_status_difference(new_status, virtio->status);

	if (virtio_status_get_device_needs_reset(&set_bits)) {
		// frontend is not allowed to set this
		err = ERROR_DENIED;
		goto out;
	} else if (virtio_status_get_failed(&set_bits)) {
		err = trigger_virtio_failed_event(virtio->backend_type, virtio);
	} else if (virtio_status_get_driver_ok(&set_bits)) {
		if (!virtio_status_get_features_ok(&virtio->status)) {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		err = trigger_virtio_driver_ok_event(virtio->backend_type,
						     virtio);
	} else if (virtio_status_get_features_ok(&set_bits)) {
		// FEATURES_OK must not be set before ACKNOWLEDGE and DRIVER
		if (!virtio_status_get_driver(&new_status) ||
		    !virtio_status_get_acknowledge(&new_status)) {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		// TODO: synchronous features_ok
		virtio->features_ok_set = true;
		err			= OK;
	} else if (virtio_status_get_driver(&set_bits)) {
		// DRIVER must not be set before ACKNOWLEDGE
		if (!virtio_status_get_acknowledge(&new_status)) {
			err = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		err = OK;
	} else {
		// ACKNOWLEDGE set, or no changes
		err = OK;
	}

	if (err == OK) {
		virtio->status = new_status;
		trigger_virtio_status_updated_event(virtio->transport_type,
						    virtio, new_status);
	}

out:
	spinlock_release(&virtio->status_lock);
	return err;
}

// Read the status register.
virtio_status_t
virtio_read_status(virtio_t *virtio)
{
	virtio_status_t status;

	spinlock_acquire(&virtio->status_lock);
	status = virtio->status;
	spinlock_release(&virtio->status_lock);

	rcu_read_finish();

	return status;
}

// Read the config generation register.
uint8_result_t
virtio_get_generation(virtio_t *virtio)
{
	uint8_t gen;

	if (atomic_load(&virtio->config_update)) {
		uint8_t old_gen = atomic_fetch_add_explicit(
			&virtio->config_gen, 1U, memory_order_relaxed);
		gen = old_gen + 1U;
	} else {
		gen = atomic_load_relaxed(&virtio->config_gen);
	}

	return uint8_result_ok(gen);
}

// Read a queue's maximum size.
count_result_t
virtio_get_queue_size_max(virtio_t *virtio, index_t queue_sel)
{
	count_result_t ret;

	index_result_t queue_sel_nospec_r =
		nospec_range_check(queue_sel, virtio->vqs);
	if (queue_sel_nospec_r.e == OK) {
		ret = count_result_ok(
			virtio->queue_size_max[queue_sel_nospec_r.r]);
	} else {
		ret = count_result_error(queue_sel_nospec_r.e);
	}

	return ret;
}

// Read a queue's ready flag.
virtio_queue_info_ptr_result_t
virtio_get_queue_info_ptr(virtio_t *virtio, index_t queue_sel)
{
	virtio_queue_info_ptr_result_t ret;

	index_result_t queue_sel_nospec_r =
		nospec_range_check(queue_sel, virtio->vqs);
	if (queue_sel_nospec_r.e == OK) {
		ret = virtio_queue_info_ptr_result_ok(
			&virtio->queue_info[queue_sel_nospec_r.r]);
	} else {
		ret = virtio_queue_info_ptr_result_error(queue_sel_nospec_r.e);
	}

	return ret;
}

// Notify the backend that a queue is ready.
error_t
virtio_queue_notify(virtio_t *virtio, index_t queue_sel)
{
	error_t ret;

	index_result_t queue_sel_nospec_r =
		nospec_range_check(queue_sel, virtio->vqs);
	if (queue_sel_nospec_r.e == OK) {
		trigger_virtio_queue_notify_event(virtio->backend_type, virtio,
						  queue_sel_nospec_r.r);
		ret = OK;
	} else {
		ret = queue_sel_nospec_r.e;
	}

	return ret;
}

// Handle a frontend read of the device features.
uint32_result_t
virtio_get_dev_features(virtio_t *virtio, index_t feature_sel)
{
	uint32_result_t ret;

	spinlock_acquire(&virtio->status_lock);

	index_result_t feature_sel_nospec_r =
		nospec_range_check(feature_sel, VIRTIO_FEAT_WORDS);
	if (feature_sel_nospec_r.e == OK) {
		// Update corresponding banked register
		ret = uint32_result_ok(
			virtio->dev_feat[feature_sel_nospec_r.r]);
	} else {
		ret = uint32_result_error(feature_sel_nospec_r.e);
	}

	spinlock_release(&virtio->status_lock);

	return ret;
}

// Handle a frontend write of the driver features.
error_t
virtio_set_drv_features(virtio_t *virtio, index_t feature_sel,
			uint32_t features)
{
	error_t ret;

	spinlock_acquire(&virtio->status_lock);

	if (virtio_status_get_features_ok(&virtio->status)) {
		ret = ERROR_BUSY;
		goto out_unlock;
	}

	index_result_t feature_sel_nospec_r =
		nospec_range_check(feature_sel, VIRTIO_FEAT_WORDS);
	if (feature_sel_nospec_r.e == OK) {
		// Update corresponding banked register
		ret					 = OK;
		virtio->drv_feat[feature_sel_nospec_r.r] = features;
	} else {
		ret = feature_sel_nospec_r.e;
	}

out_unlock:
	spinlock_release(&virtio->status_lock);

	return ret;
}

// Handle a device configuration space write by the frontend. Note that the
// maximum atomic access size is 32 bits, as per the spec. Also note that this
// might block the caller if synchronous config writes are enabled in the
// backend.
error_t
virtio_device_config_write(virtio_t *virtio, size_t offset, size_t access_size,
			   uint32_t value)
{
	error_t ret = trigger_virtio_device_config_write_event(
		virtio->backend_type, virtio, offset, access_size, value);

	// FIXME: QC Gunyah issue #252
	rcu_read_finish();

	return ret;
}

// Handle a device configuration space read by the frontend. Note that the
// maximum atomic access size is 32 bits, as per the spec.
//
// This might block the caller if posted config writes are enabled or if the
// backend is a proxy and does not cache the configuration space. Therefore, it
// drops the RCU critical section.
uint32_result_t
virtio_device_config_read(virtio_t *virtio, size_t offset, size_t access_size)
{
	uint32_result_t ret = trigger_virtio_device_config_read_event(
		virtio->backend_type, virtio, offset, access_size);

	// FIXME: QC Gunyah issue #252
	rcu_read_finish();

	return ret;
}

uint32_result_t
virtio_device_config_read_cached(virtio_t *virtio, size_t offset,
				 size_t access_size)
{
	uint32_result_t ret;

	if (virtio->config_range.size == 0U) {
		ret = uint32_result_error(ERROR_UNIMPLEMENTED);
		goto out;
	}

	uintptr_t config_base =
		virtio->config_range.base + virtio->config_offset;
	uintptr_t config_size =
		virtio->config_range.base + virtio->config_offset;

	if ((access_size == 0U) || (access_size > sizeof(uint32_t)) ||
	    util_add_overflows(offset, access_size - 1U) ||
	    ((offset + access_size - 1U) >= config_size)) {
		ret = uint32_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}
	assert(util_is_baligned(config_base, sizeof(uint32_t)));

	if (!util_is_baligned(offset, access_size)) {
		ret = uint32_result_error(ERROR_ARGUMENT_ALIGNMENT);
		goto out;
	}
	uintptr_t config_addr	   = config_base + offset;
	uintptr_t config_addr_size = config_size - offset;

	if (compiler_unexpected(config_addr_size < access_size)) {
		ret = uint32_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}

	switch (access_size) {
	case sizeof(uint8_t): {
		uint8_t	 val;
		uint8_t *buf = (uint8_t *)config_addr;
		(void)memscpy(&val, sizeof(val), buf, config_addr_size);
		ret = uint32_result_ok(val);
		break;
	}
	case sizeof(uint16_t): {
		uint16_t  val;
		uint16_t *buf = (uint16_t *)config_addr;
		(void)memscpy(&val, sizeof(val), buf, config_addr_size);
		ret = uint32_result_ok(val);
		break;
	}
	case sizeof(uint32_t): {
		uint32_t  val;
		uint32_t *buf = (uint32_t *)config_addr;
		(void)memscpy(&val, sizeof(val), buf, config_addr_size);
		ret = uint32_result_ok(val);
		break;
	}
	default:
		ret = uint32_result_error(ERROR_UNIMPLEMENTED);
		break;
	}

out:
	return ret;
}

// Determine whether the backend generates per-queue ready signals.
bool
virtio_supports_per_queue_notify(const virtio_t *virtio)
{
	return virtio->per_queue_notify;
}
