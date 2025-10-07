// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(UNIT_TESTS)

#include <hyptypes.h>
#include <string.h>

#include <hypconstants.h>

#include <allocator.h>
#include <compiler.h>
#include <log.h>
#include <panic.h>
#include <partition.h>
#include <partition_init.h>
#include <trace.h>
#include <util.h>

#include "event_handlers.h"

#define MEM_POOL_SIZE (1024 * 1024) // 1MB

// Maximum supported heap allocation size or alignment size. We filter out
// really large allocations so we can avoid having to think about corner-cases
// causing overflow.
#define MAX_ALLOC_SIZE	   (256UL * 1024UL * 1024UL)
#define MAX_ALIGNMENT_SIZE (16UL * 1024UL * 1024UL)

#define NODE_HEADER_SIZE ((size_t)sizeof(allocator_node_t))

// Minimum allocation size from the heap.
#define HEAP_MIN_ALLOC NODE_HEADER_SIZE
#define HEAP_MIN_ALIGN NODE_HEADER_SIZE

// -------------- DEBUGGING --------------
// FIXME: remove eventually
#define HEAP_DEBUG
#define OVERFLOW_DEBUG
// #define DEBUG_PRINT
//  ---------------------------------------

#if defined(HEAP_DEBUG)
#define CHECK_HEAP(x) check_heap_consistency(x)
#else
#define CHECK_HEAP(x)
#endif

static void *
give_mem_to_heap(partition_t *partition, size_t size, size_t alignment)
{
	error_t		  ret = OK;
	void_ptr_result_t alloc_ret;
	void		 *block;

	partition_t *partition_all = partition_get_root();
	// partition_get_private();
	alloc_ret = partition_alloc(partition_all, size, alignment);

	if (alloc_ret.e != OK) {
		panic("freelist_test: partition_alloc failed");
	}
	block = (void *)alloc_ret.r;

	ret = allocator_list_handle_allocator_add_ram_range(
		partition, (uintptr_t)alloc_ret.r, size,
		allocator_memattr_default());
	if (ret != OK) {
		panic("freelist_test: allocator_list_handle_allocator_add_ram_range failed");
		// LOG(DEBUG, INFO,
		//     "Memory added to heap, pointer: {:#x}, size {:#x}\n",
		//     (register_t)partition->allocator.heap,
		//     (register_t)partition->allocator.heap->size);
	}

	return block;
}

static void *
alloc_obj(allocator_t *allocator, size_t size, size_t alignment)
{
	void_ptr_result_t alloc_ret = allocator_allocate_object(
		allocator, size, alignment, allocator_hint_default());

	if (alloc_ret.e != OK) {
		panic("freelist_test: alloc_obj failed");
	}

	void *object = (void *)alloc_ret.r;

	return object;
}

static void
dealloc_objs(allocator_t *allocator, int order, void *object, size_t size,
	     void *object2, size_t size2, void *object3, size_t size3)
{
	allocator_memattr_t attr = allocator_memattr_default();

	switch (order) {
	case 0:
		// 1st -> 2nd -> 3rd
		if (object != NULL) {
			allocator_deallocate_object(allocator, object, size,
						    attr);
		}

		if (object2 != NULL) {
			allocator_deallocate_object(allocator, object2, size2,
						    attr);
		}

		if (object3 != NULL) {
			allocator_deallocate_object(allocator, object3, size3,
						    attr);
		}
		break;

	case 1:
		// 3rd -> 2nd -> 1st
		if (object3 != NULL) {
			allocator_deallocate_object(allocator, object3, size3,
						    attr);
		}

		if (object2 != NULL) {
			allocator_deallocate_object(allocator, object2, size2,
						    attr);
		}

		if (object != NULL) {
			allocator_deallocate_object(allocator, object, size,
						    attr);
		}
		break;

	case 2:
	default:
		// 1st -> 3rd -> 2nd
		if (object != NULL) {
			allocator_deallocate_object(allocator, object, size,
						    attr);
		}

		if (object3 != NULL) {
			allocator_deallocate_object(allocator, object3, size3,
						    attr);
		}

		if (object2 != NULL) {
			allocator_deallocate_object(allocator, object2, size2,
						    attr);
		}
		break;
	}
}

static void
remove_from_heap(allocator_t *allocator, void *block, size_t size)
{
	error_t ret = OK;

	if (block != NULL) {
		ret = allocator_heap_remove_memory(allocator, block, size);
	}

	if (ret != OK) {
		panic("freelist_test: remove_from_heap failed");
	}
}

