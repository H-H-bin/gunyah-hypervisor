// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypconstants.h>

#include <atomic.h>
#include <compiler.h>
#include <cpulocal.h>
#include <panic.h>
#include <partition.h>
#include <platform_timer.h>
#include <smc_trace.h>
#include <thread.h>
#include <util.h>
#if defined(INTERFACE_VCPU)
#include <vcpu.h>
#endif

#include <asm/prefetch.h>

extern smc_trace_t *hyp_smc_trace;
smc_trace_t	   *hyp_smc_trace;

void
smc_trace_init(partition_t *partition)
{
	if (hyp_smc_trace != NULL) {
		panic("smc_trace_init already initialized");
	}

	void_ptr_result_t alloc_ret = partition_alloc(
		partition, sizeof(*hyp_smc_trace), alignof(*hyp_smc_trace));
	if (alloc_ret.e != OK) {
		panic("Error allocating smc trace buffer");
	}

	hyp_smc_trace = (smc_trace_t *)alloc_ret.r;
	(void)memset_s(hyp_smc_trace, sizeof(*hyp_smc_trace), 0,
		       sizeof(*hyp_smc_trace));
}

void
smc_trace_log(smc_trace_id_t id, register_t (*registers)[SMC_TRACE_REG_MAX],
	      count_t	     num_registers)
{
	if (hyp_smc_trace == NULL) {
		goto out;
	}

	assert(num_registers <= SMC_TRACE_REG_MAX);
	uint64_t timestamp = platform_timer_get_current_ticks();

	cpu_index_t pcpu = cpulocal_get_index_unsafe();
	cpu_index_t vcpu = 0U;
	vmid_t	    vmid = 0U;

#if defined(INTERFACE_VCPU)
	thread_t *current = thread_get_self();

	if (compiler_expected(vcpu_is_vcpu(current))) {
		assert(current->addrspace != NULL);
		vmid = current->addrspace->vmid;
		vcpu = current->psci_index;
	}
#endif

	index_t cur_idx = atomic_fetch_add_explicit(&hyp_smc_trace->next_idx, 1,
						    memory_order_relaxed);
	// If we reach the end, wrap the next_idx
	if (compiler_unexpected(cur_idx >= HYP_SMC_LOG_NUM)) {
		index_t cur_head = cur_idx + 1U;
		do {
			(void)atomic_compare_exchange_ll_sc_weak(
				&hyp_smc_trace->next_idx, &cur_head,
				cur_head - HYP_SMC_LOG_NUM);
		} while (cur_head >= HYP_SMC_LOG_NUM);

		cur_idx -= HYP_SMC_LOG_NUM;
		assert(cur_idx < HYP_SMC_LOG_NUM);
	}

	smc_trace_entry_t *entry = &hyp_smc_trace->entries[cur_idx];

	prefetch_store_stream(entry);

	entry->id	 = id;
	entry->pcpu	 = (uint8_t)pcpu;
	entry->vcpu	 = (uint8_t)vcpu;
	entry->vmid	 = vmid;
	entry->regs	 = (uint8_t)num_registers;
	entry->timestamp = timestamp;

	num_registers = util_min(num_registers, SMC_TRACE_REG_MAX);

	for (count_t i = 0; i < num_registers; i++) {
		entry->x[i] = (*registers)[i];
	}
	for (count_t i = num_registers; i < SMC_TRACE_REG_MAX; i++) {
		entry->x[i] = 0U;
	}

out:
	return;
}
