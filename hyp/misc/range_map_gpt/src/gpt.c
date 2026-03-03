// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcontainers.h>

#include <atomic.h>
#include <bitmap.h>
#include <compiler.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <range_map.h>
#include <rcu.h>
#include <trace.h>
#include <util.h>

#include <events/range_map.h>

#include "event_handlers.h"
#include "internal.h"

#define SIZE_T_BITS util_width(size_t)

static_assert(sizeof(range_map_value_t) <= sizeof(uint64_t),
	      "GPT value must not be larger than 64-bits!");
static_assert((index_t)RANGE_MAP_TYPE__MAX < util_bit(RANGE_MAP_TYPE_BITS),
	      "GPT type does not fit in PTE bitfield!");

bool
range_map_gpt_handle_empty_values_equal(void)
{
	return true;
}

void
range_map_gpt_handle_reserved_callback(void)
{
	panic("range_map: Reserved callback used");
}

static range_map_pte_t
range_map_pte_empty(void)
{
	return (range_map_pte_t){
		.info  = range_map_pte_info_default(),
		.value = { .raw = 0U },
	};
}

static size_t
get_max_size(range_map_config_t config)
{
	return util_bit(range_map_config_get_max_bits(&config));
}

static size_t
get_pte_addr(range_map_pte_t pte)
{
	size_t	guard  = range_map_pte_info_get_guard(&pte.info);
	count_t shifts = range_map_pte_info_get_shifts(&pte.info);

	return guard << shifts;
}

static size_t
get_pte_size(range_map_pte_t pte)
{
	return util_bit(range_map_pte_info_get_shifts(&pte.info));
}

static bool
guard_matching(range_map_pte_t pte, size_t addr)
{
	assert(range_map_pte_info_get_type(&pte.info) != RANGE_MAP_TYPE_EMPTY);

	size_t	guard  = range_map_pte_info_get_guard(&pte.info);
	count_t shifts = range_map_pte_info_get_shifts(&pte.info);

	return (addr >> shifts) == guard;
}

static bool
entries_equal(range_map_entry_t a, range_map_entry_t b)
{
	return (a.type == b.type) &&
	       trigger_range_map_values_equal_event(a.type, a.value, b.value);
}

static bool
entry_is_valid(const range_map_t *range_map, range_map_entry_t entry)
{
	return (entry.type <= RANGE_MAP_TYPE__MAX) &&
	       bitmap_isset(&range_map->allowed_types, (index_t)entry.type);
}

static bool
entry_is_valid_or_empty(range_map_t *range_map, range_map_entry_t entry)
{
	return entry_is_valid(range_map, entry) ||
	       (entry.type == RANGE_MAP_TYPE_EMPTY);
}

static bool
pte_and_entry_equal(range_map_pte_t pte, size_t curr, range_map_entry_t entry)
{
	assert(guard_matching(pte, curr));

	size_t		  pte_addr  = get_pte_addr(pte);
	range_map_type_t  pte_type  = range_map_pte_info_get_type(&pte.info);
	range_map_value_t pte_value = pte.value;

	trigger_range_map_value_add_offset_event(pte_type, &pte_value,
						 curr - pte_addr);

	range_map_entry_t other = {
		.type  = pte_type,
		.value = pte_value,
	};

	return entries_equal(entry, other);
}

static bool
can_replace_pte(size_t curr, size_t rem, range_map_pte_t pte)
{
	size_t pte_addr = get_pte_addr(pte);
	size_t pte_size = get_pte_size(pte);

	assert(!guard_matching(pte, curr));

	// If the remaining region completely covers the range of the PTE, it is
	// safe to replace the PTE.
	return (curr <= pte_addr) && ((curr + rem) >= (pte_addr + pte_size));
}

static bool
pte_will_conflict(size_t curr, size_t rem, range_map_entry_t old,
		  range_map_pte_t pte)
{
	bool ret = true;

	assert(!guard_matching(pte, curr));

	if (old.type != RANGE_MAP_TYPE_EMPTY) {
		// We expected the GPT to not be empty at this point.
		goto out;
	}

	if (range_map_pte_info_get_type(&pte.info) == RANGE_MAP_TYPE_LEVEL) {
		// We can only be sure of a conflict with a level if its entire
		// range is going to be replaced. Otherwise, we must traverse
		// the level first to be sure of a conflict.
		ret = can_replace_pte(curr, rem, pte);
		goto out;
	}

	size_t pte_addr = get_pte_addr(pte);

	// If the range to update overlaps with the PTE, we have a conflict.
	ret = (pte_addr >= curr) && (pte_addr < (curr + rem));

out:
	return ret;
}

