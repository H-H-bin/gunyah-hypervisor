// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <vdevice.h>
#include <vic.h>
#include <virq.h>
#include <virtio.h>

#include <events/virtio_backend.h>
#include <events/virtio_mmio.h>

#include "event_handlers.h"
#include "virtio_mmio.h"

error_t
virtio_mmio_handle_virtio_startup(virtio_t *virtio)
{
	error_t ret;
	assert(virtio != NULL);

	// Configuration cache extent must be present and mapped.
	if ((virtio->config_cache_me == NULL) ||
	    (virtio->config_range.base == 0U)) {
		TRACE(ERROR, INFO, "virtio_startup mmio: no config cache");
		ret = ERROR_OBJECT_CONFIG;
		goto out;
	}

	// Device configuration offset must be at least 0x100, so we can
	// place the read-only mirror of the common registers before it.
	if (virtio->config_offset <
	    offsetof(virtio_mmio_regs_t, device_config)) {
		TRACE(ERROR, INFO,
		      "virtio_startup mmio: bad config offset {:#x}",
		      virtio->config_offset);
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}
	if (virtio->config_size <
	    (sizeof(virtio_mmio_regs_t) -
	     offsetof(virtio_mmio_regs_t, device_config))) {
		TRACE(ERROR, INFO, "virtio_startup mmio: bad config size {:#x}",
		      virtio->config_size);
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	virtio_mmio_t *virtio_mmio = &virtio->frontend_data.mmio;

	spinlock_init(&virtio_mmio->banking_lock);
#if defined(PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE) &&                          \
	PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE
	spinlock_init(&virtio_mmio->interrupt_lock);
#endif

	virtio_mmio->regs = virtio_mmio_regs_container_of_device_config(
		virtio->config_cache);

	// Write initial values into the config cache
	atomic_store_relaxed(&virtio_mmio->regs->status, virtio->status);
	if (virtio->device_type != VIRTIO_DEVICE_TYPE_INVALID) {
		atomic_store_relaxed(&virtio_mmio->regs->dev_id,
				     virtio->device_type);
	}

	// Register the config cache extent as a device, so we can trap and
	// handle accesses to read-only mappings. The management VM is expected
	// to map this read-only somewhere in the guest's address space.
	virtio_mmio->vdevice.type = VDEVICE_TYPE_VIRTIO_MMIO;
	ret			  = vdevice_attach_phys(&virtio_mmio->vdevice,
							virtio->config_cache_me);
	if (ret != OK) {
		TRACE(ERROR, INFO,
		      "virtio_startup mmio: failed vdevice_attach_phys: {:d}",
		      (register_t)ret);
		goto out;
	}

	ret = trigger_virtio_backend_device_config_activate_event(
		virtio->device_type,
		virtio_backend_container_of_virtio(virtio));
	if (ret != OK) {
		TRACE(ERROR, INFO,
		      "virtio_startup mmio: failed device_config_activate: {:d}",
		      (register_t)ret);
		goto out_vdevice;
	}

out_vdevice:
	if (ret != OK) {
		vdevice_detach_phys(&virtio_mmio->vdevice,
				    virtio->config_cache_me);
	}
out:
	return ret;
}

void
virtio_mmio_handle_virtio_shutdown(virtio_t *virtio)
{
	assert(virtio != NULL);

	virtio_mmio_t *virtio_mmio = &virtio->frontend_data.mmio;

	vic_unbind(&virtio_mmio->virq_source);

	vdevice_detach_phys(&virtio_mmio->vdevice, virtio->config_cache_me);
}

error_t
virtio_mmio_frontend_bind_virq(virtio_mmio_t *virtio_mmio, vic_t *vic,
			       virq_t virq)
{
	error_t ret = OK;

	assert(virtio_mmio != NULL);
	assert(vic != NULL);

	ret = vic_bind_shared(&virtio_mmio->virq_source, vic, virq,
			      VIRQ_TRIGGER_VIRTIO_MMIO_FRONTEND);

	if ((ret == OK) &&
	    (atomic_load_relaxed(&virtio_mmio->regs->interrupt_status) != 0U)) {
		(void)virq_assert(&virtio_mmio->virq_source, false);
	}

	return ret;
}

void
virtio_mmio_frontend_unbind_virq(virtio_mmio_t *virtio_mmio)
{
	assert(virtio_mmio != NULL);

	vic_unbind_sync(&virtio_mmio->virq_source);
}

error_t
virtio_mmio_handle_virtio_ack_features_ok(virtio_t *virtio)
{
	// Synchronous features_ok not implemented for MMIO.
	(void)virtio;
	return ERROR_UNIMPLEMENTED;
}

static void
virtio_mmio_update_generation(virtio_t *virtio)
{
	rcu_read_start();
	virtio_mmio_t *virtio_mmio = &virtio->frontend_data.mmio;
	uint8_result_t new_gen	   = virtio_get_generation(virtio);
	if (new_gen.e == OK) {
		atomic_store_relaxed(&virtio_mmio->regs->config_gen, new_gen.r);
	}
	rcu_read_finish();
}

static void
virtio_mmio_assert_irq(virtio_t *virtio, uint32_t set_interrupt_status)
{
	virtio_mmio_t *virtio_mmio = &virtio->frontend_data.mmio;
#if defined(PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE) &&                          \
	PLATFORM_NO_DEVICE_ATTR_ATOMIC_UPDATE
	spinlock_acquire(&virtio_mmio->interrupt_lock);
	uint32_t old_interrupt_status =
		atomic_load_relaxed(&virtio_mmio->regs->interrupt_status);
	uint32_t new_interrupt_status = old_interrupt_status |
					set_interrupt_status;
	bool raise_virq = (old_interrupt_status != new_interrupt_status);
	atomic_store_relaxed(&virtio_mmio->regs->interrupt_status,
			     new_interrupt_status);
	spinlock_release(&virtio_mmio->interrupt_lock);
#else
	uint32_t old_interrupt_status = atomic_fetch_or_explicit(
		&virtio_mmio->regs->interrupt_status, set_interrupt_status,
		memory_order_relaxed);
	bool raise_virq = (set_interrupt_status & ~old_interrupt_status) != 0U;
#endif
	if (raise_virq) {
		(void)virq_assert(&virtio_mmio->virq_source, false);
	}
}

void
virtio_mmio_handle_virtio_config_update_begin(virtio_t *virtio)
{
	virtio_mmio_update_generation(virtio);
}

void
virtio_mmio_handle_virtio_config_update_end(virtio_t *virtio)
{
	virtio_mmio_update_generation(virtio);
	virtio_mmio_assert_irq(virtio, (uint32_t)util_bit(1));
}

error_t
virtio_mmio_handle_virtio_queue_ready(virtio_t *virtio, index_t vq)
{
	// No per-queue VIRQ support
	(void)vq;
	virtio_mmio_assert_irq(virtio, (uint32_t)util_bit(0));
	return OK;
}

void
virtio_mmio_handle_virtio_reset_complete(virtio_t *virtio)
{
	virtio_mmio_t *virtio_mmio = &virtio->frontend_data.mmio;
	atomic_store_relaxed(&virtio_mmio->regs->status,
			     virtio_status_cast(0U));
}

bool
virtio_mmio_frontend_handle_virq_check_pending(virq_source_t *source)
{
	assert(source != NULL);

	// Deassert frontend's IRQ when interrupt_status is zero, meaning no
	// interrupts are pending to be handled
	virtio_mmio_t *virtio_mmio =
		virtio_mmio_container_of_virq_source(source);

	// Note: this is an atomic load, so there is no data race; we don't need
	// to acquire interrupt_lock on targets that use it to make updates
	// atomic; if this runs concurrently with an update, we are guaranteed
	// to see either the old or the new status.
	return (atomic_load_relaxed(&virtio_mmio->regs->interrupt_status) !=
		0U);
}
