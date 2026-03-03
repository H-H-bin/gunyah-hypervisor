// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
arm_rng_get_entropy(platform_prng_data256_t *data);

error_t
arm_rng_get_random32(uint32_t *data);

#define ARM_RNG_UUID0 0x45546e21U
#define ARM_RNG_UUID1 0x92a1433dU
#define ARM_RNG_UUID2 0xa2ea5fe2U
#define ARM_RNG_UUID3 0x16397d4eU
