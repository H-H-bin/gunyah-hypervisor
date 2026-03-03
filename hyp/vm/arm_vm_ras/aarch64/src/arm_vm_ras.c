// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypregisters.h>

#include <abort.h>
#include <atomic.h>
#include <compiler.h>
#include <cpulocal.h>
#include <ipi.h>
#include <log.h>
#include <panic.h>
#include <preempt.h>
#include <ras.h>
#include <rcu.h>
#include <scheduler.h>
#include <thread.h>
#include <trace.h>
#include <vcpu.h>
#include <vic.h>
#include <virq.h>

#include <asm/barrier.h>
#include <asm/sysregs.h>
#include <asm/system_registers.h>

#include "event_handlers.h"

CPULOCAL_DECLARE_STATIC(thread_t *_Atomic, ras_err_handler_thread);

bool
arm_vm_ras_handle_vcpu_activate_thread(thread_t		  *thread,
				       vcpu_option_flags_t options)
{
	bool ret = false;

	assert(vcpu_is_vcpu(thread));

#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
	HCR_EL2_set_TEA(&thread->vcpu_regs_el2.hcr_el2, false);
#endif

	// Initially give UEFI/HLOS full access to the RAS registers. Once RAS
	// error handler VM's threads come up, switch RAS access to them. For
	// the rest of the VMs trap accesses to RAS registers without handling
	// them, so an abort gets injected to any other VM that tries to use
	// RAS.
	//
	// If HLOS is not the RAS handler VM, it will not need to receive the
	// RAS interrupts during this temporary access.

	scheduler_lock(thread);

	if (vcpu_option_flags_get_ras_error_handler(&options)) {
		if (!vcpu_option_flags_get_pinned(&options) ||
		    !cpulocal_index_valid(thread->scheduler_affinity)) {
			goto out;
		}

#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
		HCR_EL2_set_TERR(&thread->vcpu_regs_el2.hcr_el2, false);
		rcu_read_start();
		if (!vcpu_option_flags_get_hlos_vm(&options) &&
		    (scheduler_get_primary_vcpu(thread->scheduler_affinity) !=
		     NULL)) {
			// RAS VM is not HLOS, revoke the corresponding HLOS
			// thread's RAS access
			ipi_one(IPI_REASON_RAS_HLOS_ACCESS_DISABLE,
				thread->scheduler_affinity);
		}
		rcu_read_finish();
#endif

		thread_t *_Atomic *ras_err_handler_thread_p =
			&CPULOCAL_BY_INDEX(ras_err_handler_thread,
					   thread->scheduler_affinity);
		thread_t *expected = NULL;
		if (atomic_compare_exchange_strong_explicit(
			    ras_err_handler_thread_p, &expected, thread,
			    memory_order_relaxed, memory_order_relaxed)) {
			vcpu_option_flags_set_ras_error_handler(
				&thread->vcpu_options, true);
		} else {
			goto out;
		}

		ret = true;
	} else if (vcpu_option_flags_get_hlos_vm(&options)) {
#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
		// HLOS is not the RAS handler VM, give it RAS access until the
		// RAS handler VM comes up.
		thread_t *ras_thread = arm_vm_ras_get_error_handler_thread(
			thread->scheduler_affinity);
		if (ras_thread == NULL) {
			HCR_EL2_set_TERR(&thread->vcpu_regs_el2.hcr_el2, false);
		}
#endif
		ret = true;
	} else {
#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
		HCR_EL2_set_TERR(&thread->vcpu_regs_el2.hcr_el2, true);
#endif
		ret = true;
	}

out:
	scheduler_unlock(thread);
	return ret;
}

void
arm_vm_ras_handle_object_deactivate_thread(thread_t *thread)
{
	assert(thread != NULL);

	if (cpulocal_index_valid(thread->scheduler_affinity)) {
		thread_t *_Atomic *ras_err_handler_thread_p =
			&CPULOCAL_BY_INDEX(ras_err_handler_thread,
					   thread->scheduler_affinity);
		if (atomic_load_relaxed(ras_err_handler_thread_p) == thread) {
			atomic_store_relaxed(ras_err_handler_thread_p, NULL);
		}
	}
}

#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
bool
arm_vm_ras_handle_ipi_received(void)
{
	rcu_read_start();
	thread_t *hlos = scheduler_get_primary_vcpu(cpulocal_get_index());

	// Revoke this CPU's HLOS thread's access to RAS
	if (hlos != NULL) {
		HCR_EL2_set_TERR(&hlos->vcpu_regs_el2.hcr_el2, true);
	}

	if (hlos == thread_get_self()) {
		// HLOS thread is current, set HCR_EL2.TERR as well
		HCR_EL2_t hcr = register_HCR_EL2_read();
		HCR_EL2_set_TERR(&hcr, true);
		register_HCR_EL2_write(hcr);
	}
	rcu_read_finish();

	return false;
}
#endif

