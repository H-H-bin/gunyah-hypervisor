// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypversion.h>

#include <boot.h>
#include <compiler.h>
#include <log.h>
#include <memdb.h>
#include <panic.h>
#include <partition.h>
#include <prng.h>
#include <qcbor.h>
#include <thread_init.h>
#include <trace.h>
#include <util.h>

#include <events/boot.h>

#include "boot_init.h"
#include "event_handlers.h"

const char hypervisor_version[] = HYP_CONF_STR "-" HYP_GIT_VERSION
#if defined(HYP_QUALITY)
					       " " HYP_QUALITY
#endif
	;
const char hypervisor_build_date[] = HYP_BUILD_DATE;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
extern uintptr_t __stack_chk_guard;
uintptr_t	 __stack_chk_guard __attribute__((used, visibility("hidden")));
#pragma clang diagnostic pop

noreturn void
boot_cold_init(cpu_index_t cpu) LOCK_IMPL
{
	// Set the stack canary, either globally, or for the init thread if the
	// canary is thread-local. Note that we can't do this in an event
	// handler because that might trigger a stack check failure if the event
	// handler is not inlined (e.g. in debug builds).
	uint64_result_t guard_r = prng_get64();
	assert(guard_r.e == OK);
	__stack_chk_guard = (uintptr_t)guard_r.r;

	// We can't trace/log early because the CPU index and preemption count
	// in the thread are still uninitialized.

	trigger_boot_cpu_early_init_event();
	trigger_boot_cold_init_event(cpu);
	trigger_boot_cpu_cold_init_event(cpu);

	// It's safe to log now.
	TRACE_AND_LOG(ERROR, WARN, "Hypervisor cold boot, version: {:s} ({:s})",
		      (register_t)hypervisor_version,
		      (register_t)hypervisor_build_date);

	TRACE(DEBUG, INFO, "boot_cpu_warm_init");
	trigger_boot_cpu_warm_init_event();
	TRACE(DEBUG, INFO, "boot_hypervisor_start");
	trigger_boot_hypervisor_start_event();
	TRACE(DEBUG, INFO, "boot_cpu_start");
	trigger_boot_cpu_start_event();
	TRACE(DEBUG, INFO, "entering idle");
	thread_boot_set_idle();
}

#if defined(VERBOSE) && VERBOSE
#define STACK_GUARD_BYTE 0xb8
#define STACK_GUARD_SIZE 256U
#include <string.h>

#include <panic.h>
#endif

extern char aarch64_boot_stack[];

void
boot_handle_boot_cold_init(void)
{
#if defined(VERBOSE) && VERBOSE
	// Add a red-zone to the boot stack
	errno_t err_mem = memset_s(aarch64_boot_stack, STACK_GUARD_SIZE,
				   STACK_GUARD_BYTE, STACK_GUARD_SIZE);
	if (err_mem != 0) {
		panic("Error in memset_s operation!");
	}
#endif
}

void
boot_handle_idle_start(void)
{
	char *stack_bottom = (char *)aarch64_boot_stack;

#if defined(VERBOSE) && VERBOSE
	// Check red-zone in the boot stack
	for (index_t i = 0; i < STACK_GUARD_SIZE; i++) {
		if (stack_bottom[i] != (char)STACK_GUARD_BYTE) {
			panic("boot stack overflow!");
		}
	}
#endif

	partition_t *private = partition_get_private();

	size_t stack_size = BOOT_STACK_SIZE;

	// Free the boot stack
	// Find a better place to free the boot stack
	// FIXME: QC Gunyah issue #44
	error_t err = partition_add_heap(
		private, partition_image_virt_to_phys((uintptr_t)stack_bottom),
		stack_size);
	if (err != OK) {
		panic("Error freeing stack to hypervisor partition");
	}
}

noreturn void
boot_secondary_init(cpu_index_t cpu) LOCK_IMPL
{
	// We can't trace/log early because the CPU index and preemption count
	// in the thread are still uninitialized

	trigger_boot_cpu_early_init_event();
	trigger_boot_cpu_cold_init_event(cpu);

	// It's safe to log now.
	TRACE_AND_LOG(INFO, WARN, "secondary cpu ({:d}) cold boot",
		      (register_t)cpu);

	trigger_boot_cpu_warm_init_event();
	trigger_boot_cpu_start_event();

	TRACE_LOCAL(DEBUG, INFO, "cpu cold boot complete");
	thread_boot_set_idle();
}

// Warm (second or later) power-on of any CPU.
noreturn void
boot_warm_init(void) LOCK_IMPL
{
	trigger_boot_cpu_early_init_event();
	TRACE_LOCAL(INFO, INFO, "cpu warm boot start");
	trigger_boot_cpu_warm_init_event();
	trigger_boot_cpu_start_event();
	TRACE_LOCAL(DEBUG, INFO, "cpu warm boot complete");
	thread_boot_restore_frozen();
}

static error_t
boot_do_memdb_walk(paddr_t base, size_t size, void *arg)
{
	qcbor_enc_ctxt_t *qcbor_enc_ctxt = (qcbor_enc_ctxt_t *)arg;

	if ((size == 0U) && (util_add_overflows(base, size - 1U))) {
		return ERROR_ARGUMENT_SIZE;
	}

	QCBOREncode_OpenArray(qcbor_enc_ctxt);

	QCBOREncode_AddUInt64(qcbor_enc_ctxt, base);
	QCBOREncode_AddUInt64(qcbor_enc_ctxt, size);

	QCBOREncode_CloseArray(qcbor_enc_ctxt);

	return OK;
}

error_t
boot_add_free_range(uintptr_t object, memdb_type_t type,
		    qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	error_t ret;

	QCBOREncode_OpenArrayInMap(qcbor_enc_ctxt, "free_ranges");

	ret = memdb_walk(object, type, boot_do_memdb_walk,
			 (void *)qcbor_enc_ctxt);

	QCBOREncode_CloseArray(qcbor_enc_ctxt);

	return ret;
}

void
boot_start_hypervisor_handover(void)
{
	trigger_boot_hypervisor_handover_event();
}
