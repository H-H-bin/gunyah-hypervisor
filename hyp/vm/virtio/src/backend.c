// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <atomic.h>
#include <spinlock.h>
#include <util.h>
#include <virtio.h>

#include <events/virtio.h>

#include <asm/nospec_checks.h>

// Set one of the device feature words.
error_t
virtio_set_dev_features(virtio_t *virtio, index_t feature_sel,
			uint32_t features)
{
	error_t ret;

	index_result_t feature_sel_nospec =
		nospec_range_check(feature_sel, VIRTIO_FEAT_WORDS);
	if (feature_sel_nospec.e != OK) {
		ret = feature_sel_nospec.e;
		goto out;
	}

	// Check features enforced by the hypervisor
	if (feature_sel_nospec.r == 1U) {
		uint32_t allow =
			(uint32_t)util_bit((VIRTIO_F_VERSION_1 - 32U)) |
			(uint32_t)util_bit((VIRTIO_F_ACCESS_PLATFORM - 32U)) |
			features;
		uint32_t forbid = ~(uint32_t)util_bit(
					  (VIRTIO_F_NOTIFICATION_DATA - 32U)) &
				  features;

		if ((allow != features) || (forbid != features)) {
			ret = ERROR_DENIED;
			goto out;
		}
	}

	if (!virtio_status_get_device_needs_reset(&virtio->status)) {
		ret = ERROR_BUSY;
		goto out;
	}

	virtio->dev_feat[feature_sel_nospec.r] = features;
	ret				       = OK;

out:
	return ret;
}

// Set the maximum size for one of the virtqueues.
error_t
virtio_set_queue_size_max(virtio_t *virtio, index_t queue_sel,
			  uint32_t queue_size_max)
{
	error_t ret;

	index_result_t queue_sel_nospec =
		nospec_range_check(queue_sel, virtio->vqs);
	if (queue_sel_nospec.e != OK) {
		ret = queue_sel_nospec.e;
		goto out;
	}

	if (!virtio_status_get_device_needs_reset(&virtio->status)) {
		ret = ERROR_BUSY;
		goto out;
	}

	virtio->queue_size_max[queue_sel_nospec.r] = queue_size_max;
	ret					   = OK;

out:
	return ret;
}

// Fetch the driver's requested features.
uint32_result_t
virtio_get_drv_features(virtio_t *virtio, index_t feature_sel)
{
	uint32_result_t ret;

	index_result_t feature_sel_nospec =
		nospec_range_check(feature_sel, VIRTIO_FEAT_WORDS);
	if (feature_sel_nospec.e != OK) {
		ret = uint32_result_error(feature_sel_nospec.e);
		goto out;
	}

	if (!virtio->features_ok_set) {
		// Driver has not tried to set features_ok yet
		ret = uint32_result_error(ERROR_IDLE);
		goto out;
	}

	ret = uint32_result_ok(virtio->drv_feat[feature_sel_nospec.r]);

out:
	return ret;
}

// Clear feature bits to eliminate combinations that can't be supported by the
// backend. This is only permitted after the virtio_check_features event
// is triggered, and before virtio_ack_features_ok is called. Note that
// this event is only triggered if the device was configured with the feature
// sync flag set.
error_t
virtio_clear_drv_features(virtio_t *virtio, index_t feature_sel,
			  uint32_t clear_features)
{
	error_t ret;

	index_result_t feature_sel_nospec =
		nospec_range_check(feature_sel, VIRTIO_FEAT_WORDS);
	if (feature_sel_nospec.e != OK) {
		ret = feature_sel_nospec.e;
		goto out;
	}

	if (!virtio->features_ok_set) {
		// Driver has not tried to set features_ok yet
		ret = ERROR_IDLE;
		goto out;
	}

	if (virtio_status_get_features_ok(&virtio->status)) {
		// Backend has already acknowledged features
		ret = ERROR_BUSY;
		goto out;
	}

	virtio->dev_feat[feature_sel_nospec.r] &= ~clear_features;
	ret = OK;

out:
	return ret;
}

