// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <cspace.h>
#include <memdb.h>
#include <memextent.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <partition_alloc.h>
#include <platform_mem.h>
#include <platform_memory_layout.h>
#include <qcbor.h>
#include <spinlock.h>
#include <trace.h>
#include <util.h>

#include "event_handlers.h"

static platform_ram_info_t ram_info;

error_t
platform_ram_probe(void)
{
	// FIXME: The RAM memory size is currently hardcoded to 1GB. We need to
	// find a better solution for this, possibly by using a
	// system-device-tree approach. We need to make sure that hyp RAM memory
	// ranges do not overlap with the ranges specified in the QEMU start
	// command.
	ram_info.num_ranges = 0x1;
	// TODO: Get info from DT
	ram_info.ram_range[0].base = PLATFORM_DDR_BASE;
	ram_info.ram_range[0].size = PLATFORM_DDR_SIZE;

	return OK;
}

platform_ram_info_t *
platform_get_ram_info(void)
{
	assert(ram_info.num_ranges != 0U);
	return &ram_info;
}

void
platform_add_root_heap(partition_t *partition)
{
	// We allocate 36MiB of memory from the Hyp labelled memory in the ram
	// partition table freelist.
	//  - We give 36MiB to the root partition heap and then allocate 32 MiB
	//  from the allocator to the trace buffer
	size_t trace_size      = TRACE_AREA_SIZE;
	size_t heap_extra_size = EXTRA_ROOT_HEAP_SIZE;
	size_t priv_size       = EXTRA_PRIVATE_HEAP_SIZE;

	uint64_t alloc_size = trace_size + heap_extra_size + priv_size;

	// FIXME: Currently using the end memory of the hardcoded 1Gb hyp RAM
	// memory size. We need to find a better solution for this, possibly by
	// dynamically reading the RAM memory end address from a device tree.

	paddr_t base = PLATFORM_DDR_BASE + PLATFORM_DDR_SIZE - alloc_size;

	// Add 1MiB to the hypervisor private partition
	error_t err = partition_mem_donate(partition, base, priv_size,
					   partition_get_private(), false);
	if (err != OK) {
		panic("Error donating memory");
	}

	err = partition_map_and_add_heap(partition_get_private(), base,
					 priv_size);
	if (err != OK) {
		panic("Error adding root partition heap memory");
	}

	base += priv_size;
	alloc_size -= priv_size;

	// Add the rest to the partition's heap.
	err = partition_map_and_add_heap(partition, base, alloc_size);
	if (err != OK) {
		panic("Error adding root partition heap memory");
	}

	// Allocate memory for the trace_buffer
	trace_init(partition, trace_size);
}

#if !defined(UNIT_TESTS)

static_assert(MODULE_MEM_MEMEXTENT_SPARSE, "SPARSE type must be defined");

void
soc_qemu_handle_rootvm_init(partition_t *root_partition, cspace_t *root_cspace,
			    hyp_env_data_t   *hyp_env,
			    qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	// FIXME: The memory layout for QEMU is hardcoded here. We need to find a
	// better solution for this, possibly by using a system-device-tree
	// approach, that is consumed by us, and used to generate the HLOS VM
	// device-tree. We will also need to get the addresses such as
	// hlos-entry from this config such that ultimately these can all be
	// inputs from QEMU/user.
	paddr_t hlos_vm_base = HLOS_VM_DDR_BASE;
	paddr_t hlos_vm_size = HLOS_VM_DDR_SIZE;

	assert(qcbor_enc_ctxt != NULL);

	// VM memory node. Includes entry point, DT, and rootfs
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "hlos_vm_base",
				   hlos_vm_base);
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "hlos_vm_size",
				   hlos_vm_size);
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "entry_hlos",
				   HLOS_ENTRY_POINT);
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "hlos_dt_base",
				   HLOS_DT_BASE);
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "hlos_ramfs_base",
				   HLOS_RAM_FS_BASE);

