// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <hypcontainers.h>

#if defined(INTERFACE_VCPU_RUN)
#include <addrspace.h>
#include <compiler.h>
#include <pgtable.h>
#include <qcbor.h>
#include <range_tree.h>
#include <rcu.h>
#include <scheduler.h>
#include <spinlock.h>
#include <thread.h>
#include <vcpu_run.h>

#include "event_handlers.h"

vcpu_trap_result_t
addrspace_handle_vdevice_access_fixed_addr(vmaddr_t ipa, size_t access_size,
					   register_t *value, bool is_write)
{
	thread_t *current = thread_get_self();

	vcpu_trap_result_t ret = VCPU_TRAP_RESULT_UNHANDLED;

	scheduler_lock(current);
	if (vcpu_run_is_enabled(current)) {
		addrspace_t *addrspace = current->addrspace;
		rcu_read_start();
		range_tree_lookup_result_t result = range_tree_lookup(
			&addrspace->fault_ranges, ipa, access_size);

		if ((result.size == access_size) && (result.node != NULL) &&
		    addrspace_range_container_of_range(result.node)->is_vmmio) {
			rcu_read_finish();
			current->addrspace_fault_access_ipa  = ipa;
			current->addrspace_fault_access_size = access_size;
			current->addrspace_fault_access_value =
				is_write ? *value : 0U;
			current->addrspace_fault_access_type =
				is_write ? ADDRSPACE_ACCESS_TYPE_WRITE
					 : ADDRSPACE_ACCESS_TYPE_READ;
			current->addrspace_fault_access_vmmio = true;
			// We can only get here through a translation fault
			current->addrspace_fault_access_is_mapped = false;

			scheduler_block(current,
					SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS);
			scheduler_unlock_nopreempt(current);
			(void)scheduler_yield();
			scheduler_lock_nopreempt(current);

			if (current->addrspace_fault_access_size == 0U) {
				ret = VCPU_TRAP_RESULT_RETRY;
			} else if (current->addrspace_fault_access_size ==
				   ~(size_t)0U) {
				ret = VCPU_TRAP_RESULT_FAULT;
			} else if (is_write) {
				ret = VCPU_TRAP_RESULT_EMULATED;
			} else {
				*value = current->addrspace_fault_access_value;
				ret    = VCPU_TRAP_RESULT_EMULATED;
			}
		} else {
			rcu_read_finish();
		}
	}
	scheduler_unlock(current);

	return ret;
}

vcpu_run_state_t
addrspace_handle_vcpu_run_check(const thread_t *vcpu, register_t *state_data_0,
				register_t *state_data_1,
				register_t *state_data_2)
{
	vcpu_run_state_t ret;

	if (scheduler_is_blocked(vcpu,
				 SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS)) {
		*state_data_0 = (register_t)vcpu->addrspace_fault_access_ipa;
		if (vcpu->addrspace_fault_access_vmmio) {
			*state_data_1 =
				(register_t)vcpu->addrspace_fault_access_size;
			*state_data_2 =
				(register_t)vcpu->addrspace_fault_access_value;
			ret = (vcpu->addrspace_fault_access_type ==
			       ADDRSPACE_ACCESS_TYPE_WRITE)
				      ? VCPU_RUN_STATE_ADDRSPACE_VMMIO_WRITE
				      : VCPU_RUN_STATE_ADDRSPACE_VMMIO_READ;
		} else {
			*state_data_1 =
				(register_t)vcpu->addrspace_fault_access_type;
			ret = VCPU_RUN_STATE_ADDRSPACE_PAGE_FAULT;
		}
	} else {
		ret = VCPU_RUN_STATE_BLOCKED;
	}

	return ret;
}

error_t
addrspace_handle_vcpu_run_resume_vmmio_read(thread_t  *vcpu,
					    register_t resume_data_0,
					    register_t resume_data_1,
					    register_t resume_data_2)
{
	error_t err;

	assert(scheduler_is_blocked(vcpu,
				    SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS) &&
	       vcpu->addrspace_fault_access_vmmio &&
	       (vcpu->addrspace_fault_access_type ==
		ADDRSPACE_ACCESS_TYPE_READ));

	if (resume_data_2 != 0U) {
		err = ERROR_UNIMPLEMENTED;
		goto out;
	}

	switch (resume_data_1) {
	case (register_t)ADDRSPACE_RESUME_ACTION_DEFAULT:
		vcpu->addrspace_fault_access_value = resume_data_0;
		err				   = OK;
		break;
	case (register_t)ADDRSPACE_RESUME_ACTION_RETRY:
		vcpu->addrspace_fault_access_size = 0U;
		err				  = OK;
		break;
	case (register_t)ADDRSPACE_RESUME_ACTION_FAULT:
		vcpu->addrspace_fault_access_size = ~(size_t)0U;
		err				  = OK;
		break;
	default:
		err = ERROR_UNIMPLEMENTED;
		break;
	}

	if (err == OK) {
		(void)scheduler_unblock(vcpu,
					SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS);
	}

out:
	return err;
}

