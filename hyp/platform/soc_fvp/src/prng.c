// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <arm_rng.h>
#include <platform_prng.h>

// Device unique serial number
error_t
platform_get_serial(uint32_t data[4])
{
	data[0] = 0U;
	data[1] = 0U;
	data[2] = 0U;
	data[3] = 0U;

	return OK;
}

error_t
platform_get_random32(uint32_t *data)
{
	return arm_rng_get_random32(data);
}

error_t
platform_get_entropy(platform_prng_data256_t *data)
{
	return arm_rng_get_entropy(data);
}

error_t
platform_get_rng_uuid(uint32_t data[4])
{
	// Gunyah generic RNDR - ARM TRNG interface UUID
	data[0] = ARM_RNG_UUID0;
	data[1] = ARM_RNG_UUID1;
	data[2] = ARM_RNG_UUID2;
	data[3] = ARM_RNG_UUID3;
	return OK;
}