#if defined(WATCHDOG_DISABLE)
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "watchdog_supported", false);
#endif

	// Create a device memextent to cover the full HW physical address
	// space reserved for devices, so that the resource manager can derive
	// device memextents.
	// Long term the intention is for a system device-tree to allow fine
	// grained memextent creation.

	paddr_t phys_address_start = 0U;
	size_t	phys_address_size  = util_bit(PLATFORM_PHYS_ADDRESS_BITS);

	memextent_ptr_result_t me_ret = memextent_construct(
		root_partition, root_cspace, phys_address_start,
		phys_address_size, PGTABLE_ACCESS_RW, MEMEXTENT_MEMTYPE_DEVICE,
		MEMEXTENT_TYPE_SPARSE, true, &hyp_env->device_me_capid);
	if (me_ret.e != OK) {
		panic("Error construct rootvm device memextent");
	}

	memextent_t *me = me_ret.r;

	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "device_me_capid",
				   hyp_env->device_me_capid);

	// Donate all devices to root memextent
	const phys_range_t *device_layouts;
	count_t		    device_count;

	device_layouts = platform_get_device_layouts(&device_count);
	assert((device_layouts != NULL) && (device_count > 0U));

	QCBOREncode_OpenArrayInMap(qcbor_enc_ctxt, "device_ranges");

	for (count_t i = 0U; i < device_count; i++) {
		paddr_t phys_base = device_layouts[i].base;
		size_t	size	  = device_layouts[i].size;

		error_t ret = memextent_donate_device(
			me, phys_base - phys_address_start, size);
		if (ret != OK) {
			panic("Error donate device memory to root memextent");
		}
		QCBOREncode_OpenArray(qcbor_enc_ctxt);
		QCBOREncode_AddUInt64(qcbor_enc_ctxt, phys_base);
		QCBOREncode_AddUInt64(qcbor_enc_ctxt, size);
		QCBOREncode_CloseArray(qcbor_enc_ctxt);
	}

	QCBOREncode_CloseArray(qcbor_enc_ctxt);

	// Derive memextents for GICD, GICR and watchdog to effectively remove
	// them from the device memextent we provide to the rootvm.

	me_ret = memextent_derive(me, PLATFORM_GICD_BASE, 0x10000U,
				  MEMEXTENT_MEMTYPE_DEVICE, PGTABLE_ACCESS_RW,
				  MEMEXTENT_TYPE_BASIC);
	if (me_ret.e != OK) {
		panic("Failed creation of gicd memextent");
	}
	me_ret = memextent_derive(me, PLATFORM_GICR_BASE,
				  (PLATFORM_MAX_CORES << GICR_STRIDE_SHIFT),
				  MEMEXTENT_MEMTYPE_DEVICE, PGTABLE_ACCESS_RW,
				  MEMEXTENT_TYPE_BASIC);
	if (me_ret.e != OK) {
		panic("Failed creation of gicr memextent");
	}

	// Derive extent for UART and share it with RM
	me_ret = memextent_derive(me, PLATFORM_UART_BASE, PLATFORM_UART_SIZE,
				  MEMEXTENT_MEMTYPE_DEVICE, PGTABLE_ACCESS_RW,
				  MEMEXTENT_TYPE_BASIC);
	if (me_ret.e != OK) {
		panic("Failed creation of uart memextent");
	}

	// Create a master cap for the uart memextent
	object_ptr_t obj_ptr;
	obj_ptr.memextent	  = me_ret.r;
	cap_id_result_t capid_ret = cspace_create_master_cap(
		root_cspace, obj_ptr, OBJECT_TYPE_MEMEXTENT);
	if (capid_ret.e != OK) {
		panic("Error create memextent cap id.");
	}

	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "uart_address",
				   PLATFORM_UART_BASE);
	QCBOREncode_AddUInt64ToMap(qcbor_enc_ctxt, "uart_me_capid",
				   capid_ret.r);
}
#endif
