// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypregisters.h>

#include <addrspace.h>
#include <compiler.h>
#include <partition.h>
#include <pgtable.h>
#include <rcu.h>
#include <util.h>

#include <asm/barrier.h>
#include <asm/cache.h>
#include <asm/cpu.h>

#include "useraccess.h"

static size_t
useraccess_copy_from_to_translated_pa(addrspace_va_lookup_t lookup,
				      bool from_guest, void *hyp_buf,
				      size_t remaining)
{
	void *va = partition_phys_map(lookup.phys, lookup.size);

	partition_phys_access_enable(va);

	if (compiler_unexpected(from_guest && !lookup.coherent)) {
		cache_clean_range(va, util_min(remaining, lookup.size));
	}

	size_t copied_size;
	if (from_guest) {
		copied_size = memscpy(hyp_buf, remaining, va, lookup.size);
	} else {
		copied_size = memscpy(va, lookup.size, hyp_buf, remaining);
	}

	if (compiler_unexpected(!from_guest && !lookup.coherent)) {
		cache_clean_invalidate_range(va, copied_size);
	}

	partition_phys_access_disable(va);

	partition_phys_unmap(va, lookup.phys, lookup.size);

	return copied_size;
}

static size_result_t
useraccess_copy_from_to_guest_va(gvaddr_t gvaddr, void *hvaddr, size_t size,
				 bool from_guest, bool force_access)
{
	error_t	 ret	   = OK;
	size_t	 remaining = size;
	gvaddr_t guest_va  = gvaddr;
	void	*hyp_buf   = hvaddr;

	assert(hyp_buf != NULL);
	assert(remaining != 0U);

	if (util_add_overflows((uintptr_t)hvaddr, size - 1U) ||
	    util_add_overflows(gvaddr, size - 1U)) {
		ret = ERROR_ADDR_OVERFLOW;
		goto out;
	}

	do {
		// Guest stage 2 lookups are in RCU read-side critical sections
		// so that unmap or access change operations can wait for them
		// to complete.
		rcu_read_start();

		addrspace_va_lookup_result_t lookup_r = addrspace_va_to_pa(
			guest_va, !from_guest && !force_access);

		if (compiler_expected(lookup_r.e == OK)) {
			// No fault; copy to/from the translated PA
			size_t copied_size =
				useraccess_copy_from_to_translated_pa(
					lookup_r.r, from_guest, hyp_buf,
					remaining);
			assert(copied_size > 0U);
			guest_va += copied_size;
			hyp_buf = (void *)((uintptr_t)hyp_buf + copied_size);
			remaining -= copied_size;
		} else {
			ret = lookup_r.e;
		}

		rcu_read_finish();
	} while ((remaining != 0U) && (ret == OK));

out:
	return (size_result_t){ .e = ret, .r = size - remaining };
}

size_result_t
useraccess_copy_from_guest_va(void *hyp_va, size_t hsize, gvaddr_t guest_va,
			      size_t gsize)
{
	size_result_t ret;
	bool force_access = false; // only write to guest_va requires this flag.
	if ((gsize == 0U) || (hsize < gsize)) {
		ret = size_result_error(ERROR_ARGUMENT_SIZE);
	} else {
		ret = useraccess_copy_from_to_guest_va(guest_va, hyp_va, gsize,
						       true, force_access);
	}
	return ret;
}

size_result_t
useraccess_copy_to_guest_va(gvaddr_t guest_va, size_t gsize, const void *hyp_va,
			    size_t hsize, bool force_access)
{
	size_result_t ret;
	if ((hsize == 0U) || (gsize < hsize)) {
		ret = size_result_error(ERROR_ARGUMENT_SIZE);
	} else {
		ret = useraccess_copy_from_to_guest_va(
			guest_va, (void *)(uintptr_t)hyp_va, hsize, false,
			force_access);
	}
	return ret;
}