static count_t
get_common_shifts(range_map_pte_t pte, size_t addr)
{
	count_t clz = compiler_clz(addr ^ get_pte_addr(pte));

	return (count_t)SIZE_T_BITS -
	       util_balign_down(clz, RANGE_MAP_LEVEL_BITS);
}

static index_t
get_level_index(count_t shifts, size_t addr)
{
	assert(shifts >= RANGE_MAP_LEVEL_BITS);

	return (index_t)((addr >> (shifts - RANGE_MAP_LEVEL_BITS)) &
			 util_mask(RANGE_MAP_LEVEL_BITS));
}

static range_map_stack_frame_t *
get_curr_stack_frame(range_map_stack_t *stack)
{
	assert(stack->depth != 0U);

	index_t i = stack->depth - 1U;
	assert(i < RANGE_MAP_MAX_LEVELS);

	return &stack->frame[i];
}

static count_t
get_max_entry_shifts(range_map_stack_t *stack)
{
	count_t shifts = RANGE_MAP_MAX_SIZE_BITS;

	if (stack->depth != 0U) {
		range_map_stack_frame_t *frame = get_curr_stack_frame(stack);
		assert(frame != NULL);
		shifts = range_map_frame_info_get_shifts(&frame->info);
	}

	return shifts;
}

static count_t
get_max_possible_shifts(range_map_stack_t *stack, size_t curr, size_t rem)
{
	count_t shifts = get_max_entry_shifts(stack);

	assert(rem > 0U);

	if (curr != 0U) {
		count_t align_bits = util_balign_down(compiler_ctz(curr),
						      RANGE_MAP_LEVEL_BITS);
		shifts		   = util_min(shifts, align_bits);
	}

	if (util_bit(shifts) > rem) {
		shifts = util_balign_down(compiler_ctz(rem),
					  RANGE_MAP_LEVEL_BITS);
	}

	return shifts;
}

static range_map_level_t *
get_level_from_pte(range_map_pte_t pte)
{
	range_map_level_t *level = pte.value.level;
	assert(level != NULL);

	return level;
}

static void
go_down_level(range_map_config_t config, range_map_stack_t *stack, size_t curr,
	      range_map_pte_t pte)
{
	range_map_level_t *level = get_level_from_pte(pte);

	assert(guard_matching(pte, curr));

	stack->depth++;

	range_map_stack_frame_t *frame = get_curr_stack_frame(stack);
	assert(frame != NULL);

	count_t shifts = range_map_pte_info_get_shifts(&pte.info);
	assert(shifts >= RANGE_MAP_LEVEL_BITS);

	size_t addr = get_pte_addr(pte);
	assert(addr < get_max_size(config));

	range_map_frame_info_t info = range_map_frame_info_default();
	range_map_frame_info_set_addr(&info, addr);
	range_map_frame_info_set_shifts(&info, shifts - RANGE_MAP_LEVEL_BITS);

	frame->level = level;
	frame->info  = info;
}

static bool
check_ptes_consistent(range_map_pte_t a, range_map_pte_t b, size_t offset)
{
	bool ret = false;

	range_map_type_t type = range_map_pte_info_get_type(&a.info);
	if (type != range_map_pte_info_get_type(&b.info)) {
		goto out;
	}

	range_map_value_t x = a.value;
	range_map_value_t y = b.value;

	trigger_range_map_value_add_offset_event(type, &x, offset);

	ret = trigger_range_map_values_equal_event(type, x, y);

out:
	return ret;
}

static void
write_pte_to_level(range_map_pte_t *root, range_map_stack_t *stack,
		   range_map_pte_t pte)
{
	if (stack->depth == 0U) {
		*root = pte;
	} else {
		range_map_stack_frame_t *frame = get_curr_stack_frame(stack);
		assert(frame != NULL);

		index_t i = range_map_frame_info_get_index(&frame->info);
		assert(i < RANGE_MAP_LEVEL_ENTRIES);

		frame->level->entries[i] = pte;
		range_map_frame_info_set_dirty(&frame->info, true);
	}
}

