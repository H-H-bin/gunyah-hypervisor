// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// These flags are returned to userspace in vcpu_run results, so they are public
// API and must not be changed.
#define PSCI_REQUEST_SYSTEM_RESET     (1UL << 63)
#define PSCI_REQUEST_SYSTEM_RESET2_64 (1UL << 62)

uint32_t
psci_smc_psci_version(void);
