// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Low-level page table manipulation routines.
//
// Functions in this interface generally should only be called by modules that
// are responsible for managing address spaces. Don't call them directly from
// more general modules (syscall handlers, etc).
//
// The map operation return errors in cases of inconsistent mappings where
// there are existing mappings present in the range or allocation failures.
// Unmap does not return error for non-existing mappings.
//
// The caller will generally need to operate on some higher-level model of the
// address space first, and hold locks on that model (possibly fine-grained)
// to prevent conflicting updates. The update operations defined here are not
// required to be thread-safe with respect to updates affecting overlapping
// address ranges.
// FIXME: The current implementation is not thread-safe and the caller must
// ensure that the address space being operated on is locked. This may possibly
// require different modules directly operating on a page-table to share a
// lock.
//
// A caller must always flag the start of a set of one or more map and unmap
// operations by calling the start function.  If synchronisation of updates
// with the page-table walkers (either locally or on other CPUs) can be
// deferred, then it will be deferred until a call is made to the corresponding
// commit function. The caller must always call the commit function before
// relying in any way on the updates having taken effect.
//
// In multi-processor systems, remote CPUs or IOMMU-protected devices using
// an affected address space might either continue to see the old mapping, or
// see a temporarily invalid mapping (which may extend outside the specified
// address range), especially if the mapping change has caused a page to
// change sizes. This will not occur for any memory access that the commit()
// call _inter-thread happens before_ (as defined by C18), or for any call
// to lookup() or lookup_range() that completes after an RCU grace period has
// elapsed after the commit function returns.
//
// In general, any function in this file that returns error_t or bool will
// validate its arguments and fail with an error code or false result if
// they are invalid. Any function that returns void will panic on invalid
// inputs.

//
// Hypervisor page table management.
//
// In order to correctly attribute ownership of page table levels, the caller
// must avoid allocating page table levels in one partition if they might be
// subsequently freed into another partition. This can be done by selecting
// some allocation block size that mappings will never cross, and pre-
// allocating page table levels down to that block size from a global pool.
//
// Mappings with NONE access type may be used to indicate that the hypervisor
// should only be permitted to access the mapping on behalf of a VM, and will
// take specific action to enable and disable such accesses (e.g. clearing and
// setting PAN on ARMv8.1). Not all architectures support this; a different
// technique must be used for useraccess on those that do not.
//

// Returns false if the specified address is unmapped.
bool
pgtable_hyp_lookup(uintptr_t virtual_address, paddr_t *mapped_base,
		   size_t *mapped_size, pgtable_hyp_memtype_t *mapped_memtype,
		   pgtable_access_t *mapped_access);

// Returns false if there is no mapping in the specified range. If a mapping
// is found and can be efficiently determined to be the last mapping in the
// range, the boolean *remainder_unmapped will be set to true; otherwise it
// will be unchanged. Note that the returned mapping may extend beyond the
// specified range.
bool
pgtable_hyp_lookup_range(uintptr_t virt_base, size_t virt_size,
			 uintptr_t *mapped_virt, paddr_t *mapped_phys,
			 size_t		       *mapped_size,
			 pgtable_hyp_memtype_t *mapped_memtype,
			 pgtable_access_t      *mapped_access,
			 bool		       *remainder_unmapped);

// Creates page table levels owned by the given partition which are able to
// directly map entries covering the given size, but don't actually map
// anything. This is intended for preallocating levels using the hypervisor's
// private allocator, but might be more generally useful.
error_t
pgtable_hyp_preallocate(partition_t *partition, uintptr_t virtual_address,
			size_t size);

extern opaque_lock_t pgtable_hyp_map_lock;

// Flag the start of one of more map or unmap calls.
void
pgtable_hyp_start(void) ACQUIRE_LOCK(pgtable_hyp_map_lock);

// Creates a new mapping, possibly merging adjacent mappings into large blocks.
//
// An error will be returned if there are any existing mappings in the given
// region that are not exactly identical to the requested mapping.
//
// If merge_limit is nonzero, then this will attempt to merge page table levels
// that become congruent as a result of this operation into larger pages, as
// long as the new size is less than merge_limit. Any page table levels freed by
// this will be freed into the specified partition, so merge_limit should be no
// greater than preserved_prealloc would be for an unmap operation in the same
// region.
//
// Note that this operation may cause transient translation aborts or TLB
// conflict aborts in the affected range or within a merge_limit aligned region
// around it. The caller is responsible for not making calls with a nonzero
// merge_limit that might have those effects on the hypervisor code, the stack
// of any hypervisor thread, or any other address that may be touched during the
// handling of a transient hypervisor fault.
error_t
pgtable_hyp_map_merge(partition_t *partition, uintptr_t virtual_address,
		      size_t size, paddr_t phys, pgtable_hyp_memtype_t memtype,
		      pgtable_access_t access, vmsa_shareability_t shareability,
		      size_t merge_limit) REQUIRE_LOCK(pgtable_hyp_map_lock);

