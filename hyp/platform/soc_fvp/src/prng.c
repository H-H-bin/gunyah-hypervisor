// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

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

error_t NOINLINE
platform_get_entropy(platform_prng_data256_t *data)
{
	// XXX
	(void)data;
	return OK;
}

error_t NOINLINE
platform_get_random32(uint32_t *data)
{
	// XXX
	(void)data;
	return OK;
}

error_t
platform_get_rng_uuid(uint32_t data[4])
{
	// uuidgen -s -n @oid -N Gunyah/fvp/prng
	// dfb5f34a-a451-5ddb-aed0-dadd9091970a
	data[0] = 0x4af3b5dfU;
	data[1] = 0xdb5d51a4U;
	data[2] = 0xdddad0aeU;
	data[3] = 0x0a979190U;

	return OK;
}
