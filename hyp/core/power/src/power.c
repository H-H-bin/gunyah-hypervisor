// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <bitmap.h>
#include <compiler.h>
#include <cpulocal.h>
#include <ipi.h>
#include <log.h>
#include <panic.h>
#include <platform_cpu.h>
#include <power.h>
#include <preempt.h>
#include <qcbor.h>
#include <rcu.h>
#include <scheduler.h>
#include <spinlock.h>
#include <timer_queue.h>
#include <trace.h>
#include <util.h>

#include <events/power.h>

#include <asm/event.h>

#include "event_handlers.h"

#if defined(MODULE_VM_ROOTVM)
#include <cspace.h>
#include <object.h>
#include <partition.h>
#include <partition_alloc.h>
#endif

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
#define ROOTVM_INIT 1U
#include <list.h>
#include <partition_init.h>
#include <platform_psci.h>
#include <platform_timer.h>
#include <thread.h>

#include <asm/barrier.h>

static _Atomic bool power_suspend_in_progress;
static spinlock_t   power_system_cores_lock;

// power off all the other cores than current core
static void
power_send_other_cores_power_off(void) REQUIRE_PREEMPT_DISABLED;
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND

// Trigger cpu suspend event for current cpu when entering
// an explicitly requested system or CPU level suspend state
static error_t
power_current_cpu_suspend(psci_suspend_powerstate_t pstate)
	REQUIRE_PREEMPT_DISABLED;
// Trigger cpu resume event for current cpu when exiting
// System Suspend or CPU level suspend state
static void
power_current_cpu_resume(bool was_poweroff) REQUIRE_PREEMPT_DISABLED;

static ticks_t power_cpu_on_retry_delay_ticks;

static spinlock_t power_system_lock;
static BITMAP_DECLARE(PLATFORM_MAX_CORES, power_system_running_cpus)
	PROTECTED_BY(power_system_lock);
static _Atomic count_t	      power_system_online_cpus;
static platform_power_state_t power_system_suspend_state
	PROTECTED_BY(power_system_lock);

CPULOCAL_DECLARE_STATIC(power_voting_t, power_voting);

// This is protected by the lock in the corresponding power_voting_t structure,
// but must remain a separate array because it is exposed in crash minidumps.
CPULOCAL_DECLARE_STATIC(cpu_power_state_t, power_state);

const cpu_power_state_array_t *
power_get_cpu_states_for_debug(void)
{
	return &cpulocal_power_state;
}

void
power_handle_boot_cold_init(cpu_index_t boot_cpu_index)
{
	power_cpu_on_retry_delay_ticks =
		timer_convert_ns_to_ticks(POWER_CPU_ON_RETRY_DELAY_NS);
	assert(power_cpu_on_retry_delay_ticks != 0U);

	for (cpu_index_t cpu = 0U; cpu < PLATFORM_MAX_CORES; cpu++) {
		spinlock_init(&CPULOCAL_BY_INDEX(power_voting, cpu).lock);
		spinlock_acquire_nopreempt(
			&CPULOCAL_BY_INDEX(power_voting, cpu).lock);

		timer_init_object(
			&CPULOCAL_BY_INDEX(power_voting, cpu).retry_timer,
			TIMER_ACTION_POWER_CPU_ON_RETRY);
		CPULOCAL_BY_INDEX(power_voting, cpu).retry_count = 0U;

		// Initialize the boot CPU's vote count to 1 while booting to
		// prevent the cpu going to suspend. This will be decremented
		// once the rootvm setup is completed and the rootvm VCPU has
		// voted to keep the boot core powered on.
		CPULOCAL_BY_INDEX(power_voting, cpu).vote_count =
			(cpu == boot_cpu_index) ? 1U : 0U;

		CPULOCAL_BY_INDEX(power_state, cpu) =
			(cpu == boot_cpu_index) ? CPU_POWER_STATE_COLD_BOOT
						: CPU_POWER_STATE_OFF;

		spinlock_release_nopreempt(
			&CPULOCAL_BY_INDEX(power_voting, cpu).lock);
	}

	spinlock_init(&power_system_lock);

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	spinlock_init(&power_system_cores_lock);
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND

	// FIXME: QC Gunyah issue #130
	spinlock_acquire_nopreempt(&power_system_lock);
	bitmap_set(power_system_running_cpus, (index_t)boot_cpu_index);
	spinlock_release_nopreempt(&power_system_lock);

	atomic_init(&power_system_online_cpus, 1U);
}