// Creates a new mapping. No attempt will be made to merge adjacent mappings.
static inline error_t
pgtable_hyp_map(partition_t *partition, uintptr_t virt, size_t size,
		paddr_t phys, pgtable_hyp_memtype_t memtype,
		pgtable_access_t access, vmsa_shareability_t shareability)
	REQUIRE_LOCK(pgtable_hyp_map_lock)
{
	return pgtable_hyp_map_merge(partition, virt, size, phys, memtype,
				     access, shareability, 0U);
}

// Creates a new mapping, replacing any existing mappings in the region, and
// possibly merging adjacent mappings into large blocks. The merge_limit
// argument has the same semantics as for @see pgtable_hyp_map_merge().
error_t
pgtable_hyp_remap_merge(partition_t *partition, uintptr_t virtual_address,
			size_t size, paddr_t phys,
			pgtable_hyp_memtype_t memtype, pgtable_access_t access,
			vmsa_shareability_t shareability, size_t merge_limit)
	REQUIRE_LOCK(pgtable_hyp_map_lock);

// Creates a new mapping, replacing any existing mappings in the region.
static inline error_t
pgtable_hyp_remap(partition_t *partition, uintptr_t virt, size_t size,
		  paddr_t phys, pgtable_hyp_memtype_t memtype,
		  pgtable_access_t access, vmsa_shareability_t shareability)
	REQUIRE_LOCK(pgtable_hyp_map_lock)
{
	return pgtable_hyp_remap_merge(partition, virt, size, phys, memtype,
				       access, shareability, 0U);
}

// Removes all mappings in the given range. Frees levels into the specified
// partition's allocators, but only if they cannot be used to create mappings
// of the size preserved_prealloc.  The preserved_prealloc field can therefore
// be used to prevent freeing of levels created by a previous hyp_preallocate
// call to the specified partition.
void
pgtable_hyp_unmap(partition_t *partition, uintptr_t virtual_address,
		  size_t size, size_t preserved_prealloc)
	REQUIRE_LOCK(pgtable_hyp_map_lock);
#define PGTABLE_HYP_UNMAP_PRESERVE_ALL	0U
#define PGTABLE_HYP_UNMAP_PRESERVE_NONE util_bit((sizeof(uintptr_t) * 8U) - 1U)

// Ensure that all previous hypervisor map and unmap calls are complete.
void
pgtable_hyp_commit(void) RELEASE_LOCK(pgtable_hyp_map_lock);

//
// VM page table management.
//
// VM page tables don't have the same constraints for level preallocation &
// freeing because they are always entirely owned by one partition.
//
error_t
pgtable_vm_init(partition_t *partition, pgtable_vm_t *pgtable, vmid_t vmid);

// Free all resources for page table
void
pgtable_vm_destroy(partition_t *partition, pgtable_vm_t *pgtable);

// Returns false if the specified address is unmapped.
bool
pgtable_vm_lookup(pgtable_vm_t *pgtable, vmaddr_t virtual_address,
		  paddr_t *mapped_base, size_t *mapped_size,
		  pgtable_vm_memtype_t *mapped_memtype,
		  pgtable_access_t     *mapped_vm_kernel_access,
		  pgtable_access_t     *mapped_vm_user_access);

// Returns false if there is no mapping in the specified range. If a mapping
// is found and can be efficiently determined to be the last mapping in the
// range, the boolean *remainder_unmapped will be set to true; otherwise it
// will be unchanged. Note that the returned mapping may extend beyond the
// specified range.
bool
pgtable_vm_lookup_range(pgtable_vm_t *pgtable, vmaddr_t virt_base,
			size_t virt_size, vmaddr_t *mapped_virt,
			paddr_t *mapped_phys, size_t *mapped_size,
			pgtable_vm_memtype_t *mapped_memtype,
			pgtable_access_t     *mapped_vm_kernel_access,
			pgtable_access_t     *mapped_vm_user_access,
			bool		     *remainder_unmapped);

extern opaque_lock_t pgtable_vm_map_lock;

// Flag the start of one of more map or unmap calls.
void
pgtable_vm_start(pgtable_vm_t *pgtable) ACQUIRE_LOCK(pgtable)
	ACQUIRE_LOCK(pgtable_vm_map_lock);

