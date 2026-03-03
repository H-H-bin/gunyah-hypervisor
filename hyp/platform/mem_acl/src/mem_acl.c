// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hyprights.h>

#include <cspace.h>
#include <cspace_lookup.h>
#include <list.h>
#include <memextent.h>
#include <object.h>
#include <panic.h>
#include <pgtable.h>
#include <platform_mem.h>
#include <spinlock.h>
#include <util.h>

#include "useraccess.h"

static error_t
platform_mem_acl_register(memextent_t *me, mem_acl_t acl[MAX_MEM_ACL_LEN],
			  count_t acl_len)
{
	error_t ret;

	assert(me != NULL);
	assert(acl_len <= MAX_MEM_ACL_LEN);

	spinlock_acquire(&me->lock);

	if (!list_is_empty(&me->children_list) && me->mem_acl_registered) {
		// The extent may already have active ffa_memory/SHMBs derived
		// from it; we cannot change the ACL.
		// It's okay to allow for the registered ACL to be changed if no
		// FF-A shares or SHM bridges are active.
		ret = ERROR_DENIED;
		goto out;
	}

	for (index_t i = 0U; i < acl_len; i++) {
		if ((acl[i].vmid == 0U) ||
		    (acl[i].vmid >= ADDRSPACE_NUM_VMIDS)) {
			ret = ERROR_ARGUMENT_INVALID;
			goto out;
		}

		pgtable_access_result_t access_res =
			pgtable_access_cast_safe(acl[i].perm);
		if (access_res.e != OK) {
			ret = ERROR_ARGUMENT_INVALID;
			goto out;
		}
		if (!pgtable_access_check(me->access, access_res.r)) {
			ret = ERROR_DENIED;
			goto out;
		}

		if (!mem_acl_flags_is_clean(acl[i].flags)) {
			ret = ERROR_ARGUMENT_INVALID;
			goto out;
		}

		for (index_t j = 0U; j < i; j++) {
			if (acl[i].vmid == acl[j].vmid) {
				ret = ERROR_ARGUMENT_INVALID;
				goto out;
			}
		}
	}

	(void)memset_s(me->mem_acl_info, sizeof(me->mem_acl_info), 0,
		       sizeof(me->mem_acl_info));

	for (index_t i = 0; i < acl_len; i++) {
		mem_acl_info_t *info = &me->mem_acl_info[i];
		mem_acl_info_set_vmid(info, acl[i].vmid);
		mem_acl_info_set_perm(info, acl[i].perm);
		if (mem_acl_flags_get_private(&acl[i].flags)) {
			mem_acl_info_set_private(info, true);
		}
		if (mem_acl_flags_get_identity_mapping(&acl[i].flags)) {
			mem_acl_info_set_identity_mapping(info, true);
		}
	}

	me->mem_acl_len	       = (count_t)acl_len;
	me->mem_acl_registered = true;

	ret = OK;
out:
	spinlock_release(&me->lock);
	return ret;
}

void
platform_smc_mem_acl_register(vmid_t vmid, uint64_t args[7])
{
	error_t ret;

	if (vmid != ROOT_VM_VMID) {
		ret = ERROR_UNIMPLEMENTED;
		goto out;
	}

	cap_id_t me_cap = args[1];
	gvaddr_t va	= args[2];
	size_t	 gsize	= args[3];
	uint64_t res0	= args[4];

	// Reserved for future extension.
	if (res0 != 0U) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	mem_acl_t acl[MAX_MEM_ACL_LEN] = { 0 };

	if (!util_is_baligned(gsize, sizeof(acl[0]))) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	count_t acl_len = (count_t)(gsize / sizeof(acl[0]));
	if (acl_len > MAX_MEM_ACL_LEN) {
		ret = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	// An empty ACL may be provided if we want to explicitly disallow
	// sharing memory of this extent with secure world.
	if (gsize != 0U) {
		size_result_t copy_ret = useraccess_copy_from_guest_va(
			acl, sizeof(acl), va, gsize);
		if (copy_ret.e != OK) {
			ret = copy_ret.e;
			goto out;
		}
	} else if (va != 0U) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out;
	} else {
		assert(acl_len == 0U);
	}

	// CAP_RIGHTS_MEMEXTENT_DERIVE: It means if we allow a SVM to be able to
	// derive the memextent in shm_bridge/ffa_memory creation call, RM must
	// at least have DERIVE right otherwise we should deny acl register.
	memextent_ptr_result_t me_ret = cspace_lookup_memextent(
		cspace_get_self(), me_cap, CAP_RIGHTS_MEMEXTENT_DERIVE);
	if (me_ret.e != OK) {
		ret = me_ret.e;
		goto out;
	}
	memextent_t *me = me_ret.r;
	assert(me != NULL);

	ret = platform_mem_acl_register(me, acl, acl_len);

	object_put_memextent(me);

out:
	args[0] = (register_t)ret;
}
