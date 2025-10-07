// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <allocator.h>
#include <compiler.h>
#include <hyp_aspace.h>
#include <log.h>
#include <memdb.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <pgtable.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>

#include <events/allocator.h>
#include <events/partition.h>

#include "event_handlers.h"

void_ptr_result_t NOINLINE
partition_alloc_ext(partition_t *partition, size_t bytes, size_t min_alignment,
		    allocator_hint_t hint)
{
	void_ptr_result_t ret;

	assert(bytes > 0U);

	ret = allocator_allocate_object(&partition->allocator, bytes,
					min_alignment, hint);

	if (compiler_expected(ret.e == OK)) {
		assert(ret.r != NULL);
	}
	return ret;
}

void_ptr_result_t
partition_alloc(partition_t *partition, size_t bytes, size_t min_alignment)
{
	return partition_alloc_ext(partition, bytes, min_alignment,
				   allocator_hint_default());
}

// FIXME: partition->mapped_ranges is not updated atomically. Its not an issue
// yet since its only done during single-threaded init. Once we support dynamic
// heap adjustment, it will become a problem.
// FIXME:

// FIXME:
static paddr_t
virt_to_phys(partition_t *partition, uintptr_t addr, allocator_memattr_t *attr)
{
	paddr_t phys = PADDR_INVALID;

	for (count_t i = 0U; i < util_array_size(partition->mapped_ranges);
	     i++) {
		partition_mapped_range_t *mr = &partition->mapped_ranges[i];
		if (mr->size == 0U) {
			continue;
		}
		if ((addr >= mr->virt) &&
		    (addr <= (mr->virt + (mr->size - 1U)))) {
			phys = (paddr_t)(addr - mr->virt) + mr->phys;
			if (attr != NULL) {
				*attr = mr->attr;
			}
			break;
		}
	}

	return phys;
}

static uintptr_t
phys_to_virt(partition_t *partition, paddr_t phys, size_t size,
	     allocator_memattr_t *attr)
{
	uintptr_t virt = VADDR_INVALID;

	assert(!util_add_overflows(phys, size - 1U));

	for (count_t i = 0U; i < util_array_size(partition->mapped_ranges);
	     i++) {
		partition_mapped_range_t *mr = &partition->mapped_ranges[i];
		if (mr->size == 0U) {
			continue;
		}
		if ((phys >= mr->phys) &&
		    ((phys + (size - 1U)) <= (mr->phys + (mr->size - 1U)))) {
			virt = (uintptr_t)(phys - mr->phys) + mr->virt;
			if (attr != NULL) {
				*attr = mr->attr;
			}
			break;
		}
	}

	return virt;
}

void
partition_free(partition_t *partition, void *mem, size_t bytes)
{
	assert((bytes > 0U) && !util_add_overflows((uintptr_t)mem, bytes - 1U));

	allocator_memattr_t attr;
	if (virt_to_phys(partition, (uintptr_t)mem, &attr) == PADDR_INVALID) {
		panic("Attempt to free memory not in partition");
	}

	allocator_deallocate_object(&partition->allocator, mem, bytes, attr);
}

void
partition_free_phys(partition_t *partition, paddr_t phys, size_t bytes)
{
	allocator_memattr_t attr;
	uintptr_t	    virt = phys_to_virt(partition, phys, bytes, &attr);

	if (virt == VADDR_INVALID) {
		panic("Attempt to free memory not in partition");
	}

	assert((bytes > 0U) && !util_add_overflows(virt, bytes - 1U));

	allocator_deallocate_object(&partition->allocator, (void *)virt, bytes,
				    attr);
}

paddr_t
partition_virt_to_phys(partition_t *partition, uintptr_t addr)
{
	return virt_to_phys(partition, addr, NULL);
}

error_t
partition_standard_handle_object_create_partition(
	partition_create_t partition_create)
{
	partition_t *partition = partition_create.partition;
	assert(partition != NULL);

	return allocator_init(&partition->allocator);
}

