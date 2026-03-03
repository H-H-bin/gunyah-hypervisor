// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Clears all flags and sets all bits in the mask of the doorbell.
error_t
doorbell_reset(doorbell_t *doorbell);

// Sets the masks of the doorbell. The Enable Mask is the mask of set flags
// that will cause an assertion of the virtual interrupt bound to the doorbell.
// The Ack Mask controls which flags should be automatically cleared when the
// interrupt is asserted.
error_t
doorbell_mask(doorbell_t *doorbell, doorbell_flags_t new_enable_mask,
	      doorbell_flags_t new_ack_mask);

// Binds a Doorbell to a virtual interrupt.
error_t
doorbell_bind(doorbell_t *doorbell, vic_t *vic, virq_t virq);

// Unbinds a Doorbell from a virtual interrupt.
void
doorbell_unbind(doorbell_t *doorbell) EXCLUDE_PREEMPT_DISABLED;
