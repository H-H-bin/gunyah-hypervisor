// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Stub event handlers for memdb bitmap host testing

#ifndef EVENT_HANDLERS_STUB_H
#define EVENT_HANDLERS_STUB_H

// Stub event handler functions for host testing
// These are no-op implementations to satisfy compilation

static inline void
memdb_handle_object_create_memdb_entry(void)
{
	// Stub implementation - no-op for host testing
}

static inline void
memdb_handle_object_cleanup_memdb_entry(void)
{
	// Stub implementation - no-op for host testing
}

static inline void
memdb_handle_memdb_insert(void)
{
	// Stub implementation - no-op for host testing
}

static inline void
memdb_handle_memdb_update(void)
{
	// Stub implementation - no-op for host testing
}

static inline void
memdb_handle_memdb_lookup(void)
{
	// Stub implementation - no-op for host testing
}

#endif // EVENT_HANDLERS_STUB_H