error_t
partition_standard_handle_object_activate_partition(partition_t *partition)
{
	error_t err;

	assert(partition->header.partition != NULL);
	assert(partition->header.partition != partition);

	if (partition_option_flags_get_privileged(&partition->options) &&
	    !partition_option_flags_get_privileged(
		    &partition->header.partition->options)) {
		err = ERROR_DENIED;
		goto out;
	}

	// Partitions hold a reference to themselves to prevent asynchronous
	// destruction when the last capability is deleted.
	//
	// Partitions must be explicitly destroyed to ensure that all objects in
	// them are deactivated synchronously, especially threads which might
	// still be executing on other CPUs; this self-reference will be deleted
	// after that is done. This destruction operation is not yet
	// implemented.
	(void)object_get_partition_additional(partition);

	err = OK;
out:
	return err;
}

noreturn void
partition_standard_handle_object_deactivate_partition(void)
{
	// This is currently not implemented and not needed. The self-reference
	// taken in activate() above should prevent this, but we panic here to
	// ensure that it doesn't happen by accident.
	panic("Partition deactivation attempted");
}

error_t
partition_mem_donate(partition_t *src_partition, paddr_t base, size_t size,
		     partition_t *dst_partition, bool from_heap)
{
	error_t ret;

	partition_t *hyp_partition = partition_get_private();

	if ((size != 0U) && (!util_add_overflows(base, size - 1U))) {
		if (from_heap) {
			ret = memdb_update(hyp_partition, base,
					   base + (size - 1U),
					   (uintptr_t)dst_partition,
					   MEMDB_TYPE_PARTITION,
					   (uintptr_t)&src_partition->allocator,
					   MEMDB_TYPE_ALLOCATOR);
		} else {
			ret = memdb_update(
				hyp_partition, base, base + (size - 1U),
				(uintptr_t)dst_partition, MEMDB_TYPE_PARTITION,
				(uintptr_t)src_partition, MEMDB_TYPE_PARTITION);
		}
	} else {
		ret = ERROR_ARGUMENT_SIZE;
	}

	return ret;
}

error_t
partition_add_heap(partition_t *partition, paddr_t base, size_t size)
{
	error_t ret;

	assert(partition != NULL);
	assert(size != 0U);

	partition_t *hyp_partition = partition_get_private();

	if ((size != 0U) && (!util_add_overflows(base, size - 1U))) {
		ret = memdb_update(hyp_partition, base, base + (size - 1U),
				   (uintptr_t)&partition->allocator,
				   MEMDB_TYPE_ALLOCATOR, (uintptr_t)partition,
				   MEMDB_TYPE_PARTITION);
	} else {
		ret = ERROR_ARGUMENT_SIZE;
	}

	if (ret == OK) {
		allocator_memattr_t attr;
		uintptr_t virt = phys_to_virt(partition, base, size, &attr);
		assert(virt != VADDR_INVALID);
		ret = trigger_allocator_add_ram_range_event(partition, base,
							    virt, size, attr);
	}

	return ret;
}