static size_result_t
useraccess_copy_from_to_guest_ipa(addrspace_t *addrspace, vmaddr_t ipa,
				  void *hvaddr, size_t size, bool from_guest,
				  bool force_access, bool force_coherent)
{
	error_t ret    = OK;
	size_t	offset = 0U;

	if (util_add_overflows((uintptr_t)hvaddr, size - 1U) ||
	    util_add_overflows(ipa, size - 1U)) {
		ret = ERROR_ADDR_OVERFLOW;
		goto out;
	}

	while (offset < size) {
		paddr_t		     mapped_base;
		size_t		     mapped_size;
		pgtable_vm_memtype_t mapped_memtype;
		pgtable_access_t     mapped_vm_kernel_access;
		pgtable_access_t     mapped_vm_user_access;

		// Guest stage 2 lookups are in RCU read-side critical sections
		// so that unmap or access change operations can wait for them
		// to complete.
		rcu_read_start();

		if (!pgtable_vm_lookup(
			    &addrspace->vm_pgtable, ipa + offset, &mapped_base,
			    &mapped_size, &mapped_memtype,
			    &mapped_vm_kernel_access, &mapped_vm_user_access)) {
			rcu_read_finish();
			ret = ERROR_ADDR_INVALID;
			break;
		}

		if (!force_access &&
		    !pgtable_access_check(mapped_vm_kernel_access,
					  (from_guest ? PGTABLE_ACCESS_R
						      : PGTABLE_ACCESS_W))) {
			rcu_read_finish();
			ret = ERROR_DENIED;
			break;
		}

		size_t mapping_offset = (ipa + offset) & (mapped_size - 1U);
		mapped_base += mapping_offset;
		mapped_size -= mapping_offset;

		uint8_t *vm_addr = partition_phys_map(mapped_base, mapped_size);
		partition_phys_access_enable(vm_addr);

		uint8_t *hyp_va	  = (uint8_t *)hvaddr + offset;
		size_t	 hyp_size = size - offset;
		size_t	 copied_size;

		if (from_guest) {
			if (force_coherent ||
			    (mapped_memtype != PGTABLE_VM_MEMTYPE_NORMAL_WB)) {
				cache_clean_invalidate_range(
					(void *)vm_addr,
					util_min(mapped_size, hyp_size));
			}

			copied_size =
				memscpy(hyp_va, hyp_size, vm_addr, mapped_size);
		} else {
			copied_size =
				memscpy(vm_addr, mapped_size, hyp_va, hyp_size);

			if (force_coherent ||
			    (mapped_memtype != PGTABLE_VM_MEMTYPE_NORMAL_WB)) {
				cache_clean_range((void *)vm_addr, copied_size);
			}
		}

		partition_phys_access_disable(vm_addr);
		partition_phys_unmap(vm_addr, mapped_base, mapped_size);

		rcu_read_finish();

		offset += copied_size;
	}

out:
	return (size_result_t){ .e = ret, .r = offset };
}

size_result_t
useraccess_copy_from_guest_ipa(addrspace_t *addrspace, void *hyp_va,
			       size_t hsize, vmaddr_t guest_ipa, size_t gsize,
			       bool force_access, bool force_coherent)
{
	size_result_t ret;
	if ((gsize == 0U) || (hsize < gsize)) {
		ret = size_result_error(ERROR_ARGUMENT_SIZE);
	} else {
		ret = useraccess_copy_from_to_guest_ipa(addrspace, guest_ipa,
							hyp_va, gsize, true,
							force_access,
							force_coherent);
	}
	return ret;
}

size_result_t
useraccess_copy_to_guest_ipa(addrspace_t *addrspace, vmaddr_t guest_ipa,
			     size_t gsize, const void *hyp_va, size_t hsize,
			     bool force_access, bool force_coherent)
{
	size_result_t ret;
	if ((hsize == 0U) || (gsize < hsize)) {
		ret = size_result_error(ERROR_ARGUMENT_SIZE);
	} else {
		ret = useraccess_copy_from_to_guest_ipa(
			addrspace, guest_ipa, (void *)(uintptr_t)hyp_va, hsize,
			false, force_access, force_coherent);
	}
	return ret;
}
