// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Add physical memory to a specified partition.
//
// This can be used at runtime to add usable memory that was either not included
// in the ranges returned by platform_get_ram_info() at boot time (if the
// hotplug argument is false), or else was previously removed by a call to
// platform_remove_ram_range() (if the hotplug argument is true). For example,
// it might be useful for hotplugging physical memory, or memory being unprocted
// by a higher security domain.
error_t
partition_add_ram_range(partition_t *owner, paddr_t phys_base, size_t size,
			bool hotplug);

// Remove unused physical memory from a specified partition.
//
// This reverses the effect of platform_add_ram_range(), or of returning a range
// from platform_get_ram_info() at boot time. It will fail if the memory is
// currently in use, e.g. by a hypervisor allocator or a VM memory extent.
//
// If memory is subsequently re-added, partition_add_ram_range() must be called
// with the hotplug argument set to true.
error_t
partition_remove_ram_range(partition_t *owner, paddr_t phys_base, size_t size);

// Allocate memory from a partition
//
// This allocates memory of least the requested size and minimum alignment
// specified and returns a pointer to the start of the allocation.
//
// The given hint can be used to influence the behaviour of the allocator, such
// as the memory pool to use or the method of allocation. Whether a particular
// hint is interpreted or not is dependent on the implementation of the
// allocator.
//
// Note that the memory allocated is uninitialized and it is the caller's
// responsibility to initialize or zero it.
void_ptr_result_t
partition_alloc_ext(partition_t *partition, size_t bytes, size_t min_alignment,
		    allocator_hint_t hint);

// Allocate memory from a partition using the default allocation policy.
//
// This is suitable for most hypervisor internal allocations.
void_ptr_result_t
partition_alloc(partition_t *partition, size_t bytes, size_t min_alignment);

// Free memory back to a partition
//
// The memory must have been allocated from the partition that is it is freed
// back to.
void
partition_free(partition_t *partition, void *mem, size_t bytes);

// Free memory by physical address back to a partition
//
// The memory must have been allocated from the partition that is it is freed
// back to.
void
partition_free_phys(partition_t *partition, paddr_t phys, size_t bytes);

// Get the hypervisor's private partition.
//
// This is used for allocating objects that are owned by the hypervisor itself
// and used internally, such as the idle threads.
partition_t *
partition_get_private(void);

// Get the physical address of an object owned by this partition.
//
// This is used by a limited set of internal hypervisor functions that need to
// allocate physical memory for their own use, e.g. for page table levels.
//
// The specified address must have been returned by a partition_alloc() call
// for the specified partition, and not subsequently freed; the result is not
// guaranteed to be meaningful otherwise. The caller must not assume that
// hypervisor memory is physically contiguous beyond the range allocated by that
// partition_alloc() call.
paddr_t
partition_virt_to_phys(partition_t *partition, uintptr_t addr);

// Check whether the physical address range is valid, i.e. associated to any
// partition.
//
// The physical address must be normal memory owned by a partition, but it is
// not necessary to specify the partition.
bool
partition_phys_valid(paddr_t paddr, size_t size);

// Obtain a virtual address that may be used to access a specified physical
// address.
//
// The physical address must be normal memory owned by a partition, but it is
// not necessary to specify the partition.
//
// The returned virtual address must only be accessed between calls to
// partition_phys_access_enable() and partition_phys_access_disable().
//
// This function must be paired with a call to partition_phys_unmap(). Pairs of
// calls to these functions may be nested; there may be a limit on the number of
// levels of nesting, but it shall be large enough to simultaneously access
// every level of the largest page table supported by the platform.
//
// Some implementations of this function may need to construct mappings at
// runtime (and unmap them afterwards), and therefore may be relatively slow.
// Where possible, multiple accesses to one memory region should be batched to
// minimise calls to this function.
void *
partition_phys_map(paddr_t paddr, size_t size);

// Enable accesses to a physical address mapped with partition_phys_map().
//
// The pointer should point to the specific object that will be accessed.
//
// This function must be paired with a call to partition_phys_access_disable()
// with the same address argument. Pairs of calls to these functions must not be
// nested.
//
// This function should be fast enough to call before each individual access. If
// this is not possible, it must be a no-op, and all work necessary to enable
// access to the mapping must be performed by partition_phys_map().
void
partition_phys_access_enable(const void *ptr);

// Disable accesses to a physical address mapped with partition_phys_map().
//
// The pointer should point to the specific object that was accessed.
//
// This function should be fast enough to call after each individual access. If
// this is not possible, it should be a no-op.
void
partition_phys_access_disable(const void *ptr);

// Release a virtual address returned by partition_phys_unmap().
//
// The virtual address must not be accessed after calling this function.
void
partition_phys_unmap(const void *vaddr, paddr_t paddr, size_t size);

// Donate memory from partition
//
// Update address range ownership in the memory database from src_partition to
// dst_partition.
//
// If from_heap is false, the specified range must be unused memory owned by the
// source partition.
//
// If from_heap is true, the specified range must have been returned by a call
// to partition_alloc() with the specified source partition. This is useful when
// allocating a new partition during bootstrap, from within the hypervisor.
error_t
partition_mem_donate(partition_t *src_partition, paddr_t base, size_t size,
		     partition_t *dst_partition, bool from_heap);

// Add mapped memory to the partition's allocator.
//
// The memory must have been allocated from the partition that is it is freed
// back to.
error_t
partition_add_heap(partition_t *partition, paddr_t base, size_t size);

// Map range and add memory to the partition's allocator.
//
// The memory must have been allocated from the partition that is it is freed
// back to. The given attributes can be used to specify the type of allocations
// that the memory is capable of fulfilling.
uintptr_result_t
partition_map_and_add_heap_ext(partition_t *partition, paddr_t phys,
			       size_t size, allocator_memattr_t attr);

// Map range and add memory to the partition's allocator with default memory
// attributes.
//
// This is suitable for heap memory intended for standard hypervisor
// allocations.
error_t
partition_map_and_add_heap(partition_t *partition, paddr_t base, size_t size);

#if defined(PLATFORM_TRACE_STANDALONE_REGION) &&                               \
	PLATFORM_TRACE_STANDALONE_REGION
// Map range and add memory to the partition's trace area
//
// The memory must have been allocated from the partition that is it is freed
// back to.
uintptr_result_t
partition_map_and_add_trace(partition_t *partition, paddr_t base, size_t size);
#endif
