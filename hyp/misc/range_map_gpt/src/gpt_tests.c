// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(UNIT_TESTS)

#include <assert.h>
#include <hyptypes.h>

#include <bitmap.h>
#include <compiler.h>
#include <cpulocal.h>
#include <log.h>
#include <partition_init.h>
#include <preempt.h>
#include <range_map.h>
#include <trace.h>
#include <util.h>

#include "event_handlers.h"
#include "internal.h"

static range_map_t range_map;

static range_map_entry_t
test_entry_init(range_map_type_t type, uint64_t value)
{
	return (range_map_entry_t){
		.type  = type,
		.value = { .raw = value },
	};
}

void
range_map_tests_add_offset(range_map_type_t type, range_map_value_t *value,
			   size_t offset)
{
	value->raw += (type != RANGE_MAP_TYPE_TEST_C) ? offset : (offset * 2);
}

bool
range_map_tests_values_equal(range_map_value_t x, range_map_value_t y)
{
	return x.raw == y.raw;
}

error_t
range_map_tests_callback(range_map_entry_t entry, size_t base, size_t size,
			 range_map_arg_t arg)
{
	LOG(DEBUG, INFO,
	    "GPT callback: t {:d}, v {:#x}, [{:#x}, {:#x}], arg {:#x}",
	    entry.type, entry.value.raw, base, size, arg.test);

	return OK;
}

void
range_map_gpt_handle_tests_init(void)
{
	partition_t *partition = partition_get_root();
	assert(partition != NULL);

	range_map_config_t config = range_map_config_default();
	range_map_config_set_max_bits(&config, RANGE_MAP_MAX_SIZE_BITS);

	register_t types = 0U;
	bitmap_set(&types, RANGE_MAP_TYPE_TEST_A);
	bitmap_set(&types, RANGE_MAP_TYPE_TEST_B);
	bitmap_set(&types, RANGE_MAP_TYPE_TEST_C);

	error_t err = range_map_init(&range_map, partition, config, types);
	assert(err == OK);
}

