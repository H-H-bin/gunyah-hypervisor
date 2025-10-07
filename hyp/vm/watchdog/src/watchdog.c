// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <compiler.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition_init.h>
#include <thread.h>
#include <trace.h>
#include <vic.h>
#include <watchdog.h>

#include "event_handlers.h"
#include "watchdog.h"

error_t
watchdog_attach(watchdog_t *wdt, thread_t *thread)
{
	assert(wdt != NULL);
	assert(thread != NULL);
	assert(atomic_load_relaxed(&thread->header.state) == OBJECT_STATE_INIT);
	assert(atomic_load_relaxed(&wdt->header.state) == OBJECT_STATE_ACTIVE);

	error_t err;

	if (thread->kind != THREAD_KIND_VCPU) {
		err = ERROR_ARGUMENT_INVALID;
	} else {
		watchdog_t *old_wdt = thread->watchdog;
		if (old_wdt != NULL) {
			object_put_watchdog(old_wdt);
		}

		thread->watchdog = object_get_watchdog_additional(wdt);
		err		 = OK;
	}

	return err;
}

error_t
watchdog_bind_virq(watchdog_t *wdt, vic_t *vic, virq_t virq,
		   watchdog_virq_type_t virq_type)
{
	error_t	       ret    = OK;
	virq_source_t *source = NULL;
	virq_trigger_t trigger;

	assert(wdt != NULL);

	if (virq_type == WATCHDOG_VIRQ_TYPE_BARK) {
		source	= &wdt->bark_virq_src;
		trigger = VIRQ_TRIGGER_WATCHDOG_BARK;
	} else if (virq_type == WATCHDOG_VIRQ_TYPE_BITE) {
		source	= &wdt->bite_virq_src;
		trigger = VIRQ_TRIGGER_WATCHDOG_BITE;
	} else {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	ret = vic_bind_shared(source, vic, virq, trigger);

out:
	return ret;
}

error_t
watchdog_unbind_virq(watchdog_t *wdt, watchdog_virq_type_t virq_type)
{
	error_t	       ret    = OK;
	virq_source_t *source = NULL;

	assert(wdt != NULL);

	if (virq_type == WATCHDOG_VIRQ_TYPE_BARK) {
		source = &wdt->bark_virq_src;
	} else if (virq_type == WATCHDOG_VIRQ_TYPE_BITE) {
		source = &wdt->bite_virq_src;
	} else {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	vic_unbind_sync(source);
out:
	return ret;
}

void
watchdog_handle_object_deactivate_thread(thread_t *thread)
{
	assert(thread != NULL);

	watchdog_t *wdt = thread->watchdog;

	if (wdt != NULL) {
		object_put_watchdog(wdt);
		thread->watchdog = NULL;
	}
}

error_t
watchdog_handle_object_activate_thread(thread_t *thread)
{
	assert(thread != NULL);

	if (thread->kind == THREAD_KIND_VCPU) {
		watchdog_t *wdt = thread->watchdog;
		if (wdt != NULL) {
			assert(thread->addrspace != NULL);

			// Store the VMID in the watchdog object for debugging
			// and logging purposes
			wdt->debug_id = (uint32_t)thread->addrspace->vmid;
		}
	}

	return OK;
}

error_t
watchdog_handle_object_activate_watchdog(watchdog_t *watchdog)
{
	error_t ret = OK;

	// Check that the partition has the right to set the watchdog as
	// critical
	// FIXME: for now we only allow the root partition, but in the future we
	// may want to add a way of checking that a partition has the proper
	// permissions
	if (watchdog_option_flags_get_critical_bite(&watchdog->options) &&
	    !partition_option_flags_get_privileged(
		    &watchdog->header.partition->options)) {
		ret = ERROR_DENIED;
		goto out;
	}

out:
	return ret;
}
