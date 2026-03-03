// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Sets flags of the doorbell. Returns old flags
doorbell_flags_result_t
doorbell_send(doorbell_t *doorbell, doorbell_flags_t new_flags);

// Reads and clears the flags of the doorbell. Returns old flags.
doorbell_flags_result_t
doorbell_receive(doorbell_t *doorbell, doorbell_flags_t clear_flags);
