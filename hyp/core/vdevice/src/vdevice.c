// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <object.h>
#include <range_tree.h>
#include <spinlock.h>
#include <util.h>
#include <vdevice.h>

#include "event_handlers.h"

error_t
vdevice_attach_phys(vdevice_t *vdevice, memextent_t *memextent)
{
	assert(vdevice != NULL);
	assert(memextent != NULL);
	assert(vdevice->type != VDEVICE_TYPE_NONE);

	vdevice_t *null_vdevice = NULL;
	return atomic_compare_exchange_strong_explicit(&memextent->vdevice,
						       &null_vdevice, vdevice,
						       memory_order_release,
						       memory_order_relaxed)
		       ? OK
		       : ERROR_BUSY;
}

void
vdevice_detach_phys(vdevice_t *vdevice, memextent_t *memextent)
{
	vdevice_t *old_vdevice = atomic_exchange_explicit(
		&memextent->vdevice, NULL, memory_order_relaxed);
	assert(old_vdevice == vdevice);
}

error_t
vdevice_handle_object_create_addrspace(addrspace_create_t addrspace_create)
{
	addrspace_t *addrspace = addrspace_create.addrspace;
	assert(addrspace != NULL);

	spinlock_init(&addrspace->vdevice_lock);

	return range_tree_init(&addrspace->vdevice_tree,
			       addrspace->header.partition);
}

void
vdevice_handle_object_cleanup_addrspace(addrspace_t *addrspace)
{
	assert(addrspace != NULL);

	// Note that attached vdevices hold references to the addrspace, so we
	// know that there are none by this point.
	range_tree_destroy(&addrspace->vdevice_tree, RANGE_TREE_NODE_TYPE_NONE);
}

error_t
vdevice_attach_vmaddr(vdevice_t *vdevice, addrspace_t *addrspace, vmaddr_t ipa,
		      size_t size)
{
	error_t err;

	assert(vdevice != NULL);
	assert(addrspace != NULL);
	assert(vdevice->type != VDEVICE_TYPE_NONE);

	if (vdevice->addrspace != NULL) {
		err = ERROR_BUSY;
		goto out;
	}

	spinlock_acquire(&addrspace->vdevice_lock);

	err = range_tree_insert(&addrspace->vdevice_tree, &vdevice->range, ipa,
				size);

	if (err == OK) {
		vdevice->addrspace = object_get_addrspace_additional(addrspace);
	}

	spinlock_release(&addrspace->vdevice_lock);

out:
	return err;
}

void
vdevice_detach_vmaddr(vdevice_t *vdevice)
{
	assert(vdevice != NULL);
	assert(vdevice->type != VDEVICE_TYPE_NONE);

	addrspace_t *addrspace = vdevice->addrspace;
	assert(addrspace != NULL);

	spinlock_acquire(&addrspace->vdevice_lock);

	error_t err =
		range_tree_remove(&addrspace->vdevice_tree, &vdevice->range);
	assert(err == OK);

	vdevice->addrspace = NULL;

	spinlock_release(&addrspace->vdevice_lock);

	object_put_addrspace(addrspace);
}
