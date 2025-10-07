// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
watchdog_attach(watchdog_t *wdt, thread_t *thread);

error_t
watchdog_bind_virq(watchdog_t *wdt, vic_t *vic, virq_t virq,
		   watchdog_virq_type_t virq_type);

error_t
watchdog_unbind_virq(watchdog_t *wdt, watchdog_virq_type_t virq_type)
	EXCLUDE_PREEMPT_DISABLED;