bool
range_map_gpt_handle_tests_start(void)
{
	error_t err;

	preempt_disable();

	if (cpulocal_get_index() != 0U) {
		goto out;
	}

	assert(range_map_is_empty(&range_map));

	size_t		  base, size;
	range_map_entry_t e1, e2;

	base = 0x80000000U;
	size = 0x70000;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

	err = range_map_insert(&range_map, base, size, e1, true);
	assert(err == OK);

	base = 0x80001000U;
	size = 0x4500;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);
	e2   = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x900000);

	err = range_map_update(&range_map, base, size, e1, e2);
	assert(err == OK);

	base = 0x80020010U;
	size = 0x3;
	e2   = test_entry_init(RANGE_MAP_TYPE_TEST_B, base);

	err = range_map_insert(&range_map, base, size, e2, false);
	assert(err == OK);

	base = 0x80040400U;
	size = 3;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x80040400U);
	e2   = test_entry_init(RANGE_MAP_TYPE_TEST_C, 0x400);

	err = range_map_update(&range_map, base, size, e1, e2);
	assert(err == OK);

	base = 0x80055555;
	size = 1234;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x80055555);

	err = range_map_remove(&range_map, base, size, e1);
	assert(err == OK);

	range_map_dump_ranges(&range_map);

	base = 0x80000050;
	size = 0x20;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x80000050);

	assert(!range_map_is_empty(&range_map));

	bool ret = range_map_is_contiguous(&range_map, base, size, e1);
	assert(ret);

	range_map_lookup_result_t lookup =
		range_map_lookup(&range_map, 0x8, 1U);
	LOG(DEBUG, INFO, "Lookup returned: {:d} {:#x} ({:#x})",
	    lookup.entry.type, lookup.entry.value.raw, lookup.size);

	lookup = range_map_lookup(&range_map, 0x80040001, 2);
	LOG(DEBUG, INFO, "Lookup returned: {:d} {:#x} ({:#x})",
	    lookup.entry.type, lookup.entry.value.raw, lookup.size);

	lookup = range_map_lookup(&range_map, 0x80050006, 0x20000);
	LOG(DEBUG, INFO, "Lookup returned: {:d} {:#x} ({:#x})",
	    lookup.entry.type, lookup.entry.value.raw, lookup.size);

	range_map_arg_t arg;

	base	 = 0x80000001;
	size	 = 0x6f000;
	arg.test = 0xfeed;

	err = range_map_walk(&range_map, base, size, RANGE_MAP_TYPE_TEST_A,
			     RANGE_MAP_CALLBACK_TEST, arg);
	assert(err == OK);

	base	 = 0x80040200U;
	size	 = 0x800;
	arg.test = 0xbeef;

	err = range_map_walk(&range_map, base, size, RANGE_MAP_TYPE_TEST_C,
			     RANGE_MAP_CALLBACK_TEST, arg);
	assert(err == OK);

	base = 0x100100U;
	size = 0x1;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

	err = range_map_insert(&range_map, base, size, e1, false);
	assert(err == OK);

	base = 0x100300U;
	size = 0x1;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

	err = range_map_insert(&range_map, base, size, e1, false);
	assert(err == OK);

	base = 0x100100U;
	size = 0x1;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

	err = range_map_remove(&range_map, base, size, e1);
	assert(err == OK);

	range_map_dump_ranges(&range_map);

	// Partially invalid update.
	base = 0x80030000U;
	size = 0x50000;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);
	e2   = test_entry_init(RANGE_MAP_TYPE_TEST_B, base);

	err = range_map_update(&range_map, base, size, e1, e2);
	assert(err != OK);

	size = 0x10;

	err = range_map_update(&range_map, base, size, e1, e2);
	assert(err == OK);

	range_map_dump_ranges(&range_map);

	// Attempt to insert invalid type.
	e1 = test_entry_init(RANGE_MAP_TYPE_LEVEL, 0x213123123123);

	err = range_map_insert(&range_map, 0x919100123f23, 0x1012301230, e1,
			       false);
	assert(err != OK);

	base = 0x70000000U;
	size = 0x20000000U;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_B, 0x50000000U);

	err = range_map_insert(&range_map, base, size, e1, false);
	assert(err == OK);

	range_map_dump_levels(&range_map);

	err = range_map_clear(&range_map, 0U, 0x100000000U);
	assert(err == OK);

	assert(range_map_is_empty(&range_map));

	base = 0U;
	size = 1U;
	e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

	err = range_map_insert(&range_map, base, size, e1, false);
	assert(err == OK);

	for (index_t i = 0U; i < RANGE_MAP_MAX_SIZE_BITS;
	     i += RANGE_MAP_LEVEL_BITS) {
		base = util_bit(i);
		size = 1U;
		e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

		err = range_map_insert(&range_map, base, size, e1, false);
		assert(err == OK);
	}

	range_map_dump_ranges(&range_map);

	for (index_t i = 0U; i < RANGE_MAP_MAX_SIZE_BITS;
	     i += RANGE_MAP_LEVEL_BITS) {
		base = util_bit(i);
		size = 1U;
		e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

		err = range_map_remove(&range_map, base, size, e1);
		assert(err == OK);
	}

	range_map_clear_all(&range_map);

	for (index_t i = 0U; i < RANGE_MAP_LEVEL_ENTRIES; i++) {
		base = 0xffff00000000 + (i << 8);
		size = 1U << 8;
		e1   = test_entry_init(RANGE_MAP_TYPE_TEST_A, base);

		err = range_map_insert(&range_map, base, size, e1, true);
		assert(err == OK);
	}

	range_map_dump_levels(&range_map);

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_A, 1U);

	err = range_map_insert(&range_map, 0x1000, 1U, e1, false);
	assert(err == OK);

	err = range_map_insert(&range_map, 0x1002, 1U, e1, false);
	assert(err == OK);

	err = range_map_insert(&range_map, 0x1002, 1U, e1, false);
	assert(err == OK);

	e2 = test_entry_init(RANGE_MAP_TYPE_TEST_B, 1U);

	err = range_map_insert(&range_map, 0x1010, 1U, e2, false);
	assert(err == OK);

	range_map_dump_levels(&range_map);

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_C, 2U);

	err = range_map_insert(&range_map, 0U, 0x2000, e1, true);
	assert(err != OK);

	range_map_clear(&range_map, 1U, 0x10000U);

	range_map_dump_levels(&range_map);

	range_map_clear_all(&range_map);

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0xdeadbeefbeadfeedU);

	// Base and size cause overflow
	err = range_map_insert(&range_map, 0xffffffffffffff00, 0x0, e1, true);
	assert(err == ERROR_ARGUMENT_INVALID);

	err = range_map_insert(&range_map, 0xffffffffffffff00, 0x100, e1, true);
	assert(err == ERROR_ARGUMENT_SIZE);

	err = range_map_insert(&range_map, 0xffffffffffffffff, 0x33333333, e1,
			       true);
	assert(err == ERROR_ARGUMENT_INVALID);

	// Larger than max size supported
	err = range_map_insert(&range_map, util_bit(RANGE_MAP_MAX_SIZE_BITS),
			       0x1, e1, true);
	assert(err == ERROR_ARGUMENT_SIZE);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS + 1),
			       0x33333333, e1, true);
	assert(err == ERROR_ARGUMENT_SIZE);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1, 0x1, e1,
			       true);
	assert(err == OK);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1, 0x1, e1,
			       true);
	assert(err == ERROR_BUSY);

	err = range_map_clear(&range_map, util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1,
			      0x1U);
	assert(err == OK);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 2, 0x2, e1,
			       true);
	assert(err == OK);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1, 0x1, e1,
			       true);
	assert(err == ERROR_BUSY);

	err = range_map_walk(&range_map, 0, util_bit(RANGE_MAP_MAX_SIZE_BITS),
			     RANGE_MAP_TYPE_TEST_A, RANGE_MAP_CALLBACK_TEST,
			     arg);
	assert(err == OK);

	err = range_map_clear(&range_map, util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1,
			      0x1U);
	assert(err == OK);

	err = range_map_walk(&range_map, 0, util_bit(RANGE_MAP_MAX_SIZE_BITS),
			     RANGE_MAP_TYPE_TEST_A, RANGE_MAP_CALLBACK_TEST,
			     arg);
	assert(err == OK);

	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 2, 0x1, e1,
			       true);
	assert(err == ERROR_BUSY);

	e2 = e1;
	e2.value.raw += 1;
	err = range_map_insert(&range_map,
			       util_bit(RANGE_MAP_MAX_SIZE_BITS) - 1, 0x1, e2,
			       true);
	assert(err == OK);

	err = range_map_walk(&range_map, 0, util_bit(RANGE_MAP_MAX_SIZE_BITS),
			     RANGE_MAP_TYPE_TEST_A, RANGE_MAP_CALLBACK_TEST,
			     arg);
	assert(err == OK);

	err = range_map_insert(&range_map, 0x4000000000000, 0x33333333, e1,
			       true);
	assert(err == OK);

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x3333444455550000U);

	err = range_map_insert(&range_map, 0x234000000000, 0x679823213, e1,
			       true);
	assert(err == OK);

	// Attempt to insert range with overlap at end
	err = range_map_insert(&range_map, 0x233cdf123000, 0x82681e3f2, e1,
			       true);
	assert(err != OK);

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x3333444455556666U);
	e2 = test_entry_init(RANGE_MAP_TYPE_TEST_B, 0x777788889999);

	// Attempt to update range that doesn't match at the end
	err = range_map_update(&range_map, 0x234000006666, 0x73f8c532ab, e1,
			       e2);
	assert(err != OK);

	err = range_map_update(&range_map, 0x234000006666, 0x123e34, e1, e2);
	assert(err == OK);

	range_map_dump_ranges(&range_map);

	base = 0x7ab2348e293;
	size = 0x123809193;

	e1 = test_entry_init(RANGE_MAP_TYPE_TEST_A, 0x183741ea175);

	err = range_map_insert(&range_map, 0U, base, e1, false);
	assert(err == OK);

	e1.value.raw += base + size;

	err = range_map_insert(&range_map, base + size,
			       RANGE_MAP_MAX_SIZE - base - size, e1, false);
	assert(err == OK);

	e1.value.raw -= size;

	err = range_map_insert(&range_map, base, size, e1, false);
	assert(err == OK);

	e1.value.raw -= base;

	// GPT should now have a single contiguous entry
	assert(range_map_is_contiguous(&range_map, 0U, RANGE_MAP_MAX_SIZE, e1));

	range_map_dump_ranges(&range_map);

	range_map_destroy(&range_map);
out:
	preempt_enable();

	return false;
}

#else

extern char unused;

#endif
