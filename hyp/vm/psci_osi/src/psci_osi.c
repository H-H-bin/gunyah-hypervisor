// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>
#include <hyprights.h>

#include <atomic.h>
#include <bitmap.h>
#include <compiler.h>
#include <cpulocal.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <idle.h>
#include <ipi.h>
#include <list.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition_alloc.h>
#include <platform_cpu.h>
#include <platform_psci.h>
#include <power.h>
#include <preempt.h>
#include <psci.h>
#include <rcu.h>
#include <scheduler.h>
#include <spinlock.h>
#include <task_queue.h>
#include <thread.h>
#include <timer_queue.h>
#include <trace.h>
#include <trace_helpers.h>
#include <util.h>
#include <vcpu.h>
#include <vic.h>
#include <virq.h>

#include <events/power.h>
#include <events/psci.h>
#include <events/vpm.h>

#include "event_handlers.h"
#include "psci_arch.h"
#include "psci_common.h"
#include "psci_helper.h"
#include "psci_pm_list.h"
#include "vpm_base.h"

static ticks_t psci_retry_denied_min_delay_ticks;

CPULOCAL_DECLARE_STATIC(psci_retry_denied_t, psci_retry_denied);

extern list_t partition_list;

void
psci_osi_handle_boot_cold_init(void)
{
	error_t ret = platform_psci_set_suspend_mode(PSCI_MODE_OSI);
	assert(ret == OK);

	psci_retry_denied_min_delay_ticks =
		timer_convert_ns_to_ticks(PSCI_RETRY_DENIED_MIN_DELAY_NS);
	assert(psci_retry_denied_min_delay_ticks != 0U);
}

void
psci_osi_handle_boot_cpu_cold_init(cpu_index_t cpu)
{
	CPULOCAL_BY_INDEX(psci_retry_denied, cpu).count = 0U;
	timer_init_object(&CPULOCAL_BY_INDEX(psci_retry_denied, cpu).timer,
			  TIMER_ACTION_PSCI_RETRY_DENIED);
}

uint32_t
psci_cpu_suspend_features(void)
{
	uint32_t ret;

#if defined(PLATFORM_PSCI_USE_ORIGINAL_POWERSTATE_FORMAT)
	// If any platform's need to use the old PSCI power-state IDs, we may
	// need to do some refactoring to separate the virtual-PSCI stateID
	// types from platform types, possibly defining both formats.
	//
	// The below returned values are the virtual PSCI exposed to VMs.
#error unimplemented
#endif
	thread_t *vcpu = thread_get_self();

	if (vcpu_option_flags_get_hlos_vm(&vcpu->vcpu_options)) {
		// HLOS supports OS-Initiated mode, extended StateID
		ret = 3U;
	} else {
		// Only Platform Co-ordinated mode, extended StateID
		ret = 2U;
	}

	return ret;
}

psci_suspend_powerstate_stateid_t
psci_vcpu_get_poweroff_state(thread_t *vcpu, cpu_index_t cpu,
			     register_t online_cpus)
{
	psci_suspend_powerstate_stateid_t stateid;

	// FIXME: QC Gunyah issue #129
	if (vcpu->vpm_group->psci_mode == PSCI_MODE_PC) {
		stateid = platform_psci_deepest_level_stateid(
			cpu, PLATFORM_MAX_HIERARCHY);
	} else {
#if PLATFORM_MAX_HIERARCHY == 2
		bool running = false;
		assert(cpu == vcpu->psci_index);
		register_t cluster_mask = platform_cluster_mask(cpu);
		running			= (cluster_mask & online_cpus) != 0U;

		if (!running) {
			stateid = platform_psci_deepest_level_stateid(cpu, 1);
		} else {
			stateid = platform_psci_deepest_level_stateid(cpu, 0);
		}
#else
		(void)online_cpus;
		stateid = platform_psci_deepest_level_stateid(cpu, 0);
#endif
	}

	return stateid;
}

static void
psci_osi_trace_state_change(const char		     *event,
			    vpm_group_suspend_state_t old_state,
			    vpm_group_suspend_state_t new_state)
{
	thread_t *current = thread_get_self();

	for (index_t idx = 1; idx <= PLATFORM_MAX_HIERARCHY; idx++) {
		if (psci_get_level_state(&old_state, idx) !=
		    psci_get_level_state(&new_state, idx)) {
			TRACE(PSCI, PSCI_VPM_STATE_CHANGED,
			      "PSCI {:s} VM {:d} - Level= {:d}: {:#x} -> {:#x}",
			      (uintptr_t)event, current->addrspace->vmid, idx,
			      psci_get_level_state(&old_state, idx),
			      psci_get_level_state(&new_state, idx));
		}
	}
}