void
power_handle_boot_cpu_warm_init(void)
{
	spinlock_acquire_nopreempt(&CPULOCAL(power_voting).lock);

	while (CPULOCAL(power_state) == CPU_POWER_STATE_OFF) {
		spinlock_release_nopreempt(&CPULOCAL(power_voting).lock);
		spinlock_acquire_nopreempt(&CPULOCAL(power_voting).lock);
	}

	cpu_power_state_t state = CPULOCAL(power_state);

	assert((state == CPU_POWER_STATE_COLD_BOOT) ||
	       (state == CPU_POWER_STATE_STARTED) ||
	       (state == CPU_POWER_STATE_SUSPEND));
	CPULOCAL(power_state) = CPU_POWER_STATE_ONLINE;

	if (state == CPU_POWER_STATE_STARTED) {
		trigger_power_cpu_online_event();

		(void)atomic_fetch_add_explicit(&power_system_online_cpus, 1U,
						memory_order_release);
		asm_event_wake_updated();

#if defined(DISABLE_PSCI_CPU_OFF) && DISABLE_PSCI_CPU_OFF
		power_voting_t *voting = &CPULOCAL(power_voting);
		voting->vote_count++;
#endif
	}
	spinlock_release_nopreempt(&CPULOCAL(power_voting).lock);

	// FIXME: QC Gunyah issue #130
	spinlock_acquire_nopreempt(&power_system_lock);
	if (bitmap_empty(power_system_running_cpus, PLATFORM_MAX_CORES)) {
		// CPU_POWER_STATE_STARTED could be seen due to a
		// last-cpu-suspend/cpu_on race.
		assert((state == CPU_POWER_STATE_STARTED) ||
		       (state == CPU_POWER_STATE_SUSPEND));
		trigger_power_system_idle_exit_event(
			power_system_suspend_state);
	}
	bitmap_set(power_system_running_cpus, (index_t)cpulocal_get_index());
	spinlock_release_nopreempt(&power_system_lock);
}

error_t
power_handle_power_cpu_suspend(platform_power_state_t state)
{
	error_t	    err	   = OK;
	cpu_index_t cpu_id = cpulocal_get_index();

	// FIXME: QC Gunyah issue #130
	spinlock_acquire_nopreempt(&power_system_lock);
	bitmap_clear(power_system_running_cpus, (index_t)cpu_id);
	if (bitmap_empty(power_system_running_cpus, PLATFORM_MAX_CORES)) {
		power_system_suspend_state = state;
		err = trigger_power_system_idle_enter_event(state);
		if (err != OK) {
			bitmap_set(power_system_running_cpus, (index_t)cpu_id);
		}
	}
	spinlock_release_nopreempt(&power_system_lock);

	if (err == OK) {
		spinlock_acquire_nopreempt(&CPULOCAL(power_voting).lock);
		assert(CPULOCAL(power_state) == CPU_POWER_STATE_ONLINE);
		CPULOCAL(power_state) = CPU_POWER_STATE_SUSPEND;
		spinlock_release_nopreempt(&CPULOCAL(power_voting).lock);
	}

	return err;
}

void
power_handle_power_cpu_resume(bool was_poweroff)
{
	// A cpu that was warm booted updates its state in the cpu warm-boot
	// event.
	if (!was_poweroff) {
		spinlock_acquire_nopreempt(&CPULOCAL(power_voting).lock);
		assert(CPULOCAL(power_state) == CPU_POWER_STATE_SUSPEND);
		CPULOCAL(power_state) = CPU_POWER_STATE_ONLINE;
		spinlock_release_nopreempt(&CPULOCAL(power_voting).lock);

		// FIXME: QC Gunyah issue #130
		spinlock_acquire_nopreempt(&power_system_lock);
		if (bitmap_empty(power_system_running_cpus,
				 PLATFORM_MAX_CORES)) {
			trigger_power_system_idle_exit_event(
				power_system_suspend_state);
		}
		bitmap_set(power_system_running_cpus,
			   (index_t)cpulocal_get_index());
		spinlock_release_nopreempt(&power_system_lock);
	} else {
		spinlock_acquire_nopreempt(&power_system_lock);
		// power_system_running_cpus should be updated in the warm init
		// event.
		assert(!bitmap_empty(power_system_running_cpus,
				     PLATFORM_MAX_CORES));
		spinlock_release_nopreempt(&power_system_lock);
	}
}