rcu_update_status_t
range_map_gpt_handle_rcu_free_level(rcu_entry_t *entry)
{
	range_map_level_atomic_t *level =
		range_map_level_atomic_container_of_rcu_entry(entry);
	assert(level != NULL);
	assert(level->partition != NULL);

	partition_free(level->partition, level, sizeof(*level));

	return rcu_update_status_default();
}

static void
try_clean(range_map_pte_t *root, partition_t *partition,
	  range_map_stack_t *stack, range_map_level_t *level,
	  count_t entry_shifts)
{
	count_t		filled_count	= 0U;
	range_map_pte_t first_pte	= range_map_pte_empty();
	range_map_pte_t last_filled_pte = range_map_pte_empty();
	bool		can_merge	= true;

	assert(partition != NULL);

	for (index_t i = 0U; i < RANGE_MAP_LEVEL_ENTRIES; i++) {
		range_map_pte_t curr_pte = level->entries[i];

		if (range_map_pte_info_get_type(&curr_pte.info) ==
		    RANGE_MAP_TYPE_EMPTY) {
			can_merge = false;
		} else {
			filled_count++;
			last_filled_pte = curr_pte;
			// Merging entries is only possible if they fill up
			// the entire level.
			if (range_map_pte_info_get_shifts(&curr_pte.info) !=
			    entry_shifts) {
				can_merge = false;
			}
		}

		if (can_merge) {
			if (i == 0U) {
				first_pte = curr_pte;
			} else {
				size_t offset = (size_t)i << entry_shifts;
				if (!check_ptes_consistent(first_pte, curr_pte,
							   offset)) {
					can_merge = false;
				}
			}
		} else if (filled_count > 1U) {
			break;
		} else {
			// We may still be able to clean, continue iterating.
		}
	}

	if (filled_count <= 1U) {
		// Either the level is empty, or the last filled
		// PTE is the only one in the level.
		write_pte_to_level(root, stack, last_filled_pte);
		partition_free(partition, level, sizeof(range_map_level_t));
	} else if (can_merge) {
		// All entries consistent, we can merge into one PTE.
		assert(filled_count == RANGE_MAP_LEVEL_ENTRIES);
		count_t new_shifts = entry_shifts + RANGE_MAP_LEVEL_BITS;
		size_t	new_guard  = get_pte_addr(first_pte) >> new_shifts;
		range_map_pte_info_set_guard(&first_pte.info, new_guard);
		range_map_pte_info_set_shifts(&first_pte.info, new_shifts);
		write_pte_to_level(root, stack, first_pte);
		partition_free(partition, level, sizeof(range_map_level_t));
	} else {
		// No cases where we can free the level, do nothing.
	}
}

static void
go_up_level(range_map_pte_t *root, partition_t *partition,
	    range_map_stack_t *stack, bool write)
{
	assert(stack->depth > 0U);

	range_map_stack_frame_t *frame = get_curr_stack_frame(stack);
	assert(frame != NULL);

	stack->depth--;

	if (write && range_map_frame_info_get_dirty(&frame->info)) {
		// XXX Do we need a better heuristic to determine if a
		// clean is required? We could maintain a count of filled
		// entries in each level, but do we want this additional
		// memory consumption?
		count_t shifts = range_map_frame_info_get_shifts(&frame->info);
		try_clean(root, partition, stack, frame->level, shifts);
	} else {
		assert(!range_map_frame_info_get_dirty(&frame->info));
	}
}

static range_map_pte_t
get_curr_pte(range_map_pte_t *root, partition_t *partition,
	     range_map_stack_t *stack, size_t curr, bool write)
{
	range_map_pte_t pte;

	while (stack->depth > 0U) {
		range_map_stack_frame_t *frame = get_curr_stack_frame(stack);
		assert(frame != NULL);

		count_t shifts = range_map_frame_info_get_shifts(&frame->info);
		size_t	addr   = range_map_frame_info_get_addr(&frame->info);
		assert(curr >= addr);

		index_t idx = (index_t)((curr - addr) >> shifts);
		if (idx < RANGE_MAP_LEVEL_ENTRIES) {
			range_map_frame_info_set_index(&frame->info, idx);
			pte = frame->level->entries[idx];
			goto out;
		}

		go_up_level(root, partition, stack, write);
	}

	assert(stack->depth == 0U);

	pte = *root;

out:
	return pte;
}