// Same as psci_cpu_suspend, but it sets the suspend state to the deepest
// cluster-level.
static psci_ret_t
psci_system_suspend(paddr_t entry_point_address, register_t context_id)
	EXCLUDE_PREEMPT_DISABLED
{
	psci_ret_t   ret;
	thread_t    *current   = thread_get_self();
	vpm_group_t *vpm_group = current->vpm_group;

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	if ((current->vpm_mode != VPM_MODE_PSCI) &&
	    vpm_group->power_system_suspend) {
		ret = PSCI_RET_DENIED;
		goto out;
	}
#endif

	// Check if all other VCPUs in the VM are in poweroff state, triggered
	// by a psci_cpu_off call, and return DENIED otherwise.
	//
	// Since we don't permit non-psci calls to power-on vcpus, except for
	// the very first time starting a VM, we don't need to worry about CPUs
	// being powered on asynchronously here.

	register_t cpu_bit = util_bit(current->psci_index);
	static_assert(sizeof(vpm_group->psci_started_vcpus) ==
			      sizeof(register_t),
		      "psci cpu bitmap");
	register_t started_vcpus =
		atomic_load_relaxed(&vpm_group->psci_started_vcpus[0]);
	// The vcpu making this call should be online, all others in the VM
	// should be offline. Since CPUs can't power-on themselves, or be
	// powered on by another VM, this check is sufficient to check that all
	// VCPUs are off.
	// Note: psci_started_vcpus bits are cleared in the vcpu_stopped event,
	// after the vcpu has been blocked with SCHEDULER_BLOCK_VCPU_OFF
	if (started_vcpus != cpu_bit) {
		TRACE(PSCI, PSCI_PSTATE_VALIDATION,
		      "psci_system_suspend: DENIED - VM {:d}",
		      current->addrspace->vmid);
		ret = PSCI_RET_DENIED;
		goto out;
	}

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	// Check if the vpm_group is a platform system suspend controller.
	if (vpm_group->power_system_suspend) {
		// Trigger Power System Suspend events to power off all the
		// cores and make the platform's system suspend call.
		if (power_system_suspend() == OK) {
			TRACE_AND_LOG(DEBUG, INFO,
				      "power_system_suspend Exit - VM {:d}",
				      current->addrspace->vmid);
			ret = PSCI_RET_SUCCESS;
			goto out;
		} else {
			TRACE_AND_LOG(
				DEBUG, INFO,
				"power_system_suspend Entry Denied - VM {:d}",
				current->addrspace->vmid);
			ret = PSCI_RET_DENIED;
			goto out;
		}
	}
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND

	// Regular VM scope virtual system-suspend

	vpm_group_suspend_state_t new_state;
	vpm_group_suspend_state_t old_state =
		atomic_load_acquire(&current->vpm_group->psci_vm_suspend_state);

	// Set the system_suspend flag in the suspend_state indicating it is a
	// PSCI system suspend request.
	do {
		new_state = old_state;
		vpm_group_suspend_state_set_system_suspend(&new_state, true);

	} while (!atomic_compare_exchange_strong_explicit(
		&current->vpm_group->psci_vm_suspend_state, &old_state,
		new_state, memory_order_acq_rel, memory_order_acquire));

	psci_suspend_powerstate_t pstate = psci_suspend_powerstate_default();

	// FIXME: QC Gunyah issue #129
	cpulocal_begin();
	psci_suspend_powerstate_stateid_t stateid =
		platform_psci_deepest_level_stateid(cpulocal_get_index(),
						    PLATFORM_MAX_HIERARCHY);
	cpulocal_end();

	psci_suspend_powerstate_set_StateID(&pstate, stateid);
	psci_suspend_powerstate_set_StateType(
		&pstate, PSCI_SUSPEND_POWERSTATE_TYPE_POWERDOWN);

	ret = psci_suspend(pstate, entry_point_address, context_id);

out:
	return ret;
}

bool
psci_osi_system_suspend_32(uint32_t arg1, uint32_t arg2, uint32_t *ret0)
{
	bool	  handled;
	thread_t *current = thread_get_self();

	if (compiler_unexpected(current->vpm_group == NULL)) {
		handled = false;
	} else {
		psci_ret_t ret = psci_system_suspend(arg1, arg2);
		*ret0	       = (uint32_t)ret;
		handled	       = true;
	}

	return handled;
}

bool
psci_osi_system_suspend_64(uint64_t arg1, uint64_t arg2, uint64_t *ret0)
{
	bool	  handled;
	thread_t *current = thread_get_self();

	if (compiler_unexpected(current->vpm_group == NULL)) {
		handled = false;
	} else {
		psci_ret_t ret = psci_system_suspend(arg1, arg2);
		*ret0	       = (uint64_t)ret;
		handled	       = true;
	}

	return handled;
}