// Creates a new mapping.
//
// If try_map is true, it returns an error if any existing mappings are present
// in the range that are not exactly identical to the requested mapping. If
// try_map is false, any existing mappings in the specified range are removed or
// updated.
//
// If the operation fails, it will remove any mappings that were created, unless
// the protected option is set. If try_map is false, this will not restore any
// mappings that were removed.
//
// If allow_merge is true, then any page table levels that become congruent as a
// result of this operation will be merged into larger pages.
//
// If protected is true, then the page will be marked with the protected option,
// which prevents a subsequent unmap call removing the page unless
// pgtable_vm_modify_protected() is called with the unlock flag. Also, if an
// error occurs, the mapping may have been partially created; explicit unlock
// and unmap calls are required to guarantee that it has been removed.
//
// pgtable_vm_start() must have been called before this call.
error_t
pgtable_vm_map(partition_t *partition, pgtable_vm_t *pgtable,
	       vmaddr_t virtual_address, size_t size, paddr_t phys,
	       pgtable_vm_memtype_t memtype, pgtable_access_t vm_kernel_access,
	       pgtable_access_t vm_user_access, bool try_map, bool allow_merge,
	       bool protected) REQUIRE_LOCK(pgtable)
	REQUIRE_LOCK(pgtable_vm_map_lock);

// Removes all mappings in the given range.
//
// pgtable_vm_start() must have been called before this call.
error_t
pgtable_vm_unmap(partition_t *partition, pgtable_vm_t *pgtable,
		 vmaddr_t virtual_address, size_t size) REQUIRE_LOCK(pgtable)
	REQUIRE_LOCK(pgtable_vm_map_lock);

// Remove only mappings that match the physical address within the specified
// range.
//
// If protected is true, this operation will fail with ERROR_DENIED if it
// encounters a matching page that was mapped with the protected option set
// and was not subsequently unlocked by a pgtable_vm_modify_protected() call
// with the unlock flag set.
//
// pgtable_vm_start() must have been called before this call.
error_t
pgtable_vm_unmap_matching(partition_t *partition, pgtable_vm_t *pgtable,
			  vmaddr_t virtual_address, paddr_t phys, size_t size,
			  bool protected) REQUIRE_LOCK(pgtable)
	REQUIRE_LOCK(pgtable_vm_map_lock);

// Find and update any protected pages in the address space.
//
// If the unlock flag is set, protected pages will be unlocked, so that a
// protected unmap operation can remove them. This must also make the pages
// inaccessible, at least for write accesses, though any TLB invalidation for
// this purpose can be deferred if the sync argument is not set. It is
// implementation defined whether unlocked pages are inaccessible for read
// accesses.
//
// If the sanitise flag is set, this will zero and cache flush locked
// protected pages up to a fixed size limit (in addition to unlocking them if
// requested), and then terminate the operation early. This is done so the page
// table lock can be dropped by the caller to provide preemption points in a
// large sanitise operation. Note that this requires the page to be mapped with
// write permissions; it will return ERROR_DENIED if an attempt is made to
// sanitise a read-only page.
//
// The result is the size of the region that was successfully modified,
// including any pages that were already unlocked or unmapped. The size result
// is valid regardless of whether an error is returned.
//
// pgtable_vm_start() must have been called before this call.
size_result_t
pgtable_vm_modify_protected(partition_t *partition, pgtable_vm_t *pgtable,
			    vmaddr_t virtual_address, size_t size, bool unlock,
			    bool sanitise, bool sync)
	REQUIRE_LOCK(pgtable_vm_map_lock);

// Mark a protected page in the address space as having been accessed.
//
// Given the address of a faulting access in a page table, this function will
// look up the address to see whether it is marked as protected and unlocked,
// and would have the specified permissions if it was locked. If it is, then the
// page table will be updated to allow the operation to proceed and to mark the
// page as locked, and the function returns success. Otherwise, the function
// returns ERROR_DENIED.
//
// pgtable_vm_start() must have been called before this call.
error_t
pgtable_vm_access_protected(pgtable_vm_t *pgtable, vmaddr_t virtual_address,
			    bool write) REQUIRE_LOCK(pgtable_vm_map_lock);

// Ensure that all preceding VM map, unmap and unlock calls are complete.
void
pgtable_vm_commit(pgtable_vm_t *pgtable) RELEASE_LOCK(pgtable)
	RELEASE_LOCK(pgtable_vm_map_lock);

// Set VTCR and VTTBR registers with page table vtcr and vttbr bitfields values.
void
pgtable_vm_load_regs(pgtable_vm_t *vm_pgtable);

// Validate page table access
bool
pgtable_access_check(pgtable_access_t access, pgtable_access_t access_check);

// Mask a pagetable access
pgtable_access_t
pgtable_access_mask(pgtable_access_t access, pgtable_access_t access_mask);

// Check if input page table accesses are equal
bool
pgtable_access_is_equal(pgtable_access_t access, pgtable_access_t access_check);

// Get combined access
pgtable_access_t
pgtable_access_combine(pgtable_access_t access1, pgtable_access_t access2);