static error_t
power_try_cpu_on(power_voting_t *voting, cpu_index_t cpu)
	REQUIRE_LOCK(voting -> lock)
{
	error_t ret;

	if (!platform_cpu_exists(cpu)) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}
	if (!platform_cpu_functional(cpu)) {
		ret = ERROR_FAILURE;
		goto out;
	}

	cpu_power_state_t *state      = &CPULOCAL_BY_INDEX(power_state, cpu);
	cpu_power_state_t  prev_state = *state;

	if ((prev_state != CPU_POWER_STATE_OFF) &&
	    (prev_state != CPU_POWER_STATE_OFFLINE)) {
		// CPU has already been started, or didn't get to power off.
		ret = OK;
		goto out;
	}

	// Mark the CPU as started so we don't call cpu_on twice.  Do this
	// before calling platform_cpu_on to avoid power-on races.
	*state = CPU_POWER_STATE_STARTED;

	ret = platform_cpu_on(cpu);

	if (ret == OK) {
		voting->retry_count = 0U;
		goto out;
	} else if ((ret == ERROR_RETRY) &&
		   (voting->retry_count < MAX_CPU_ON_RETRIES)) {
		// We are racing with a power-off, and it is too late to prevent
		// the power-off completing. We need to wait until power-off is
		// complete and then retry. Enqueue the retry timer, if it is
		// not already queued.
		if (!timer_is_queued(&voting->retry_timer)) {
			timer_enqueue(&voting->retry_timer,
				      timer_get_current_timer_ticks() +
					      power_cpu_on_retry_delay_ticks);
		}

		// If we're racing with power-off, that means the CPU is
		// functional and the power-on should not fail, so report
		// success to the caller. If the retry does fail, we panic.
		ret = OK;
	} else if (ret == ERROR_RETRY) {
		// We ran out of retry attempts.
		ret = ERROR_FAILURE;
	} else {
		// platform_cpu_on() failed and cannot be retried; just return
		// the error status.
	}

	// Restore the CPU state overwritten above.
	*state = prev_state;

out:
	return ret;
}

error_t
power_vote_cpu_on(cpu_index_t cpu)
{
	error_t ret;

	assert(cpulocal_index_valid(cpu));
	power_voting_t *voting = &CPULOCAL_BY_INDEX(power_voting, cpu);

	spinlock_acquire(&voting->lock);
	if (voting->vote_count == 0U) {
		ret = power_try_cpu_on(voting, cpu);
		if (ret != OK) {
			goto out;
		}
	}

	voting->vote_count++;
	ret = OK;

out:
	spinlock_release(&voting->lock);
	return ret;
}

void
power_vote_cpu_off(cpu_index_t cpu)
{
	assert(cpulocal_index_valid(cpu));
	power_voting_t *voting = &CPULOCAL_BY_INDEX(power_voting, cpu);

	spinlock_acquire(&voting->lock);
	assert(voting->vote_count > 0U);
	voting->vote_count--;

	if (voting->vote_count == 0U) {
		// Any outstanding retries can be cancelled.
		voting->retry_count = 0U;
		timer_dequeue(&voting->retry_timer);

		// Send an IPI to rerun the idle handlers in case the CPU
		// is already idle in WFI or suspend.
		ipi_one(IPI_REASON_IDLE, cpu);
	}
	spinlock_release(&voting->lock);
}

