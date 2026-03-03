// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

paddr_result_t
memextent_memdb_walk_map(memextent_t *memextent, addrspace_t *addrspace,
			 vmaddr_t vbase, paddr_t pbase, size_t size,
			 pgtable_vm_memtype_t  memtype,
			 pgtable_access_t      user_access,
			 pgtable_access_t      kernel_access,
			 addrspace_map_flags_t map_flags)
	REQUIRE_ADDRSPACE_LOCK(addrspace);

error_t
memextent_memdb_walk_unmap(memextent_t *memextent, addrspace_t *addrspace,
			   vmaddr_t vbase, paddr_t pbase, size_t size,
			   addrspace_map_flags_t map_flags)
	REQUIRE_ADDRSPACE_LOCK(addrspace);
