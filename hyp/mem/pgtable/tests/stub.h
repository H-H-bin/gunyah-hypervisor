// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef STUB_H
#define STUB_H

void
init(void);

void_ptr_result_t
partition_alloc(partition_t *partition, size_t bytes, size_t min_alignment);

error_t
partition_free(partition_t *partition, void *mem, size_t bytes);

error_t
partition_free_phys(partition_t *partition, paddr_t mem, size_t bytes);

void *
partition_phys_map(paddr_t paddr, size_t size);

void
partition_phys_unmap(void *vaddr, paddr_t paddr, size_t size);

void
partition_phys_access_enable(void *ptr);

void
partition_phys_access_disable(void *ptr);

void
pgtable_handle_boot_runtime_warm_init(void);

bool
has_pmem_allocated(void);

void
reset_pmem_allocation(void);

void
cleanup(void);

#ifndef NDEBUG
void
dump_memory_allocation(void);
#endif

partition_t *
partition_get_private(void);

paddr_t
partition_virt_to_phys(partition_t *part, uintptr_t vaddr);

void
panic(const char *msg);

void
pgtable_vm_init_regs(pgtable_vm_t *vm_pgtable);

#endif // STUB_H
