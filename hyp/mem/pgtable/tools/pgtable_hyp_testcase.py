#!/usr/bin/env python3
# Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# 2019 Cog Systems Pty Ltd.
#
# SPDX-License-Identifier: BSD-3-Clause

""" Script for test cases generation.

This module could generate one test input file for page table stage 1 test
case.

Todo:
    * Support stage 2 test input file
    * Support memory type & access
    * Refine destroy, should unmap previous mapping address
    ranges/preservations first.
"""

import bisect
import random
import math
import enum
import sys
import sortedcontainers
import queue
from transitions import Machine


class AddressManager:
    def __init__(self, page_size, partition, start_addr, addr_sz):
        assert (page_size & (page_size - 1)) == 0
        assert (addr_sz & (page_size - 1)) == 0

        self.page_size = page_size
        self.partition = partition
        self.start_addr = start_addr
        self.size = addr_sz

    def get_partition(self):
        return self.partition

    def get_preserved_size(self):
        if self.parent_vm is None:
            return 0
        return self.size

    def is_belonged(self, addr):
        return self.start_addr <= addr < self.start_addr + self.size

    def is_full(self):
        # need implement
        raise Exception()

    def is_empty(self):
        raise Exception()

    def get_preserved_level_4k(self):
        """
        Current rule only require a single preserved level for a particular
        size:
        size > 512G: level 0 should be preserved (always the truth)
        size >= 1G: level 1 should be preserved
        size >= 2M: level 2 should be preserved
        size < 2M, level 3 should be preserved
        """
        size_2m = 2 * 1024 * 1024
        size_1g = 1 * 1024 * 1024 * 1024
        size_512g = 512 * 1024 * 1024 * 1024

        size = self.size
        if size >= size_512g:
            return 0
        elif size >= size_1g:
            return 1
        elif size >= size_2m:
            return 2
        else:
            return 3

    def round_to_level_size(self, size):
        """
        Return the rounded size for preservation. Any preservation should
        occupy a whole table level, or else, we can not distinguish them.
        size > 512G: several entries at level 0, should preserve
        all = 512G * 512
        size >= 1G: just preserve 1G, should be 512G
        size >= 2M: level 2 should be preserved
        size < 2M, level 3 should be preserved
        """
        size_2m = 2 * 1024 * 1024
        size_1g = 1 * 1024 * 1024 * 1024
        size_512g = 512 * 1024 * 1024 * 1024

        if size < size_2m:
            return size_2m
        elif size < size_1g:
            return size_1g
        elif size < size_512g:
            return size_512g
        else:
            # wrong size, would occupy the whole address space range
            raise Exception()

    def alloc(self, alloc_size):
        """
        Return (success, addr). Success == False means allocation failure.
        """
        return (False, 0)

    def free(self, addr):
        pass

    def destroy(self):
        if self.parent_vm is not None:
            self.parent_vm.free(self.start_addr)
        else:
            # should not destroy a address manager without parent
            raise Exception()

    def get_random_free_address(self):
        """
        Return random address which is free.
        """
        # need implementation
        raise Exception()

    def max_alloc_sz(self):
        raise Exception()

    def get_allocations(self):
        """
        Return list of (address, size) per allocation
        """
        return []


