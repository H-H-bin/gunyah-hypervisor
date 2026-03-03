// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

watchdog_ticks_t
platform_watchdog_ms_to_ticks(watchdog_milliseconds_t ms);

watchdog_milliseconds_t
platform_watchdog_ticks_to_ms(watchdog_ticks_t ticks);

watchdog_ticks_t
platform_watchdog_get_counter(void);

watchdog_abs_ticks_t
platform_watchdog_get_last_pat(void);

bool
platform_watchdog_is_expired(void);

bool
platform_watchdog_is_frozen(void);

void
platform_watchdog_pat(void);

// After power on, reset watchdog registers to a known state.
// Leaves the watchdog disabled.
void
platform_watchdog_init(void);

void
platform_watchdog_reset(void);

watchdog_ticks_t
platform_watchdog_get_bark(void);

void
platform_watchdog_set_bark(watchdog_ticks_t bark);

void
platform_watchdog_set_bite(watchdog_ticks_t bite);

void
platform_watchdog_set_enable(bool enable);
