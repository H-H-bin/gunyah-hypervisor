// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Configure a virtio frontend device.
//
// This function must be called by a backend implementation prior to activating
// the device. The backend must allocate memory for the virtio_t structure
// within an enclosing structure or object, and supply its own type which will
// be used to trigger backend events. The specified transport type must be one
// that has been registered by a frontend driver module; otherwise,
// ERROR_UNIMPLEMENTED will be returned.
//
// The specified memextent contains a cache of the device configuration space.
// This will be mapped in the hypervisor address space and used to implement
// configuration space reads. Depending on the frontend implementation and the
// option flags, providing it may be optional. If it is not NULL, this function
// will take a reference to it, which will be released by virtio_shutdown().
//
// If the frontend is a virtual MMIO device, the offset must be exactly 256, and
// the extent must be separately mapped read-only in the guest VM. The first 256
// bytes will be used by the frontend to provide the common configuration
// registers.
//
// If the frontend is a virtual PCIE device, the offset may be any multiple of 4
// that is less than the memextent size by at least the configuration area size.
// The entire memextent will be presented to the guest VM through a BAR, and the
// VIRTIO_PCI_CAP_DEVICE_CFG capability will declare the given offset and size.
//
// If this function returns OK, virtio_cleanup() must be called before the
// virtio structure is freed or reused.
error_t
virtio_configure(virtio_t *virtio, partition_t *partition,
		 virtio_backend_type_t	 backend_type,
		 virtio_transport_type_t transport_type,
		 virtio_device_type_t device_type, count_t vqs,
		 size_t config_size, memextent_t *config_cache_me,
		 size_t config_offset, bool per_queue_notify);

// Activate a frontend.
//
// This function must be called after virtio_configure() to activate the device.
// Normally this will happen in the enclosing object's activate handler.
//
// If this function returns OK, virtio_shutdown() must be called at least one
// RCU grace period prior to calling virtio_cleanup().
error_t
virtio_activate(virtio_t *virtio);

// Deactivate a frontend.
//
// The storage for the virtio structure must not be freed or reused until after
// a call to virtio_cleanup().
void
virtio_shutdown(virtio_t *virtio);

// Clean up a frontend.
//
// This must be called no less than one RCU grace period after
// virtio_shutdown(). The storage for the virtio structure may be freed or
// reused after this function returns.
void
virtio_cleanup(virtio_t *virtio);

// Start an access or update that must be synchronised with status changes.
void
virtio_status_lock(virtio_t *virtio) ACQUIRE_SPINLOCK(virtio -> status_lock);
void
virtio_status_lock_nopreempt(virtio_t *virtio)
	ACQUIRE_SPINLOCK_NP(virtio -> status_lock);

// Finish an access or update that must be synchronised with status changes.
void
virtio_status_unlock(virtio_t *virtio) RELEASE_SPINLOCK(virtio -> status_lock);
void
virtio_status_unlock_nopreempt(virtio_t *virtio)
	RELEASE_SPINLOCK_NP(virtio -> status_lock);

//
// Backend control plane API.
//
// This may be exposed to a host VM through hypercalls, or called internally by
// a backend implementation.
//
// All of these functions assume that the caller holds a reference to an object
// containing the virtio structure, so RCU protection is not required.
//

// Set one of the device feature words. Returns ERROR_BUSY if there is no
// pending device reset.
error_t
virtio_set_dev_features(virtio_t *virtio, index_t feature_sel,
			uint32_t features)
	REQUIRE_SPINLOCK(virtio -> status_lock);

// Set the maximum size for one of the virtqueues. Returns ERROR_BUSY if there
// is no pending device reset.
error_t
virtio_set_queue_size_max(virtio_t *virtio, index_t queue_sel,
			  uint32_t queue_size_max)
	REQUIRE_SPINLOCK(virtio -> status_lock);

// Fetch the driver's requested features. Returns ERROR_IDLE if the driver has
// not set the FEATURES_OK bit yet and the finalised flag is true. If the
// finalised flag is false, this may return bits that have not been accepted by
// the backend.
uint32_result_t
virtio_get_drv_features(const virtio_t *virtio, index_t feature_sel,
			bool finalised) REQUIRE_SPINLOCK(virtio -> status_lock);

// Clear feature bits to eliminate combinations that can't be supported by the
// backend. This is only permitted after the virtio_check_features event
// is triggered, and before virtio_ack_features_ok is called. Note that
// this event is only triggered if the device was configured with the feature
// sync flag set.
error_t
virtio_clear_drv_features(virtio_t *virtio, index_t feature_sel,
			  uint32_t clear_features)
	REQUIRE_SPINLOCK(virtio -> status_lock);

