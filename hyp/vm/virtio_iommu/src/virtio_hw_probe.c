// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include "virtio_hw_probe.h"

// When Virtio-iommu ATTACH_TABLE probes the SMMUv3 HW it needs some sensible
// values. These values will be based on the actual HW of the platform.
// The Hypervisor may need to change some values reported to the guest based on
// VMID and endpoint.
// Keeping this file aside to reduce noise from the more general code.

error_t
virtio_iommu_setup_hw_probe_idrs(virtio_iommu_t *virtio_iommu,
				 uint32_t	 endpoint,
				 virtio_iommu_probe_hw_arm_smmuv3_t *idrs)
{
	// We can do some checks here do give some endpoints different
	// settings.
	(void)virtio_iommu;
	(void)endpoint;

	// Hyp can change settings as needed.
	// Do we need to find a better way to encode these?
	// IDR2, IDR4 currently unsupported. Leave them zero.
	SMMU_V3_IDR0_t idr0 =
		SMMU_V3_IDR0_cast(PLATFORM_VIRTIO_IOMMU_ATTACH_TABLE_IDR0);
	SMMU_V3_IDR1_t idr1 =
		SMMU_V3_IDR1_cast(PLATFORM_VIRTIO_IOMMU_ATTACH_TABLE_IDR1);
	SMMU_V3_IDR3_t idr3 =
		SMMU_V3_IDR3_cast(PLATFORM_VIRTIO_IOMMU_ATTACH_TABLE_IDR3);
	SMMU_V3_IDR5_t idr5 =
		SMMU_V3_IDR5_cast(PLATFORM_VIRTIO_IOMMU_ATTACH_TABLE_IDR5);

	// Sanity check
	// Substream ID max is 20bits.
	assert(SMMU_V3_IDR1_get_SSIDSIZE(&idr1) <=
	       SMMU_V3_SUBSTREAM_ID_MAX_BITS);

	idrs->idr0 = SMMU_V3_IDR0_raw(idr0);
	idrs->idr1 = SMMU_V3_IDR1_raw(idr1);
	idrs->idr3 = SMMU_V3_IDR3_raw(idr3);
	idrs->idr5 = SMMU_V3_IDR5_raw(idr5);

	return OK;
}
