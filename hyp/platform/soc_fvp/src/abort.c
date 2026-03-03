// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <qcbor.h>
#include <smccc.h>

#include "event_handlers.h"

void
soc_fvp_handle_power_system_off(void)
{
	uint64_t hyp_args[6] = { 0 };
	uint64_t hyp_ret[4]  = { 0 };

	smccc_function_id_t fn_id = smccc_create_fn_id(
		PSCI_FUNCTION_SYSTEM_OFF, SMCCC_OWNER_ID_STANDARD, false, true);

	smccc_1_1_call(fn_id, &hyp_args, &hyp_ret, NULL, CLIENT_ID_HYP, false);
}

bool
soc_fvp_handle_power_system_reset(uint64_t reset_type, uint64_t cookie,
				  error_t *error)
{
	(void)reset_type;
	(void)cookie;

	*error = ERROR_UNIMPLEMENTED;

	return true;
}