idle_state_t
power_handle_idle_yield(bool in_idle_thread)
{
	idle_state_t idle_state = IDLE_STATE_IDLE;

	if (!in_idle_thread) {
		goto out;
	}

	if (rcu_has_pending_updates()) {
		goto out;
	}

	power_voting_t *voting = &CPULOCAL(power_voting);
	spinlock_acquire_nopreempt(&voting->lock);
	if (voting->vote_count == 0U) {
		error_t err = OK;

		spinlock_acquire_nopreempt(&power_system_lock);
		cpu_index_t cpu_id = cpulocal_get_index();
		bitmap_clear(power_system_running_cpus, (index_t)cpu_id);
		if (bitmap_empty(power_system_running_cpus,
				 PLATFORM_MAX_CORES)) {
			power_system_suspend_state =
				(platform_power_state_t){ 0 };
			err = trigger_power_system_idle_enter_event(
				power_system_suspend_state);
			if (err != OK) {
				bitmap_set(power_system_running_cpus,
					   (index_t)cpu_id);
			}
		}
		spinlock_release_nopreempt(&power_system_lock);

		if (err == OK) {
			assert(CPULOCAL(power_state) == CPU_POWER_STATE_ONLINE);

			(void)atomic_fetch_sub_explicit(
				&power_system_online_cpus, 1U,
				memory_order_relaxed);

			while (asm_event_load_before_wait(
				       &power_system_online_cpus) == 0U) {
				// We should never offline all the CPUs,
				// however a CPU may offline while another is
				// powering on, and we may see the
				// power_system_running_cpus count being zero
				// spuriously.  Wait for another core to come
				// on-line before continuing.
				asm_event_wait(&power_system_online_cpus);
			}

			trigger_power_cpu_offline_event();
			CPULOCAL(power_state) = CPU_POWER_STATE_OFFLINE;
			spinlock_release_nopreempt(&voting->lock);

			platform_cpu_off();

			idle_state = IDLE_STATE_WAKEUP;
		} else {
			spinlock_release_nopreempt(&voting->lock);
		}
	} else {
		spinlock_release_nopreempt(&voting->lock);
	}

out:
	return idle_state;
}

bool
power_handle_timer_action(timer_t *timer)
{
	assert_debug(timer != NULL);

	power_voting_t *voting = power_voting_container_of_retry_timer(timer);
	assert(voting != NULL);
	cpu_index_t cpu = CPULOCAL_PTR_INDEX(power_voting, voting);

	spinlock_acquire_nopreempt(&voting->lock);
	error_t ret = OK;
	if (voting->vote_count > 0U) {
		voting->retry_count++;
		ret = power_try_cpu_on(voting, cpu);
	}
	spinlock_release_nopreempt(&voting->lock);

	if (ret != OK) {
		panic("Failed to power on a CPU that was previously on");
	}

	return true;
}

#if defined(MODULE_VM_ROOTVM)
void
power_handle_rootvm_init(cspace_t	  *root_cspace,
			 qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	cap_id_result_t capid_ret;
	power_create_t	params = { 0U };

	// create system power object
	power_ptr_result_t result =
		partition_allocate_power(partition_get_private(), params);
	if (result.e != OK) {
		LOG(ERROR, WARN, "create power object failed: {:d}",
		    (register_t)result.e);
		goto fail;
	}
	error_t err = object_activate_power(result.r);
	if (err != OK) {
		panic("Failed to activate power object");
	}

	object_ptr_t obj_ptr = { .power = result.r };

	capid_ret = cspace_create_master_cap(root_cspace, obj_ptr,
					     OBJECT_TYPE_POWER);
	if (capid_ret.e != OK) {
		object_put_power(obj_ptr.power);
		goto fail;
	}
	cap_id_t sys_power_cap = capid_ret.r;

	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "system_power_capid",
				   sys_power_cap);

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
	QCBOREncode_AddBoolToMap(qcbor_enc_ctxt, "system_suspend", true);

#endif
	goto out;

fail:
	panic("failed to create power object");
out:
	return;
}

// The Boot CPU power count is initialised to 1. Decrement the count after the
// root VM initialization.
void
power_handle_rootvm_started(void)
{
	power_vote_cpu_off(cpulocal_get_index());
}
#endif

void
power_handle_boot_hypervisor_handover(void)
{
	// Ensure the running core is the only core online. There is no easy way
	// to do this race-free, but it doesn't really matter for our purpose.
	count_t on_count = 0;
	for (cpu_index_t cpu = 0U; cpu < PLATFORM_MAX_CORES; cpu++) {
		cpu_power_state_t state = CPULOCAL_BY_INDEX(power_state, cpu);
		if ((state != CPU_POWER_STATE_OFF) &&
		    (state != CPU_POWER_STATE_OFFLINE)) {
			on_count++;
		}
	}

	if (on_count != 1U) {
		panic("Hypervisor hand-over requested with multiple CPUs on");
	}
}

#if defined(POWER_START_ALL_CORES)
void
power_handle_boot_hypervisor_start(void)
{
	cpu_index_t boot_cpu = cpulocal_get_index();

	for (cpu_index_t cpu = 0U; cpulocal_index_valid(cpu); cpu++) {
		if (cpu == boot_cpu) {
			continue;
		}

		power_vote_cpu_on(cpu);
	}
}
#endif

