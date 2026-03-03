// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(HYPERCALLS)
#include <assert.h>
#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <allocator.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition.h>
#include <util.h>

#include "useraccess.h"

// Placeholders for unimplemented objects

// Dynamic creation of partitions is not yet implemented
hypercall_partition_create_partition_result_t
hypercall_partition_create_partition(cap_id_t src_partition_cap,
				     cap_id_t cspace_cap)
{
	(void)src_partition_cap;
	(void)cspace_cap;
	return (hypercall_partition_create_partition_result_t){
		.error	 = ERROR_UNIMPLEMENTED,
		.new_cap = CSPACE_CAP_INVALID,
	};
}

error_t
hypercall_partition_donate(partition_donate_flags_t flags,
			   cap_id_t partition_cap, register_t arg2,
			   paddr_t base, size_t size)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	if (partition_donate_flags_get_res0(&flags) != 0U) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, partition_cap, CAP_RIGHTS_PARTITION_DONATE);
	if (p.e != OK) {
		ret = p.e;
		goto out;
	}

	partition_t *partition = p.r;

	partition_donate_type_t type = partition_donate_flags_get_type(&flags);
	switch (type) {
	case PARTITION_DONATE_TYPE_TO_PARTITION:
		// Arg2 is the destination partition cap.
		p = cspace_lookup_partition(cspace, (cap_id_t)arg2,
					    CAP_RIGHTS_PARTITION_DONATE);
		if (p.e == OK) {
			ret = partition_mem_donate(partition, base, size, p.r,
						   false);
			object_put_partition(p.r);
		} else {
			ret = p.e;
		}
		break;
	case PARTITION_DONATE_TYPE_ADD_HEAP: {
		// Arg2 is the allocator memory attributes.
		allocator_memattr_t attr =
			allocator_memattr_cast((uint16_t)arg2);
		if (allocator_memattr_is_clean(attr)) {
			ret = partition_map_and_add_heap_ext(partition, base,
							     size, attr)
				      .e;
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case PARTITION_DONATE_TYPE_REMOVE_HEAP:
		// Arg2 is Res0.
		if (arg2 == 0U) {
			ret = partition_unmap_and_remove_heap(partition, base,
							      size);
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	default:
		ret = ERROR_ARGUMENT_INVALID;
		break;
	}

	object_put_partition(partition);

out:
	return ret;
}

static error_t
partition_heap_get_stats(partition_t *partition, gvaddr_t guest_va, size_t size,
			 allocator_memattr_t attr)
{
	error_t ret;

	if (!allocator_memattr_is_clean(attr)) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	allocator_stats_t stats = { 0 };

	error_t err = allocator_get_stats(&partition->allocator, attr, &stats);
	if (err != OK) {
		ret = err;
		goto out;
	}

	size_result_t copy_ret = useraccess_copy_to_guest_va(
		guest_va, size, &stats, sizeof(stats), false);
	if (copy_ret.e != OK) {
		ret = copy_ret.e;
		goto out;
	}

	ret = OK;

out:
	return ret;
}

error_t
hypercall_partition_query(cap_id_t partition_cap, partition_query_flags_t flags,
			  register_t addr, size_t size, register_t arg4)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	if (partition_query_flags_get_res0(&flags) != 0U) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	partition_ptr_result_t p = cspace_lookup_partition(
		cspace, partition_cap, CAP_RIGHTS_PARTITION_QUERY);
	if (p.e != OK) {
		ret = p.e;
		goto out;
	}

	partition_t *partition = p.r;

	partition_query_type_t type = partition_query_flags_get_type(&flags);
	switch (type) {
	case PARTITION_QUERY_TYPE_HEAP_IS_FREE:
		// Arg4 is Res0.
		ret = (arg4 == 0U) ? partition_heap_is_free(partition,
							    (paddr_t)addr, size)
				   : ERROR_ARGUMENT_INVALID;
		break;
	case PARTITION_QUERY_TYPE_HEAP_STATS:
		// Arg4 is the allocator memory attributes.
		ret = partition_heap_get_stats(
			partition, (gvaddr_t)addr, size,
			allocator_memattr_cast((uint16_t)arg4));
		break;
	default:
		ret = ERROR_ARGUMENT_INVALID;
		break;
	}

	object_put_partition(partition);

out:
	return ret;
}

#else
extern int unused;
#endif