static void
update_curr_pte(range_map_pte_t *root, range_map_stack_t *stack, size_t addr,
		count_t shifts, range_map_type_t type, range_map_value_t value)
{
	range_map_pte_t new_pte = range_map_pte_empty();

	if (type != RANGE_MAP_TYPE_EMPTY) {
		range_map_pte_info_set_guard(&new_pte.info, addr >> shifts);
		range_map_pte_info_set_shifts(&new_pte.info, shifts);
		range_map_pte_info_set_type(&new_pte.info, type);
		new_pte.value = value;
	}

	write_pte_to_level(root, stack, new_pte);
}

static void
split_pte_and_fill_level(range_map_level_t *level, range_map_pte_t old_pte,
			 count_t shifts)
{
	size_t		  pte_addr = get_pte_addr(old_pte);
	size_t		  pte_size = util_bit(shifts);
	range_map_type_t  type	   = range_map_pte_info_get_type(&old_pte.info);
	range_map_value_t value	   = old_pte.value;

	range_map_pte_t new_pte = old_pte;
	range_map_pte_info_set_shifts(&new_pte.info, shifts);

	for (index_t i = 0U; i < RANGE_MAP_LEVEL_ENTRIES; i++) {
		range_map_pte_info_set_guard(&new_pte.info, pte_addr >> shifts);
		new_pte.value = value;

		level->entries[i] = new_pte;

		pte_addr += pte_size;

		trigger_range_map_value_add_offset_event(type, &value,
							 pte_size);
	}
}

static error_t
allocate_level(range_map_pte_t *root, partition_t *partition,
	       range_map_stack_t *stack, range_map_pte_t old_pte,
	       count_t new_shifts, bool fill)
{
	error_t err = OK;

	size_t alloc_size  = sizeof(range_map_level_t);
	size_t alloc_align = alignof(range_map_level_t);

	void_ptr_result_t alloc_ret =
		partition_alloc(partition, alloc_size, alloc_align);
	if (alloc_ret.e != OK) {
		err = alloc_ret.e;
		goto out;
	}
	range_map_level_t *level = (range_map_level_t *)alloc_ret.r;
	*level			 = (range_map_level_t){ 0 };

	size_t	addr		= get_pte_addr(old_pte);
	count_t old_shifts	= range_map_pte_info_get_shifts(&old_pte.info);
	range_map_value_t value = { .level = level };

	if (fill) {
		assert(old_shifts == new_shifts);
		split_pte_and_fill_level(level, old_pte,
					 new_shifts - RANGE_MAP_LEVEL_BITS);
	} else {
		assert(old_shifts < new_shifts);
		index_t i	  = get_level_index(new_shifts, addr);
		level->entries[i] = old_pte;
	}

	update_curr_pte(root, stack, addr, new_shifts, RANGE_MAP_TYPE_LEVEL,
			value);

out:
	return err;
}

static void
free_all_levels(partition_t *partition, range_map_pte_t pte)
{
	range_map_level_t *levels[RANGE_MAP_MAX_LEVELS] = { get_level_from_pte(
		pte) };
	index_t		   level_idx[RANGE_MAP_MAX_LEVELS] = { 0 };

	count_t depth = 1U;
	while (depth > 0U) {
		index_t i = depth - 1U;
		assert(i < RANGE_MAP_MAX_LEVELS);

		range_map_level_t *level = levels[i];
		assert(level != NULL);

		index_t j = level_idx[i];
		if (j == RANGE_MAP_LEVEL_ENTRIES) {
			partition_free(partition, level,
				       sizeof(range_map_level_t));
			levels[i]    = NULL;
			level_idx[i] = 0U;
			depth--;
			continue;
		}

		range_map_pte_t curr_pte = level->entries[j];
		if (range_map_pte_info_get_type(&curr_pte.info) ==
		    RANGE_MAP_TYPE_LEVEL) {
			assert(i < (RANGE_MAP_MAX_LEVELS - 1U));
			levels[i + 1U] = get_level_from_pte(curr_pte);
			depth++;
		}

		level_idx[i]++;
	}
}

static size_t
update_curr_pte_and_get_size(range_map_pte_t *root, range_map_stack_t *stack,
			     size_t curr, size_t rem, range_map_entry_t new)
{
	count_t shifts = get_max_possible_shifts(stack, curr, rem);

	update_curr_pte(root, stack, curr, shifts, new.type, new.value);

	return util_bit(shifts);
}

static size_t
get_next_pte_base(range_map_stack_t *stack, size_t curr)
{
	count_t shifts = get_max_entry_shifts(stack);

	return util_p2align_down(curr, shifts) + util_bit(shifts);
}