class RunlengthAddressManager(AddressManager):
    """
    Simply manage address by run length.
    """

    def __init__(self, parent_vm, page_size, partition, start_addr, addr_sz):
        super().__init__(page_size, partition, start_addr, addr_sz)
        self.parent_vm = parent_vm

        def get_address(element):
            return element[0]

        self.free_list = sortedcontainers.SortedList(
            [(start_addr, addr_sz)], key=get_address
        )
        self.allocations = {}

    def alloc(self, alloc_size):
        assert alloc_size & (self.page_size - 1) == 0
        i = iter(self.free_list)
        address, size = next((x for x in i if x[1] >= alloc_size), (0, 0))
        if size == 0:
            return (False, 0)

        self.free_list.remove((address, size))
        alloc_addr = address
        remaining_size = size - alloc_size
        if remaining_size != 0:
            remaining_addr = alloc_addr + alloc_size
            self.free_list.add((remaining_addr, remaining_size))

        self.allocations[alloc_addr] = alloc_size

        return (True, alloc_addr)

    def free(self, addr):
        # need to merge
        assert addr in self.allocations

        size = self.allocations[addr]
        self.allocations.pop(addr)

        # shouldn't have items with the same address
        final_addr = addr
        final_size = size
        pos = self.free_list.bisect_left((addr, size))
        # check merge to upper if there is, should remove upper first
        if pos != len(self.free_list):
            upper_addr = self.free_list[pos][0]
            upper_size = self.free_list[pos][1]
            assert upper_addr > final_addr
            if upper_size == final_addr + final_size:
                self.free_list.pop(pos)
                final_size += upper_size

        if pos != 0:
            lower_addr = self.free_list[pos - 1][0]
            lower_size = self.free_list[pos - 1][1]
            assert lower_addr < final_addr
            if lower_addr + lower_size == final_addr:
                self.free_list.pop(pos - 1)
                final_addr = lower_addr
                final_size += lower_size

        self.free_list.add((final_addr, final_size))

    def is_full(self):
        return len(self.allocations) == 0

    def is_empty(self):
        return len(self.free_list) == 0

    def get_random_free_address(self):
        """
        Return random address which is still free (not allocated).
        """
        assert not self.is_empty()
        k, size = random.choice(self.free_list)
        offset = random.randint(0, size - 1)
        return k + offset

    def get_preserved_size(self):
        if self.parent_vm is None:
            return 0
        else:
            return self.parent_vm.get_preserved_size()

    def max_alloc_sz(self):
        max_size = 0
        for addr, size in self.free_list:
            if max_size < size:
                max_size = size

        return max_size

    def get_allocations(self):
        return list(self.allocations.items())


