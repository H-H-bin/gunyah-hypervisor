// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Return true if the specified VM page table might have transiently invalid
// entries caused by a break-before-make sequence during an in-place split or
// merge of a valid block mapping. This can cause the hypervisor to retry VM
// instructions that have taken a stage 2 translation fault.
bool
pgtable_vm_undergoing_bbm(pgtable_vm_t *pgtable) EXCLUDE_LOCK(pgtable);

#if defined(PGTABLE_VM_PLATFORM_MANAGED) && PGTABLE_VM_PLATFORM_MANAGED
// It may be possible that Gunyah does not manage the page tables for certain
// VMs. This function allows us to get page tables to do lookups, but no
// mappings.
error_t
pgtable_vm_platform_init(partition_t *partition, pgtable_vm_t *pgtable,
			 vmid_t vmid, VTTBR_EL2_t vttbr, VTCR_EL2_t vtcr);
#endif