static psci_ret_t
psci_switch_suspend_mode(psci_mode_t new_mode)
{
	psci_ret_t ret = PSCI_RET_SUCCESS;

	thread_t    *thread    = thread_get_self();
	vpm_group_t *vpm_group = thread->vpm_group;
	cpu_index_t  psci_idx;

	assert(vpm_group != NULL);

	rcu_read_start();

	vpm_group_suspend_state_t vm_state =
		atomic_load_acquire(&vpm_group->psci_vm_suspend_state);
	uint64_t vcpus_state =
		vpm_group_suspend_state_get_level_0_state(&vm_state);

	cpu_index_t cpu_index = 0U;
	index_t	    cpu_state;

	for (index_t i = 0U; i < util_array_size(vpm_group->psci_cpus); i++) {
		thread = atomic_load_consume(&vpm_group->psci_cpus[i]);
		if (thread == NULL) {
			continue;
		}

		psci_idx = thread->psci_index;
		if (psci_idx == cpu_index) {
			continue;
		}
		cpu_state =
			(index_t)((vcpus_state >>
				   (psci_idx * PLATFORM_PSCI_L0_STATE_BITS)) &
				  PLATFORM_PSCI_L0_MASK);
		if ((((new_mode == PSCI_MODE_OSI) &&
		      !scheduler_is_blocked(thread, SCHEDULER_BLOCK_VCPU_OFF) &&
		      !platform_psci_is_node_active(cpu_state, 0U)) ||
		     ((new_mode == PSCI_MODE_PC) &&
		      !scheduler_is_blocked(thread,
					    SCHEDULER_BLOCK_VCPU_OFF)))) {
			ret = PSCI_RET_DENIED;
			TRACE(PSCI, PSCI_PSTATE_VALIDATION,
			      "psci_switch_suspend_mode: DENIED - MODE {:d}, CPU {:d}, CPU_STATE {:d}",
			      new_mode, psci_idx, cpu_state);
			goto out;
		}
	}

	// If conditions met, change suspend mode of vpm group
	vpm_group->psci_mode = new_mode;

out:
	rcu_read_finish();

	return ret;
}

bool
psci_osi_set_suspend_mode(uint32_t arg1, uint32_t *ret0)
{
	bool	   handled;
	psci_ret_t ret;

	thread_t *current = thread_get_self();

	if (current->vpm_group == NULL) {
		handled = false;
		goto out_err;
	}

	if ((psci_mode_t)arg1 == current->vpm_group->psci_mode) {
		ret = PSCI_RET_SUCCESS;
		goto out_ret;
	}

	switch ((psci_mode_t)arg1) {
	case PSCI_MODE_PC:
	case PSCI_MODE_OSI:
		ret = psci_switch_suspend_mode((psci_mode_t)arg1);
		if (ret == PSCI_RET_DENIED) {
			TRACE(PSCI, INFO,
			      "psci_set_suspend_mode - DENIED - VM {:d}",
			      current->addrspace->vmid);
		}

		if (ret == PSCI_RET_DENIED) {
			TRACE(PSCI, INFO,
			      "psci_set_suspend_mode - DENIED - VM {:d}",
			      current->addrspace->vmid);
		}

		break;
	default:
		ret = PSCI_RET_INVALID_PARAMETERS;
		TRACE(PSCI, INFO,
		      "psci_set_suspend_mode - INVALID_PARAMETERS - VM {:d}",
		      current->addrspace->vmid);
		break;
	}

out_ret:
	*ret0	= (uint32_t)ret;
	handled = true;
out_err:
	return handled;
}

error_t
psci_osi_handle_object_activate_vpm_group(vpm_group_t *vpm_group)
{
	spinlock_init(&vpm_group->psci_lock);
	task_queue_init(&vpm_group->psci_virq_task);

	// Default psci mode to be platfom-coordinated
	vpm_group->psci_mode = PSCI_MODE_PC;

	// Initialize vcpus and cluster states of the vpm to the deepest
	// suspend state
	vpm_group->psci_system_suspend_count = 1;
	// FIXME: QC Gunyah issue #129
	cpulocal_begin();
	index_t cpu_num = cpulocal_get_index();
	cpulocal_end();

	index_t			  lpm_state, node_counts;
	vpm_group_suspend_state_t vm_state = vpm_group_suspend_state_default();

	for (index_t level = 0; level <= PLATFORM_MAX_HIERARCHY; level++) {
		lpm_state   = platform_psci_deepest_lpm_state(cpu_num, level);
		node_counts = psci_get_level_node_count(level);
		for (index_t counts = 0; counts < node_counts; counts++) {
			psci_set_level_state(&vm_state, level, counts,
					     lpm_state);
		}
	}

	vpm_group_suspend_state_set_system_suspend(&vm_state, true);

	atomic_store_release(&vpm_group->psci_vm_suspend_state, vm_state);

	return OK;
}