error_t
addrspace_handle_vcpu_run_resume_vmmio_write(thread_t  *vcpu,
					     register_t resume_data_0,
					     register_t resume_data_1,
					     register_t resume_data_2)
{
	error_t err;

	assert(scheduler_is_blocked(vcpu,
				    SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS) &&
	       vcpu->addrspace_fault_access_vmmio &&
	       (vcpu->addrspace_fault_access_type ==
		ADDRSPACE_ACCESS_TYPE_WRITE));

	(void)resume_data_0;
	if (resume_data_2 != 0U) {
		err = ERROR_UNIMPLEMENTED;
		goto out;
	}

	switch (resume_data_1) {
	case (register_t)ADDRSPACE_RESUME_ACTION_DEFAULT:
		// Nothing to do here
		err = OK;
		break;
	case (register_t)ADDRSPACE_RESUME_ACTION_RETRY:
		vcpu->addrspace_fault_access_size = 0U;
		err				  = OK;
		break;
	case (register_t)ADDRSPACE_RESUME_ACTION_FAULT:
		vcpu->addrspace_fault_access_size = ~(size_t)0U;
		err				  = OK;
		break;
	default:
		err = ERROR_UNIMPLEMENTED;
		break;
	}

	if (err == OK) {
		(void)scheduler_unblock(vcpu,
					SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS);
	}

out:
	return err;
}

static vcpu_trap_result_t
addrspace_handle_page_fault(iss_da_ia_fsc_t fsc, vmaddr_result_t fault_ipa,
			    FAR_EL2_t far, addrspace_access_type_t access_type)
{
	thread_t	  *current = thread_get_self();
	vcpu_trap_result_t ret;
	vmaddr_t	   vmaddr;

	scheduler_lock(current);
	if (!vcpu_run_is_enabled(current)) {
		ret = VCPU_TRAP_RESULT_UNHANDLED;
		goto out;
	}

	addrspace_t *addrspace = current->addrspace;

	if ((fsc == ISS_DA_IA_FSC_PERMISSION_L0) ||
	    (fsc == ISS_DA_IA_FSC_PERMISSION_L1) ||
	    (fsc == ISS_DA_IA_FSC_PERMISSION_L2) ||
	    (fsc == ISS_DA_IA_FSC_PERMISSION_L3)) {
		if (compiler_expected(fault_ipa.e != OK)) {
			// The IPA for a permission fault is usually unknown
			// because the TLB only caches the VA->PA translation.
			// We need to do a stage 1 lookup to find the IPA.
			gvaddr_t	va = FAR_EL2_get_VirtualAddress(&far);
			vmaddr_result_t lookup_ret =
				addrspace_va_to_ipa_read(va);

			// The lookup may fail if there was a race with a stage
			// 1 page table operation or TLB invalidate that removed
			// the stage 1 mapping that triggered the fault in the
			// first place, or if either the stage 1 or stage 2
			// table changed in such a way as to trigger a stage 2
			// fault during the stage 1 walk.
			//
			// In either case, we retry the faulting instruction,
			// which should deliver the new fault to the right
			// place.
			if (compiler_unexpected(lookup_ret.e != OK)) {
				ret = VCPU_TRAP_RESULT_RETRY;
				goto out;
			}
			vmaddr = lookup_ret.r;
		} else {
			// A valid fault IPA is possible if a stage 1 lookup
			// tried to update the access or dirty flags in a PTE
			// stored in a read-only stage 2 page.
			vmaddr = fault_ipa.r;
		}
	} else if ((fsc == ISS_DA_IA_FSC_TRANSLATION_L0) ||
		   (fsc == ISS_DA_IA_FSC_TRANSLATION_L1) ||
		   (fsc == ISS_DA_IA_FSC_TRANSLATION_L2) ||
		   (fsc == ISS_DA_IA_FSC_TRANSLATION_L3)) {
		// For a stage 2 translation flag fault, the fault IPA should
		// always be valid.
		assert(fault_ipa.e == OK);
		vmaddr = fault_ipa.r;
	} else if ((fsc == ISS_DA_IA_FSC_ACCESS_FLAG_L0) ||
		   (fsc == ISS_DA_IA_FSC_ACCESS_FLAG_L1) ||
		   (fsc == ISS_DA_IA_FSC_ACCESS_FLAG_L2) ||
		   (fsc == ISS_DA_IA_FSC_ACCESS_FLAG_L3)) {
		// For a stage 2 access flag fault, the fault IPA should always
		// be valid, and we may be able handle it in the hypervisor
		// instead of forwarding it to the VM (if protected mappings
		// are locked using the access flag).
		assert(fault_ipa.e == OK);
		vmaddr = fault_ipa.r;

		pgtable_vm_start(&addrspace->vm_pgtable);
		error_t err = pgtable_vm_access_protected(
			&addrspace->vm_pgtable, vmaddr,
			access_type == ADDRSPACE_ACCESS_TYPE_WRITE);
		pgtable_vm_commit(&addrspace->vm_pgtable);
		if (err == OK) {
			ret = VCPU_TRAP_RESULT_RETRY;
			goto out;
		}

		// Fall through to passing the fault to the VM.
	} else {
		// Wrong fault type; not handled by this module.
		ret = VCPU_TRAP_RESULT_UNHANDLED;
		goto out;
	}

	// We need to call range_map_lookup() in an RCU critical section to
	// ensure that levels aren't freed while it is accessing them, but we
	// can end the critical section immediately afterwards since we are not
	// dereferencing anything.
	rcu_read_start();
	range_tree_lookup_result_t result =
		range_tree_lookup(&addrspace->fault_ranges, vmaddr, 1U);
	rcu_read_finish();

	// Note that we allow either private or VMMIO ranges here. That is to
	// ensure we report ISV=0 faults in the VMMIO ranges, which is needed
	// for unprotected VMs with shared main memory. Any ISV=1 fault should
	// have already been handled at this point because the vdevice handlers
	// for this event have higher priority.
	if (result.node != NULL) {
		current->addrspace_fault_access_ipa   = vmaddr;
		current->addrspace_fault_access_type  = access_type;
		current->addrspace_fault_access_vmmio = false;
		current->addrspace_fault_access_is_mapped =
			(fsc != ISS_DA_IA_FSC_TRANSLATION_L0) &&
			(fsc != ISS_DA_IA_FSC_TRANSLATION_L1) &&
			(fsc != ISS_DA_IA_FSC_TRANSLATION_L2) &&
			(fsc != ISS_DA_IA_FSC_TRANSLATION_L3);

		scheduler_block(current,
				SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS);
		scheduler_unlock_nopreempt(current);
		(void)scheduler_yield();
		scheduler_lock_nopreempt(current);

		if (current->addrspace_fault_access_size == ~(size_t)0U) {
			ret = VCPU_TRAP_RESULT_FAULT;
		} else {
			ret = VCPU_TRAP_RESULT_RETRY;
		}
	} else {
		ret = VCPU_TRAP_RESULT_UNHANDLED;
	}

out:
	scheduler_unlock(current);
	return ret;
}

