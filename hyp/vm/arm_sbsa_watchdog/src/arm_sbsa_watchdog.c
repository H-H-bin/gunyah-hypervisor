// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#if defined(INTERFACE_VCPU)

#include <hypcontainers.h>
#include <hyprights.h>

#include <compiler.h>
#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <platform_timer.h>
#include <platform_watchdog.h>
#include <thread.h>
#include <util.h>
#include <vdevice.h>
#include <watchdog.h>

#include "event_handlers.h"

static uint64_t
calculate_current_compare_reg(watchdog_t *wdt)
{
	watchdog_abs_ticks_t wdt_now = platform_watchdog_get_last_pat() +
				       platform_watchdog_get_counter();
	watchdog_abs_ticks_t remaining_time_wdt_ticks =
		wdt->next_timeout - wdt_now;
	watchdog_milliseconds_t remaining_time_ms =
		platform_watchdog_ticks_to_ms(remaining_time_wdt_ticks);
	ticks_t remaining_time_ticks =
		platform_timer_convert_ms_to_ticks(remaining_time_ms);
	ticks_t now = platform_timer_get_current_ticks_sync();
	return now + remaining_time_ticks;
}

static void
write_new_compare_reg(watchdog_t *wdt, sbsa_wdog_compare_t wcv)
{
	// Calculate how much in the future it is
	ticks_t			compare_ticks = sbsa_wdog_compare_raw(wcv);
	ticks_t			now = platform_timer_get_current_ticks_sync();
	ticks_t			timeout_ticks = compare_ticks - now;
	watchdog_milliseconds_t timeout_ms =
		platform_timer_convert_ticks_to_ms(timeout_ticks);

	// Set the bark or bite accordingly
	watchdog_set_active_time(wdt, timeout_ms);
}

static void
watchdog_reg_read_refresh(size_t offset, register_t *value)
{
	if (offset == offsetof(sbsa_watchdog_refresh_frame_t, id)) {
		// W_IIDR
		sbsa_wdog_iid_t id = sbsa_wdog_iid_default();
		*value		   = sbsa_wdog_iid_raw(id);
	} else {
		// All other registers are treated as RAZ.
		*value = 0;
	}
}

static void
watchdog_reg_read_control(watchdog_t *wdt, size_t offset, register_t *value)
{
	if (offset == offsetof(sbsa_watchdog_control_frame_t, wcs)) {
		sbsa_wdog_control_t wcs = sbsa_wdog_control_default();
		sbsa_wdog_control_set_enable(&wcs, watchdog_is_enabled(wdt));
		sbsa_wdog_control_set_ws0_status(&wcs,
						 watchdog_is_expired(wdt));
		*value = sbsa_wdog_control_raw(wcs);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wor_lo)) {
		sbsa_wdog_offset_t wor = sbsa_wdog_offset_cast(
			platform_timer_convert_ms_to_ticks(wdt->offset_reg_ms));
		*value = sbsa_wdog_offset_get_offset_lo(&wor);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wor_hi)) {
		sbsa_wdog_offset_t wor = sbsa_wdog_offset_cast(
			platform_timer_convert_ms_to_ticks(wdt->offset_reg_ms));
		*value = sbsa_wdog_offset_get_offset_hi(&wor);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wcv_lo)) {
		ticks_t wcv = calculate_current_compare_reg(wdt);
		*value	    = (uint32_t)wcv;
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wcv_hi)) {
		ticks_t wcv = calculate_current_compare_reg(wdt);
		*value	    = (uint32_t)(wcv >> 32);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, id)) {
		// W_IIDR
		sbsa_wdog_iid_t id = sbsa_wdog_iid_default();
		*value		   = sbsa_wdog_iid_raw(id);
	} else {
		// All other registers are treated as RAZ.
		*value = 0;
	}
}

static void
watchdog_reg_write_refresh(watchdog_t *wdt, size_t offset)
{
	if (offset == offsetof(sbsa_watchdog_refresh_frame_t, refresh)) {
		watchdog_pat(wdt);
	} else {
		// All other registers are treated as WI.
	}
}

