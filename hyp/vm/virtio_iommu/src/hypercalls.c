// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <spinlock.h>
#include <virtio.h>

// Let RM Specify the max number of domains
// For a SMMU with SoC devices this can come from IORT count etc.
// For a SMMU with PCI devices this can be some selected max.
// This is to limit memory use so virtio-iommu cannot be used to
// consume all HYP memory.
// Can we limit this to the number of PCI busses on a platform?
// A PCI bus can attach 32 cards each with 8 functions.

error_t
hypercall_virtio_iommu_configure(cap_id_t virtio_iommu_cap, cap_id_t iommu_cap,
				 virtio_iommu_options_t options,
				 cap_id_t		addrspace_cap)
{
	error_t	      ret;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	if (!virtio_iommu_options_is_clean(options) ||
	    !virtio_iommu_options_get_addrspace_valid(&options)) {
		ret = ERROR_UNIMPLEMENTED;
		goto out;
	}

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, virtio_iommu_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE,
		&type);
	if (compiler_unexpected(o.e != OK)) {
		ret = o.e;
		goto fail_iommu_lookup;
	}
	if (type != OBJECT_TYPE_VIRTIO_IOMMU) {
		ret = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		object_put(type, o.r);
		goto fail_iommu_lookup;
	}
	virtio_iommu_t *virtio_iommu = o.r.virtio_iommu;

	addrspace_ptr_result_t addrspace_r = cspace_lookup_addrspace(
		cspace, addrspace_cap, CAP_RIGHTS_ADDRSPACE_ATTACH);
	if (addrspace_r.e != OK) {
		ret = addrspace_r.e;
		goto fail_addrspace_lookup;
	}

#if defined(MODULE_PLATFORM_SMMUV3) && MODULE_PLATFORM_SMMUV3 && SMMU_V3_ENABLE
	smmuv3_ptr_result_t smmu_r = cspace_lookup_smmuv3(
		cspace, iommu_cap, CAP_RIGHTS_SMMUV3_CONFIGURE);
	if (smmu_r.e != OK) {
		ret = smmu_r.e;
		goto fail_smmu_lookup;
	}

	spinlock_acquire(&virtio_iommu->header.lock);
	if (atomic_load_relaxed(&virtio_iommu->header.state) !=
	    OBJECT_STATE_INIT) {
		ret = ERROR_OBJECT_STATE;
		goto fail_iommu_state;
	}

	error_t err = virtio_configure(
		&virtio_iommu->virtio, virtio_iommu->header.partition,
		VIRTIO_BACKEND_TYPE_IOMMU, VIRTIO_TRANSPORT_TYPE_PCI,
		VIRTIO_DEVICE_TYPE_IOMMU, VIRTIO_IOMMU_NUM_QUEUES,
		sizeof(virtio_iommu_config_t), NULL, 0, true);
	if (err != OK) {
		ret = err;
		goto fail_virtio_configure;
	}

	virtio_iommu->max_streams =
		virtio_iommu_options_get_max_streams(&options);

	if (virtio_iommu->iommu != NULL) {
		object_put_smmuv3(virtio_iommu->iommu);
	}
	virtio_iommu->iommu = object_get_smmuv3_additional(smmu_r.r);

	if (virtio_iommu->viommu.addrspace != NULL) {
		object_put_addrspace(virtio_iommu->viommu.addrspace);
	}
	virtio_iommu->viommu.addrspace =
		object_get_addrspace_additional(addrspace_r.r);

	ret = OK;

fail_virtio_configure:
fail_iommu_state:
	spinlock_release(&virtio_iommu->header.lock);
	object_put_smmuv3(smmu_r.r);
fail_smmu_lookup:
#else
	(void)iommu_cap;
	ret = ERROR_UNIMPLEMENTED;
#endif
	object_put_addrspace(addrspace_r.r);
fail_addrspace_lookup:
	object_put_virtio_iommu(virtio_iommu);
fail_iommu_lookup:
out:
	return ret;
}