#if defined(PLATFORM_ENABLE_SYSTEM_SUSPEND) && PLATFORM_ENABLE_SYSTEM_SUSPEND
static void
power_send_other_cores_power_off(void)
{
	TRACE_LOCAL(DEBUG, INFO, "power_send_other_cores_power_off");
	assert_preempt_disabled();

	ipi_others(IPI_REASON_SECONDARY_CPU_OFF);
}

static void
power_resume_other_cores(void) REQUIRE_PREEMPT_DISABLED
{
	cpu_index_t current_cpu = cpulocal_get_index();
	TRACE_LOCAL(DEBUG, INFO, "power_resume_other_cores");
	assert_preempt_disabled();

	for (cpu_index_t cpu = 0U; cpu < PLATFORM_MAX_CORES; cpu++) {
		if ((cpu == current_cpu) || (!platform_cpu_functional(cpu))) {
			continue;
		}

		power_voting_t *voting = &CPULOCAL_BY_INDEX(power_voting, cpu);

		spinlock_acquire_nopreempt(&voting->lock);
		if (voting->vote_count != 0U) {
			cpu_power_state_t *state =
				&CPULOCAL_BY_INDEX(power_state, cpu);
			*state	    = CPU_POWER_STATE_STARTED;
			error_t ret = platform_cpu_on(cpu);
			if (ret != OK) {
				TRACE_AND_LOG(
					ERROR, WARN,
					"power_resume_other_cores: failed cpu {:d}",
					cpu);
			}
		}
		spinlock_release_nopreempt(&voting->lock);
	}
}

bool
power_handle_ipi_received_secondary_cpu_off(void) REQUIRE_PREEMPT_DISABLED
{
	TRACE_LOCAL(DEBUG, INFO, "power_handle_ipi_received_secondary_cpu_off");

	// power off core only in case of system suspend in progress
	if (atomic_load_relaxed(&power_suspend_in_progress)) {
		power_voting_t *voting = &CPULOCAL(power_voting);
		spinlock_acquire_nopreempt(&voting->lock);
		assert(CPULOCAL(power_state) == CPU_POWER_STATE_ONLINE);
		CPULOCAL(power_state) = CPU_POWER_STATE_OFFLINE;
		spinlock_release_nopreempt(&voting->lock);

		platform_cpu_off();
	}

	// Always reschedule after resuming
	return true;
}

static error_t
power_poll_cores_state_off(void) REQUIRE_PREEMPT_DISABLED
{
	error_t	    ret;
	cpu_index_t current_cpu = cpulocal_get_index();

	// Retry count is for all CPUs. Since we broadcast the CPU_OFF request
	// we expect the cores to power-off in parallel, so if there is a delay
	// waiting for one core, the next core should not get a new timeout.
	count_t retry_count = 0;

	// checking/polling all cores power OFF except current core
	for (cpu_index_t cpu = 0U; cpu < PLATFORM_MAX_CORES; cpu++) {
		// check for current cpu or non-functional cpu
		if ((cpu == current_cpu) || (!platform_cpu_functional(cpu))) {
			continue;
		}

		do {
			bool_result_t off_ret = platform_cpu_powered_off(cpu);
			if (off_ret.e != OK) {
				TRACE_AND_LOG(
					ERROR, WARN,
					"platform_cpu_powered_off error: {:d}",
					(register_t)off_ret.e);
				ret = off_ret.e;
				goto out;
			}
			if (off_ret.r) {
				// core is off
				break;
			}
			retry_count++;
			if (retry_count == MAX_CPU_OFF_RETRIES) {
				TRACE_AND_LOG(
					ERROR, WARN,
					"power_poll_cores_state_off: timeout");
				ret = ERROR_FAILURE;
				goto out;
			}
			platform_timer_ndelay(POWER_CPU_OFF_RETRY_DELAY_NS);
		} while (true);
	}

	ret = OK;

out:
	return ret;
}