vcpu_trap_result_t
addrspace_handle_vcpu_trap_data_abort_guest_paging(ESR_EL2_t	   esr,
						   vmaddr_result_t ipa,
						   FAR_EL2_t	   far)
{
	ESR_EL2_ISS_DATA_ABORT_t iss =
		ESR_EL2_ISS_DATA_ABORT_cast(ESR_EL2_get_ISS(&esr));
	bool		is_write = ESR_EL2_ISS_DATA_ABORT_get_WnR(&iss);
	iss_da_ia_fsc_t fsc	 = ESR_EL2_ISS_DATA_ABORT_get_DFSC(&iss);

	return addrspace_handle_page_fault(
		fsc, ipa, far,
		is_write ? ADDRSPACE_ACCESS_TYPE_WRITE
			 : ADDRSPACE_ACCESS_TYPE_READ);
}

vcpu_trap_result_t
addrspace_handle_vcpu_trap_pf_abort_guest_paging(ESR_EL2_t	 esr,
						 vmaddr_result_t ipa,
						 FAR_EL2_t	 far)
{
	ESR_EL2_ISS_INST_ABORT_t iss =
		ESR_EL2_ISS_INST_ABORT_cast(ESR_EL2_get_ISS(&esr));
	iss_da_ia_fsc_t fsc = ESR_EL2_ISS_INST_ABORT_get_IFSC(&iss);

	return addrspace_handle_page_fault(fsc, ipa, far,
					   ADDRSPACE_ACCESS_TYPE_EXECUTE);
}

error_t
addrspace_handle_vcpu_run_resume_page_fault(thread_t  *vcpu,
					    register_t resume_data_0,
					    register_t resume_data_1,
					    register_t resume_data_2)
{
	error_t err;

	assert(scheduler_is_blocked(vcpu,
				    SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS) &&
	       !vcpu->addrspace_fault_access_vmmio);

	if ((resume_data_1 != 0U) || (resume_data_2 != 0U)) {
		err = ERROR_UNIMPLEMENTED;
		goto out;
	}

	switch (resume_data_0) {
	case (register_t)ADDRSPACE_RESUME_ACTION_DEFAULT:
	case (register_t)ADDRSPACE_RESUME_ACTION_RETRY:
		// Nothing to do here
		err = OK;
		break;
	case (register_t)ADDRSPACE_RESUME_ACTION_FAULT:
		vcpu->addrspace_fault_access_size = ~(size_t)0U;
		err				  = OK;
		break;
	default:
		err = ERROR_UNIMPLEMENTED;
		break;
	}

	(void)scheduler_unblock(vcpu, SCHEDULER_BLOCK_ADDRSPACE_FAULT_ACCESS);

out:
	return err;
}
#endif // INTERFACE_VCPU_RUN
