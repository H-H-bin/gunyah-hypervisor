// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Configure a VM's FF-A properties
error_t
ffa_vm_configure(cap_id_t addrspace_cap, uint64_t uuid_lo, uint64_t uuid_hi,
		 uint64_t exec_context_count, uint64_t properties);

error_t
ffa_shutdown_and_cleanup(cap_id_t addrspace_cap) EXCLUDE_PREEMPT_DISABLED;
