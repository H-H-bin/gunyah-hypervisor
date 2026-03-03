// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

void
virtio_iommu_disable_all_streams(virtio_iommu_t *virtio_iommu)
	REQUIRE_SPINLOCK(virtio_iommu -> lock);

void
virtio_iommu_disable_range_streams(virtio_iommu_t	 *virtio_iommu,
				   smmuv3_stream_range_t *range)
	REQUIRE_SPINLOCK(virtio_iommu -> lock);

void
virtio_iommu_process_queue_notification(virtio_iommu_t *virtio_iommu,
					index_t		qnum)
	REQUIRE_SPINLOCK(virtio_iommu -> lock);

smmuv3_stream_range_t *
virtio_iommu_find_range(virtio_iommu_t	  *virtio_iommu,
			viommu_stream_id_t stream_id) REQUIRE_RCU_READ;