// Acknowledge the features requested by the driver.
error_t
virtio_ack_features_ok(virtio_t *virtio)
{
	error_t ret;

	if (!virtio->features_ok_set) {
		// Driver has not tried to set features_ok yet
		ret = ERROR_IDLE;
		goto out_locked;
	}

	if (virtio_status_get_features_ok(&virtio->status)) {
		// Backend has already acknowledged features
		ret = ERROR_BUSY;
		goto out_locked;
	}

	ret = trigger_virtio_ack_features_ok_event(virtio->transport_type,
						   virtio);
	if (ret == OK) {
		virtio_status_set_features_ok(&virtio->status, true);
	}

out_locked:
	return ret;
}

// Fetch virtqueue information set by the driver.
virtio_queue_info_result_t
virtio_get_queue_info(virtio_t *virtio, index_t queue_sel, bool finalised)
{
	virtio_queue_info_result_t ret;

	index_result_t queue_sel_nospec =
		nospec_range_check(queue_sel, virtio->vqs);
	if (queue_sel_nospec.e != OK) {
		ret = virtio_queue_info_result_error(queue_sel_nospec.e);
		goto out;
	}

	if (finalised && !virtio->features_ok_set) {
		// Driver has not tried to set features_ok yet
		ret = virtio_queue_info_result_error(ERROR_IDLE);
		goto out;
	}

	ret = virtio_queue_info_result_ok(
		virtio->queue_info[queue_sel_nospec.r]);

out:
	return ret;
}

// Begin a non-atomic config space update. This does not need to be called for
// atomic updates.
error_t
virtio_config_update_begin(virtio_t *virtio)
{
	// Set the update flag so generation reads will always increment. This
	// must be sequentially consistent to prevent reordering vs generation
	// count reads.
	atomic_store(&virtio->config_update, true);

	// Tell the frontend, in case it has its own cache of the generation
	trigger_virtio_config_update_begin_event(virtio->transport_type,
						 virtio);

	return OK;
}

// End a config space update (atomic or otherwise) and notify the frontend.
error_t
virtio_config_update_end(virtio_t *virtio)
{
	// Increment the generation count so any concurrent non-atomic readers
	// will know about the update
	(void)atomic_fetch_add_explicit(&virtio->config_gen, 1U,
					memory_order_relaxed);

	// Clear the update flag so generation reads don't have to increment
	atomic_store(&virtio->config_update, false);

	// Tell the frontend, so it can trigger a config update IRQ and possibly
	// update its cache of the generation
	trigger_virtio_config_update_end_event(virtio->transport_type, virtio);

	return OK;
}

// Notify the frontend that a queue is ready.
error_t
virtio_queue_ready(virtio_t *virtio, index_t vq)
{
	return trigger_virtio_queue_ready_event(virtio->transport_type, virtio,
						vq);
}

// Notify the frontend that the backend needs to be reset.
error_t
virtio_needs_reset(virtio_t *virtio)
{
	(void)virtio_status_set_device_needs_reset(&virtio->status, true);
	bool driver_ok = virtio_status_get_driver_ok(&virtio->status);

	if (driver_ok) {
		// Spec requires a config update notification in this case
		trigger_virtio_config_update_end_event(virtio->transport_type,
						       virtio);
	}

	return OK;
}

// Notify the frontend that a reset it requested is complete.
error_t
virtio_reset_complete(virtio_t *virtio)
{
	error_t err;

	if (virtio->reset_request) {
		virtio->status		= virtio_status_cast(0U);
		virtio->features_ok_set = false;
		virtio->reset_request	= false;
		trigger_virtio_reset_complete_event(virtio->transport_type,
						    virtio);
		err = OK;
	} else {
		err = ERROR_BUSY;
	}

	return err;
}
