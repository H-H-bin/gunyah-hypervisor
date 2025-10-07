// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcall_def.h>
#include <hyprights.h>

#include <atomic.h>
#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <partition.h>
#include <spinlock.h>
#include <util.h>
#include <virq.h>

#include <asm/nospec_checks.h>

#include "useraccess.h"
#include "virtio_input.h"

error_t
hypercall_virtio_input_configure(cap_id_t virtio_backend_cap, uint64_t devids,
				 uint32_t prop_bits, uint32_t num_evtypes,
				 uint32_t num_absaxes)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;
	partition_t	 *partition	 = virtio_backend->header.partition;

	// Must be a virtio-input device
	if (virtio_backend->virtio.device_type != VIRTIO_DEVICE_TYPE_INPUT) {
		ret = ERROR_OBJECT_CONFIG;
		goto release_virtio_object_unlocked;
	}

	// Validate the upper bound for evtypes and absaxes
	if ((num_evtypes > VIRTIO_INPUT_MAX_EV_TYPES) ||
	    (num_absaxes > VIRTIO_INPUT_MAX_ABS_AXES)) {
		ret = ERROR_ARGUMENT_INVALID;
		goto release_virtio_object_unlocked;
	}

	spinlock_acquire(&virtio_backend->header.lock);

	size_t		  alloc_size = 0U;
	void_ptr_result_t alloc_ret;
	// allocate mem for evtypes, if not already allocated and count is > 0
	if ((num_evtypes > 0U) &&
	    (atomic_load_relaxed(&virtio_backend->input_data->ev_bits_count) ==
	     0U)) {
		alloc_size = num_evtypes * sizeof(virtio_input_ev_bits_t);
		alloc_ret  = partition_alloc(partition, alloc_size,
					     alignof(virtio_input_ev_bits_t));
		if (alloc_ret.e != OK) {
			ret = ERROR_NOMEM;
			goto release_virtio_object;
		}
		(void)memset_s(alloc_ret.r, alloc_size, 0, alloc_size);
		virtio_input_ev_bits_t *ev_bits =
			(virtio_input_ev_bits_t *)alloc_ret.r;

		// set entry of each ev as VIRTIO_INPUT_SUBSEL_INVALID
		for (uint32_t entry = 0; entry < num_evtypes; entry++) {
			ev_bits[entry].subsel =
				(uint8_t)VIRTIO_INPUT_SUBSEL_INVALID;
		}

		virtio_backend->input_data->ev_bits = ev_bits;

		atomic_store_release(&virtio_backend->input_data->ev_bits_count,
				     num_evtypes);
	} else if (num_evtypes > 0U) {
		ret = ERROR_BUSY;
		goto release_virtio_object;
	} else {
		// it means device has no evtypes to register, no worries
	}

	// allocate mem for absaxes, if not already allocated and count is > 0
	if ((num_absaxes > 0U) &&
	    (atomic_load_relaxed(&virtio_backend->input_data->absinfo_count) ==
	     0U)) {
		assert(virtio_backend->input_data->absinfo == NULL);

		alloc_size = num_absaxes * sizeof(virtio_input_absinfo_t);
		alloc_ret  = partition_alloc(partition, alloc_size,
					     alignof(virtio_input_absinfo_t));
		if (alloc_ret.e != OK) {
			ret = ERROR_NOMEM;
			goto release_virtio_object;
		}
		(void)memset_s(alloc_ret.r, alloc_size, 0, alloc_size);
		virtio_input_absinfo_t *absinfo =
			(virtio_input_absinfo_t *)alloc_ret.r;

		// set entry of each absinfo as VIRTIO_INPUT_SUBSEL_INVALID
		for (uint32_t entry = 0; entry < num_absaxes; entry++) {
			absinfo[entry].subsel =
				(uint8_t)VIRTIO_INPUT_SUBSEL_INVALID;
		}

		virtio_backend->input_data->absinfo = absinfo;

		atomic_store_release(&virtio_backend->input_data->absinfo_count,
				     num_absaxes);
	} else if (num_absaxes > 0U) {
		ret = ERROR_BUSY;
		goto release_virtio_object;
	} else {
		// device has no absaxes info to register
	}

	// save the devids and propbits
	virtio_backend->input_data->devids    = devids;
	virtio_backend->input_data->prop_bits = prop_bits;

	ret = OK;
release_virtio_object:
	spinlock_release(&virtio_backend->header.lock);
