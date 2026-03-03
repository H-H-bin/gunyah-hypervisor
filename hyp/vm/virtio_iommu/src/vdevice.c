// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hyprights.h>

#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <vpci.h>

#include "event_handlers.h"

error_t
virtio_iommu_handle_vpci_attach(vpci_t *vpci, index_t *slot,
				cap_id_t device_cap)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_iommu_ptr_result_t p = cspace_lookup_virtio_iommu(
		cspace, device_cap, CAP_RIGHTS_VIRTIO_IOMMU_BIND_VPCI);

	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}

	virtio_iommu_t *iommu = p.r;

	if (iommu->virtio.transport_type != VIRTIO_TRANSPORT_TYPE_PCI) {
		ret = ERROR_OBJECT_CONFIG;
		goto out_ref;
	}

	index_result_t slot_r = vpci_bind_device(
		vpci, *slot, &iommu->virtio.frontend_data.pci.device);

	ret = slot_r.e;
	if (ret == OK) {
		*slot = slot_r.r;
	}

out_ref:
	object_put_virtio_iommu(iommu);

out:
	return ret;
}
