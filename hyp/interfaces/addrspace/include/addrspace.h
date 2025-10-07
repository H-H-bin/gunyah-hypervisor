// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// This function attaches an address space to a thread.
//
// An address space can only be attached to a thread before it starts running
// and the address space should already be in an activate state.
//
// The function returns an error if the thread is not vcpu kind, if it belongs
// to a HLOS VM and if it the address space has not been activated.
error_t
addrspace_attach_thread(addrspace_t *addrspace, thread_t *thread);

// Get a pointer to the thread's addrspace.
addrspace_t *
addrspace_get_self(void);

// Configure the address space.
//
// The object's header lock must be held and object state must be
// OBJECT_STATE_INIT.
error_t
addrspace_configure(addrspace_t *addrspace, vmid_t vmid);

// Configure the address space information area.
//
// The object's header lock must be held and object state must be
// OBJECT_STATE_INIT.
error_t
addrspace_configure_info_area(addrspace_t *addrspace, memextent_t *info_area_me,
			      vmaddr_t ipa);

// Nominate an address range as being handled by an unprivileged VMM.
//
// This can return ERROR_NORESOURCES if the implementation has reached a limit
// of nominated address ranges, ERROR_ARGUMENT_INVALID if the specified range
// overlaps an existing VMMIO range, or ERROR_UNIMPLEMENTED if there is no way
// to forward faults to an unprivileged VMM.
error_t
addrspace_add_fault_range(addrspace_t *addrspace, vmaddr_t base, size_t size,
			  bool vmmio);

// Remove an address range from the ranges handled by an unprivileged VMM.
//
// The range must match one that was previously added to the address space by
// calling addrspace_add_fault_range().
error_t
addrspace_remove_fault_range(addrspace_t *addrspace, vmaddr_t base, size_t size,
			     bool vmmio);

// Translate a VA to PA in the current guest address space.
//
// This function will automatically retry the lookup if it fails for any reason
// that would have been automatically resolved by the addrspace module for an
// access directly from the VM. This includes, for example, transient faults
// caused by concurrent address space updates on other cores.
//
// The following errors may be returned:
//
// - ERROR_DENIED if the lookup is a write access and the stage 2 mapping is
//   read-only,
// - ERROR_ADDR_INVALID for any other non-transient fault in stage 2, or
// - ERROR_ARGUMENT_INVALID for any fault in stage 1, regardless of the cause of
//   the fault.
//
// If successful, the result is a structure containing:
// - phys: The PA corresponding to the specified guest VA address.
// - size: The minimum size of the guest mapping starting from the returned PA.
// - coherent: : flag indicating whether the effective cache attribute of the
//   guest's mapping is cache-coherent with the hypervisor's direct mapping of
//   the PA (if accessed via partition_phys_map()). If this flag is false, the
//   caller must perform appropriate cache maintenance before or after accessing
//   the PA via the direct mapping.
//
// Requires the RCU read lock, because the returned physical address might be
// unmapped concurrently and then reused after an RCU grace period.
addrspace_va_lookup_result_t
addrspace_va_to_pa(gvaddr_t addr, bool write) REQUIRE_RCU_READ;

// Translate a VA to PA in the current guest address space, without retrying.
//
// This is the same as addrspace_va_to_pa(), except that it will return
// ERROR_RETRY instead of retrying if a transient translation fault occurs.
// Generally this should be used in fault handlers, where a successful retry
// might not fault at all.
addrspace_va_lookup_result_t
addrspace_va_to_pa_noretry(gvaddr_t addr, bool write) REQUIRE_RCU_READ;

// Translate a VA to IPA in the current guest address space.
//
// This function is only used to obtain an IPA for an access that is known to
// have taken a stage 2 permission fault, so there is no need to check stage 1
// permissions. Therefore, it always simulates a read access, and does not
// automatically handle TLB conflicts or break-before-make.
//
// Returns ERROR_DENIED if the lookup faults in stage 2 (during the stage 1 page
// table walk), or ERROR_ADDR_INVALID if it faults in stage 1, regardless of the
// cause of the fault.
vmaddr_result_t
addrspace_va_to_ipa_read(gvaddr_t addr);

// Check whether an address range is within the address space.
error_t
addrspace_check_range(addrspace_t *addrspace, vmaddr_t base, size_t size);

// Map into an addrspace.
error_t
addrspace_map(addrspace_t *addrspace, vmaddr_t vbase, size_t size, paddr_t phys,
	      pgtable_vm_memtype_t memtype, pgtable_access_t kernel_access,
	      pgtable_access_t user_access, addrspace_map_flags_t map_flags);

// Unmap from an addrspace.
error_t
addrspace_unmap(addrspace_t *addrspace, vmaddr_t vbase, size_t size,
		paddr_t phys, addrspace_map_flags_t map_flags);

// Unlock and/or sanitise protected pages in an addrspace.
size_result_t
addrspace_modify_pages(addrspace_t *addrspace, vmaddr_t vbase, size_t size,
		       addrspace_modify_pages_flags_t flags);

// Lookup a mapping in the addrspace.
addrspace_lookup_result_t
addrspace_lookup(addrspace_t *addrspace, vmaddr_t vbase, size_t size);

// Allocate some memory in the info page.
//
// This generally should only be used prior to starting the VM, because the
// allocation will become visible to the VM immediately. If this is not
// possible, the data format should contain a validity bit or flag which can be
// checked by the VM before reading.
//
// The allocated memory is guaranteed to have been zero-initialised prior to the
// startup of the VM. It is mapped cacheable in the hypervisor address space. It
// may be necessary to clean the data cache to make written data visible to the
// VM.
//
// It is not possible to free info page memory once it is allocated. It will be
// freed when the addrspace object is released.
//
// If the alloc_desc argument is true, a descriptor will be allocated and
// returned; it will initially be invalid and must be enabled by calling
// addrspace_enable_info_area(). Otherwise, the descriptor will be NULL and the
// caller must use some other mechanism to inform the VM of the allocated IPA.
//
// Modules calling this API should set alloc_desc to false. It is expected that
// info area entries are only allocated using the hypercall interface.
addrspace_alloc_info_area_result_t
addrspace_alloc_info_area(addrspace_t *addrspace, size_t size, size_t alignment,
			  bool alloc_desc);

// Enable an info area descriptor.
//
// This should only be called after successfully initialising the payload of the
// info area.
void
addrspace_enable_info_area(addrspace_info_area_entry_t	   *descriptor,
			   addrspace_info_area_entry_type_t type);