class BuddyAddressManager(AddressManager):
    """
    Simply manage virtual/physical memory address with buddy algorithm. Seems
    useless if no need to generate massive test cases...

    Assuming the start address is 0.
    """

    # max level count
    level_cnt = 36

    def __init__(self, parent_vm, page_size, partition, start_addr, addr_sz):
        super().__init__(page_size, partition, start_addr, addr_sz)

        if parent_vm is not None:
            assert (addr_sz & (addr_sz - 1)) == 0
            assert (start_addr & (addr_sz - 1)) == 0

        self.parent_vm = parent_vm
        self.zones = []
        self.preserved_vms = []

        self.allocations = {}
        self.bit_cnt = addr_sz.bit_length() - 1

        # the size actually allocated from parent, since we need to occupy
        # the whole table level
        self.actual_allocated_size = 0

        i = 0
        while i < BuddyAddressManager.level_cnt:
            self.zones.append([])
            i += 1

        # FIXME: could simplify the calculation
        idx = math.floor(math.log2(addr_sz / page_size))
        sz = page_size * (1 << idx)
        cnt = addr_sz / sz
        assert math.modf(cnt)[0] == 0

        i = 0
        while i < cnt:
            self.zones[idx].append(start_addr + i * sz)
            i += 1

    def alloc(self, alloc_size):
        """
        Alloc one chunk of address from zones with specified size. Rounds up to
        zone size
        Return (success, addr). Success == False means allocation failure.
        """
        # in case if alloc_size is just the size of zone, minus 1
        size = 1 << (alloc_size - 1).bit_length()
        idx = size.bit_length() - self.page_size.bit_length()
        assert idx < self.level_cnt

        if len(self.zones[idx]) == 0:
            if not self.split_bottom_up(idx):
                return (False, 0)

        assert len(self.zones[idx]) != 0
        # get last one from that zone
        addr = self.zones[idx].pop()

        # record the allocation
        self.allocations[addr] = size

        return (True, addr)

    def alloc_preservation(self, preserved_size, partition):
        """
        Preserve certain address, and track/return BuddyAddressManager
        to caller.

        FIXME: it's not symmetrical. To free the preserved range, must call it
        from BuddyAddressManager object.
        """
        assert (preserved_size & (preserved_size - 1)) == 0

        allocate_size = self.round_to_level_size(preserved_size)
        self.actual_allocated_size = allocate_size

        success, va = self.alloc(allocate_size)
        if not success:
            return (None, 0)

        ret = BuddyAddressManager(
            self,
            self.page_size,
            partition,
            va,
            preserved_size)
        self.preserved_vms.append(ret)

        return (ret, va)

    def free_preserved(self):
        """
        Release address space resource to parent manager.
        """
        assert self.parent_vm is not None
        assert self in self.parent_vm.preserved_vms

        self.parent_vm.free(self.start_addr)
        self.parent_vm.preserved_vms.remove(self)

        return (
            self.start_addr,
            self.size,
            self.parent_vm.get_preserved_size())

    def free(self, addr):
        assert addr in self.allocations

        size = self.allocations[addr]
        idx = int(math.log2(size / self.page_size))
        # make sure the size matches zone size
        assert math.modf(idx)[0] == 0

        insert_addr = addr
        while idx < BuddyAddressManager.level_cnt:
            can, merge_addr, del_pos = self.can_merge(idx, insert_addr)
            if can:
                self.zones[idx].pop(del_pos)
                idx += 1
                insert_addr = merge_addr
            else:
                break

        # directly insert to that zone
        bisect.insort(self.zones[idx], insert_addr)

        self.allocations.pop(addr)

    def split_bottom_up(self, target_idx):
        """
        Find another address and split down to target zone.
        Return false if failed.
        """
        # find one zone with free area, start from target idx
        top_idx = target_idx + 1
        while top_idx < BuddyAddressManager.level_cnt:
            if len(self.zones[top_idx]) != 0:
                break
            top_idx += 1
        else:
            # no address available any more
            return False

        # split the area down to the target idx
        idx = top_idx
        while idx > target_idx:
            # it always has a lower zone
            assert idx > 0

            sz = self.page_size * (1 << idx)
            # remove the last from current zone
            addr = self.zones[idx].pop()
            bisect.insort(self.zones[idx - 1], addr)
            bisect.insort(self.zones[idx - 1], int(addr + sz / 2))

            idx -= 1

        return True

    def can_merge(self, idx, addr):
        """
        Check if we can merge buddy together. Return
        (can merge or not, start address for merge, element pos need delete)
        """
        if len(self.zones[idx]) == 0:
            return (False, 0, 0)

        sz = self.page_size * (1 << idx)

        # stronger constrain, require address to be sz aligned
        assert addr & (sz - 1) == 0
        i = int(addr / sz)
        if i & 1 == 0:
            merge_addr = addr
            check_addr = addr + sz
        else:
            merge_addr = addr - sz
            check_addr = addr - sz

        pos = bisect.bisect_left(self.zones[idx], check_addr)
        if pos != len(self.zones[idx]) and self.zones[idx][pos] == check_addr:
            return (True, merge_addr, pos)

        return (False, 0, 0)

    def is_full(self):
        return self.max_alloc_sz() == self.size

    def is_empty(self):
        return self.max_alloc_sz() == 0

    def has_preservation(self):
        return len(self.preserved_vms) != 0

    def max_alloc_sz(self):
        sz = 0
        i = self.level_cnt - 1
        while len(self.zones[i]) == 0:
            i -= 1

            # failed to find memory
            if i < 0:
                break
        else:
            sz = self.page_size * (1 << i)

        return sz

    def dump(self):
        sz = self.page_size
        for area in self.zones:
            print("Size({:d}): [".format(sz), end="")
            for i in area:
                print("{:#x} ".format(i), end="")
            print("]")
            sz *= 2

    def get_allocations(self):
        return list(self.allocations.items())

    def get_random_free_address(self):
        assert not self.is_empty()
        available_zones = [z for z in self.zones if len(z) != 0]
        assert len(available_zones) != 0

        zone = random.choice(available_zones)
        idx = self.zones.index(zone)
        size = 1 << (idx + self.page_size.bit_length() - 1)
        addr = random.choice(zone)
        offset = random.randint(0, size - 1)
        return addr + offset