// Implements the System Suspend sequence
static error_t
power_enter_system_suspend(void) REQUIRE_PREEMPT_DISABLED
{
	error_t ret;

	TRACE_LOCAL(DEBUG, INFO, "power_enter_system_suspend");

	// Indicate that we are going to suspend, so the IPI handler should
	// honor the power-off request.
	atomic_store_relaxed(&power_suspend_in_progress, true);
	atomic_thread_fence(memory_order_seq_cst);

	// Power off all other online cores
	power_send_other_cores_power_off();

	// Poll all cores to be off
	error_t poll_ret = power_poll_cores_state_off();
	if (poll_ret != OK) {
		// We can't return an error here, it introduces a very
		// difficult to solve race where we might timeout and at the
		// same time a CPU does happen to notice the power-off IPI.
		panic("failed to power off all cores");
	}

	psci_suspend_powerstate_t pstate = psci_suspend_powerstate_default();
	error_t cpu_suspend_result	 = power_current_cpu_suspend(pstate);
	if (cpu_suspend_result != OK) {
		ret = cpu_suspend_result;
		goto out;
	}

	TRACE_LOCAL(DEBUG, INFO, "calling platform_system_suspend()");

	// Trigger power_system_suspend event before calling platform code
	error_t suspend_err = trigger_power_system_suspend_event();
	if (suspend_err != OK) {
		TRACE_AND_LOG(ERROR, WARN,
			      "power_system_suspend event failed: {:d}",
			      (register_t)suspend_err);
		ret = suspend_err;
		goto resume_cores;
	}

	// Request the platform to enter system suspend
	bool_result_t res = platform_system_suspend();
	if (res.e != OK) {
		// Platform returned, error or denied the system suspend.
		TRACE_AND_LOG(ERROR, WARN,
			      "power_enter_system_suspend: err {:d}",
			      (register_t)res.e);
		ret = ERROR_DENIED;
	} else {
		ret = OK;
	}

	// Trigger power_system_resume event after platform code returns
	trigger_power_system_resume_event((res.e == OK) && res.r);

resume_cores:
	// We have ensured that all cores were powered off above. In case of an
	// error to power-off, we resume all cores below. We clear the
	// power_suspend_in_progress flag prior to turning on any cores in case
	// a pending IPI remains in the interrupt controller.
	atomic_store_explicit(&power_suspend_in_progress, false,
			      memory_order_release);
	// Prevent any compiler re-ordering
	atomic_thread_fence(memory_order_seq_cst);

	TRACE_LOCAL(DEBUG, INFO, "power_exit_system_suspend");

	// trigger cpu resume event
	power_current_cpu_resume(true);

	// Resume power to all saved cores
	power_resume_other_cores();

out:
	return ret;
}

error_t
power_system_suspend(void)
{
	error_t ret;

	preempt_disable();

	// Prevent concurrent system suspend calls
	if (!spinlock_trylock_nopreempt(&power_system_cores_lock)) {
		ret = ERROR_BUSY;
		goto out;
	}

	// Try enter system suspend state
	ret = power_enter_system_suspend();
	// Once here, we were woken up or suspend returned an error

	spinlock_release_nopreempt(&power_system_cores_lock);

out:
	preempt_enable();
	return ret;
}
#endif // PLATFORM_ENABLE_SYSTEM_SUSPEND

static error_t
power_current_cpu_suspend(psci_suspend_powerstate_t pstate)
	REQUIRE_PREEMPT_DISABLED
{
	psci_suspend_powerstate_set_StateType(
		&pstate, PSCI_SUSPEND_POWERSTATE_TYPE_POWERDOWN);
	error_t suspend_result = trigger_power_cpu_suspend_event(pstate, true);

	return suspend_result;
}

static void
power_current_cpu_resume(bool was_poweroff) REQUIRE_PREEMPT_DISABLED
{
	bool first_cpu = true;

	trigger_power_cpu_resume_event(was_poweroff, first_cpu);
}

error_t
power_cpu_suspend(psci_suspend_powerstate_t power_state)
{
	error_t ret;

	TRACE_AND_LOG(DEBUG, INFO, "power_cpu_suspend entry");
	preempt_disable();

	error_t cpu_suspend_result = power_current_cpu_suspend(power_state);
	if (cpu_suspend_result != OK) {
		ret = cpu_suspend_result;
		goto out;
	}

	TRACE_AND_LOG(DEBUG, INFO, "calling platform_cpu_suspend()");

	// Request the platform to enter cpu suspend
	bool_result_t res = platform_cpu_suspend(power_state);
	ret		  = res.e;

	// Platform returned, success or denied the cpu suspend.
	// Returning same return/error code to caller
	TRACE_AND_LOG(DEBUG, INFO, "power_cpu_suspend exit: ret {:d}",
		      (register_t)ret);

	// trigger cpu resume event
	power_current_cpu_resume(false);

out:
	preempt_enable();
	return ret;
}
