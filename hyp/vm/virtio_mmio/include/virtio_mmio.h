// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
virtio_mmio_frontend_bind_virq(virtio_mmio_t *virtio_mmio, vic_t *vic,
			       virq_t virq);

void
virtio_mmio_frontend_unbind_virq(virtio_mmio_t *virtio_mmio)
	EXCLUDE_PREEMPT_DISABLED;
