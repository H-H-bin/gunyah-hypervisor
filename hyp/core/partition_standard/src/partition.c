// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcontainers.h>

#include <allocator.h>
#include <atomic.h>
#include <compiler.h>
#include <hyp_aspace.h>
#include <list.h>
#include <log.h>
#include <memdb.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <pgtable.h>
#include <rcu.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>

#include <events/allocator.h>
#include <events/partition.h>

#include <asm/cache.h>
#include <asm/cpu.h>

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

// FIXME: QC Gunyah issue #245
static paddr_t
virt_to_phys(partition_t *partition, uintptr_t addr, allocator_memattr_t *attr)
{
	paddr_t phys = PADDR_INVALID;

	rcu_read_start();
	LIST_FOREACH_CONTAINER_CONSUME_BEGIN(partition_mapped_range_t,
					     &partition->mapped_ranges,
					     partition_mapped_range, list_node,
					     mr)
		if ((addr >= mr->virt) &&
		    (addr <= (mr->virt + (mr->size - 1U)))) {
			phys = (paddr_t)(addr - mr->virt) + mr->phys;
			if (attr != NULL) {
				*attr = mr->attr;
			}
			break;
		}
	LIST_FOREACH_CONTAINER_CONSUME_END
	rcu_read_finish();

	return phys;
}

static uintptr_t
phys_to_virt(partition_t *partition, paddr_t phys, size_t size,
	     allocator_memattr_t *attr)
{
	uintptr_t virt = VADDR_INVALID;

	assert(!util_add_overflows(phys, size - 1U));

	rcu_read_start();
	LIST_FOREACH_CONTAINER_CONSUME_BEGIN(partition_mapped_range_t,
					     &partition->mapped_ranges,
					     partition_mapped_range, list_node,
					     mr)
		if ((phys >= mr->phys) &&
		    ((phys + (size - 1U)) <= (mr->phys + (mr->size - 1U)))) {
			virt = (uintptr_t)(phys - mr->phys) + mr->virt;
			if (attr != NULL) {
				*attr = mr->attr;
			}
			break;
		}
	LIST_FOREACH_CONTAINER_CONSUME_END
	rcu_read_finish();

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

	list_init(&partition->mapped_ranges);

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

	if (src_partition == dst_partition) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	if ((size == 0U) || util_add_overflows(base, size - 1U)) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	partition_t *hyp_partition = partition_get_private();
	if (from_heap) {
		ret = memdb_update(hyp_partition, base, base + (size - 1U),
				   (uintptr_t)dst_partition,
				   MEMDB_TYPE_PARTITION,
				   (uintptr_t)&src_partition->allocator,
				   MEMDB_TYPE_ALLOCATOR);
	} else {
		ret = memdb_update(hyp_partition, base, base + (size - 1U),
				   (uintptr_t)dst_partition,
				   MEMDB_TYPE_PARTITION,
				   (uintptr_t)src_partition,
				   MEMDB_TYPE_PARTITION);
	}

out:
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
	       size_t size, allocator_memattr_t attr, bool is_heap)
{
	uintptr_result_t ret;
	error_t		 err;

	spinlock_acquire(&partition->header.lock);

	if (partition->mapped_count == PARTITION_MAX_MAPPED_RANGE_COUNT) {
		ret = uintptr_result_error(ERROR_DENIED);
		goto out;
	}

	// FIXME: QC Gunyah issue #74
	partition_mapped_range_t *mr = NULL;
	void_ptr_result_t	  alloc_ret =
		partition_alloc(hyp_partition, sizeof(*mr), alignof(*mr));
	if (alloc_ret.e != OK) {
		ret = uintptr_result_error(alloc_ret.e);
		goto out;
	}

	(void)memset_s(alloc_ret.r, sizeof(*mr), 0, sizeof(*mr));

	// Use large page size for virt-phys alignment.
	paddr_t phys_align_base =
		util_balign_down(phys, PGTABLE_HYP_LARGE_PAGE_SIZE);
	size_t phys_align_offset = phys - phys_align_base;
	size_t phys_align_size	 = phys_align_offset + size;

	virt_range_result_t vr = hyp_aspace_allocate(phys_align_size);
	if (vr.e != OK) {
		partition_free(hyp_partition, alloc_ret.r, sizeof(*mr));
		ret = uintptr_result_error(vr.e);
		goto out;
	}

	uintptr_t virt = vr.r.base + phys_align_offset;

	pgtable_hyp_start();
	// FIXME: QC Gunyah issue #74
	err = pgtable_hyp_map(hyp_partition, virt, size, phys,
			      PGTABLE_HYP_MEMTYPE_WRITEBACK, PGTABLE_ACCESS_RW,
			      VMSA_SHAREABILITY_INNER_SHAREABLE);
	pgtable_hyp_commit();
	if ((err == OK) && is_heap) {
		err = trigger_allocator_add_ram_range_event(partition, phys,
							    virt, size, attr);
	}
	if (err != OK) {
		// FIXME: QC Gunyah issue #74
		hyp_aspace_unmap_and_deallocate(hyp_partition, vr.r);
		partition_free(hyp_partition, alloc_ret.r, sizeof(*mr));
		ret = uintptr_result_error(err);
	} else {
		mr = (partition_mapped_range_t *)alloc_ret.r;

		mr->virt	= virt;
		mr->phys	= phys;
		mr->size	= size;
		mr->alloc_range = vr.r;
		// FIXME: QC Gunyah issue #245
		mr->attr    = attr;
		mr->is_heap = is_heap;

		list_insert_at_tail_release(&partition->mapped_ranges,
					    &mr->list_node);
		partition->mapped_count++;

		ret = uintptr_result_ok(virt);
	}

out:
	spinlock_release(&partition->header.lock);

	return ret;
}

