// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(UNIT_TESTS)

// Clear a range in the range map.
error_t
range_map_clear(range_map_t *range_map, size_t base, size_t size);

// Returns true if an entry is contiguous over a range in the range map.
bool
range_map_is_contiguous(range_map_t *range_map, size_t base, size_t size,
			range_map_entry_t entry);

// Walk over the range map and dump all contiguous ranges.
//
// This function is intended for debug use only.
void
range_map_dump_ranges(range_map_t *range_map);

// Walk over the range map and dump all levels.
//
// This function is intended for debug use only.
void
range_map_dump_levels(range_map_t *range_map);

#endif // UNIT_TESTS
