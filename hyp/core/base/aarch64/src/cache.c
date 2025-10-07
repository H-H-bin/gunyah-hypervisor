// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <util.h>

#include <asm/cache.h>
#include <asm/cpu.h>

void
cache_clean_range(const void *data, size_t size)
{
	CACHE_CLEAN_RANGE(data, size);
}

void
cache_clean_invalidate_range(const void *data, size_t size)
{
	CACHE_CLEAN_INVALIDATE_RANGE(data, size);
}

void
cache_invalidate_inst_all(void)
{
	CTR_EL0_t ctr = register_CTR_EL0_read();

	if (!CTR_EL0_get_DIC(&ctr)) {
		// The CPU requires invalidation for data-to-instruction
		// coherency. Invalidate the whole instruction cache for all
		// cores in the inner shareable domain.
		__asm__ volatile("ic ialluis; dsb ish" ::: "memory");
	}
}
