// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypregisters.h>

#include <arm_rng.h>
#include <atomic.h>
#include <attributes.h>
#include <cpulocal.h>
#include <preempt.h>

#include <asm/barrier.h>

#if !defined(ARCH_ARM_FEAT_RNG) || !ARCH_ARM_FEAT_RNG
#error ARCH_ARM_FEAT_RNG not set
#endif

// We use a per-cpu counter in case the implementation is not shared, and we
// need to ensure reseeding occurs on each core. If the prng HW is shared,
// then the worst case reseeding interval is 32*(N cores).
CPULOCAL_DECLARE_STATIC(count_t, rng_reseed_count);

static ALWAYS_INLINE uint64_result_t
arm_rng_read_RNDR(void)
{
	bool	 ok;
	uint64_t res;
	// Read RNDR and check the status code for success
	__asm__ volatile(".arch_extension rng	;"
			 "mrs	%[res], RNDR	;"
			 "cset	%w[ok], ne	;"
			 ".arch_extension norng	;"
			 : [res] "=r"(res), [ok] "=r"(ok)::"cc");

	uint64_result_t ret;
	if (ok) {
		ret = uint64_result_ok(res);
	} else {
		ret = uint64_result_error(ERROR_FAILURE);
	}
	return ret;
}

static ALWAYS_INLINE bool
arm_rng_read_RNDRRS(uint64_t *data)
{
	bool	 ok;
	uint64_t res;
	// Read RNDRRS and check the status code for success
	__asm__ volatile(".arch_extension rng	;"
			 "mrs	%[res], RNDRRS	;"
			 "cset	%w[ok], ne	;"
			 ".arch_extension norng	;"
			 : [res] "=r"(res), [ok] "=r"(ok)::"cc");
	*data = res;
	return ok;
}

error_t NOINLINE
arm_rng_get_entropy(platform_prng_data256_t *data)
{
	error_t	 ret	      = ERROR_FAILURE;
	count_t	 i	      = 0U;
	count_t	 retries      = 64;
	uint64_t prng_data[4] = { 0 };

	assert(data != NULL);

	do {
		uint64_result_t res = arm_rng_read_RNDR();
		if (res.e == OK) {
			prng_data[i] = res.r;
			i++;
		} else {
			retries--;
		}
	} while ((i < 4U) && (retries != 0U));

	if (i == 4U) {
		(void)memscpy(data, sizeof(*data), &prng_data,
			      sizeof(prng_data));
		(void)memset_s(&prng_data, sizeof(prng_data), 0,
			       sizeof(prng_data));

		ret = OK;

		// Issue a reseed read, ignoring the result.
		uint64_t tmp;
		(void)arm_rng_read_RNDRRS(&tmp);
	}

	return ret;
}

error_t NOINLINE
arm_rng_get_random32(uint32_t *data)
{
	error_t ret	= ERROR_BUSY;
	count_t retries = 16;

	assert(data != NULL);

	cpulocal_begin();

	uint64_result_t res;
	do {
		res = arm_rng_read_RNDR();
		retries--;
	} while ((res.e != OK) && (retries != 0U));

	if (res.e == OK) {
		*data = (uint32_t)res.r;
		ret   = OK;

		count_t count = CPULOCAL(rng_reseed_count)++;

		if ((count % 32) == 0U) {
			// Issue a reseed read, ignoring the result.
			uint64_t tmp;
			(void)arm_rng_read_RNDRRS(&tmp);
		}
	}
	cpulocal_end();

	return ret;
}