vpm_state_t
vpm_get_state(vpm_group_t *vpm_group)
{
	vpm_state_t vpm_state = VPM_STATE_NO_STATE;

	vpm_group_suspend_state_t vm_state =
		atomic_load_acquire(&vpm_group->psci_vm_suspend_state);

	if (vcpus_state_is_any_awake(vm_state, PLATFORM_MAX_HIERARCHY, 0)) {
		vpm_state = VPM_STATE_RUNNING;
	} else {
		if (vpm_group_suspend_state_get_system_suspend(&vm_state)) {
			// System suspend state was set
			vpm_state = VPM_STATE_SYSTEM_SUSPENDED;
		} else {
			vpm_state = VPM_STATE_CPUS_SUSPENDED;
		}
	}

	return vpm_state;
}

static void
psci_trigger_virq(vpm_group_t *vpm_group)
{
	(void)object_get_vpm_group_additional(vpm_group);
	if (!task_queue_schedule(&vpm_group->psci_virq_task,
				 TASK_QUEUE_CLASS_VPM_GROUP_VIRQ)) {
		object_put_vpm_group(vpm_group);
	}
}

/*
 * this clears the vcpu state for core which has started to boot from
 * hw followed by firmware cluster and suspend states are still
 * cleared by the same in wake-up path by calling into psci_vcpu_wakeup
 */
void
psci_vcpu_clear_vcpu_state(thread_t *thread)
{
	if (thread->vpm_mode != VPM_MODE_PSCI) {
		goto out;
	}

	assert(thread->vpm_group != NULL);

	vpm_group_t *vpm_group = thread->vpm_group;
	cpu_index_t  vcpu_id   = thread->psci_index;

	vpm_group_suspend_state_t old_state =
		atomic_load_acquire(&vpm_group->psci_vm_suspend_state);

	vpm_group_suspend_state_t new_state;

	do {
		new_state = old_state;
		psci_set_level_state(&new_state, 0, vcpu_id, 0U);

	} while (!atomic_compare_exchange_strong_explicit(
		&vpm_group->psci_vm_suspend_state, &old_state, new_state,
		memory_order_acq_rel, memory_order_acquire));

out:
	// Nothing to do for non PSCI threads
	return;
}

void
psci_vcpu_resume(thread_t *thread)
{
	assert(thread->vpm_mode != VPM_MODE_NONE);

	uint32_t node_index		 = 0U;
	bool	 vpm_resume_from_suspend = false;
	scheduler_lock_nopreempt(thread);
	psci_vpm_active_vcpus_get(scheduler_get_active_affinity(thread),
				  thread);
	scheduler_unlock_nopreempt(thread);

	if (thread->vpm_mode != VPM_MODE_PSCI) {
		goto out;
	}

	assert(thread->vpm_group != NULL);

	vpm_group_t *vpm_group = thread->vpm_group;
	cpu_index_t  vcpu_id   = thread->psci_index;

	thread->psci_suspend_state = psci_suspend_powerstate_default();

	vpm_group_suspend_state_t old_state =
		atomic_load_relaxed(&vpm_group->psci_vm_suspend_state);
	vpm_group_suspend_state_t new_state;

	do {
		new_state = old_state;
		psci_set_level_state(&new_state, 0, vcpu_id, 0U);

		// Leave the cluster bits set for PC mode (until subsequent fix
		// for PC is added in)
		if (thread->vpm_group->psci_mode != PSCI_MODE_PC) {
			for (index_t lvl = 1U; lvl < PLATFORM_MAX_HIERARCHY;
			     lvl++) {
				node_index = platform_psci_get_level_index(
					vcpu_id, lvl);
				psci_set_level_state(&new_state, lvl,
						     node_index, 0U);
			}
		}

		// for last level
		node_index = platform_psci_get_level_index(
			vcpu_id, PLATFORM_MAX_HIERARCHY);
		psci_set_level_state(&new_state, PLATFORM_MAX_HIERARCHY,
				     node_index, 0U);

		vpm_group_suspend_state_set_system_suspend(&new_state, false);

	} while (!atomic_compare_exchange_strong_explicit(
		&vpm_group->psci_vm_suspend_state, &old_state, new_state,
		memory_order_relaxed, memory_order_relaxed));

	vpm_resume_from_suspend =
		(psci_get_level_state(&old_state, PLATFORM_MAX_HIERARCHY) !=
		 0U);
	if (vpm_resume_from_suspend) {
		// Prevent reordering of the above psci_vm_suspend_state update
		// past the psci_system_suspend_count update below. Matches the
		// fence in psci_vcpu_suspend().
		atomic_thread_fence(memory_order_seq_cst);

		spinlock_acquire_nopreempt(&vpm_group->psci_lock);

		assert((vpm_group->psci_system_suspend_count == 0) ||
		       (vpm_group->psci_system_suspend_count == 1));
		vpm_group->psci_system_suspend_count--;

		if (vpm_group->psci_system_suspend_count == 0) {
			TRACE(PSCI, PSCI_VPM_SYSTEM_RESUME,
			      "psci VM {:d} - system resume",
			      thread->addrspace->vmid);

			if (vpm_group_suspend_state_get_system_suspend(
				    &old_state)) {
				psci_trigger_virq(vpm_group);
			}

			trigger_vpm_system_resume_event(thread);
		}

		spinlock_release_nopreempt(&vpm_group->psci_lock);
	}

	psci_osi_trace_state_change("resume", old_state, new_state);

out:
	// Nothing to do for non PSCI threads
	return;
}