thread_t *
arm_vm_ras_get_error_handler_thread(cpu_index_t cpu)
{
	thread_t *ras_thread = atomic_load_consume(
		&CPULOCAL_BY_INDEX(ras_err_handler_thread, cpu));

	return ras_thread;
}

#if defined(ARCH_ARM_FEAT_RAS) || defined(ARCH_ARM_FEAT_RASv1p1)
// VDISR_EL2 needs to be context switched for the VM that handles the SErrors,
// and zeroed out for the rest. However it is probably faster to just
// context-switch it for all the VMs.
void
arm_vm_ras_handle_vcpu_save_state(void)
{
	thread_t *thread = thread_get_self();

	thread->ras.ras_regs.vdisr_el2 = register_VDISR_EL2_read();
}

void
arm_vm_ras_handle_vcpu_load_state(void)
{
	thread_t *thread = thread_get_self();

	register_VDISR_EL2_write(thread->ras.ras_regs.vdisr_el2);
	register_VSESR_EL2_write(thread->ras.ras_regs.vsesr_el2);
}
#endif

// Inject the SError to the RAS VM
static bool
arm_vm_ras_serror_inject(ESR_EL2_ISS_SERROR_t iss) REQUIRE_PREEMPT_DISABLED
{
	cpu_index_t cpu = cpulocal_get_index();

	TRACE_AND_LOG(INFO, WARN, "SError received on CPU {:d}, ISS {:#x}", cpu,
		      ESR_EL2_ISS_SERROR_raw(iss));

	// We will inject the SErrors to the VM that is configured to handle the
	// RAS errors.
	thread_t *thread = atomic_load_consume(
		&CPULOCAL_BY_INDEX(ras_err_handler_thread, cpu));

	if (thread == NULL) {
		abort_kernel("No vCPUs found for SError injection, aborting",
			     ABORT_REASON_RAS);
	}

	scheduler_lock_nopreempt(thread);

	if (scheduler_get_affinity(thread) != cpu) {
		abort_kernel(
			"SError target vCPU has incorrect affinity, aborting",
			ABORT_REASON_RAS);
	}

	TRACE_AND_LOG(DEBUG, INFO, "Injecting SError to VM {:d}",
		      thread->addrspace->vmid);

#if ARCH_AARCH64_32BIT_EL1
	if (!HCR_EL2_get_RW(&thread->vcpu_regs_el2.hcr_el2)) {
		// VM is running in AArch32
		VSESR_EL2_A32_EL1_t vsesr = VSESR_EL2_A32_EL1_default();
		VSESR_EL2_A32_EL1_set_ExT(&vsesr, false);
		// The first two bits of AArch64 AET map to AArch32 AET
		VSESR_EL2_A32_EL1_set_AET(
			&vsesr, ESR_EL2_ISS_SERROR_get_AET(&iss) & 0x03);
		thread->ras.ras_regs.vsesr_el2 = VSESR_EL2_A32_EL1_raw(vsesr);
	} else
#endif
	{
		VSESR_EL2_A64_EL1_t vsesr = VSESR_EL2_A64_EL1_default();
		VSESR_EL2_A64_EL1_set_IDS(&vsesr, false);
		VSESR_EL2_A64_EL1_set_ISS(&vsesr, ESR_EL2_ISS_SERROR_raw(iss));
		thread->ras.ras_regs.vsesr_el2 = VSESR_EL2_A64_EL1_raw(vsesr);
	}

	// The only field in HCR_EL2 that might change during guest's execution
	// is VSE, and we are setting it below. Therefore there is no need to
	// read the live register first.
	HCR_EL2_set_VSE(&thread->vcpu_regs_el2.hcr_el2, true);
	if (thread_get_self() == thread) {
		register_HCR_EL2_write(thread->vcpu_regs_el2.hcr_el2);
	}
	vcpu_wakeup(thread);
	scheduler_unlock_nopreempt(thread);

	return true;
}

bool
arm_vm_ras_handle_vcpu_trap_serror(ESR_EL2_ISS_SERROR_t iss)
{
	return arm_vm_ras_serror_inject(iss);
}

bool
arm_vm_ras_handle_preempt_abort(void)
{
	ESR_EL2_t	     esr = register_ESR_EL2_read_ordered(&asm_ordering);
	ESR_EL2_ISS_SERROR_t iss =
		ESR_EL2_ISS_SERROR_cast(ESR_EL2_get_ISS(&esr));

	return arm_vm_ras_serror_inject(iss);
}

vcpu_trap_result_t
arm_vm_ras_handle_vcpu_trap_wfi(void)
{
	vcpu_trap_result_t result = VCPU_TRAP_RESULT_UNHANDLED;
	thread_t	  *thread = thread_get_self();
	assert(thread != NULL);

	if (HCR_EL2_get_VSE(&thread->vcpu_regs_el2.hcr_el2)) {
		// SError is pending for this thread, reject the WFI request
		// and wake up the thread immediately
		result = VCPU_TRAP_RESULT_EMULATED;
	}

	return result;
}
