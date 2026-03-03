// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <compiler.h>
#include <rcu.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>
#include <virq.h>
#include <virtio.h>

#include <events/virtio_mmio.h>

#include <asm/nospec_checks.h>

#include "event_handlers.h"

static virtio_t *
virtio_from_virtio_mmio(virtio_mmio_t *virtio_mmio)
{
	return virtio_container_of_frontend_data(
		virtio_frontend_container_of_mmio(virtio_mmio));
}

static bool
virtio_mmio_access_allowed(size_t size, size_t offset)
{
	bool ret;

	// First check if the access is size-aligned
	if ((offset & (size - 1U)) != 0UL) {
		ret = false;
	} else if (size == sizeof(uint32_t)) {
		// Word accesses, always allowed
		ret = true;
	} else if (size == sizeof(uint8_t)) {
		// Byte accesses only allowed for config
		ret = util_offset_in_range(offset, virtio_mmio_regs_t,
					   device_config);
	} else {
		// Invalid access size
		ret = false;
	}

	return ret;
}

static error_t
virtio_mmio_write_queue_sel(virtio_mmio_t *virtio_mmio, uint32_t val)
	REQUIRE_RCU_READ
{
	error_t ret;

	spinlock_acquire(&virtio_mmio->banking_lock);
	virtio_status_lock_nopreempt(virtio_from_virtio_mmio(virtio_mmio));
	virtio_mmio->queue_sel = val;

	// Get the queue info and maximum size. Note that both of these
	// functions do Spectre-safe bounds checks, so we don't need one here.
	virtio_queue_info_result_t queue_info_r =
		virtio_get_queue_info(virtio_from_virtio_mmio(virtio_mmio),
				      virtio_mmio->queue_sel, false);
	count_result_t queue_size_max_r = virtio_get_queue_size_max(
		virtio_from_virtio_mmio(virtio_mmio), virtio_mmio->queue_sel);

	// Update corresponding banked registers with read permission. Note that
	// an out-of-range value is not an error; it simply returns a zero max
	// size to indicate that the queue does not exist.
	atomic_store_relaxed(&virtio_mmio->regs->queue_num_max,
			     (queue_size_max_r.e == OK) ? queue_size_max_r.r
							: 0U);
	atomic_store_relaxed(
		&virtio_mmio->regs->queue_ready,
		((queue_info_r.e == OK) && queue_info_r.r.ready) ? 1U : 0U);
	ret = OK;

	virtio_status_unlock_nopreempt(virtio_from_virtio_mmio(virtio_mmio));
	spinlock_release(&virtio_mmio->banking_lock);
	return ret;
}

static error_t
virtio_mmio_write_dev_feat_sel(virtio_mmio_t *virtio_mmio, uint32_t val)
	REQUIRE_RCU_READ
{
	error_t ret;

	index_result_t feature_sel_r =
		nospec_range_check(val, VIRTIO_FEAT_WORDS);
	if (feature_sel_r.e != OK) {
		ret = feature_sel_r.e;
		goto out;
	}

	uint32_result_t features_r = virtio_get_dev_features(
		virtio_from_virtio_mmio(virtio_mmio), feature_sel_r.r);

	// Update corresponding banked register
	spinlock_acquire(&virtio_mmio->banking_lock);
	virtio_mmio->dev_feat_sel = feature_sel_r.r;
	atomic_store_relaxed(&virtio_mmio->regs->dev_feat, features_r.r);
	spinlock_release(&virtio_mmio->banking_lock);
	ret = OK;

out:
	return ret;
}

static error_t
virtio_mmio_write_drv_feat_sel(virtio_mmio_t *virtio_mmio, uint32_t val)
	REQUIRE_RCU_READ
{
	error_t ret;

	index_result_t feature_sel_r =
		nospec_range_check(val, VIRTIO_FEAT_WORDS);
	if (feature_sel_r.e != OK) {
		ret = feature_sel_r.e;
		goto out;
	}

	virtio_mmio->drv_feat_sel = feature_sel_r.r;
	ret			  = OK;

out:
	return ret;
}

