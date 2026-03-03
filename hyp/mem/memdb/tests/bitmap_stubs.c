// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Comprehensive stubs for memdb bitmap implementation host testing
// Based on patterns from hyp/mem/pgtable/tests/stub.c

#define timer_t hyp_timer_t
#include <hyptypes.h>

#include <hypcontainers.h>

#include <memdb.h>
#include <rcu.h>
#undef timer_t

#define register_t std_register_t
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#undef register_t

// Partition management stubs (from pgtable/tests/stub.c pattern)
// Note: For host testing, we need to handle the partition assertion
// The real memdb code asserts that partition == partition_get_private()
// We'll use a simple approach: return the first partition passed to any memdb
// function

static partition_t *test_partition = NULL;

partition_t *
partition_get_private(void)
{
	return test_partition;
}

// Function to set the test partition (called from memdb functions)
void
memdb_set_test_partition(partition_t *partition)
{
	test_partition = partition;
}

void_ptr_result_t
partition_alloc(partition_t *partition, size_t bytes, size_t min_alignment)
{
	void_ptr_result_t ret;
	void		 *ptr = malloc(bytes);

	if (ptr != NULL) {
		ret.e = OK;
		ret.r = ptr;
	} else {
		ret.e = ERROR_NOMEM;
		ret.r = NULL;
	}

	return ret;
}

void
partition_free(partition_t *partition, void *ptr, size_t bytes)
{
	// For host testing, just use free()
	// In real hypervisor, this would return memory to the partition's
	// allocator
	(void)partition;
	(void)bytes;
	free(ptr);
}

// RCU synchronization stubs
void
rcu_read_start(void)
{
	// No-op for host testing
}

void
rcu_read_finish(void)
{
	// No-op for host testing
}

void
rcu_enqueue(rcu_entry_t *rcu_entry, rcu_update_class_t rcu_update_class)
{
	// For host testing, immediately process the RCU update
	// In real hypervisor, this would defer the processing until grace
	// period ends

	if (rcu_entry == NULL) {
		return;
	}

	// Handle different update classes to properly free the containing
	// object
	switch (rcu_update_class) {
	case RCU_UPDATE_CLASS_MEMDB_RELEASE_LEVEL_TABLE: {
		// Find the containing memdb_level_table_t and free it
		memdb_level_table_t *table =
			memdb_level_table_container_of_rcu_entry(rcu_entry);
		free(table);
		break;
	}
	case RCU_UPDATE_CLASS_MEMDB_RELEASE_LEVEL_BITMAP: {
		// Find the containing memdb_level_bitmap_t and free it
		memdb_level_bitmap_t *bitmap =
			memdb_level_bitmap_container_of_rcu_entry(rcu_entry);
		free(bitmap);
		break;
	}
	default:
		// For other update classes, we don't know how to handle them in
		// host testing
		printf("Warning: Unhandled RCU update class %d in host test stub\n",
		       (int)rcu_update_class);
		break;
	}
}

// Bitmap atomic operations stubs
uint64_t
bitmap_atomic_extract(uint64_t *bitmap, index_t bit_index, count_t bit_count,
		      bool clear)
{
	// Simple non-atomic implementation for host testing
	uint64_t result = 0;

	if (bitmap == NULL || bit_count == 0 || bit_count > 64) {
		return 0;
	}

	// Extract bits from the bitmap
	for (count_t i = 0; i < bit_count; i++) {
		index_t	 byte_index = (bit_index + i) / 8;
		index_t	 bit_offset = (bit_index + i) % 8;
		uint8_t *byte_ptr   = (uint8_t *)bitmap + byte_index;

		if ((*byte_ptr >> bit_offset) & 1) {
			result |= (1ULL << i);

			// Clear the bit if requested
			if (clear) {
				*byte_ptr &= ~(1 << bit_offset);
			}
		}
	}

	return result;
}

bool
bitmap_atomic_insert(uint64_t *bitmap, index_t bit_index, count_t bit_count,
		     uint64_t value)
{
	// Simple non-atomic implementation for host testing
	if (bitmap == NULL || bit_count == 0 || bit_count > 64) {
		return false;
	}

	// Insert bits into the bitmap
	for (count_t i = 0; i < bit_count; i++) {
		index_t	 byte_index = (bit_index + i) / 8;
		index_t	 bit_offset = (bit_index + i) % 8;
		uint8_t *byte_ptr   = (uint8_t *)bitmap + byte_index;

		if ((value >> i) & 1) {
			*byte_ptr |= (1 << bit_offset);
		} else {
			*byte_ptr &= ~(1 << bit_offset);
		}
	}

	return true;
}

// Tracing stubs
void
hyp_trace(uint32_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2,
	  uint64_t arg3)
{
	// No-op for host testing
	// In debug builds, could print trace information
	(void)id;
	(void)arg0;
	(void)arg1;
	(void)arg2;
	(void)arg3;
}

void
trigger_trace_log_event(void)
{
	// No-op for host testing
}

// Error handling stubs (from pgtable/tests/stub.c)
#if !defined(NDEBUG)
void
assert_failed(const char *file, int line, const char *func, const char *err)
{
	printf("ASSERT FAILED: %s:%d in %s(): %s\n", file, line, func, err);
	exit(-1);
}
#endif

void
panic(const char *msg)
{
	printf("PANIC: %s\n", msg);
	exit(-1);
}

// Memdb initialization is now implemented in memdb.c with #ifdef
// HYP_STANDALONE_TEST
