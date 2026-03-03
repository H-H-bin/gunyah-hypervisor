// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Bind a stream ID range to a specific viommu.
//
// This claims exclusive ownership of the physical range for the viommu. The
// streams in this range are guaranteed to be initially detached; that is, the
// STEs are either unallocated, invalid, or configured to always abort.
//
// The viommu pointer is not reference-counted; all stream ranges bound to a
// viommu must be unbound by its deactivation handler.
smmuv3_stream_range_ptr_result_t
smmuv3_bind_stream_range(smmuv3_t *smmu, viommu_t *viommu,
			 smmu_v3_stream_id_t base_id, count_t count);

// Unbind a stream ID range from a viommu.
//
// This will clear any remaining valid STEs in this range, and will free the
// range structure. The caller must guarantee that all accesses to the range
// structure are complete after an RCU grace period has elapsed.
void
smmuv3_unbind_stream_range(smmuv3_stream_range_t *range);

// Attach a valid STE to a stream ID within a bound range. If there is an
// existing valid STE, it will be removed. If the prefetch flag is true,
// then a prefetch command will be queued for the STE.
error_t
smmuv3_attach_stream(smmuv3_stream_range_t	  *range,
		     smmu_v3_stream_id_t	   stream_id,
		     const smmuv3_stream_config_t *s1_cfg, bool prefetch_ste);

// Remove the STE for the stream ID within the specified range.
void
smmuv3_detach_stream(smmuv3_stream_range_t *range,
		     smmu_v3_stream_id_t    stream_id);

void
smmuv3_sync(smmuv3_t *smmu, bool raise_interrupt, bool wait);

void
smmuv3_cfgi_cd_all(smmuv3_stream_range_t *range, smmu_v3_stream_id_t stream_id);

void
smmuv3_tlbi_el1_vmid(smmuv3_t *smmu, viommu_t *viommu);

void
smmuv3_tlbi_el1_asid(smmuv3_t *smmu, viommu_t *viommu, smmu_v3_asid_t asid);

void
smmuv3_tlbi_el1_address_range(smmuv3_t *smmu, viommu_t *viommu, paddr_t address,
			      uint64_t nr_pages, uint8_t page_size, bool leaf);

void
smmuv3_tlbi_el1_asid_address_range(smmuv3_t *smmu, viommu_t *viommu,
				   smmu_v3_asid_t asid, paddr_t address,
				   uint64_t nr_pages, uint8_t page_size,
				   bool leaf);

void
smmu_v3_atc_inv_stream_all(smmuv3_stream_range_t *range,
			   smmu_v3_stream_id_t	  stream_id);

void
smmu_v3_atc_inv_stream_address_range(smmuv3_stream_range_t *range,
				     smmu_v3_stream_id_t    stream_id,
				     paddr_t address, uint8_t sz_bits);
