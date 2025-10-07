// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(UNIT_TESTS) && UNIT_TESTS
#include <assert.h>
#include <hyptypes.h>

#include <compiler.h>
#include <log.h>
#include <platform_timer.h>
#include <trace.h>

#include "event_handlers.h"

bool
arm_arch_timer_handle_tests_start(void)
{
	bool failed;

	// Avoid optimisations
	volatile nanoseconds_t	ns1, ns2;
	volatile milliseconds_t ms1, ms2;
	volatile ticks_t	t1 = platform_timer_get_current_ticks();
	volatile ticks_t	t2;

	// Simple sanity check that current ticks increments
	count_t wait = 0U;

	failed = true;
	while (wait < 1000000U) {
		t2 = platform_timer_get_current_ticks();
		if (t1 != t2) {
			failed = false;
			break;
		}
		wait += 1U;
	}
	if (failed) {
		goto out;
	}
	LOG(DEBUG, INFO, "Needed {:d} iterations for ticks to increment", wait);

	// === Platform timer conversion tests ===
	uint64_t y99_secs = 99U * 365U * 24U * 60U * 60U;

	ns1 = y99_secs * 1000000000U;

	// Check for overflow at 99 years
	t1  = platform_timer_convert_ns_to_ticks(ns1);
	ns2 = platform_timer_convert_ticks_to_ns(t1);

	uint64_t tdiff;
	if (ns1 > ns2) {
		tdiff = ns1 - ns2;
	} else {
		tdiff = ns2 - ns1;
	}
	if (tdiff > 100U) {
		LOG(DEBUG, INFO,
		    "Converting 99 years to ticks and back failed");
		failed = true;
		goto out;
	}
	LOG(DEBUG, INFO, "Converting 99 years to ticks error = {:d}ns", tdiff);

	uint64_t y1999_secs = 1999U * 365U * 24U * 60U * 60U;

	ms1 = y1999_secs * 1000U;

	// Check for overflow at 1999 years
	t1  = platform_timer_convert_ms_to_ticks(ms1);
	ms2 = platform_timer_convert_ticks_to_ms(t1);

	if (ms1 > ms2) {
		tdiff = ms1 - ms2;
	} else {
		tdiff = ms2 - ms1;
	}
	if (tdiff > 10U) {
		LOG(DEBUG, INFO,
		    "Converting 1999 years to ticks and back failed");
		failed = true;
		goto out;
	}
	LOG(DEBUG, INFO, "Converting 1999 years to ticks error = {:d}ms",
	    tdiff);

	// Test PLATFORM_ARCH_TIMER_FREQ conversions
	t1 = platform_timer_convert_ns_to_ticks(
		((uint64_t)1000U * 1000500000U) / PLATFORM_ARCH_TIMER_FREQ);
	if (t1 != 1000U) {
		LOG(DEBUG, INFO, "Converting NS to ticks failed {:d}", t1);
		failed = true;
		goto out;
	}

out:
	LOG(DEBUG, INFO, "Platform timer tests {:s}",
	    (register_t)(failed ? "failed" : "passed"));
	return failed;
}
#else
extern char unused;
#endif
