// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// The list implementation consists of a circular double linked list and the
// list type contains a head pointer to the first element of the list.

// All the following functions require the list to be locked if it may be
// accessed by other threads, unless noted otherwise.

void
list_init(list_t *list);

list_node_t *
list_get_head(list_t *list);

bool
list_is_empty(list_t *list);

void
list_insert_at_head(list_t *list, list_node_t *node);

void
list_insert_at_tail(list_t *list, list_node_t *node);

// This function inserts a node in order, where the ordering is defined by the
// caller.
//
// If we want, for example, to insert a node in increasing order, then the
// caller needs to provide a function pointer that returns true if node a is
// smaller than node b, according to the caller's criteria.
//
// Returns true if the new node is placed at the head of the list, or false if
// the new node has been inserted after the head.
bool
list_insert_in_order(list_t *list, list_node_t *node,
		     bool (*compare_fn)(list_node_t *a, list_node_t *b));

void
list_insert_after_node(list_t *list, list_node_t *prev, list_node_t *node);

// The _release variants of insert must be used on any list that is iterated
// with a _consume iterator.
void
list_insert_at_tail_release(list_t *list, list_node_t *node);

// This function returns true if node has been removed from head and the list is
// not empty after the deletion.
//
// If the list is ever iterated by a _consume iterator, then the specified node
// must not be either freed or added to another list until an RCU grace period
// has elapsed; i.e. rcu_enqueue() or rcu_sync() must be called after this
// function returns.
bool
list_delete_node(list_t *list, list_node_t *node);

// Simple iterator. The list must be locked if other threads might modify it,
// and the iterator must not delete nodes.
#define list_foreach(node, list)                                               \
	for ((node) = atomic_load_relaxed(&(list)->head.next);                 \
	     (node) != &(list)->head;                                          \
	     (node) = atomic_load_relaxed(&(node)->next))

#define LIST_FOREACH_CONTAINER_BEGIN_LOAD_(ctype, list_ptr, cname, nname,      \
					   varname, load)                      \
	{                                                                      \
		ctype	    *varname;                                          \
		list_node_t *util_cpp_unique_ident(n) =                        \
			load(&(list_ptr)->head.next);                          \
		while (util_cpp_unique_ident(n) != &(list_ptr)->head) {        \
			(varname) = cname##_container_of_##nname(              \
				util_cpp_unique_ident(n));                     \
			util_cpp_unique_ident(n) =                             \
				load(&util_cpp_unique_ident(n)->next);

#define LIST_FOREACH_CONTAINER_END_LOAD_                                       \
	}                                                                      \
	}

// Simple deletion-safe container iterator. The list must be locked if other
// threads might modify it, and the iterator must not delete nodes.
// Usage:
// LIST_FOREACH_CONTAINER_BEGIN(type, list_ptr, container_name,
// node_member_name)
//     // loop body here - 'node' variable contains current container
// LIST_FOREACH_CONTAINER_END
#define LIST_FOREACH_CONTAINER_BEGIN(ctype, list_ptr, cname, nname, varname)   \
	LIST_FOREACH_CONTAINER_BEGIN_LOAD_(ctype, list_ptr, cname, nname,      \
					   varname, atomic_load_relaxed)

#define LIST_FOREACH_CONTAINER_END LIST_FOREACH_CONTAINER_END_LOAD_

// Container iterator with consume memory ordering. This macro provides
// atomic_load_consume() semantics for safe concurrent access to list nodes.
// It must be used in the following scenario:
//
// Lists that are not locked must be RCU-protected. The iterator must be used
// within an RCU critical section.  The list is not locked, however other
// threads that insert nodes must use the _release variants of the insert
// functions, and any thread that deletes a node must allow an RCU grace period
// to elapse before freeing the removed list container objects.
//
// Usage for RCU-protected lists:
// rcu_read_start();
// LIST_FOREACH_CONTAINER_CONSUME_BEGIN(type, list_ptr, container_name,
//                                      node_member_name, var_name)
//     // loop body here - 'var_name' variable contains current container
//     // RCU-safe access without locks
// LIST_FOREACH_CONTAINER_CONSUME_END(list_ptr, container_name,
//                                    node_member_name, var_name)
// rcu_read_finish();
#define LIST_FOREACH_CONTAINER_CONSUME_BEGIN(ctype, list_ptr, cname, nname,    \
					     varname)                          \
	LIST_FOREACH_CONTAINER_BEGIN_LOAD_(ctype, list_ptr, cname, nname,      \
					   varname, atomic_load_consume)

#define LIST_FOREACH_CONTAINER_CONSUME_END LIST_FOREACH_CONTAINER_END_LOAD_
