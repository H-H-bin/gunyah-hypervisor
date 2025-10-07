// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// The range tree is RCU-protected for read operations, and locked with a
// spinlock for write operations.

// Initialise the range tree.
//
// The provided partition will be used for memory allocation, if required by the
// tree implementation.
error_t
range_tree_init(range_tree_t *tree, partition_t *partition);

// Destroy a range tree.
//
// For any existing nodes in the tree, the range_tree_release_node event
// will be triggered with the specified node_type argument as its selector.
// This may be RANGE_TREE_NODE_TYPE_NONE if the tree is known to be empty.
//
// This must not be called until all readers are known to have completed.
void
range_tree_destroy(range_tree_t *tree, range_tree_node_type_t node_type);

// Insert a node into the range tree, covering a specified range.
//
// The specified range is required to be empty, and the specified node is
// required not to already be inserted in any range tree. If the node was
// previously inserted, an RCU grace period must have elapsed since it was
// removed.
error_t
range_tree_insert(range_tree_t *tree, range_tree_node_t *node, size_t base,
		  size_t size);

// Remove a node from the range tree.
//
// Note that this does not trigger the range_tree_release_node event. The caller
// is responsible for arranging for the node to be freed.
//
// The node must not be freed or reused before an RCU grace period has elapsed.
error_t
range_tree_remove(range_tree_t *tree, range_tree_node_t *node);

// Lookup a range in the range tree.
//
// This function returns the entry found at the given base, and returns the size
// of this entry, which will be capped at max_size.
range_tree_lookup_result_t
range_tree_lookup(range_tree_t *tree, size_t base, size_t max_size)
	REQUIRE_RCU_READ;
