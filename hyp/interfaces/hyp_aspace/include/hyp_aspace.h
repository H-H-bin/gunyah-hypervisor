// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Allocate a contiguous block of virtual memory of at least the specified size.
//
// The size will be rounded up to the allocation granularity, which is typically
// several megabytes. The returned address may be randomised if KASLR is in use
// and should not be assumed to be contiguous with any prior allocations for the
// same partition.
virt_range_result_t
hyp_aspace_allocate(size_t min_size);

// Free a range of virtual memory previously returned by hyp_aspace_allocate(),
// and unmap anything that is mapped in the range.
void
hyp_aspace_unmap_and_deallocate(partition_t *partition,
				virt_range_t virt_range);

// Create a 1:1 mapping of the given physical address range, accessible by the
// hypervisor without calling partition_phys_access_begin.
//
// The partition provided is used to allocate memory for page table levels (if
// required). The same partition must be provided when hyp_aspace_unmap_direct
// is called for the VA range.
//
// This should only be used by platform-specific legacy code that assumes 1:1
// mappings.
error_t
hyp_aspace_map_direct(partition_t *partition, paddr_t phys, size_t size,
		      pgtable_access_t access, pgtable_hyp_memtype_t memtype,
		      vmsa_shareability_t share);

// Remove a mapping created by hyp_aspace_map_direct().
//
// The partition provided is used as the partition to free page table levels
// (if any) to. Must be the same partition used for the VA range.
error_t
hyp_aspace_unmap_direct(partition_t *partition, paddr_t phys, size_t size);

// Check for the existence of any mappings in the hypervisor address space for
// the given range. Note that when the kernel is using ARMv8.1-PAN (or an
// equivalent), there may be mappings in this range which are accessible only
// after calling partition_phys_access_begin(); this function ignores such
// mappings.
lookup_result_t
hyp_aspace_is_mapped(uintptr_t virt, size_t size, pgtable_access_t access);

error_t
hyp_aspace_va_to_pa_el2_read(void *addr, paddr_t *pa, MAIR_ATTR_t *memattr,
			     vmsa_shareability_t *shareability);

error_t
hyp_aspace_va_to_pa_el2_write(void *addr, paddr_t *pa, MAIR_ATTR_t *memattr,
			      vmsa_shareability_t *shareability);

// Return the offset used for the physaccess mappings. For the CPUs that support
// PAN this is a compile-time constant offset, and for the older CPUs it is
// randomised on every boot.
uintptr_t
hyp_aspace_get_physaccess_offset(void);

// Returns the base of the memory used for virtual address allocation
uintptr_t
hyp_aspace_get_alloc_base(void);