uintptr_result_t
partition_map_and_add_heap_ext(partition_t *partition, paddr_t phys,
			       size_t size, allocator_memattr_t attr)
{
	uintptr_result_t ret;
	error_t		 err;

	assert(partition != NULL);

	if ((size == 0U) || util_add_overflows(phys, size - 1U)) {
		ret = uintptr_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = uintptr_result_error(ERROR_ARGUMENT_ALIGNMENT);
		goto out;
	}

	// This should not be called for memory already mapped.
	if (phys_to_virt(partition, phys, size, NULL) != VADDR_INVALID) {
		ret = uintptr_result_error(ERROR_BUSY);
		goto out;
	}

	// FIXME: QC Gunyah issue #74
	// Mapping the partition should preallocate top page-table levels from
	// the hyp partition and then map with the target partition, but we
	// have a chicken-and-egg problem to solve: if the target partition has
	// no memory yet (because it is new) then it can't allocate page
	// tables. We will probably need to seed new partition allocators with
	// some memory from the parent partition.
	partition_t *hyp_partition = partition_get_private();

	err = memdb_update(hyp_partition, phys, phys + (size - 1U),
			   (uintptr_t)&partition->allocator,
			   MEMDB_TYPE_ALLOCATOR, (uintptr_t)partition,
			   MEMDB_TYPE_PARTITION);
	if (err != OK) {
		ret = uintptr_result_error(err);
		goto out;
	}

	// Add a new mapped range for the memory.
	ret = new_memory_add(partition, hyp_partition, phys, size, attr, true);
	if (ret.e == OK) {
		LOG(DEBUG, INFO,
		    "added heap: partition {:#x}, virt {:#x}, phys {:#x}, size {:#x}",
		    (uintptr_t)partition, ret.r, phys, size);
	} else {
		err = memdb_update(hyp_partition, phys, phys + (size - 1U),
				   (uintptr_t)partition, MEMDB_TYPE_PARTITION,
				   (uintptr_t)&partition->allocator,
				   MEMDB_TYPE_ALLOCATOR);
		if (err != OK) {
			panic("memdb_update");
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

rcu_update_status_t
partition_standard_free_mapped_range(rcu_entry_t *entry)
{
	partition_mapped_range_t *mr =
		partition_mapped_range_container_of_rcu_entry(entry);

	// FIXME: QC Gunyah issue #74
	partition_t *hyp_partition = partition_get_private();
	hyp_aspace_unmap_and_deallocate(hyp_partition, mr->alloc_range);
	partition_free(hyp_partition, mr, sizeof(*mr));

	return rcu_update_status_default();
}

error_t
partition_unmap_and_remove_heap(partition_t *partition, paddr_t phys,
				size_t size)
{
	error_t	     ret;
	partition_t *hyp_partition = partition_get_private();

	assert(partition != NULL);
	assert(hyp_partition != NULL);

	if ((size == 0U) || (util_add_overflows(phys, size - 1U))) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = ERROR_ARGUMENT_ALIGNMENT;
		goto out;
	}

	spinlock_acquire(&partition->header.lock);

	// Find the mapped range matching the given heap region. We only support
	// removing the entire mapped range.
	partition_mapped_range_t *mr = NULL;
	LIST_FOREACH_CONTAINER_BEGIN(partition_mapped_range_t,
				     &partition->mapped_ranges,
				     partition_mapped_range, list_node, node)
		if ((node->phys == phys) && (node->size == size)) {
			mr = node;
			break;
		}
	LIST_FOREACH_CONTAINER_END

	if ((mr == NULL) || !mr->is_heap) {
		spinlock_release(&partition->header.lock);
		ret = ERROR_DENIED;
		goto out;
	}

	uintptr_t	    virt = mr->virt;
	allocator_memattr_t attr = mr->attr;

	// Detach the mapped range from the partition.
	ret = trigger_allocator_remove_ram_range_event(partition, phys, virt,
						       size, attr);
	if (ret == OK) {
		(void)list_delete_node(&partition->mapped_ranges,
				       &mr->list_node);
		partition->mapped_count--;
	}

	// Once detached, it is safe to perform the rest of the cleanup without
	// the partition lock.
	spinlock_release(&partition->header.lock);

	if (ret != OK) {
		goto out;
	}

	// Sanitize the memory before unmapping and freeing.
	(void)memset_s((void *)virt, size, 0, size);
	CACHE_CLEAN_INVALIDATE_RANGE(virt, size);

	// Although the mapped range may still be accessed during virt-to-phys
	// translations, if we successfully removed the range from the allocator
	// then there should be no objects actively using the mapped range. So
	// it is safe to immediately unmap the range.
	// FIXME: QC Gunyah issue #74
	pgtable_hyp_start();
	pgtable_hyp_unmap(hyp_partition, virt, size, size);
	pgtable_hyp_commit();

	rcu_enqueue(&mr->rcu_entry,
		    RCU_UPDATE_CLASS_PARTITION_RELEASE_MAPPED_RANGE);

	ret = memdb_update(hyp_partition, phys, phys + (size - 1U),
			   (uintptr_t)partition, MEMDB_TYPE_PARTITION,
			   (uintptr_t)&partition->allocator,
			   MEMDB_TYPE_ALLOCATOR);
	if (ret == OK) {
		LOG(DEBUG, INFO,
		    "removed heap: partition {:#x}, phys {:#x}, size {:#x}",
		    (uintptr_t)partition, phys, size);
	} else {
		error_t err = new_memory_add(partition, hyp_partition, phys,
					     size, attr, true)
				      .e;
		if (err != OK) {
			panic("Failed to revert heap removal");
		}
	}

out:
	return ret;
}

error_t
partition_heap_is_free(partition_t *partition, paddr_t phys, size_t size)
{
	error_t ret;

	assert(partition != NULL);

	if ((size == 0U) || util_add_overflows(phys, size - 1U)) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = ERROR_ARGUMENT_ALIGNMENT;
		goto out;
	}

	partition_mapped_range_t *mr = NULL;
	rcu_read_start();
	LIST_FOREACH_CONTAINER_CONSUME_BEGIN(partition_mapped_range_t,
					     &partition->mapped_ranges,
					     partition_mapped_range, list_node,
					     node)
		if ((node->phys == phys) && (node->size == size)) {
			mr = node;
			break;
		}
	LIST_FOREACH_CONTAINER_CONSUME_END

	if ((mr != NULL) && mr->is_heap) {
		ret = trigger_allocator_range_is_free_event(
			partition, phys, mr->virt, size, mr->attr);
	} else {
		ret = ERROR_DENIED;
	}
	rcu_read_finish();

out:
	return ret;
}

#if defined(PLATFORM_TRACE_STANDALONE_REGION) &&                               \
	PLATFORM_TRACE_STANDALONE_REGION
uintptr_result_t
partition_map_and_add_trace(partition_t *partition, paddr_t phys, size_t size)
{
	uintptr_result_t ret;
	error_t		 err;

	assert(partition != NULL);
	assert(size != 0U);

	if ((size == 0U) || (util_add_overflows(phys, size - 1U))) {
		ret = uintptr_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}

	if (!util_is_baligned(phys, PGTABLE_HYP_PAGE_SIZE) ||
	    !util_is_baligned(size, PGTABLE_HYP_PAGE_SIZE)) {
		ret = uintptr_result_error(ERROR_ARGUMENT_ALIGNMENT);
		goto out;
	}

	partition_t *hyp_partition = partition_get_private();
	err = memdb_update(hyp_partition, phys, phys + (size - 1U),
			   (uintptr_t)NULL, MEMDB_TYPE_TRACE,
			   (uintptr_t)partition, MEMDB_TYPE_PARTITION);
	if (err != OK) {
		ret = uintptr_result_error(err);
		goto out;
	}

	// Add a new mapped range for the memory.
	ret = new_memory_add(partition, hyp_partition, phys, size,
			     allocator_memattr_default(), false);
	if (ret.e == OK) {
		LOG(DEBUG, INFO,
		    "added trace: partition {:#x}, virt {:#x}, phys {:#x}, size {:#x}",
		    (uintptr_t)partition, ret.r, phys, size);
	} else {
		err = memdb_update(hyp_partition, phys, phys + (size - 1U),
				   (uintptr_t)partition, MEMDB_TYPE_PARTITION,
				   (uintptr_t)NULL, MEMDB_TYPE_TRACE);
		if (err != OK) {
			panic("memdb_update");
		}
	}

out:
	return ret;
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
