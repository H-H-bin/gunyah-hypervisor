// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// gcc -O2 test_scale.c -o test_scale && ./test_scale
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <unistd.h>

typedef uint32_t count_t;
typedef uint32_t index_t;

#define assert_debug assert

#define util_bit(b) ((uintmax_t)1U << (b))

#define util_balign_down(x, a) ((x) & ~((a) - 1U))
#define util_balign_up(x, a)   util_balign_down((x) + ((a) - 1U), a)

#define compiler_clz(x)                                                        \
	(assert((x) != 0U), (index_t) _Generic((x),                            \
		 unsigned long long: __builtin_clzll,                          \
		 unsigned long: __builtin_clzl,                                \
		 unsigned int: __builtin_clz)(x))

#define compiler_msb(x) ((sizeof(x) * (size_t)CHAR_BIT) - 1U - compiler_clz(x))

#define compiler_unexpected(x...) x

#define TLBI_RANGE_SCALE_MAX (uint8_t)3U // 0x3

// Find a (scale, num) pair for the requested range.
//
// The range covered by the TLBI range instructions is:
// ((NUM + 1) * (2 ^ (5 * SCALE + 1)) * Translation_Granule_Size
//
// Returns false if the requested size is bigger than the maximum possible range
// size (8GB for 4K granules) after alignment.
static bool
tlbi_range_find_scale_num(uint64_t size, count_t granule_shift, uint8_t *scale,
			  uint8_t *num)
{
	bool success;
	// size must be at least two granule_shift pages
	assert_debug(size >= util_bit(granule_shift + 1U));

	// Find the most significant bit set in (size-1)
	count_t msb = compiler_msb(size - 1U);

	count_t scale_bits = msb - granule_shift;
	count_t scale_calc = (scale_bits == 0U) ? 0U : ((scale_bits - 1U) / 5U);

	if (compiler_unexpected(scale_calc > TLBI_RANGE_SCALE_MAX)) {
		success = false;
	} else {
		*scale = (uint8_t)scale_calc;
		uint8_t shift =
			(uint8_t)((5U * scale_calc) + 1U + granule_shift);

		*num = (uint8_t)(util_balign_up(size, util_bit(shift)) >>
				 shift) -
		       1U;

		success = true;
	}

	return success;
}

int
main(void)
{
	printf("scale_test:\n");

	count_t granule = 12;
	count_t skip	= 12;

	for (uint64_t x = util_bit(granule + 1); x <= util_bit(granule + 22);
	     x += util_bit(skip)) {
		uint8_t scale, num;
		bool ret = tlbi_range_find_scale_num(x, granule, &scale, &num);

		if (ret) {
			uint64_t range = (num + 1) * util_bit((scale * 5) + 1) *
					 util_bit(granule);

			printf(" size: %#lx:  SCALE %d, NUM %d  == %#lx\n", x,
			       scale, num, range);
			assert(range >= x);
		} else {
			printf(" size: %#lx - XXX\n", x);
		}

		if (compiler_msb(x >> skip) > 5 /*6*/) {
			skip += 1;
		}
	}
}
