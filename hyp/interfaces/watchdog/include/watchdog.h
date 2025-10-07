// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
watchdog_configure(watchdog_t		  *watchdog,
		   watchdog_option_flags_t watchdog_options);

void
watchdog_control(watchdog_t *wdt, bool enable);

bool
watchdog_is_enabled(watchdog_t *wdt);

bool
watchdog_is_expired(watchdog_t *wdt);

watchdog_milliseconds_t
watchdog_ms_since_last_pat(watchdog_t *wdt);

bool
watchdog_set_times(watchdog_t *wdt, watchdog_milliseconds_t bark_ms,
		   watchdog_milliseconds_t bite_ms);

// Sets the current active timeout based on wdt->action
bool
watchdog_set_active_time(watchdog_t *wdt, watchdog_milliseconds_t timeout_ms);

watchdog_milliseconds_t
watchdog_get_bark(watchdog_t *wdt);

watchdog_milliseconds_t
watchdog_get_bite(watchdog_t *wdt);

void
watchdog_pat(watchdog_t *wdt);

// Explicitly pat the hypervisor watchdog.
// This API should only be used in cases where a long running test case may be
// running with interrupts disabled.
void
watchdog_pat_hyp(void);

// Returns the watchdog object that caused a fatal hypervisor abort.
watchdog_t *
watchdog_get_fatal(void);

// Freeze the specified watchdog.
//
// Calls to this function are counted and must be balanced by the corresponding
// number of unfreeze calls before the watchdog will restart.
//
// If the reset parameter is true, the freeze will implicitly pat the watchdog.
// This is effective even if the watchdog is already frozen.
error_t
watchdog_freeze(watchdog_t *wdt, bool reset);

// Unfreeze the specified watchdog.
//
// Returns ERROR_BUSY if the watchdog was already unfrozen. This should be
// ignored by internal callers, because this is accessible to VMs which might
// make unbalanced unfreeze calls.
error_t
watchdog_unfreeze(watchdog_t *wdt);