static size_result_t
handle_write(range_map_pte_t *root, range_map_config_t config,
	     partition_t *partition, range_map_stack_t *stack, size_t curr,
	     size_t rem, range_map_entry_t old, range_map_entry_t new,
	     bool match)
{
	size_result_t	 ret = size_result_ok(0U);
	range_map_pte_t	 pte = get_curr_pte(root, partition, stack, curr, true);
	range_map_type_t type = range_map_pte_info_get_type(&pte.info);

	if (type == RANGE_MAP_TYPE_EMPTY) {
		// Empty PTEs don't have a valid guard, which is why we can't
		// perform a guard check here.
		if (match && (old.type != RANGE_MAP_TYPE_EMPTY)) {
			// We expected a non-empty PTE at this point.
			ret.e = ERROR_BUSY;
		} else if (new.type == RANGE_MAP_TYPE_EMPTY) {
			// No need to update an already empty entry, skip to the
			// next entry.
			ret.r = get_next_pte_base(stack, curr) - curr;
		} else {
			// We can safely update the PTE.
			ret.r = update_curr_pte_and_get_size(root, stack, curr,
							     rem, new);
		}
	} else if (!guard_matching(pte, curr)) {
		// The current address isn't mapped in the GPT.
		if (!match && can_replace_pte(curr, rem, pte)) {
			// It is safe to overwrite this PTE.
			ret.r = update_curr_pte_and_get_size(root, stack, curr,
							     rem, new);
			// If the old PTE was a level, we need to ensure it
			// and all levels below it are freed.
			if (type == RANGE_MAP_TYPE_LEVEL) {
				free_all_levels(partition, pte);
			}
		} else if (match && pte_will_conflict(curr, rem, old, pte)) {
			// We either expected the GPT to be filled at the
			// current offset, or to be empty at the PTE's offset.
			ret.e = ERROR_BUSY;
		} else {
			// Allocate a new common level and retry.
			count_t shifts = get_common_shifts(pte, curr);
			ret.e = allocate_level(root, partition, stack, pte,
					       shifts, false);
		}
	} else if (type == RANGE_MAP_TYPE_LEVEL) {
		// Guard matches for this level, traverse down it.
		go_down_level(config, stack, curr, pte);
	} else if (match && !pte_and_entry_equal(pte, curr, old)) {
		// We aren't matching the expected old value.
		ret.e = ERROR_BUSY;
	} else {
		// The PTE has an old value, which we may not be fully
		// replacing. Determine if we need to split the PTE into a new
		// level or not.
		count_t old_shifts = range_map_pte_info_get_shifts(&pte.info);
		count_t new_shifts = get_max_possible_shifts(stack, curr, rem);
		if (old_shifts > new_shifts) {
			assert(old_shifts >= RANGE_MAP_LEVEL_BITS);
			// Split entry into a new level and retry.
			ret.e = allocate_level(root, partition, stack, pte,
					       old_shifts, true);
		} else if ((old_shifts < new_shifts) && match) {
			// The old PTE doesn't cover the entire region that we
			// want to update, which means there is a mismatch.
			ret.e = ERROR_BUSY;
		} else {
			// Either the shifts are matching or we don't care about
			// the old entry, we can safely update the PTE.
			ret.r = update_curr_pte_and_get_size(root, stack, curr,
							     rem, new);
		}
	}

	return ret;
}

static error_t
do_walk_callback(range_map_read_data_t *data)
{
	error_t err = OK;

	if (data->size > 0U) {
		err = trigger_range_map_walk_callback_event(
			data->cb, data->entry, data->base, data->size,
			data->arg);
	}

	return err;
}

static void
log_range(size_t base, size_t size, range_map_entry_t entry)
{
	if ((entry.type != RANGE_MAP_TYPE_EMPTY) && (size > 0U)) {
		LOG(DEBUG, INFO, "[{:#x}, {:#x}]: type {:d}, value {:#x}", base,
		    size, (register_t)entry.type, entry.value.raw);
	}
}

