// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Get the TrustZone version information.
//
// Returns the TrustZone version as a 32-bit value. This function queries
// the secure world to determine the version of the TrustZone implementation.
uint32_t
smccc_get_tz_version(void);

// Issue an SMCCC v1.1 compliant SMC call to EL3.
//
// This function performs a Secure Monitor Call (SMC) using the SMCCC v1.1
// specification. It supports up to 6 input arguments and returns up to 4
// output values along with session information.
//
// Arguments:
//   fn_id: SMCCC function identifier to be passed in w0/x0
//   args: pointer to array of 6 input arguments for x1-x6
//   ret: pointer to array for 4 returned register values x0-x3
//   session_ret: pointer to store session-specific return value
//   client_id: identifier for the calling client (passed in w7)
//   is_caller_fast: Avoid checking for wakeups in the caller
void
smccc_1_1_call(smccc_function_id_t fn_id,
	       const uint64_t (*args)[SMCCC_1_1_ARGS],
	       uint64_t (*ret)[SMCCC_1_1_RETS], uint64_t *session_ret,
	       uint32_t client_id, bool is_caller_fast);

// Issue an SMCCC v1.2 compliant SMC call to EL3.
//
// This function performs a Secure Monitor Call (SMC) using the SMCCC v1.2
// specification. It supports up to 17 input arguments and returns up to 18
// output values.
//
// Arguments:
//   fn_id: SMCCC function identifier to be passed in w0/x0
//   args: pointer to array of 17 input arguments for x1-x17
//   ret: pointer to array for 18 returned register values x0-x17
//   force_interruptible: overrides standard SMCCC assumptions about fastcalls
//                        being atomic and non-interruptible. Primarily used
//                        for FF-A (Firmware Framework for Arm A-profile) calls
void
smccc_1_2_call(smccc_function_id_t fn_id,
	       const uint64_t (*args)[SMCCC_1_2_ARGS],
	       uint64_t (*ret)[SMCCC_1_2_RETS], bool force_interruptible);

// Create an SMCCC function identifier.
//
// This helper function constructs a properly formatted SMCCC function ID
// by combining the function number, owner ID, calling convention, and
// call type into a single identifier value.
//
// Arguments:
//   function: the specific function number within the owner's namespace
//   owner_id: identifies the owner/service provider of the function
//   is_smc64: true for 64-bit calling convention, false for 32-bit
//   is_fast: true for fast calls (atomic), false for yielding calls
//
// Returns: properly formatted SMCCC function identifier
smccc_function_id_t
smccc_create_fn_id(smccc_function_t function, smccc_owner_id_t owner_id,
		   bool is_smc64, bool is_fast);
