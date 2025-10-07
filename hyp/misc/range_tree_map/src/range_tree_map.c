// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// This is a trivial implementation of range_tree based on range_map. It relies
// on a range_map implementation with optional support for rcu_read, which is
// not part of the public range_map API.

#include <assert.h>
#include <hyptypes.h>

#include <panic.h>
#include <range_map.h>
#include <range_tree.h>
#include <util.h>

#include <events/range_tree.h>

#include "event_handlers.h"

error_t
range_tree_init(range_tree_t *tree, partition_t *partition)
{
	range_map_config_t config = range_map_config_default();
	range_map_config_set_max_bits(&config, RANGE_TREE_MAP_BITS);
	range_map_config_set_rcu_read(&config, true);

	return range_map_init(
		&tree->map, partition, config,
		util_bit((index_t)RANGE_MAP_TYPE_RANGE_TREE_NODE));
}

void
range_tree_destroy(range_tree_t *tree, range_tree_node_type_t node_type)
{
	error_t err = range_map_walk(
		&tree->map, 0, util_bit(RANGE_TREE_MAP_BITS),
		RANGE_MAP_TYPE_RANGE_TREE_NODE,
		RANGE_MAP_CALLBACK_RANGE_TREE_RELEASE,
		(range_map_arg_t){ .range_tree_node_type = node_type });
	assert(err == OK);
	range_map_destroy(&tree->map);
}

error_t
range_tree_insert(range_tree_t *tree, range_tree_node_t *node, size_t base,
		  size_t size)
{
	range_map_entry_t entry = {
		.type  = RANGE_MAP_TYPE_RANGE_TREE_NODE,
		.value = { .range_tree_node = node },
	};

	node->base = base;
	node->size = size;

	return range_map_insert(&tree->map, base, size, entry, true);
}

error_t
range_tree_remove(range_tree_t *tree, range_tree_node_t *node)
{
	range_map_entry_t entry = {
		.type  = RANGE_MAP_TYPE_RANGE_TREE_NODE,
		.value = { .range_tree_node = node },
	};

	return range_map_remove(&tree->map, node->base, node->size, entry);
}

range_tree_lookup_result_t
range_tree_lookup(range_tree_t *tree, size_t base, size_t max_size)
{
	range_map_lookup_result_t map_r =
		range_map_lookup(&tree->map, base, max_size);

	return (range_tree_lookup_result_t){
		.node = (map_r.entry.type == RANGE_MAP_TYPE_RANGE_TREE_NODE)
				? map_r.entry.value.range_tree_node
				: NULL,
		.size = map_r.size,
	};
}

bool
range_tree_map_handle_range_map_values_equal(range_map_type_t  type,
					     range_map_value_t x,
					     range_map_value_t y)
{
	assert(type == RANGE_MAP_TYPE_RANGE_TREE_NODE);

	return x.range_tree_node == y.range_tree_node;
}

error_t
range_tree_map_handle_range_map_walk_callback(range_map_entry_t entry,
					      range_map_arg_t	arg)
{
	assert(entry.type == RANGE_MAP_TYPE_RANGE_TREE_NODE);

	if (arg.range_tree_node_type == RANGE_TREE_NODE_TYPE_NONE) {
		panic("range_tree_destroy: tree is not empty");
	}

	return trigger_range_tree_release_node_event(
		arg.range_tree_node_type, entry.value.range_tree_node);
}