static size_result_t
handle_read(range_map_pte_t *root, range_map_config_t config,
	    range_map_stack_t *stack, size_t curr, size_t rem,
	    range_map_read_op_t op, range_map_read_data_t *data)
{
	size_result_t	  ret	= size_result_ok(0U);
	range_map_pte_t	  pte	= get_curr_pte(root, NULL, stack, curr, false);
	range_map_type_t  type	= range_map_pte_info_get_type(&pte.info);
	range_map_value_t value = { .raw = 0U };

	size_t pte_addr = get_pte_addr(pte);
	size_t pte_size = get_pte_size(pte);
	size_t end_addr;

	if (type == RANGE_MAP_TYPE_EMPTY) {
		// The empty range ends at the next PTE.
		end_addr = get_next_pte_base(stack, curr);
	} else if (!guard_matching(pte, curr)) {
		// The current address isn't mapped in the GPT, so we
		// treat it as empty.
		type = RANGE_MAP_TYPE_EMPTY;
		if (curr < pte_addr) {
			// The range ends at the start of the PTE.
			end_addr = pte_addr;
		} else {
			// The range ends at the next PTE.
			end_addr = get_next_pte_base(stack, curr);
		}
	} else if (type == RANGE_MAP_TYPE_LEVEL) {
		go_down_level(config, stack, curr, pte);
		goto out;
	} else {
		end_addr = util_balign_down(curr + pte_size, pte_size);
		value	 = pte.value;

		trigger_range_map_value_add_offset_event(type, &value,
							 curr - pte_addr);
	}

	size_t size = util_min(end_addr - curr, rem);

	range_map_entry_t curr_entry = {
		.type  = type,
		.value = value,
	};

	range_map_entry_t cmp_entry = data->entry;

	trigger_range_map_value_add_offset_event(cmp_entry.type,
						 &cmp_entry.value, data->size);

	bool equal = entries_equal(curr_entry, cmp_entry);
	if (equal) {
		data->size += size;
	} else {
		if (op == RANGE_MAP_READ_OP_LOOKUP) {
			if (data->base == curr) {
				data->entry = curr_entry;
				data->size  = size;
			} else {
				ret.e = ERROR_FAILURE;
			}
		} else if (op == RANGE_MAP_READ_OP_IS_CONTIGUOUS) {
			ret.e = ERROR_FAILURE;
		} else if (op == RANGE_MAP_READ_OP_WALK) {
			ret.e = do_walk_callback(data);
			if (curr_entry.type == cmp_entry.type) {
				data->base  = curr;
				data->size  = size;
				data->entry = curr_entry;
			} else {
				data->base = curr + size;
				data->size = 0U;
			}
		} else if (op == RANGE_MAP_READ_OP_DUMP_RANGE) {
			log_range(data->base, data->size, data->entry);
			data->entry = curr_entry;
			data->base  = curr;
			data->size  = size;
		} else {
			panic("range_map: Invalid read operation");
		}
	}

	ret.r = size;

out:
	return ret;
}

static size_result_t
range_map_do_write(range_map_t *range_map, size_t base, size_t size,
		   range_map_entry_t old, range_map_entry_t new, bool match)
{
	size_result_t	   ret	     = size_result_ok(0U);
	range_map_pte_t	  *root	     = &range_map->root;
	range_map_config_t config    = range_map->config;
	partition_t	  *partition = range_map->partition;

	range_map_stack_t stack;
	stack.depth = 0U;

	range_map_entry_t x = old;
	range_map_entry_t y = new;

	size_t offset = 0U;
	while ((ret.e == OK) && (offset < size)) {
		ret = handle_write(root, config, partition, &stack,
				   base + offset, size - offset, x, y, match);
		if ((ret.e == OK) && (ret.r != 0U)) {
			offset += ret.r;
			trigger_range_map_value_add_offset_event(
				x.type, &x.value, ret.r);
			trigger_range_map_value_add_offset_event(
				y.type, &y.value, ret.r);
		}
	}

	ret.r = offset;

	// Unwind the GPT stack to finish any required cleanup.
	while (stack.depth > 0U) {
		go_up_level(root, partition, &stack, true);
	}

	return ret;
}

static error_t
range_map_write(range_map_t *range_map, size_t base, size_t size,
		range_map_entry_t old, range_map_entry_t new, bool match)
{
	size_result_t ret;

	assert(range_map != NULL);

	if ((size == 0U) || util_add_overflows(base, size - 1U)) {
		ret = size_result_error(ERROR_ARGUMENT_INVALID);
		goto out;
	}

	if ((base + size - 1U) > (get_max_size(range_map->config) - 1U)) {
		ret = size_result_error(ERROR_ARGUMENT_SIZE);
		goto out;
	}

	assert(entry_is_valid_or_empty(range_map, old));
	assert(entry_is_valid_or_empty(range_map, new));

	ret = range_map_do_write(range_map, base, size, old, new, match);
	if ((ret.e != OK) && (ret.r != 0U)) {
		size_result_t revert = range_map_do_write(
			range_map, base, ret.r, new, old, true);
		if (revert.e != OK) {
			panic("range_map: Failed to revert write!");
		}
	}

out:
	return ret.e;
}