static error_t
osi_mode_suspend_state_validation_at_level(thread_t		    *current,
					   vpm_group_suspend_state_t new_state,
					   index_t		     level)
{
	error_t	 ret	   = OK;
	uint32_t start_idx = 0, children_counts = 0;

	cpu_index_t vcpu_id = current->psci_index;
	ret = platform_psci_get_index_by_level(vcpu_id, &start_idx,
					       &children_counts, level);
	if (ret != OK) {
		goto out;
	}

	index_t child_level = level - 1U;

	index_t state_bit = 0U, state_mask = 0U;
	psci_get_level_bits_mask(child_level, &state_bit, &state_mask);
	uint64_t lpm_state = psci_get_level_state(&new_state, child_level);

	index_t idle_state = 0U, psci_index;
	for (index_t i = 0U; i < children_counts; i++) {
		psci_index = start_idx + i;
		idle_state = (index_t)(lpm_state >> (psci_index * state_bit)) &
			     state_mask;
		if (platform_psci_is_node_active(idle_state, child_level)) {
			ret = ERROR_DENIED;
			goto out;
		}
	}

out:
	return ret;
}

static bool
psci_vcpu_check_all_vcpus(thread_t *current, vpm_group_suspend_state_t state)
{
	assert(current->vpm_group != NULL);

	vpm_group_t *vpm_group = current->vpm_group;
	uint64_t     lpm_state = psci_get_level_state(&state, 0U);
	uint64_t     idle_state;
	// Default value is true
	bool vpm_suspend = true;

	// Check every individual VCPU, because
	// PC mode doesn't set cluster states
	// correctly yet
	for (index_t i = 0; i < PLATFORM_MAX_CORES; i++) {
		thread_t *vcpu = atomic_load_consume(&vpm_group->psci_cpus[i]);
		if (vcpu == NULL) {
			continue;
		}
		idle_state = (uint64_t)((lpm_state >>
					 (i * PLATFORM_PSCI_L0_STATE_BITS)) &
					PLATFORM_PSCI_L0_MASK);
		// checking if no vcpus active
		// in entire vm
		if (platform_psci_is_node_active((index_t)idle_state, 0U)) {
			TRACE(PSCI, PSCI_VPM_SYSTEM_SUSPEND,
			      "psci VM {:d} - cpu {:d} active while vpm_suspend",
			      current->addrspace->vmid, i);
			vpm_suspend = false;
			break;
		}
	}

	return vpm_suspend;
}