release_virtio_object_unlocked:
	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}

error_t
hypercall_virtio_input_set_data(cap_id_t virtio_backend_cap, uint32_t sel,
				uint32_t subsel, uint32_t size, vmaddr_t data)
{
	error_t	  ret;
	cspace_t *cspace = cspace_get_self();

	virtio_backend_ptr_result_t p = cspace_lookup_virtio_backend(
		cspace, virtio_backend_cap, CAP_RIGHTS_VIRTIO_BACKEND_CONFIG);
	if (compiler_unexpected(p.e != OK)) {
		ret = p.e;
		goto out;
	}
	virtio_backend_t *virtio_backend = p.r;

	// Must be a virtio-input device
	if (virtio_backend->virtio.device_type != VIRTIO_DEVICE_TYPE_INPUT) {
		ret = ERROR_CSPACE_WRONG_OBJECT_TYPE;
		goto release_virtio_object;
	}

	switch ((virtio_input_config_select_t)sel) {
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_ID_NAME: {
		// Only subsel 0 is valid for this sel value
		// size 0 is allowed
		if ((subsel == 0U) && (size != 0U)) {
			// copy data from guest va; size is checked by this API
			ret = useraccess_copy_from_guest_va(
				      virtio_backend->input_data->name,
				      sizeof(virtio_backend->input_data->name),
				      data, size)
				      .e;
			if (ret == OK) {
				virtio_backend->input_data->name_size = size;
			} else {
				// ret set to argument size error
				virtio_backend->input_data->name_size = 0U;
			}
		} else if (size == 0U) {
			// allowed
			virtio_backend->input_data->name_size = 0U;
			ret				      = OK;
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_ID_SERIAL: {
		// Only subsel 0 is valid for this sel value
		// size 0 is allowed
		if ((subsel == 0U) && (size != 0U)) {
			// copy data from guest va; size is checked by this API
			ret = useraccess_copy_from_guest_va(
				      virtio_backend->input_data->serial,
				      sizeof(virtio_backend->input_data->serial),
				      data, size)
				      .e;
			if (ret == OK) {
				virtio_backend->input_data->serial_size = size;
			} else {
				// ret set to argument size error
				virtio_backend->input_data->serial_size = 0U;
			}
		} else if (size == 0U) {
			// allowed
			virtio_backend->input_data->serial_size = 0U;
			ret					= OK;
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_ID_DEVIDS: {
		// Only subsel 0 is valid for this sel value
		if (subsel == 0U) {
			// copy data from guest va; size is checked by this API
			// TODO: should we memset here?
			ret = useraccess_copy_from_guest_va(
				      &virtio_backend->input_data->devids,
				      sizeof(virtio_backend->input_data->devids),
				      data, size)
				      .e;
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_PROP_BITS: {
		// Only subsel 0 is valid for this sel value
		if (subsel == 0U) {
			// copy data from guest va; size is checked by this API
			// TODO: should we memset here?
			ret = useraccess_copy_from_guest_va(
				      &virtio_backend->input_data->prop_bits,
				      sizeof(virtio_backend->input_data
						     ->prop_bits),
				      data, size)
				      .e;
		} else {
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_EV_BITS: {
		// check if mem is allocated for ev_bits
		count_t ev_bits_count = atomic_load_acquire(
			&virtio_backend->input_data->ev_bits_count);
		if (ev_bits_count > 0U) {
			ret = virtio_input_set_data_sel_ev_bits(
				ev_bits_count,
				virtio_backend->input_data->ev_bits, subsel,
				size, data);
		} else {
			// Not properly configured
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_ABS_INFO: {
		// check if mem is allocated for absinfo
		count_t absinfo_count = atomic_load_acquire(
			&virtio_backend->input_data->absinfo_count);
		if (absinfo_count > 0U) {
			ret = virtio_input_set_data_sel_abs_info(
				absinfo_count,
				virtio_backend->input_data->absinfo, subsel,
				size, data);
		} else {
			// Not properly configured
			ret = ERROR_ARGUMENT_INVALID;
		}
		break;
	}
	case VIRTIO_INPUT_CONFIG_SELECT_CFG_UNSET:
	default:
		// invalid select event
		ret = ERROR_ARGUMENT_INVALID;
		break;
	}

release_virtio_object:
	object_put_virtio_backend(virtio_backend);
out:
	return ret;
}