static range_map_entry_t
range_map_entry_empty(void)
{
	return (range_map_entry_t){
		.type  = RANGE_MAP_TYPE_EMPTY,
		.value = { .raw = 0U },
	};
}

static error_t
range_map_read(range_map_t *range_map, size_t base, size_t size,
	       range_map_read_op_t op, range_map_read_data_t *data)
{
	size_result_t	   ret	  = size_result_ok(0U);
	range_map_pte_t	  *root	  = &range_map->root;
	range_map_config_t config = range_map->config;

	if ((size == 0U) || util_add_overflows(base, size - 1U)) {
		ret.e = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	if ((base + size - 1U) > (get_max_size(range_map->config) - 1U)) {
		ret.e = ERROR_ARGUMENT_SIZE;
		goto out;
	}

	assert(entry_is_valid_or_empty(range_map, data->entry));

	range_map_stack_t stack;
	stack.depth = 0U;

	size_t offset = 0U;
	while ((ret.e == OK) && (offset < size)) {
		ret = handle_read(root, config, &stack, base + offset,
				  size - offset, op, data);
		offset += ret.r;
	}

out:
	return ret.e;
}

error_t
range_map_init(range_map_t *range_map, partition_t *partition,
	       range_map_config_t config, register_t allowed_types)
{
	error_t err = OK;

	if (range_map_config_get_max_bits(&config) > RANGE_MAP_MAX_SIZE_BITS) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	if (bitmap_isset(&allowed_types, (index_t)RANGE_MAP_TYPE_EMPTY) ||
	    bitmap_isset(&allowed_types, (index_t)RANGE_MAP_TYPE_LEVEL) ||
	    ((allowed_types & ~util_mask((index_t)RANGE_MAP_TYPE__MAX + 1U)) !=
	     0U)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	range_map->root = range_map_pte_empty();

	range_map->partition	 = object_get_partition_additional(partition);
	range_map->config	 = config;
	range_map->allowed_types = allowed_types;

out:
	return err;
}

void
range_map_destroy(range_map_t *range_map)
{
	range_map_clear_all(range_map);
	object_put_partition(range_map->partition);
}

error_t
range_map_insert(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t entry, bool expect_empty)
{
	error_t err;

	if (entry_is_valid(range_map, entry)) {
		err = range_map_write(range_map, base, size,
				      range_map_entry_empty(), entry,
				      expect_empty);
	} else {
		err = ERROR_ARGUMENT_INVALID;
	}

	return err;
}

error_t
range_map_update(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t old_entry, range_map_entry_t new_entry)
{
	error_t err;

	if (entry_is_valid(range_map, old_entry) &&
	    entry_is_valid(range_map, new_entry)) {
		err = range_map_write(range_map, base, size, old_entry,
				      new_entry, true);
	} else {
		err = ERROR_ARGUMENT_INVALID;
	}

	return err;
}

error_t
range_map_remove(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t entry)
{
	error_t err;

	if (entry_is_valid(range_map, entry)) {
		err = range_map_write(range_map, base, size, entry,
				      range_map_entry_empty(), true);
	} else {
		err = ERROR_ARGUMENT_INVALID;
	}

	return err;
}

#if defined(UNIT_TESTS)
#define STATIC_TESTED
#else
#define STATIC_TESTED static
#endif

STATIC_TESTED error_t
range_map_clear(range_map_t *range_map, size_t base, size_t size)
{
	return range_map_write(range_map, base, size, range_map_entry_empty(),
			       range_map_entry_empty(), false);
}

void
range_map_clear_all(range_map_t *range_map)
{
	error_t err =
		range_map_clear(range_map, 0U, get_max_size(range_map->config));
	assert(err == OK);
}

bool
range_map_is_empty(range_map_t *range_map)
{
	return range_map_pte_info_get_type(&range_map->root.info) ==
	       RANGE_MAP_TYPE_EMPTY;
}

range_map_lookup_result_t
range_map_lookup(range_map_t *range_map, size_t base, size_t max_size)
{
	range_map_read_data_t read = { .base = base };

	size_t bounded_max_size = max_size;
	if (util_add_overflows(base, bounded_max_size)) {
		bounded_max_size = SIZE_MAX - base + 1U;
	}
	if (bounded_max_size > get_max_size(range_map->config)) {
		bounded_max_size = get_max_size(range_map->config) - base;
	}
	assert(bounded_max_size <= max_size);

	if (bounded_max_size == 0U) {
		// Return invalid / 0 size
		goto out;
	}

	error_t err = range_map_read(range_map, base, bounded_max_size,
				     RANGE_MAP_READ_OP_LOOKUP, &read);
	assert((err == OK) || (err == ERROR_FAILURE));

out:
	return (range_map_lookup_result_t){
		.entry = read.entry,
		.size  = read.size,
	};
}

#if defined(UNIT_TESTS)
bool
range_map_is_contiguous(range_map_t *range_map, size_t base, size_t size,
			range_map_entry_t entry)
{
	bool		      ret;
	range_map_read_data_t read = {
		.entry = entry,
	};

	if (entry_is_valid(range_map, entry)) {
		ret = range_map_read(range_map, base, size,
				     RANGE_MAP_READ_OP_IS_CONTIGUOUS,
				     &read) == OK;
	} else {
		ret = false;
	}

	return ret;
}
#endif

error_t
range_map_walk(range_map_t *range_map, size_t base, size_t size,
	       range_map_type_t type, range_map_callback_t callback,
	       range_map_arg_t arg)
{
	error_t		      err;
	range_map_read_data_t read = {
		.entry = { .type = type },
		.cb    = callback,
		.arg   = arg,
	};

	if ((callback < RANGE_MAP_CALLBACK__MIN) ||
	    (callback > RANGE_MAP_CALLBACK__MAX) ||
	    (callback == RANGE_MAP_CALLBACK_RESERVED)) {
		err = ERROR_ARGUMENT_INVALID;
		goto out;
	}

	if (entry_is_valid(range_map, read.entry)) {
		err = range_map_read(range_map, base, size,
				     RANGE_MAP_READ_OP_WALK, &read);
		if (err == OK) {
			err = do_walk_callback(&read);
		}
	} else {
		err = ERROR_ARGUMENT_INVALID;
	}

out:
	return err;
}

#if defined(UNIT_TESTS)
void
range_map_dump_ranges(range_map_t *range_map)
{
	range_map_read_data_t read = { .base = 0U, .size = 0U };

	LOG(DEBUG, INFO, "Dumping ranges of GPT {:#x}", (register_t)range_map);

	error_t err = range_map_read(range_map, 0U,
				     get_max_size(range_map->config),
				     RANGE_MAP_READ_OP_DUMP_RANGE, &read);
	assert(err == OK);

	log_range(read.base, read.size, read.entry);
}

void
range_map_dump_levels(range_map_t *range_map)
{
	range_map_pte_t	  *root	  = &range_map->root;
	range_map_config_t config = range_map->config;

	LOG(DEBUG, INFO, "Dumping levels of GPT {:#x}", (register_t)range_map);

	range_map_stack_t stack;
	stack.depth = 0U;

	size_t curr = 0U;
	while (curr < get_max_size(config)) {
		range_map_pte_t pte =
			get_curr_pte(root, NULL, &stack, curr, false);
		count_t entry_shifts = get_max_entry_shifts(&stack);

		if (!util_is_p2aligned(curr, entry_shifts)) {
			// We have already logged this PTE, go to the next
			// entry.
			curr = util_p2align_up(curr, entry_shifts);
			continue;
		}

		size_t	guard  = range_map_pte_info_get_guard(&pte.info);
		count_t shifts = range_map_pte_info_get_shifts(&pte.info);
		range_map_type_t  type = range_map_pte_info_get_type(&pte.info);
		range_map_value_t value = pte.value;

		LOG(DEBUG, INFO, "{:d} {:#x} {:d} {:d} {:#x}", stack.depth,
		    guard, shifts, type, value.raw);

		if (type == RANGE_MAP_TYPE_LEVEL) {
			curr = get_pte_addr(pte);
			go_down_level(config, &stack, curr, pte);
		} else {
			curr += util_bit(entry_shifts);
		}
	}
}
#endif