static void
virtio_mmio_write_interrupt_ack(virtio_mmio_t	  *virtio_mmio,
				virtio_interrupt_t clear_interrupt_status)
{
#if defined(PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE) &&                          \
	PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE
	spinlock_acquire(&virtio_mmio->interrupt_lock);
	virtio_interrupt_t interrupt_status =
		atomic_load_relaxed(&virtio_mmio->regs->interrupt_status);
	atomic_store_relaxed(&virtio_mmio->regs->interrupt_status,
			     virtio_interrupt_difference(
				     interrupt_status, clear_interrupt_status));
	spinlock_release(&virtio_mmio->interrupt_lock);
#else
	(void)virtio_interrupt_atomic_difference(
		&virtio_mmio->regs->interrupt_status, clear_interrupt_status,
		memory_order_relaxed);
#endif
}

static error_t
virtio_mmio_write_status(virtio_mmio_t *virtio_mmio, uint32_t val)
	RELEASE_RCU_READ
{
	virtio_status_t new_status = virtio_status_cast((uint8_t)val);
	virtio_t       *virtio	   = virtio_from_virtio_mmio(virtio_mmio);

	if (virtio_status_is_empty(new_status)) {
		// Reset requested; clear the queue_ready field. The underlying
		// flags will be cleared by virtio_write_status().
		atomic_store_relaxed(&virtio_mmio->regs->queue_ready, 0U);
		// Also clear the interrupt status.
#if defined(PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE) &&                          \
	PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE
		spinlock_acquire(&virtio_mmio->interrupt_lock);
		atomic_store_relaxed(&virtio_mmio->regs->interrupt_status,
				     virtio_interrupt_default());
		spinlock_release(&virtio_mmio->interrupt_lock);
#else
		atomic_store_relaxed(&virtio_mmio->regs->interrupt_status,
				     virtio_interrupt_default());
#endif
		(void)virq_clear(&virtio_mmio->virq_source);
	}

	error_t err = virtio_write_status(virtio, new_status);

	if (err == OK) {
		// Read back the updated status. This is done to ensure that
		// the write blocks until the reset is complete, if blocking
		// resets are enabled.
		(void)virtio_read_status(virtio);
	} else {
		rcu_read_finish();
	}

	return err;
}

static error_t
virtio_mmio_write_queue(virtio_mmio_t *virtio_mmio, size_t offset, uint32_t val)
	REQUIRE_RCU_READ
{
	error_t ret;

	spinlock_acquire(&virtio_mmio->banking_lock);
	virtio_status_lock_nopreempt(virtio_from_virtio_mmio(virtio_mmio));
	virtio_queue_info_ptr_result_t queue_info_r = virtio_get_queue_info_ptr(
		virtio_from_virtio_mmio(virtio_mmio), virtio_mmio->queue_sel);
	if (queue_info_r.e != OK) {
		ret = queue_info_r.e;
		goto out_locked;
	}
	virtio_queue_info_t *queue_info = queue_info_r.r;

	switch (offset) {
	case offsetof(virtio_mmio_regs_t, queue_num): {
		count_result_t max_r = virtio_get_queue_size_max(
			virtio_from_virtio_mmio(virtio_mmio),
			virtio_mmio->queue_sel);
		if (max_r.e != OK) {
			ret = max_r.e;
			break;
		}
		queue_info->size = util_min(val, max_r.r);
		ret		 = OK;
		break;
	}

	case offsetof(virtio_mmio_regs_t, queue_ready):
		queue_info->ready = (val != 0U);
		atomic_store_relaxed(&virtio_mmio->regs->queue_ready,
				     queue_info->ready ? 1U : 0U);
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_desc_low):
		queue_info->desc &= ~util_mask(32);
		queue_info->desc |= (paddr_t)val;
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_desc_high):
		queue_info->desc &= util_mask(32);
		queue_info->desc |= (paddr_t)val << 32;
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_drv_low):
		queue_info->drv &= ~util_mask(32);
		queue_info->drv |= (paddr_t)val;
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_drv_high):
		queue_info->drv &= util_mask(32);
		queue_info->drv |= (paddr_t)val << 32;
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_dev_low):
		queue_info->dev &= ~util_mask(32);
		queue_info->dev |= (paddr_t)val;
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, queue_dev_high):
		queue_info->dev &= util_mask(32);
		queue_info->dev |= (paddr_t)val << 32;
		ret = OK;
		break;

	default:
		ret = ERROR_ARGUMENT_INVALID;
		break;
	}

out_locked:
	virtio_status_unlock_nopreempt(virtio_from_virtio_mmio(virtio_mmio));
	spinlock_release(&virtio_mmio->banking_lock);

	return ret;
}

