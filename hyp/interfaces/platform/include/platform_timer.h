// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

ticks_t
platform_timer_get_timeout(void);

void
platform_timer_cancel_timeout(void) REQUIRE_PREEMPT_DISABLED;

// Must be called with preempt_disabled
void
platform_timer_set_timeout(ticks_t timeout) REQUIRE_PREEMPT_DISABLED;

uint32_t
platform_timer_get_frequency(void);

// Unsynchronized ticks value, might be read speculatively or out of order
ticks_t
platform_timer_get_current_ticks(void);

// Synchronized ticks value, not speculated or read out of order with respect
// to multiple calls
uint64_t
platform_timer_get_current_ticks_sync(void);

ticks_t
platform_timer_convert_ns_to_ticks(nanoseconds_t ns);

nanoseconds_t
platform_timer_convert_ticks_to_ns(ticks_t ticks);

ticks_t
platform_timer_convert_ms_to_ticks(milliseconds_t ms);

milliseconds_t
platform_timer_convert_ticks_to_ms(ticks_t ticks);

void
platform_timer_ndelay(nanoseconds_t duration);