class Ops(enum.IntEnum):
    INIT = 0
    MAP = 1
    UNMAP = 2
    LOOKUP = 3
    MEM_ACC = 4
    DESTROY = 5
    CHECK_TABLE_LEVEL = 6
    PREALLOC = 7
    FREE_PREALLOC = 8
    PARTIAL_UNMAP = 9
    PARTIAL_MAP = 10
    CHECK_UNMAP_AREA = 11
    CHECK_ENTRY_CNT = 12
    REMAP_SPLIT = 13
    REMAP_MERGE = 14


class TestCase:
    states = ["start", "running", "stop"]
    transitions = [{"trigger": "init",
                    "source": "start",
                    "dest": "running",
                    "after": "gen_init_op",
                    },
                   {"trigger": "destroy",
                    "source": "running",
                    "dest": "stop",
                    "after": "gen_destroy_op",
                    },
                   {"trigger": "step",
                    "source": "running",
                    "dest": "=",
                    "after": "gen_step_ops"},
                   ]

    # the same as C test file
    init_op = 0
    map_op = 1
    unmap_op = 2
    lookup_op = 3
    mem_acc_op = 4
    destroy_op = 5
    check_table_level_op = 6
    prealloc_op = 7
    check_entry_cnt = 8
    remap_op = 9

    pa_sz = 4 * 1024 * 1024 * 1024
    page_size = 4096

    bottom_address_space_bit_cnt = 34

    def __init__(self):
        # NOTE: The same as pgtable.c, might change
        # FIXME: a better way to get this value
        self.top_bit_cnt = 39
        self.start_level = 1
        top_start_addr = ((1 << 64) - 1) & (~((1 << self.top_bit_cnt) - 1))
        top_vm_sz = 1 << self.top_bit_cnt

        # restrict range from [31 to 39]
        # NOTE: can only test one case for one test case
        self.bottom_bit_cnt = self.bottom_address_space_bit_cnt
        bottom_vm_sz = 1 << self.bottom_bit_cnt

        self.machine = Machine(
            model=self,
            states=TestCase.states,
            transitions=TestCase.transitions,
            initial="start",
            auto_transitions=False,
        )

        self.top_vm = BuddyAddressManager(
            None, self.page_size, 0, top_start_addr, top_vm_sz
        )
        self.bottom_vm = BuddyAddressManager(
            None, self.page_size, 0, 0, bottom_vm_sz)
        self.pm = BuddyAddressManager(None, self.page_size, 0, 0, self.pa_sz)

        # list of preserved area
        self.preserved = []

        # list of partial unmap
        self.partial_unmaps = []

        self.allocations = []
        self.test_ops = queue.Queue()
        self.step_ops = {
            Ops.MAP: self.gen_map_op,
            Ops.UNMAP: self.gen_unmap_op,
            Ops.MEM_ACC: self.gen_memacc_op,
            Ops.CHECK_TABLE_LEVEL: self.gen_check_table_level_op,
            Ops.PREALLOC: self.gen_prealloc_op,
            Ops.FREE_PREALLOC: self.gen_unmap_preserved_op,
            Ops.PARTIAL_MAP: self.gen_partial_map_op,
            Ops.PARTIAL_UNMAP: self.gen_partial_unmap_op,
            Ops.CHECK_UNMAP_AREA: self.gen_check_unmap_area_op,
            Ops.CHECK_ENTRY_CNT: self.gen_check_entry_cnt_op,
            Ops.REMAP_SPLIT: self.gen_remap_split_op,
            Ops.REMAP_MERGE: self.gen_remap_merge_op,
        }
        # the operation should for next step, decide which transition should be
        # performed
        self.next_ops = Ops.MAP

    def _destroy_vm_and_children(self, parent_vm):
        children = tuple(a for a in self.allocations
                         if a[3].parent_vm == parent_vm)
        for va, pa, sz, vm in children:
            self.allocations.remove((va, pa, sz, vm))
            assert vm.is_belonged(va)
            self._destroy_vm_and_children(vm)
        parent_vm.destroy()

    def gen_init_op(self):
        self.test_ops.put("{:d}".format(self.init_op))

    def gen_destroy_op(self):
        # FIXME: should free all allocations one by one
        # free mapping in allocation
        # free preserved
        self.test_ops.put("{:d}".format(self.destroy_op))

    def gen_map_op(self):
        # FIXME: refine the logic
        vms = [self.top_vm, self.bottom_vm] + self.preserved
        available_vms = [v for v in vms if v.max_alloc_sz() != 0]
        if len(available_vms) == 0:
            return

        parent_vm = random.choice(available_vms)
        merge_limit = parent_vm.get_preserved_size() or (1 << 39)

        # Randomise merge limit to test merging at all levels
        merge_limit_bits = (merge_limit - 1).bit_length()
        merge_limit_bits = random.randint(0, merge_limit_bits)
        merge_limit = 1 << merge_limit_bits

        max_sz = min(parent_vm.max_alloc_sz(), self.pm.max_alloc_sz())
        # no memory, skip map
        # FIXME: should use condition to avoid map transition
        if max_sz <= 0:
            return

        sz = random.randrange(self.page_size, max_sz + 1, self.page_size)

        success, va = parent_vm.alloc(sz)
        assert success
        success, pa = self.pm.alloc(sz)
        assert success

        vm = RunlengthAddressManager(
            parent_vm, parent_vm.page_size, parent_vm.partition, va, sz
        )
        self.allocations.append((va, pa, sz, vm))

        self.test_ops.put("# map")
        self.test_ops.put(
            "{:d} {:#x} {:d} {:#x} {:d}".format(
                self.map_op, va, sz, pa, merge_limit))

    def gen_remap_split_op(self):
        available_allocations = [
            a for a in self.allocations if not a[3].is_empty()]
        if len(available_allocations) == 0:
            return

        parent_vm = random.choice(available_allocations)[3]
        merge_limit = parent_vm.get_preserved_size() or (1 << 39)

        # Randomise merge limit to test merging at all levels
        merge_limit_bits = (merge_limit - 1).bit_length()
        merge_limit_bits = random.randint(0, merge_limit_bits)
        merge_limit = 1 << merge_limit_bits

        max_sz = min(parent_vm.max_alloc_sz(), self.pm.max_alloc_sz())
        # no memory, skip map
        # FIXME: should use condition to avoid map transition
        if max_sz <= self.page_size:
            return

        sz = random.randrange(self.page_size, max_sz + 1, self.page_size)

        success, va = parent_vm.alloc(sz)
        assert success
        success, pa = self.pm.alloc(sz)
        assert success

        vm = RunlengthAddressManager(
            parent_vm, parent_vm.page_size, parent_vm.partition, va, sz
        )
        self.allocations.append((va, pa, sz, vm))

        self.test_ops.put("# remap (split)")
        self.test_ops.put(
            "{:d} {:#x} {:d} {:#x} {:d}".format(
                self.remap_op, va, sz, pa, merge_limit))

    def gen_remap_merge_op(self):
        available_parent_allocations = [
            a for a in self.allocations if not a[3].is_full()]
        if len(available_parent_allocations) == 0:
            return
        parent_va, parent_pa, parent_sz, parent_vm = \
            random.choice(available_parent_allocations)
        merge_limit = parent_vm.get_preserved_size() or (1 << 39)

        # Randomise merge limit to test merging at all levels
        merge_limit_bits = (merge_limit - 1).bit_length()
        merge_limit_bits = random.randint(0, merge_limit_bits)
        merge_limit = 1 << merge_limit_bits

        available_allocations = [
            a for a in self.allocations if a[3].parent_vm == parent_vm]
        if len(available_allocations) == 0:
            return

        va, pa, sz, vm = random.choice(available_allocations)
        self.allocations.remove((va, pa, sz, vm))
        assert vm.is_belonged(va)

        self._destroy_vm_and_children(vm)
        self.pm.free(pa)

        offset = va - parent_va
        assert offset >= 0
        assert offset + sz <= parent_sz

        self.test_ops.put("# remap (merge)")
        self.test_ops.put(
            "{:d} {:#x} {:d} {:#x} {:d}".format(
                self.remap_op, va, sz, parent_pa + offset, merge_limit))

    def gen_unmap_op(self):
        if len(self.allocations) == 0:
            return

        va, pa, sz, vm = random.choice(self.allocations)

        if any(a[3] == vm.parent_vm for a in self.allocations):
            return

        self.allocations.remove((va, pa, sz, vm))
        assert vm.is_belonged(va)

        self._destroy_vm_and_children(vm)
        self.pm.free(pa)

        preserved = vm.get_preserved_size()

        self.test_ops.put("# unmap")
        self.test_ops.put(
            "{:d} {:#x} {:d} {:d}".format(self.unmap_op, va, sz, preserved)
        )

    def gen_unmap_preserved_op(self):
        if len(self.preserved) == 0:
            return

        unused_preserved = [p for p in self.preserved if p.is_full()]
        if len(unused_preserved) == 0:
            return

        p = random.choice(unused_preserved)

        assert not p.has_preservation()

        addr, sz, preserved_size = p.free_preserved()
        self.preserved.remove(p)

        self.test_ops.put("# unmap preserved")
        self.test_ops.put(
            "{:d} {:#x} {:d} {:d}".format(
                self.unmap_op, addr, sz, preserved_size))

    def gen_memacc_op(self):
        available_allocations = [
            a for a in self.allocations if not a[3].is_empty()]
        if len(available_allocations) == 0:
            return

        va, pa, sz, vm = random.choice(available_allocations)
        offset = vm.get_random_free_address() - va

        # expect success always
        comment = "# va({:#x}) pa({:#x}) sz({:d}) offset({:d})".format(
            va, pa, sz, offset
        )

        va += offset
        pa += offset

        # Just access one byte
        # Based on current implementation, it makes no difference to access one
        # byte or several bytes except across page boundary...
        self.test_ops.put(comment)
        self.test_ops.put(
            "{:d} {:#x} {:d} {:#x} 0".format(
                self.mem_acc_op, va, 1, pa))

    def gen_check_table_level_op(self):
        if len(self.preserved) == 0:
            return

        vm = random.choice(self.preserved)
        level = vm.get_preserved_level_4k()
        if level <= (self.start_level + 1):
            return

        addr = vm.start_addr
        size = vm.size

        self.test_ops.put("# check preserved {:#x} {:d}".format(addr, size))
        self.test_ops.put(
            "{:d} {:#x} {:d}".format(self.check_table_level_op, addr, level)
        )

    def gen_prealloc_op(self):
        vms = [self.top_vm, self.bottom_vm] + self.preserved
        available_vms = [v for v in vms if v.max_alloc_sz() != 0]
        if len(available_vms) == 0:
            return

        vm = random.choice(available_vms)
        max_sz = vm.max_alloc_sz()
        if max_sz <= 0:
            return

        sz = random.randint(self.page_size, max_sz)
        # round up to power of 2
        sz = 1 << (sz.bit_length() - 1)
        # cannot continue if it cannot occupy the whole level
        if max_sz < vm.round_to_level_size(sz):
            return

        partition = random.randint(0, sys.maxsize)
        preservation, va = vm.alloc_preservation(sz, partition)
        assert preservation is not None

        # add to preallocations
        # FIXME: track partition
        self.preserved.append(preservation)

        self.test_ops.put("# preserved {:#x} {:d}".format(va, sz))
        self.test_ops.put(
            "{:d} {:#x} {:d}".format(
                self.prealloc_op, va, sz))

    def gen_partial_map_op(self):
        available_parent_allocations = [
            a for a in self.allocations if not a[3].is_full()]
        if len(available_parent_allocations) == 0:
            return

        parent_va, parent_pa, parent_sz, parent_vm = \
            random.choice(available_parent_allocations)
        merge_limit = parent_vm.get_preserved_size() or (1 << 39)

        # Randomise merge limit to test merging at all levels
        merge_limit_bits = (merge_limit - 1).bit_length()
        merge_limit_bits = random.randint(0, merge_limit_bits)
        merge_limit = 1 << merge_limit_bits

        map_addr, map_sz = random.choice(parent_vm.get_allocations())
        map_physical_addr = parent_pa + map_addr - parent_va

        if any(child_va == map_addr and child_vm.parent_vm == parent_vm
               for child_va, _, _, child_vm in self.allocations):
            # This allocation has been remapped, so we can't map it (it needs
            # to be chosen by remap_merge or unmap first).
            # TODO: map anyway and assert return of ERROR_EXISTING_MAPPING
            return

        parent_vm.free(map_addr)

        self.test_ops.put(
            "# map partial for {:#x} {:d} {:#x}".format(parent_va, parent_sz,
                                                        parent_pa))
        self.test_ops.put(
            "{:d} {:#x} {:d} {:#x} {:d}".format(
                self.map_op, map_addr, map_sz, map_physical_addr, merge_limit
            )
        )

    def gen_partial_unmap_op(self):
        if len(self.allocations) == 0:
            return

        va, pa, sz, vm = random.choice(self.allocations)
        max_sz = vm.max_alloc_sz()
        if max_sz <= 0:
            return

        unmap_size = random.randint(self.page_size, max_sz)
        unmap_size = unmap_size & (~(self.page_size - 1))
        success, addr = vm.alloc(unmap_size)
        if not success:
            return
        preserved = vm.get_preserved_size()

        self.test_ops.put(
            "# partial unmap from {:#x} {:d} {:#x}".format(
                va, sz, pa))
        self.test_ops.put(
            "{:d} {:#x} {:d} {:d}".format(
                self.unmap_op,
                addr,
                unmap_size,
                preserved))

    def gen_check_unmap_area_op(self):
        vms = [self.top_vm, self.bottom_vm] + self.preserved
        available_vms = [v for v in vms if not v.is_empty()]
        if len(available_vms) == 0:
            return

        vm = random.choice(available_vms)
        addr = vm.get_random_free_address()
        comment = "# unmapped are in [{:#x}, {:#x})".format(
            vm.start_addr, vm.start_addr + vm.size
        )

        self.test_ops.put(comment)
        self.test_ops.put("{:d} {:#x} {:d} {:#x} 1".format(
            self.mem_acc_op, addr, 0, 0
        ))

    def gen_check_entry_cnt_op(self):
        if len(self.allocations) == 0:
            # then just pick one random range
            vm = random.choice([self.top_vm, self.bottom_vm])
            offset = random.randint(0, vm.size - self.page_size)
            offset = offset & (~(self.page_size - 1))
            size = random(self.page_size, vm.size - offset)
            # the address and size for entry cnt check
            size = size & (~(self.page_size - 1))
            addr = vm.start_address + offset
            comment = "# entry check from vm ({:#x}, {:d})".format(
                vm.start_address, vm.size
            )
        else:
            va, pa, sz, vm = random.choice(self.allocations)
            if sz <= self.page_size:
                return
            offset = random.randint(0, sz - self.page_size)
            offset = offset & (~(self.page_size - 1))
            size = random.randint(self.page_size, sz - offset)
            size = size & (~(self.page_size - 1))
            addr = va + offset
            comment = "# entry cnt check allocation " + \
                "va({:#x}) pa({:#x}) sz{:d})".format(va, pa, sz)

        self.test_ops.put(comment)
        self.test_ops.put(
            "{:d} {:#x} {:d}".format(
                self.check_entry_cnt, addr, size))

    def gen_step_ops(self):
        self.step_ops[self.next_ops]()

    def random_next_step(self):
        # unmap need to be first
        if (self.top_vm.max_alloc_sz() == 0 and self.bottom_vm.max_alloc_sz()
                == 0) or self.pm.max_alloc_sz() == 0:
            return Ops.UNMAP

        if len(self.allocations) == 0:
            return Ops.MAP

        return random.choice(list(self.step_ops.keys()))

    def run(self, max_steps, seed):
        self.init()

        random.seed(seed)
        yield "# seed({:d})".format(seed)

        for step in range(max_steps):
            self.next_ops = self.random_next_step()
            self.step()
            while not self.test_ops.empty():
                yield self.test_ops.get()

        self.destroy()


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--outfile', default=sys.stdout,
                        type=argparse.FileType('w'))
    parser.add_argument('-s', '--seed', type=int,
                        default=random.randrange(0, 1 << 64))
    parser.add_argument('-m', '--max-steps', type=int, default=(1 << 20))

    opts = parser.parse_args()

    m = TestCase()

    for op in m.run(opts.max_steps, opts.seed):
        print(op, file=opts.outfile)


if __name__ == "__main__":
    main()