static uintptr_result_t
new_memory_add(partition_t *partition, partition_t *hyp_partition, paddr_t phys,
	       size_t size, allocator_memattr_t attr)
{
	uintptr_result_t ret;
	error_t		 err;

	partition_mapped_range_t *mr = NULL;
	for (count_t i = 0U; i < util_array_size(partition->mapped_ranges);
	     i++) {
		if (partition->mapped_ranges[i].size == 0U) {
			mr = &partition->mapped_ranges[i];
			break;
		}
	}

	if (mr == NULL) {
		ret = uintptr_result_error(ERROR_NORESOURCES);
		goto out;
	}

	// Use large page size for virt-phys alignment.
	paddr_t phys_align_base =
		util_balign_down(phys, PGTABLE_HYP_LARGE_PAGE_SIZE);
	size_t phys_align_offset = phys - phys_align_base;
	size_t phys_align_size	 = phys_align_offset + size;

	virt_range_result_t vr = hyp_aspace_allocate(phys_align_size);
	if (vr.e != OK) {
		ret = uintptr_result_error(vr.e);
		goto out;
	}

	uintptr_t virt = vr.r.base + phys_align_offset;

	pgtable_hyp_start();
	// FIXME:
	err = pgtable_hyp_map(hyp_partition, virt, size, phys,
			      PGTABLE_HYP_MEMTYPE_WRITEBACK, PGTABLE_ACCESS_RW,
			      VMSA_SHAREABILITY_INNER_SHAREABLE);
	pgtable_hyp_commit();
	if (err == OK) {
		err = trigger_allocator_add_ram_range_event(partition, phys,
							    virt, size, attr);
	}
	if (err != OK) {
		// FIXME:
		// This should unmap the failed range, freeing to the target
		// partition and preserve the levels that were preallocated,
		// followed by unmapping the preserved tables (if they are
		// empty), freeing to the hyp_partition.
		pgtable_hyp_start();
		pgtable_hyp_unmap(hyp_partition, virt, size,
				  PGTABLE_HYP_UNMAP_PRESERVE_NONE);
		pgtable_hyp_commit();
		hyp_aspace_deallocate(partition, vr.r);
		ret = uintptr_result_error(err);
	} else {
		mr->virt = virt;
		mr->phys = phys;
		mr->size = size;
		// FIXME:
		mr->attr = attr;

		LOG(DEBUG, INFO,
		    "added heap: partition {:#x}, virt {:#x}, phys {:#x}, size {:#x}",
		    (uintptr_t)partition, virt, phys, size);
		ret = uintptr_result_ok(virt);
	}

out:
	return ret;
}

uintptr_result_t
partition_map_and_add_heap_ext(partition_t *partition, paddr_t phys,
			       size_t size, allocator_memattr_t attr)
{
	uintptr_result_t ret;
	error_t		 err;

	assert(partition != NULL);
	assert(size != 0U);

	// This should not be called for memory already mapped.
	if (phys_to_virt(partition, phys, size, NULL) != VADDR_INVALID) {
		panic("Attempt to add memory already in partition");
	}

	// FIXME:
	// Mapping the partition should preallocate top page-table levels from
	// the hyp partition and then map with the target partition, but we
	// have a chicken-and-egg problem to solve: if the target partition has
	// no memory yet (because it is new) then it can't allocate page
	// tables. We will probably need to seed new partition allocators with
	// some memory from the parent partition.
	partition_t *hyp_partition = partition_get_private();

	if ((size == 0U) || (util_add_overflows(phys, size - 1U))) {
		ret = uintptr_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = uintptr_result_error(ERROR_ARGUMENT_ALIGNMENT);
		goto out;
	}

	err = memdb_update(hyp_partition, phys, phys + (size - 1U),
			   (uintptr_t)&partition->allocator,
			   MEMDB_TYPE_ALLOCATOR, (uintptr_t)partition,
			   MEMDB_TYPE_PARTITION);
	if (err != OK) {
		ret = uintptr_result_error(err);
		goto out;
	}

	spinlock_acquire(&partition->header.lock);

	// Add a new mapped range for the memory.
	ret = new_memory_add(partition, hyp_partition, phys, size, attr);

	spinlock_release(&partition->header.lock);

	if (ret.e != OK) {
		err = memdb_update(hyp_partition, phys, phys + (size - 1U),
				   (uintptr_t)partition, MEMDB_TYPE_PARTITION,
				   (uintptr_t)&partition->allocator,
				   MEMDB_TYPE_ALLOCATOR);
		if (err != OK) {
			panic("Error updating memdb.");
		}
	}
out:
	return ret;
}

error_t
partition_map_and_add_heap(partition_t *partition, paddr_t base, size_t size)
{
	return partition_map_and_add_heap_ext(partition, base, size,
					      allocator_memattr_default())
		.e;
}

#if defined(PLATFORM_TRACE_STANDALONE_REGION) &&                               \
	PLATFORM_TRACE_STANDALONE_REGION

