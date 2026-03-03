// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(HYPERCALLS)
#include <hyptypes.h>

#include <hypcall_def.h>

#include <platform_security.h>
#include <trace.h>
#include <util.h>

#include "trace_internal.h"

hypercall_trace_configure_result_t
hypercall_trace_configure(register_t arg1, register_t arg2,
			  trace_configure_parameter_t param)
{
	hypercall_trace_configure_result_t res = { 0 };

	if (platform_security_state_debug_disabled()) {
		res.error = ERROR_DENIED;
		goto out;
	}

	switch (param) {
	case TRACE_CONFIGURE_PARAMETER_CLASS_FLAGS: {
		register_t set_flags   = arg1;
		register_t clear_flags = arg2;

		// Clear the bits that hypercalls are not allowed to change
		register_t set	 = set_flags & trace_public_class_flags;
		register_t clear = clear_flags & trace_public_class_flags;

		trace_update_class_flags(set, clear);

		res.ret	  = trace_get_class_flags();
		res.error = OK;
		break;
	}
	case TRACE_CONFIGURE_PARAMETER_NOTIFY_ENABLE: {
		register_t notify_enable = arg1;
		if (arg2 != 0U) {
			res.error = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		if (notify_enable == (register_t)0U) {
			res.error = trace_update_notify_mask(false);
		} else if (notify_enable == (register_t)1U) {
			res.error = trace_update_notify_mask(true);
		} else {
			res.error = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	default:
		res.error = ERROR_ARGUMENT_INVALID;
		break;
	}

out:
	return res;
}
#else
extern int unused;
#endif
