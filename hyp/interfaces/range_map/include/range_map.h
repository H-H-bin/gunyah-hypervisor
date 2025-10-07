// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// The range map is not thread-safe by default. The caller is responsible for
// synchronising read and write operations.

// Initialise the range map.
error_t
range_map_init(range_map_t *range_map, partition_t *partition,
	       range_map_config_t config, register_t allowed_types);

// Destroy the range map.
void
range_map_destroy(range_map_t *range_map);

// Insert a range into the range map.
//
// If expect_empty is true, the operation will fail if any part of the range is
// not found to be empty during insertion. Otherwise, any existing entries in
// the range will be overwritten.
error_t
range_map_insert(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t entry, bool expect_empty);

// Update a range in the range map.
//
// The update will fail if all entries over the range do not match the given old
// entry. If successful, all old entries will be replaced with the new entry.
error_t
range_map_update(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t old_entry, range_map_entry_t new_entry);

// Remove a range from the range map.
//
// This will fail if all entries over the range do not match the given entry.
error_t
range_map_remove(range_map_t *range_map, size_t base, size_t size,
		 range_map_entry_t entry);

// Clear the entire range map.
void
range_map_clear_all(range_map_t *range_map);

// Returns true if the range map is empty.
bool
range_map_is_empty(range_map_t *range_map);

// Lookup a range in the range map.
//
// This function returns the entry found at the given base, and also the size of
// the entry, which will be capped at max_size. If the map is empty at the given
// base, an entry with EMPTY type is returned, and the size is the size of the
// empty region, similarly capped at max_size.
range_map_lookup_result_t
range_map_lookup(range_map_t *range_map, size_t base, size_t max_size);

// Walk over a range in the range map and perform a callback for regions
// matching the given type.
error_t
range_map_walk(range_map_t *range_map, size_t base, size_t size,
	       range_map_type_t type, range_map_callback_t callback,
	       range_map_arg_t arg);