// Acknowledge the features requested by the driver.
error_t
virtio_ack_features_ok(virtio_t *virtio)
	REQUIRE_SPINLOCK(virtio -> status_lock);

// Fetch virtqueue information set by the driver. Returns ERROR_IDLE if
// the queue configuration is not active yet (i.e. DRIVER_OK is not set) and
// the finalised flag is true.
virtio_queue_info_result_t
virtio_get_queue_info(virtio_t *virtio, index_t queue_sel, bool finalised)
	REQUIRE_SPINLOCK(virtio -> status_lock);

// Begin a non-atomic config space update. This does not need to be called for
// atomic updates.
error_t
virtio_config_update_begin(virtio_t *virtio);

// End a config space update (atomic or otherwise) and notify the frontend.
error_t
virtio_config_update_end(virtio_t *virtio);

// Notify the frontend that a queue is ready.
error_t
virtio_queue_ready(virtio_t *virtio, index_t vq);

// Notify the frontend that a reset is required.
void
virtio_needs_reset(virtio_t *virtio) REQUIRE_SPINLOCK(virtio -> status_lock);

// Notify the frontend that a requested reset is complete.
error_t
virtio_reset_complete(virtio_t *virtio) REQUIRE_SPINLOCK(virtio -> status_lock);

//
// Frontend control plane API.
//
// This is maxy be exposed to a guest VM through emulated MMIO or a message
// based interface, or called internally by a frontend driver.
//
// All of these functions assume that the virtio pointer is RCU-protected.
//

// Try to set driver-controlled bits in the status register.
error_t
virtio_write_status(virtio_t *virtio, virtio_status_t status)
	EXCLUDE_SPINLOCK(virtio->status_lock) REQUIRE_RCU_READ;

// Read back the status register.
//
// This may block if there is a requested reset and it is not yet complete.
// Therefore, it drops the RCU critical section.
virtio_status_t
virtio_read_status(virtio_t *virtio) EXCLUDE_SPINLOCK(virtio->status_lock)
	RELEASE_RCU_READ;

// Read the config generation register.
uint8_result_t
virtio_get_generation(virtio_t *virtio) REQUIRE_RCU_READ;

// Read a queue's maximum size.
count_result_t
virtio_get_queue_size_max(virtio_t *virtio, index_t queue_sel)
	REQUIRE_SPINLOCK(virtio -> status_lock) REQUIRE_RCU_READ;

// Obtain a queue configuration structure for read or update. Returns ERROR_BUSY
// if the queue configuration is already active (i.e. DRIVER_OK is set).
virtio_queue_info_ptr_result_t
virtio_get_queue_info_ptr(virtio_t *virtio, index_t queue_sel)
	REQUIRE_SPINLOCK(virtio -> status_lock) REQUIRE_RCU_READ;

// Determine whether the backend generates per-queue ready signals.
bool
virtio_supports_per_queue_notify(const virtio_t *virtio);

// Notify the backend that a queue is ready.
error_t
virtio_queue_notify(virtio_t *virtio, index_t queue_sel) REQUIRE_RCU_READ;

// Handle a frontend read of the device features.
uint32_result_t
virtio_get_dev_features(virtio_t *virtio, index_t feature_sel)
	EXCLUDE_SPINLOCK(virtio->status_lock) REQUIRE_RCU_READ;

// Handle a frontend write of the driver features.
error_t
virtio_set_drv_features(virtio_t *virtio, index_t feature_sel,
			uint32_t features) EXCLUDE_SPINLOCK(virtio->status_lock)
	REQUIRE_RCU_READ;

// Handle a device configuration space write by the frontend. Note that the
// maximum atomic access size is 32 bits, as per the spec.
//
// This might block the caller if synchronous config writes are enabled.
// Therefore, it drops the RCU critical section.
error_t
virtio_device_config_write(virtio_t *virtio, size_t offset, size_t access_size,
			   uint32_t value) RELEASE_RCU_READ;

// Handle a device configuration space read by the frontend. Note that the
// maximum atomic access size is 32 bits, as per the spec.
//
// This might block the caller if posted config writes are enabled or if the
// backend is a proxy and does not cache the configuration space. Therefore, it
// drops the RCU critical section.
uint32_result_t
virtio_device_config_read(virtio_t *virtio, size_t offset, size_t access_size)
	RELEASE_RCU_READ;

// Default handler for virtio_device_config_read.
uint32_result_t
virtio_device_config_read_cached(virtio_t *virtio, size_t offset,
				 size_t access_size) REQUIRE_RCU_READ;