static error_t
psci_check_vcpu_vm_suspend(thread_t		     *current,
			   vpm_group_suspend_state_t *new_state,
			   bool			     *vpm_suspend)
{
	error_t			  ret;
	vpm_group_suspend_state_t old_state;

	assert(current->vpm_group != NULL);

	vpm_group_t *vpm_group = current->vpm_group;
	cpu_index_t  vcpu_id   = current->psci_index;

	index_t shallowest = 0U;

	index_t cpu_state =
		platform_psci_get_lpm_state(current->psci_suspend_state, 0);
	index_t lpm_state = platform_psci_get_lpm_state(
		current->psci_suspend_state, PLATFORM_MAX_HIERARCHY);

	ret = OK;

	// Atomically update vm_suspend_state
	if (current->vpm_group->psci_mode == PSCI_MODE_PC) {
		// Set vcpus_state of corresponding cpu and if no vcpus is left
		// active, then iterate through the VCPUs of the VM and update
		// cluster_suspend_state with the shallowest state.
		//
		// We must use acquire/release ordering in this loop; the
		// acquire is to guarantee that we read the other VCPUs'
		// psci_suspend_state variables after determining that we are
		// the last VCPU, and the release is to ensure that the same
		// loop running on other suspending VCPUs sees our correct
		// psci_suspend_state before relying on this VCPU being
		// suspended.

		old_state =
			atomic_load_acquire(&vpm_group->psci_vm_suspend_state);

		do {
			*vpm_suspend = false;
			*new_state   = old_state;

			psci_set_level_state(new_state, 0, vcpu_id, cpu_state);

			if (!vcpus_state_is_any_awake(*new_state, 1, vcpu_id)) {
				// last cores in cluster
				shallowest = lpm_state;

				rcu_read_start();

				for (index_t i = 0; i < PLATFORM_MAX_CORES;
				     i++) {
					thread_t *vcpu = atomic_load_consume(
						&vpm_group->psci_cpus[i]);
					if (vcpu == NULL) {
						continue;
					}

					index_t sy = platform_psci_get_lpm_state(
						vcpu->psci_suspend_state,
						PLATFORM_MAX_HIERARCHY);

					shallowest =
						platform_psci_shallowest_lpm_state(
							shallowest, sy);
				}

				rcu_read_finish();

				*vpm_suspend = (shallowest != 0U);

				if (*vpm_suspend) {
					*vpm_suspend =
						psci_vcpu_check_all_vcpus(
							current, *new_state);
				}

				psci_set_level_state(new_state,
						     PLATFORM_MAX_HIERARCHY, 0U,
						     shallowest);
			}

			// Clear system suspend flag if we cannot request it
			if (!(*vpm_suspend)) {
				vpm_group_suspend_state_set_system_suspend(
					new_state, false);
			}

		} while (!atomic_compare_exchange_strong_explicit(
			&vpm_group->psci_vm_suspend_state, &old_state,
			*new_state, memory_order_acq_rel,
			memory_order_acquire));
	} else {
		// Set vcpus_state of corresponding cpu and update
		// cluster_suspend_state with requested state. If we are
		// requesting a cluster power off state and a cpu in vcpus_state
		// is not in cpu off state or if we are requesting a cluster
		// retention state and a cpu in vcpus_state is in cpu active
		// state, return DENIED and do not update vm_suspend_state.
		*vpm_suspend = (lpm_state != 0U);

		old_state =
			atomic_load_relaxed(&vpm_group->psci_vm_suspend_state);

		do {
			// Start with cluster-level validation
			*new_state = old_state;
			psci_set_level_state(new_state, 0, vcpu_id, cpu_state);

			for (index_t level = 1; level <= PLATFORM_MAX_HIERARCHY;
			     level++) {
				index_t idle_state = platform_psci_get_lpm_state(
					current->psci_suspend_state, level);
				if (idle_state != 0U) {
					ret = osi_mode_suspend_state_validation_at_level(
						current, *new_state, level);
					if (ret != OK) {
						goto out;
					}
					index_t node_index =
						platform_psci_get_level_index(
							vcpu_id, level);
					psci_set_level_state(new_state, level,
							     node_index,
							     idle_state);
				}
			}
		} while (!atomic_compare_exchange_strong_explicit(
			&vpm_group->psci_vm_suspend_state, &old_state,
			*new_state, memory_order_relaxed,
			memory_order_relaxed));
	}
	psci_osi_trace_state_change("suspend", old_state, *new_state);
out:
	return ret;
}

error_t
psci_vcpu_suspend(thread_t *current)
{
	error_t ret;

	assert(current->vpm_mode != VPM_MODE_NONE);

	// Decrement refcount of the PCPU
	scheduler_lock_nopreempt(current);
	psci_vpm_active_vcpus_put(scheduler_get_active_affinity(current),
				  current);
	scheduler_unlock_nopreempt(current);

	if (current->vpm_mode != VPM_MODE_PSCI) {
		ret = OK;
		goto out;
	}

	assert(current->vpm_group != NULL);

	vpm_group_t *vpm_group = current->vpm_group;

	bool vpm_suspend = false;

	vpm_group_suspend_state_t new_state = { 0 };

	ret = psci_check_vcpu_vm_suspend(current, &new_state, &vpm_suspend);

	if (ret != OK) {
		goto out;
	}

	if (vpm_suspend) {
		// Prevent reordering of the above psci_vm_suspend_state update
		// past the psci_system_suspend_count update below. Matches the
		// fence in psci_vcpu_resume().
		atomic_thread_fence(memory_order_seq_cst);

		spinlock_acquire_nopreempt(&vpm_group->psci_lock);

		assert((vpm_group->psci_system_suspend_count == 0) ||
		       (vpm_group->psci_system_suspend_count == -1));
		vpm_group->psci_system_suspend_count++;

		if (vpm_group->psci_system_suspend_count > 0) {
			TRACE(PSCI, PSCI_VPM_SYSTEM_SUSPEND,
			      "psci VM {:d} - system suspend",
			      current->addrspace->vmid);

			if (platform_psci_check_vpm_threshold(
				    current->psci_suspend_state,
				    vpm_group->power_state_threshold) ||
			    vpm_group_suspend_state_get_system_suspend(
				    &new_state)) {
				TRACE_AND_LOG(ERROR, WARN,
					      "psci_trigger_virq --->");
				psci_trigger_virq(vpm_group);

				if (vpm_group_option_flags_get_explicit_wakeup(
					    &vpm_group->options)) {
					// scheduler block vcpu with psci system
					// suspend
					scheduler_lock_nopreempt(current);
					scheduler_block(
						current,
						SCHEDULER_BLOCK_PSCI_SYSTEM_SUSPEND);
					scheduler_unlock_nopreempt(current);
				}
			}

			trigger_vpm_system_suspend_event(current);
		}

		spinlock_release_nopreempt(&vpm_group->psci_lock);
	}

out:
	// If function returns denied, we must undo what has been done here.
	// This is we need to get the reference of the vcpus again and update
	// the vcpus state
	if (ret != OK) {
		psci_vcpu_resume(current);
	}

	return ret;
}