static error_t
new_memory_add_trace(partition_t *partition, paddr_t phys, size_t size,
		     partition_mapped_range_t **mr, uintptr_result_t *virt_ret)
{
	error_t	  ret = OK;
	uintptr_t virt;

	for (count_t i = 0U; i < util_array_size(partition->mapped_ranges);
	     i++) {
		if (partition->mapped_ranges[i].size == 0U) {
			*mr = &partition->mapped_ranges[i];
			break;
		}
	}

	if (*mr == NULL) {
		ret = ERROR_NORESOURCES;
		goto out;
	}

	// Use large page size for virt-phys alignment.
	paddr_t phys_align_base =
		util_balign_down(phys, PGTABLE_HYP_LARGE_PAGE_SIZE);
	size_t phys_align_offset = phys - phys_align_base;
	size_t phys_align_size	 = phys_align_offset + size;

	virt_range_result_t vr = hyp_aspace_allocate(phys_align_size);
	if (vr.e != OK) {
		ret = vr.e;
		goto out;
	}

	virt	    = vr.r.base + phys_align_offset;
	(*mr)->virt = virt;
	(*mr)->phys = phys;
	(*mr)->size = size;

	pgtable_hyp_start();
	ret = pgtable_hyp_map(partition, virt, size, phys,
			      PGTABLE_HYP_MEMTYPE_WRITEBACK, PGTABLE_ACCESS_RW,
			      VMSA_SHAREABILITY_INNER_SHAREABLE);
	if (ret == OK) {
		pgtable_hyp_commit();
	} else {
		pgtable_hyp_unmap(partition, virt, size,
				  PGTABLE_HYP_UNMAP_PRESERVE_NONE);
		pgtable_hyp_commit();
		hyp_aspace_deallocate(partition, vr.r);
	}
	if (ret == OK) {
		(*virt_ret).r = virt;
		LOG(DEBUG, INFO,
		    "added trace: partition {:#x}, virt {:#x}, phys {:#x}, size {:#x}",
		    (uintptr_t)partition, (*virt_ret).r, phys, size);
	}

out:
	return ret;
}

uintptr_result_t
partition_map_and_add_trace(partition_t *partition, paddr_t phys, size_t size)
{
	error_t		 ret;
	error_t		 err	  = OK;
	uintptr_result_t virt_ret = { 0 };

	assert(partition != NULL);
	assert(size != 0U);

	if ((size == 0U) || (util_add_overflows(phys, size - 1U))) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = ERROR_ARGUMENT_ALIGNMENT;
		goto out;
	}

	partition_t *hyp_partition = partition_get_private();
	ret = memdb_update(hyp_partition, phys, phys + (size - 1U),
			   (uintptr_t)NULL, MEMDB_TYPE_TRACE,
			   (uintptr_t)partition, MEMDB_TYPE_PARTITION);
	if (ret != OK) {
		goto out;
	}

	// Add a new mapped range for the memory.
	partition_mapped_range_t *mr = NULL;

	ret = new_memory_add_trace(partition, phys, size, &mr, &virt_ret);

	if (ret != OK) {
		err = memdb_update(hyp_partition, phys, phys + (size - 1U),
				   (uintptr_t)partition, MEMDB_TYPE_PARTITION,
				   (uintptr_t)NULL, MEMDB_TYPE_TRACE);
		if (err != OK) {
			panic("Error updating memdb.");
		}

		if (mr != NULL) {
			mr->virt = 0U;
			mr->phys = 0U;
			mr->size = 0U;
		}
	}
out:
	virt_ret.e = ret;
	return virt_ret;
}
#endif

error_t
partition_add_ram_range(partition_t *owner, paddr_t phys_base, size_t size,
			bool hotplug)
{
	return trigger_partition_add_ram_range_event(owner, phys_base, size,
						     hotplug);
}

error_t
partition_remove_ram_range(partition_t *owner, paddr_t phys_base, size_t size)
{
	return trigger_partition_remove_ram_range_event(owner, phys_base, size);
}
