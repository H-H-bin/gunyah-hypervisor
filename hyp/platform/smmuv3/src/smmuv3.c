// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <atomic.h>
#include <compiler.h>
#include <hyp_aspace.h>
#include <log.h>
#include <panic.h>
#include <partition.h>
#include <pgtable.h>
#include <trace.h>

// Includes generated handlers.
#include "event_handlers.h"

#if (PLATFORM_SMMU_V3_TCU_COUNT != 1)
#error "smmuv3: Only support 1 TCU for now"
#endif

#if (!defined(SMMU_V3_HAS_MMU_600) || !SMMU_V3_HAS_MMU_600)
#error "smmuv3: Only MMU-600 is supported for now"
#endif

#if ((defined(SMMU_V3_PASSTHROUGH) && SMMU_V3_PASSTHROUGH) &&                  \
     (defined(SMMU_V3_GLOBAL_BYPASS) && SMMU_V3_GLOBAL_BYPASS))
#error "smmuv3: Cannot set pass-through and global-bypass at the same time"
#endif

// Support only one SMMUv3 TCU for now
static smmu_v3_t *smmu_v3;

// It is sometimes desirable to be able to inspect the state in the
// debugger from the secure world point of view. Expose the physical addresses
// so the debuggers can find them.
extern paddr_t platform_smmuv3_base;
paddr_t	       platform_smmuv3_base = PLATFORM_SMMU_V3_BASE;

void
smmuv3_handle_boot_cold_init(cpu_index_t boot_cpu_index)
{
	(void)boot_cpu_index;

	// SMMU has at least 2 * 64k pages of core registers
	size_t		    regs_size = sizeof(*smmu_v3);
	virt_range_result_t range     = hyp_aspace_allocate(regs_size);
	if (range.e != OK) {
		panic("smmuv3: Register address allocation failed.");
	}

	pgtable_hyp_start();

	// Map the SMMU TCU pages
	error_t ret = pgtable_hyp_map(partition_get_private(), range.r.base,
				      regs_size, platform_smmuv3_base,
				      PGTABLE_HYP_MEMTYPE_NOSPEC_NOCOMBINE,
				      PGTABLE_ACCESS_RW,
				      VMSA_SHAREABILITY_NON_SHAREABLE);
	if (ret != OK) {
		panic("smmuv3: Mapping of register pages failed.");
	}

	pgtable_hyp_commit();

	smmu_v3 = (smmu_v3_t *)range.r.base;

	// Sanity check - verify the HW IDs
	SMMU_V3_IIDR_t iidr = atomic_load_relaxed(&smmu_v3->smmu_page0.iidr);
	assert(SMMU_V3_IIDR_get_Implementer(&iidr) == SMMU_V3_IMPLEMENTER);
	assert(SMMU_V3_IIDR_get_ProductID(&iidr) == SMMU_V3_PRODUCT_ID);

	SMMU_V3_AIDR_t aidr = atomic_load_relaxed(&smmu_v3->smmu_page0.aidr);
	assert(SMMU_V3_AIDR_get_ArchMajorRev(&aidr) == SMMU_V3_MAJOR_REV);
	assert(SMMU_V3_AIDR_get_ArchMinorRev(&aidr) == SMMU_V3_MINOR_REV);
}

void
smmuv3_handle_boot_hypervisor_start(void)
{
#if defined(SMMU_V3_PASSTHROUGH) && SMMU_V3_PASSTHROUGH

	// Log and exit.
	// Do not configure the SMMU, let HLOS take ownership
	SMMU_V3_AIDR_t aidr = atomic_load_relaxed(&smmu_v3->smmu_page0.aidr);
	LOG(DEBUG, INFO, "smmuv3: Found SMMUv3.{:d}",
	    SMMU_V3_AIDR_get_ArchMinorRev(&aidr));
	LOG(DEBUG, INFO, "smmuv3: Pass-through to PVM");
	return;

#else // SMMU_V3_PASSTHROUGH

	SMMU_V3_AIDR_t aidr = atomic_load_relaxed(&smmu_v3->smmu_page0.aidr);
	LOG(DEBUG, INFO, "smmuv3: Found SMMUv3.{:d}",
	    SMMU_V3_AIDR_get_ArchMinorRev(&aidr));

	SMMU_V3_CR0_t  cr0;
	SMMU_V3_CR0_t  cr0ack;
	SMMU_V3_GBPA_t gbpa;

	// First check there are no pending updates
	cr0    = atomic_load_relaxed(&smmu_v3->smmu_page0.cr0);
	cr0ack = atomic_load_relaxed(&smmu_v3->smmu_page0.cr0ack);
	gbpa   = atomic_load_relaxed(&smmu_v3->smmu_page0.gbpa);
	assert(SMMU_V3_CR0_is_equal(cr0, cr0ack));
	assert(!SMMU_V3_GBPA_get_Update(&gbpa));

	// Set the pass-through config
	// Set all to preserve incoming attributes
	// SHCFG is non-zero for 'use incoming'
	SMMU_V3_GBPA_set_MTCFG(&gbpa, false);
	SMMU_V3_GBPA_set_ALLOCCFG(&gbpa, false);
	SMMU_V3_GBPA_set_SHCFG(&gbpa, true);
	SMMU_V3_GBPA_set_PRIVCFG(&gbpa, false);
	SMMU_V3_GBPA_set_INSTCFG(&gbpa, false);
#if defined(SMMU_V3_GLOBAL_BYPASS) && SMMU_V3_GLOBAL_BYPASS
	LOG(DEBUG, INFO, "smmuv3: Enable global bypass");
	SMMU_V3_GBPA_set_ABORT(&gbpa, false);
#else
	LOG(DEBUG, INFO, "smmuv3: Enable global abort");
	SMMU_V3_GBPA_set_ABORT(&gbpa, true);
#endif
	// Set update flag to check for completion
	SMMU_V3_GBPA_set_Update(&gbpa, true);

	atomic_store_relaxed(&smmu_v3->smmu_page0.gbpa, gbpa);

	// Wait for update to complete
	do {
		gbpa = atomic_load_relaxed(&smmu_v3->smmu_page0.gbpa);
	} while (SMMU_V3_GBPA_get_Update(&gbpa) != 0U);

	// Disable the SMMU
	SMMU_V3_CR0_set_SMMUEN(&cr0, false);
	atomic_store_relaxed(&smmu_v3->smmu_page0.cr0, cr0);

	// Wait for update to complete
	do {
		cr0    = atomic_load_relaxed(&smmu_v3->smmu_page0.cr0);
		cr0ack = atomic_load_relaxed(&smmu_v3->smmu_page0.cr0ack);
	} while (!SMMU_V3_CR0_is_equal(cr0, cr0ack));

#endif // SMMU_V3_PASSTHROUGH
}
