// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <compiler.h>
#include <trace.h>

#include "event_handlers.h"

void
trace_profile_handle_thread_entry_from_user_trace(thread_entry_reason_t reason)
{
	TRACE_PROFILE(1, 0U, PROFILE_EL2_ENTRY,
		      "thread entry from user, reason {:d}",
		      (register_t)reason);
}

void
trace_profile_handle_thread_exit_to_user_trace(thread_entry_reason_t reason)
{
	TRACE_PROFILE(1, 0U, PROFILE_EL2_EXIT,
		      "thread exit to user, reason {:d}", (register_t)reason);
}

bool
trace_profile_handle_ipi_received_trace(ipi_reason_t reason)
{
	TRACE_PROFILE(2, 0U, INFO, "ipi received, reason {:d}",
		      (register_t)reason);
	return false;
}