static void
watchdog_reg_write_control(watchdog_t *wdt, size_t offset, register_t *value)
{
	if (offset == offsetof(sbsa_watchdog_control_frame_t, wcs)) {
		sbsa_wdog_control_t wcs =
			sbsa_wdog_control_cast((uint32_t)*value);
		watchdog_control(wdt, sbsa_wdog_control_get_enable(&wcs));
		// Implicit pat
		watchdog_pat(wdt);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wor_lo)) {
		// Read the current offset
		sbsa_wdog_offset_t wor = sbsa_wdog_offset_cast(
			platform_timer_convert_ms_to_ticks(wdt->offset_reg_ms));
		// Update the lower half
		sbsa_wdog_offset_set_offset_lo(&wor, (uint32_t)*value);
		// Convert to milliseconds
		watchdog_milliseconds_t bark_ms =
			platform_timer_convert_ticks_to_ms(
				sbsa_wdog_offset_raw(wor));
		// Set the new bark and bite values
		watchdog_set_times(wdt, bark_ms, 2 * bark_ms);
		wdt->offset_reg_ms = bark_ms;
		// Implicit pat
		watchdog_pat(wdt);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wor_hi)) {
		// Read the current offset
		sbsa_wdog_offset_t wor = sbsa_wdog_offset_cast(
			platform_timer_convert_ms_to_ticks(wdt->offset_reg_ms));
		// Update the upper half
		sbsa_wdog_offset_set_offset_hi(&wor, (uint16_t)*value);
		// Convert to milliseconds
		watchdog_milliseconds_t bark_ms =
			platform_timer_convert_ticks_to_ms(
				sbsa_wdog_offset_raw(wor));
		// Set the new bark and bite values
		watchdog_set_times(wdt, bark_ms, 2 * bark_ms);
		wdt->offset_reg_ms = bark_ms;
		// Implicit pat
		watchdog_pat(wdt);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wcv_lo)) {
		// Calculate the current compare value
		sbsa_wdog_compare_t wcv = sbsa_wdog_compare_cast(
			calculate_current_compare_reg(wdt));
		// Update it with the new value
		sbsa_wdog_compare_set_compare_lo(&wcv, (uint32_t)*value);
		wdt->compare_overridden = true;

		write_new_compare_reg(wdt, wcv);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, wcv_hi)) {
		// Calculate the current compare value
		sbsa_wdog_compare_t wcv = sbsa_wdog_compare_cast(
			calculate_current_compare_reg(wdt));
		// Update it with the new value
		sbsa_wdog_compare_set_compare_hi(&wcv, (uint32_t)*value);
		wdt->compare_overridden = true;

		write_new_compare_reg(wdt, wcv);
	} else if (offset == offsetof(sbsa_watchdog_control_frame_t, id)) {
		// W_IIDR, WI
	} else {
		// All other registers are treated as WI.
	}
}

vcpu_trap_result_t
arm_sbsa_watchdog_handle_vdevice_access(vdevice_type_t type, vdevice_t *vdevice,
					size_t offset, size_t access_size,
					register_t *value, bool is_write)
{
	assert(vdevice != NULL);

	vcpu_trap_result_t ret;

	assert(type == VDEVICE_TYPE_WATCHDOG);

	watchdog_t *wdt = watchdog_container_of_vdevice(vdevice);
	assert(wdt != NULL);

	// Only 32-bit and 32-bit-aligned accesses are allowed
	if (access_size != sizeof(uint32_t) ||
	    !util_is_baligned(offset, sizeof(uint32_t))) {
		ret = VCPU_TRAP_RESULT_FAULT;
		goto out;
	}

	if (is_write) {
		if (offset < SBSA_WATCHDOG_FRAME_STRIDE) {
			// Control frame
			watchdog_reg_write_control(wdt, offset, value);
		} else {
			// Refresh frame
			watchdog_reg_write_refresh(
				wdt, offset - SBSA_WATCHDOG_FRAME_STRIDE);
		}
	} else {
		if (offset < SBSA_WATCHDOG_FRAME_STRIDE) {
			// Control frame
			watchdog_reg_read_control(wdt, offset, value);
		} else {
			// Refresh frame
			watchdog_reg_read_refresh(
				offset - SBSA_WATCHDOG_FRAME_STRIDE, value);
		}
	}
	ret = VCPU_TRAP_RESULT_EMULATED;

out:
	return ret;
}

error_t
arm_sbsa_watchdog_handle_object_create_watchdog(watchdog_create_t params)
{
	watchdog_t *wdt = params.watchdog;
	assert(wdt != NULL);

	wdt->offset_reg_ms	= 0U;
	wdt->compare_overridden = false;
	vdevice_init(&wdt->vdevice);

	return OK;
}

error_t
arm_sbsa_watchdog_handle_addrspace_attach_vdevice(
	addrspace_t *addrspace, cap_id_t vdevice_cap, vmaddr_t vbase,
	size_t size, addrspace_attach_vdevice_flags_t flags)
{
	error_t	  err;
	cspace_t *cspace = cspace_get_self();

	watchdog_ptr_result_t wdt_r = cspace_lookup_watchdog(
		cspace, vdevice_cap, CAP_RIGHTS_WATCHDOG_ATTACH_VDEVICE);
	if (compiler_unexpected(wdt_r.e) != OK) {
		err = wdt_r.e;
		goto out;
	}

	if ((!util_is_baligned(vbase, SBSA_WATCHDOG_FRAME_STRIDE)) ||
	    (size != SBSA_WATCHDOG_SIZE) || (flags.raw != 0U)) {
		err = ERROR_ARGUMENT_INVALID;
	} else {
		err = vdevice_attach_vmaddr(VDEVICE_TYPE_WATCHDOG,
					    &wdt_r.r->vdevice, addrspace, vbase,
					    SBSA_WATCHDOG_SIZE);
	}

	object_put_watchdog(wdt_r.r);
out:
	return err;
}

void
arm_sbsa_watchdog_handle_watchdog_pat_pre(watchdog_t *wdt)
{
	if (!wdt->is_hyp && wdt->compare_overridden) {
		// Configure the bark and bite times again in case they had been
		// overwritten by a direct write to the COMPARE register.
		wdt->bark_time		= wdt->offset_reg_ms;
		wdt->bite_time		= wdt->offset_reg_ms * 2;
		wdt->compare_overridden = false;
	}
}

void
arm_sbsa_watchdog_handle_object_deactivate_watchdog(watchdog_t *watchdog)
{
	if (watchdog->vdevice.type != VDEVICE_TYPE_NONE) {
		vdevice_detach_vmaddr(&watchdog->vdevice);
	}
}
#endif
