// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

virtio_virtq_desc_chain_t
virtio_virtq_read_desc_chain(virtio_iommu_t *virtio_iommu, index_t qnum,
			     index_t first_desc_idx, uint8_t *req_buf,
			     size_t req_buf_size);

void
virtio_virtq_write_reply(virtio_iommu_t *virtio_iommu, index_t qnum,
			 count_t			  first_desc_idx,
			 const virtio_virtq_desc_chain_t *chain,
			 const uint8_t *reply_buf, size_t reply_size);