static void
get_vpm_groups_aggregated_state(cpu_index_t		   cpu_index,
				psci_suspend_powerstate_t *suspend_state,
				index_t			   last_cpu_level)
{
	index_t level	  = last_cpu_level;
	list_t *part_list = &partition_list;

	index_t shallowest_state[PLATFORM_MAX_HIERARCHY + 1U] = { 0 };

	rcu_read_start();

	// only core level lpms applicable
	if (level == 0U) {
		goto out;
	}

	// initialize to deepest possible mode
	for (index_t lvl = 1U; lvl <= PLATFORM_MAX_HIERARCHY; lvl++) {
		shallowest_state[lvl] =
			platform_psci_deepest_lpm_state(cpu_index, lvl);
	}

	LIST_FOREACH_CONTAINER_CONSUME_BEGIN(partition_t, part_list, partition,
					     partition_list_node, partition)
		list_t *group_list = &partition->vpm_group_list;

		LIST_FOREACH_CONTAINER_CONSUME_BEGIN(vpm_group_t, group_list,
						     vpm_group,
						     vpm_group_list_node, group)
			vpm_group_suspend_state_t vm_state =
				atomic_load_acquire(
					&group->psci_vm_suspend_state);

			for (index_t lvl = 1U; lvl <= PLATFORM_MAX_HIERARCHY;
			     lvl++) {
				uint32_t node_index =
					platform_psci_get_level_index(cpu_index,
								      lvl);
				index_t lpm_state =
					(index_t)psci_get_level_state(&vm_state,
								      lvl);
				index_t state_bits = 0U, state_mask = 0U;
				psci_get_level_bits_mask(lvl, &state_bits,
							 &state_mask);

				index_t idle_state =
					((lpm_state >>
					  (node_index * state_bits)) &
					 state_mask);

				shallowest_state[lvl] =
					platform_psci_shallowest_lpm_state(
						shallowest_state[lvl],
						idle_state);

				if (shallowest_state[lvl] == 0U) {
					level = lvl - 1U;
					goto out;
				}
			}
		LIST_FOREACH_CONTAINER_CONSUME_END
	LIST_FOREACH_CONTAINER_CONSUME_END

out:
	for (index_t lvl = 1U; lvl <= level; lvl++) {
		platform_psci_set_lpm_state(suspend_state,
					    shallowest_state[lvl], lvl);
	}
	platform_psci_set_last_cpu_level(suspend_state, level);
	rcu_read_finish();
}

