// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifdef __EVENTS_DSL__
#define acquire_addrspace_lock(addrspace)                                      \
	acquire_lock addrspace->vm_pgtable;                                    \
	acquire_lock pgtable_vm_map_lock
#define release_addrspace_lock(addrspace)                                      \
	release_lock addrspace->vm_pgtable;                                    \
	release_lock pgtable_vm_map_lock
#define require_addrspace_lock(addrspace)                                      \
	require_lock addrspace->vm_pgtable;                                    \
	require_lock pgtable_vm_map_lock
#define exclude_addrspace_lock(addrspace) exclude_lock addrspace->vm_pgtable
#else
#define ACQUIRE_ADDRSPACE_LOCK(addrspace)                                      \
	ACQUIRE_LOCK(addrspace->vm_pgtable) ACQUIRE_LOCK(pgtable_vm_map_lock)
#define RELEASE_ADDRSPACE_LOCK(addrspace)                                      \
	RELEASE_LOCK(addrspace->vm_pgtable) RELEASE_LOCK(pgtable_vm_map_lock)
#define REQUIRE_ADDRSPACE_LOCK(addrspace)                                      \
	REQUIRE_LOCK(addrspace->vm_pgtable) REQUIRE_LOCK(pgtable_vm_map_lock)
#define EXCLUDE_ADDRSPACE_LOCK(addrspace) EXCLUDE_LOCK(addrspace->vm_pgtable)
#endif