static error_t
virtio_mmio_vdevice_write(virtio_mmio_t *virtio_mmio, size_t offset,
			  uint32_t val, size_t access_size) REQUIRE_RCU_READ
{
	error_t ret;

	switch (offset) {
	case offsetof(virtio_mmio_regs_t, dev_feat_sel):
		ret = virtio_mmio_write_dev_feat_sel(virtio_mmio, val);
		break;

	case offsetof(virtio_mmio_regs_t, drv_feat):
		ret = virtio_set_drv_features(
			virtio_from_virtio_mmio(virtio_mmio),
			virtio_mmio->drv_feat_sel, val);
		break;

	case offsetof(virtio_mmio_regs_t, drv_feat_sel):
		ret = virtio_mmio_write_drv_feat_sel(virtio_mmio, val);
		break;

	case offsetof(virtio_mmio_regs_t, queue_sel):
		ret = virtio_mmio_write_queue_sel(virtio_mmio, val);
		break;

	case offsetof(virtio_mmio_regs_t, queue_notify):
		ret = virtio_queue_notify(virtio_from_virtio_mmio(virtio_mmio),
					  val);
		break;

	case offsetof(virtio_mmio_regs_t, interrupt_ack):
		virtio_mmio_write_interrupt_ack(
			virtio_mmio, virtio_interrupt_cast((uint8_t)val));
		ret = OK;
		break;

	case offsetof(virtio_mmio_regs_t, status):
		ret = virtio_mmio_write_status(virtio_mmio, val);
		// The status write might have blocked, and has therefore
		// dropped the RCU critical section. Re-acquire it.
		// FIXME: QC Gunyah issue #252
		rcu_read_start();
		break;

	case offsetof(virtio_mmio_regs_t, queue_num):
	case offsetof(virtio_mmio_regs_t, queue_ready):
	case offsetof(virtio_mmio_regs_t, queue_desc_low):
	case offsetof(virtio_mmio_regs_t, queue_desc_high):
	case offsetof(virtio_mmio_regs_t, queue_drv_low):
	case offsetof(virtio_mmio_regs_t, queue_drv_high):
	case offsetof(virtio_mmio_regs_t, queue_dev_low):
	case offsetof(virtio_mmio_regs_t, queue_dev_high):
		ret = virtio_mmio_write_queue(virtio_mmio, offset, val);
		break;

	default:
		if (offset >= offsetof(virtio_mmio_regs_t, device_config)) {
			ret = virtio_device_config_write(
				virtio_from_virtio_mmio(virtio_mmio),
				offset - offsetof(virtio_mmio_regs_t,
						  device_config),
				access_size, val);
			// The config write might have blocked, and has
			// therefore dropped the RCU critical section.
			// Re-acquire it.
			// FIXME: QC Gunyah issue #252
			rcu_read_start();
		} else {
			ret = ERROR_UNIMPLEMENTED;
		}
		break;
	}

	TRACE(ERROR, INFO,
	      "virtio_mmio_vdevice_write: offset {:#x} size {:#x}"
	      " val {:#x} -> {:d}",
	      offset, access_size, val, (register_t)ret);

	return ret;
}

vcpu_trap_result_t
virtio_mmio_handle_vdevice_access(vdevice_t *vdevice, size_t offset,
				  size_t access_size, register_t *value,
				  bool is_write) REQUIRE_RCU_READ
{
	vcpu_trap_result_t ret;

	// Trap only writes from virtio's frontend
	if (!is_write) {
		ret = VCPU_TRAP_RESULT_UNHANDLED;
		goto out;
	}

	assert((vdevice != NULL) &&
	       (vdevice->type == VDEVICE_TYPE_VIRTIO_MMIO));
	virtio_mmio_t *virtio_mmio = virtio_mmio_container_of_vdevice(vdevice);

	if (!virtio_mmio_access_allowed(access_size, offset)) {
		ret = VCPU_TRAP_RESULT_FAULT;
		goto out;
	}

	ret = (virtio_mmio_vdevice_write(virtio_mmio, offset, (uint32_t)*value,
					 access_size) == OK)
		      ? VCPU_TRAP_RESULT_EMULATED
		      : VCPU_TRAP_RESULT_FAULT;

out:
	return ret;
}