idle_state_t
psci_osi_handle_idle_yield(bool in_idle_thread)
{
	assert_preempt_disabled();

	idle_state_t idle_state = IDLE_STATE_IDLE;

	if (!in_idle_thread) {
		goto out;
	}

	if (rcu_has_pending_updates()) {
		goto out;
	}

	cpu_index_t cpu = cpulocal_get_index();

	// Check if there is any vcpu running in this cpu
	if (!psci_vpm_active_vcpus_is_zero(cpu)) {
		goto out;
	}

	// Check if we are waiting to retry after being denied by EL3
	psci_retry_denied_t *retry_denied =
		&CPULOCAL_BY_INDEX(psci_retry_denied, cpu);
	if (timer_is_queued(&retry_denied->timer)) {
		goto out;
	}

	list_t *psci_pm_list = psci_pm_list_get_self();
	assert(psci_pm_list != NULL);

	psci_suspend_powerstate_t pstate = psci_suspend_powerstate_default();
	index_t cpu_state = platform_psci_deepest_lpm_state(cpu, 0);

	// Iterate through affine VCPUs and get the shallowest cpu-level state
	rcu_read_start();
	LIST_FOREACH_CONTAINER_CONSUME_BEGIN(thread_t, psci_pm_list, thread,
					     psci_pm_list_node, vcpu)
		index_t cpu1 = platform_psci_get_lpm_state(
			vcpu->psci_suspend_state, 0);
		cpu_state = platform_psci_shallowest_lpm_state(cpu_state, cpu1);
	LIST_FOREACH_CONTAINER_CONSUME_END
	rcu_read_finish();

	// Do not go to suspend if shallowest cpu state is zero. This may happen
	// if a vcpu has started after doing the initial check of 'any vcpu
	// running in this cpu' and therefore has been added to the psci_pm_list
	// with a psci_suspend_state of 0.
	if (cpu_state == 0U) {
		goto out;
	}

	platform_psci_set_lpm_state(&pstate, cpu_state, 0U);

	if (platform_psci_is_node_poweroff(cpu_state, 0U)) {
		psci_suspend_powerstate_set_StateType(
			&pstate, PSCI_SUSPEND_POWERSTATE_TYPE_POWERDOWN);
	} else {
		psci_suspend_powerstate_set_StateType(
			&pstate,
			PSCI_SUSPEND_POWERSTATE_TYPE_STANDBY_OR_RETENTION);
	}

	index_t last_cpu_level = psci_clear_vpm_active_pcpus_bit(cpu);
	if (last_cpu_level > 0U) {
		get_vpm_groups_aggregated_state(cpu, &pstate, last_cpu_level);
		if (last_cpu_level == PLATFORM_MAX_HIERARCHY) {
			TRACE(PSCI, PSCI_SYSTEM_SUSPEND,
			      "psci Last CPU to suspend");
		}
	}

	// Fence to prevent any power_cpu_suspend event handlers conditional on
	// last_cpu (especially the trigger of power_system_suspend) being
	// reordered before the psci_clear_vpm_active_pcpus_bit() above. This
	// matches the fence before the resume event below.
	atomic_thread_fence(memory_order_seq_cst);

	error_t suspend_result = trigger_power_cpu_suspend_event(
		pstate, psci_suspend_powerstate_get_StateType(&pstate) ==
				PSCI_SUSPEND_POWERSTATE_TYPE_POWERDOWN);
	if (suspend_result == OK) {
		bool_result_t ret;

		TRACE(PSCI, INFO, "psci power_cpu_suspend {:#x}",
		      psci_suspend_powerstate_raw(pstate));

		ret = platform_cpu_suspend(pstate);

		// Check if this is the first cpu to wake up
		bool first_cpu = psci_set_vpm_active_pcpus_bit(cpu);
		if (first_cpu) {
			TRACE(PSCI, PSCI_SYSTEM_RESUME,
			      "First CPU to wakeup from suspend");
		}
		suspend_result = ret.e;

		// Fence to prevent any power_cpu_resume event handlers
		// conditional on first_cpu (especially the trigger of
		// power_system_resume) being reordered before the
		// psci_set_vpm_active_pcpus_bit() above. This matches the
		// fence before the suspend event above.
		atomic_thread_fence(memory_order_seq_cst);

		trigger_power_cpu_resume_event((ret.e == OK) && ret.r,
					       first_cpu);
		TRACE(PSCI, INFO,
		      "psci power_cpu_suspend wakeup; poweroff {:d} system_resume {:d} error {:d}",
		      ret.r, first_cpu, (register_t)ret.e);
	} else {
		TRACE(PSCI, INFO, "psci power_cpu_suspend failed: {:d}",
		      (unsigned int)suspend_result);
		(void)psci_set_vpm_active_pcpus_bit(cpu);
	}

	if ((suspend_result == OK) || (suspend_result == ERROR_BUSY)) {
		// Return from successful suspend, or suspend failure due to a
		// pending wakeup. Poll for wakeup events.
		idle_state = idle_wakeup();

		// The suspend wasn't denied, so reset the backoff counter.
		retry_denied->count = 0U;
	} else if (suspend_result != ERROR_DENIED) {
		TRACE_AND_LOG(ERROR, WARN, "ERROR: psci suspend error {:d}",
			      (register_t)suspend_result);
		panic("unhandled suspend error");
	} else if (retry_denied->count >= PSCI_RETRY_DENIED_MAX_COUNT) {
		TRACE(PSCI, INFO,
		      "psci too many consecutive PSCI_DENIED errors, entering idle");
		retry_denied->count = 0U;
		// Continue to idle.
	} else {
		ticks_t delay = (psci_retry_denied_min_delay_ticks +
				 ((psci_retry_denied_min_delay_ticks * cpu) /
				  PLATFORM_MAX_CORES))
				<< retry_denied->count;
		TRACE(PSCI, INFO, "psci: delaying suspend by {:d} ticks",
		      delay);
		timer_enqueue(&retry_denied->timer,
			      timer_get_current_timer_ticks() + delay);
		retry_denied->count++;

		// suspend state was denied, re-run psci aggregation.
		idle_state = IDLE_STATE_WAKEUP;
	}

out:
	return idle_state;
}

error_t
psci_osi_handle_thread_context_switch_pre(void)
{
	if (thread_get_self()->kind == THREAD_KIND_IDLE) {
		psci_retry_denied_t *retry_denied =
			&CPULOCAL(psci_retry_denied);
		if (retry_denied->count > 0U) {
			timer_dequeue(&retry_denied->timer);
		}
		retry_denied->count = 0U;
	}

	return OK;
}