// Test 1:
// - Give 1 chunk of memory to the heap of pool_size passed
// - Allocate objects of passed sizes
// - Free all the objects in order specied in 'order' variable.
// - Remove pool from heap
static void
test1(int order, size_t alignment, size_t pool_size, size_t size, size_t size2,
      size_t size3)
{
	partition_t *partition = partition_get_private();

	// ---------------- Giving memory to heap ---------------------
	void *block = give_mem_to_heap(partition, pool_size, alignment);

	// ---------------- Allocating object from heap ---------------
	void *object  = alloc_obj(&partition->allocator, size, alignment);
	void *object2 = alloc_obj(&partition->allocator, size2, alignment);
	void *object3 = alloc_obj(&partition->allocator, size3, alignment);

	// ---------------- Deallocating object to heap ----------------
	dealloc_objs(&partition->allocator, order, object, size, object2, size2,
		     object3, size3);

	// ---------------- Removing memory to heap --------------------
	remove_from_heap(&partition->allocator, block, pool_size);
}

// Test 2:
// - Give 3 chunks of memory to the heap of pool_size passed
// - Allocate objects of passed sizes
// - Free all the objects in order specied in 'order' variable.
// - Remove all pools from heap
static void
test2(int order, size_t alignment, size_t pool_size, size_t pool_size2,
      size_t pool_size3, size_t size, size_t size2, size_t size3)
{
	partition_t *partition = partition_get_root();

	// ---------------- Giving memory to heap ---------------------
	void *block  = give_mem_to_heap(partition, pool_size, alignment);
	void *block2 = give_mem_to_heap(partition, pool_size2, alignment);
	void *block3 = give_mem_to_heap(partition, pool_size3, alignment);

	// ---------------- Allocating object from heap ---------------
	void *object  = alloc_obj(&partition->allocator, size, alignment);
	void *object2 = alloc_obj(&partition->allocator, size2, alignment);
	void *object3 = alloc_obj(&partition->allocator, size3, alignment);

	// ---------------- Deallocating object to heap ----------------
	dealloc_objs(&partition->allocator, order, object, size, object2, size2,
		     object3, size3);

	// ---------------- Removing memory to heap --------------------
	remove_from_heap(&partition->allocator, block, pool_size);
	remove_from_heap(&partition->allocator, block2, pool_size2);
	remove_from_heap(&partition->allocator, block3, pool_size3);
}

static void
tests_choice(uint32_t test)
{
	size_t alignment  = sizeof(void *);
	size_t pool_size  = MEM_POOL_SIZE;
	size_t pool_size2 = 2UL * NODE_HEADER_SIZE;
	size_t pool_size3 = 4UL * NODE_HEADER_SIZE;
	size_t size	  = MEM_POOL_SIZE / 2;
	size_t size2	  = 48;
	size_t size3	  = MEM_POOL_SIZE / 2 - 48;

	if (test == 1) {
		// Default:
		// Allocate 3 objects emptying the free list
		// Deallocate in this order: 1st -> 3rd -> 2nd
		// so that we can check that when the 2nd object is
		// freed there is a merge of all free blocks

#if defined(OVERFLOW_DEBUG)
		// Extra 2*NODE_HEADER_SIZE for overflow checks
		pool_size = pool_size + (6 * NODE_HEADER_SIZE);
#endif
		// Order 0) 1->2->3 | 1) 3->2->1 | 2) 1->3->2

		pool_size = 0x0300;
		alignment = 0x0100;
		size	  = 0x0100;
		size2	  = 0x0100;
		size3	  = 0x0100;

		test1(0, alignment, pool_size, size, size2, size3);

		// test1(1,  alignment, pool_size, size, size2, size3);

		// test1(2,  alignment, pool_size, size, size2, size3);

	} else {
		// Default:
		// - Allocate an object that consumes 1st pool
		// - Allocate an object that does not fit in next pool
		//   but has go the 3rd one
		// - Allocate a smaller object from 2nd pool
		//   (now first) and needs alignment
		// order	  = 1;
		pool_size = MEM_POOL_SIZE;
		size	  = MEM_POOL_SIZE;
		size2	  = 36;
		size3	  = 10;

#if defined(OVERFLOW_DEBUG)
		// Extra 2*NODE_HEADER_SIZE for overflow checks
		pool_size  = pool_size + (2 * NODE_HEADER_SIZE);
		pool_size2 = pool_size2 + (2 * NODE_HEADER_SIZE);
		pool_size3 = pool_size3 + (2 * NODE_HEADER_SIZE);
#endif

		pool_size  = 0x0200;
		pool_size2 = 0x0300;
		pool_size3 = 0x0400;
		alignment  = 0x0100;
		size	   = 0x0100;
		size2	   = 0x0100;
		size3	   = 0x0100;

		test2(0, alignment, pool_size, pool_size2, pool_size3, size,
		      size2, size3);

		// test2(1, alignment, pool_size, pool_size2, pool_size3, size,
		//       size2, size3);

		// test2(2, alignment, pool_size, pool_size2, pool_size3, size,
		//       size2, size3);
	}
}

bool
test_allocater(void)
{
	uint32_t option = 1;

	LOG(DEBUG, INFO, "\n----------- freelist_tests Started ----------\n");
	while (option != 3) {
		if (option != 3) {
			tests_choice(option);
			option++;
		}
	}
	LOG(DEBUG, INFO, "\n----------- freelist_tests Completed ----------\n");

	return 0;
}
#else

extern char unused;
#endif
