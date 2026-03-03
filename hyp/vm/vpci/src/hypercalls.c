// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <addrspace.h>
#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <rcu.h>
#include <spinlock.h>
#include <util.h>
#include <vic.h>
#include <vpci.h>

#include <events/vpci.h>

static size_result_t
vpci_aperture_check(addrspace_t *aspace, vpci_aperture_t aperture,
		    size_t min_size, size_t max_size, bool optional)
{
	size_result_t ret;

	vmaddr_t base = vpci_aperture_get_base(&aperture);
	index_t	 bits = vpci_aperture_get_bits(&aperture);

	if (!optional || (bits != 0U)) {
		size_t size = util_bit(bits);
		if ((size < min_size) || (size > max_size)) {
			ret = size_result_error(ERROR_ARGUMENT_SIZE);
			goto out;
		}
		if (!util_is_p2aligned(base, bits)) {
			ret = size_result_error(ERROR_ARGUMENT_ALIGNMENT);
			goto out;
		}
		if (util_add_overflows(base, size)) {
			ret = size_result_error(ERROR_ADDR_OVERFLOW);
			goto out;
		}
		error_t err = addrspace_check_range(aspace, base, size);
		if (err != OK) {
			ret = size_result_error(err);
			goto out;
		}
		ret = size_result_ok(size);
	} else {
		ret = size_result_ok(0U);
	}
out:
	return ret;
}

error_t
hypercall_vpci_configure(cap_id_t vpci_cap, cap_id_t aspace_cap,
			 cap_id_t vic_cap, vpci_aperture_t cam_aperture,
			 vpci_aperture_t     npmem_aperture,
			 vpci_aperture_t     pmem_aperture,
			 vpci_aperture_t     io_aperture,
			 vpci_option_flags_t options)
{
	error_t	      ret;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, vpci_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE, &type);
	if (compiler_unexpected(o.e != OK)) {
		ret = o.e;
		goto out_vpci_released;
	}
	if (type != OBJECT_TYPE_VPCI) {
		ret = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		object_put(type, o.r);
		goto out_vpci_released;
	}
	vpci_t *vpci = o.r.vpci;
	spinlock_acquire(&vpci->header.lock);

	if (atomic_load_relaxed(&vpci->header.state) != OBJECT_STATE_INIT) {
		ret = ERROR_OBJECT_STATE;
		goto out_vpci_ref;
	}

	if (vpci_option_flags_get_res0(&options) != 0U) {
		ret = ERROR_ARGUMENT_INVALID;
		goto out_vpci_ref;
	}
	bool pcie = vpci_option_flags_get_pcie(&options);
#if VPCI_PCIE_ONLY
	if (!pcie) {
		// Conventional PCI is not implemented yet
		ret = ERROR_ARGUMENT_INVALID;
		goto out_vpci_ref;
	}
#else
#error Conventional PCI not implemented
#endif

	// The address space cap needs map rights, because devices attached to
	// this bus can update mappings in the nominated apertures. It does not
	// need attach rights, which are for accessing the address space.
	addrspace_ptr_result_t aspace_r = cspace_lookup_addrspace(
		cspace, aspace_cap, CAP_RIGHTS_ADDRSPACE_MAP);
	if (compiler_unexpected(aspace_r.e != OK)) {
		ret = aspace_r.e;
		goto out_vpci_ref;
	}

	vmaddr_t      cam_base	 = vpci_aperture_get_base(&cam_aperture);
	size_result_t cam_size_r = vpci_aperture_check(
		aspace_r.r, cam_aperture, util_bit(pcie ? 21U : 17U),
		util_bit(pcie ? 28U : 24U) - 1U, false);
	if (cam_size_r.e != OK) {
		ret = cam_size_r.e;
		goto out_aspace_ref;
	}

	vmaddr_t      npmem_base   = vpci_aperture_get_base(&npmem_aperture);
	size_result_t npmem_size_r = vpci_aperture_check(
		aspace_r.r, npmem_aperture, 20U, SIZE_MAX, false);
	if (npmem_size_r.e != OK) {
		ret = npmem_size_r.e;
		goto out_aspace_ref;
	}

	vmaddr_t      pmem_base	  = vpci_aperture_get_base(&pmem_aperture);
	size_result_t pmem_size_r = vpci_aperture_check(
		aspace_r.r, pmem_aperture, 20U, SIZE_MAX, true);
	if (pmem_size_r.e != OK) {
		ret = pmem_size_r.e;
		goto out_aspace_ref;
	}

	vmaddr_t      io_base	= vpci_aperture_get_base(&io_aperture);
	size_result_t io_size_r = vpci_aperture_check(aspace_r.r, io_aperture,
						      12U, SIZE_MAX, true);
	if (io_size_r.e != OK) {
		ret = io_size_r.e;
		goto out_aspace_ref;
	}

	// The VIC cap needs bind-source rights, because devices bound to this
	// bus can act as MSI sources.
	vic_ptr_result_t vic_r =
		cspace_lookup_vic(cspace, vic_cap, CAP_RIGHTS_VIC_BIND_SOURCE);
	if (compiler_unexpected(vic_r.e != OK)) {
		ret = vic_r.e;
		goto out_aspace_ref;
	}

	// All the arguments are ok. Update the object.
	if (vpci->vic != NULL) {
		object_put_vic(vpci->vic);
	}
	vpci->vic = object_get_vic_additional(vic_r.r);

	if (vpci->addrspace != NULL) {
		object_put_addrspace(vpci->addrspace);
	}
	vpci->addrspace = object_get_addrspace_additional(aspace_r.r);

	vpci->cam_base	 = cam_base;
	vpci->cam_size	 = cam_size_r.r;
	vpci->npmem_base = npmem_base;
	vpci->npmem_size = npmem_size_r.r;
	vpci->pmem_base	 = pmem_base;
	vpci->pmem_size	 = pmem_size_r.r;
	vpci->io_base	 = io_base;
	vpci->io_size	 = io_size_r.r;

	ret = OK;

	object_put_vic(vic_r.r);
out_aspace_ref:
	object_put_addrspace(aspace_r.r);
out_vpci_ref:
	spinlock_release(&vpci->header.lock);
	object_put_vpci(vpci);
out_vpci_released:

	return ret;
}

hypercall_vpci_attach_result_t
hypercall_vpci_attach(cap_id_t vpci_cap, index_t slot_index,
		      cap_id_t device_cap)
{
	error_t	      err;
	cspace_t     *cspace = cspace_get_self();
	object_type_t type;

	object_ptr_result_t o = cspace_lookup_object_any(
		cspace, vpci_cap, CAP_RIGHTS_GENERIC_OBJECT_ACTIVATE, &type);
	if (compiler_unexpected(o.e != OK)) {
		err = o.e;
		goto out_vpci_released;
	}
	if (type != OBJECT_TYPE_VPCI) {
		err = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		object_put(type, o.r);
		goto out_vpci_released;
	}
	vpci_t *vpci = o.r.vpci;
	spinlock_acquire(&vpci->header.lock);

	if (atomic_load_relaxed(&vpci->header.state) != OBJECT_STATE_INIT) {
		err = ERROR_OBJECT_STATE;
		goto out_vpci_ref;
	}

	err = trigger_vpci_attach_event(vpci, &slot_index, device_cap);

out_vpci_ref:
	spinlock_release(&vpci->header.lock);
	object_put_vpci(vpci);
out_vpci_released:
	return (hypercall_vpci_attach_result_t){ .error	     = err,
						 .slot_index = slot_index };
}
