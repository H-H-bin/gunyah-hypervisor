// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <panic.h>
#include <psci.h>
#include <smccc.h>
#include <util.h>

#include "event_handlers.h"

#define SMCCC_VERSION_INIT 0U

static uint32_t smccc_tz_version = SMCCC_VERSION_INIT;

uint32_t
smccc_get_tz_version(void)
{
	assert_safety(smccc_tz_version != SMCCC_VERSION_INIT);

	return smccc_tz_version;
}

void
arm_smccc_handle_boot_cold_init(void)
{
	uint32_t psci_ver = psci_smc_psci_version();

	if (psci_ver == (uint32_t)PSCI_RET_NOT_SUPPORTED) {
		panic("no psci");
	}
	if (psci_ver < 0x10000U) {
		panic("psci_version");
	}

	smccc_function_id_t smccc_version_id = smccc_create_fn_id(
		(smccc_function_t)SMCCC_ARCH_FUNCTION_VERSION,
		SMCCC_OWNER_ID_ARCH, false, true);

	{
		smccc_function_id_t psci_features_id = smccc_create_fn_id(
			(smccc_function_t)PSCI_FUNCTION_PSCI_FEATURES,
			SMCCC_OWNER_ID_STANDARD, false, true);
		uint64_t hyp_args[6] = { 0U };
		uint64_t hyp_ret[4]  = { 0U };

		hyp_args[0] = smccc_function_id_raw(smccc_version_id);

		smccc_1_1_call(psci_features_id, &hyp_args, &hyp_ret, NULL,
			       CLIENT_ID_HYP, false);
		if ((int32_t)hyp_ret[0] != (int32_t)PSCI_RET_SUCCESS) {
			// SMCCC version because we know PSCI_VERSION >= v1.0
			smccc_tz_version = 0x10000U;
			goto out;
		}
	}

	uint64_t hyp_args[6] = { 0U };
	uint64_t hyp_ret[4]  = { 0U };

	// Check the supported SMCCC version now
	smccc_1_1_call(smccc_version_id, &hyp_args, &hyp_ret, NULL,
		       CLIENT_ID_HYP, false);

	if ((int32_t)hyp_ret[0] < 0) {
		smccc_ret_t smccc_ret = smccc_ret_raw_cast((int32_t)hyp_ret[0]);

		if (smccc_ret == SMCCC_RET_NOT_SUPPORTED) {
			smccc_tz_version = 0x10000U;
		} else {
			panic("invalid smccc_version");
		}
	} else {
		assert_safety(hyp_ret[0] < util_bit(32));

		smccc_tz_version = (uint32_t)hyp_ret[0];
	}
out:
	return;
}
