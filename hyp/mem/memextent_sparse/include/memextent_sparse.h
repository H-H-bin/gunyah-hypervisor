// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(MEMEXTENT_SPARSE_DONATE_NOMAP) && MEMEXTENT_SPARSE_DONATE_NOMAP
// These are variants of the normal memextent_sparse operations donate that are
// intended to be used by platform code that uses memextents internally to track
// memory ownership, while the mappings are updated separately; that is, they
// are essentially wrappers around memdb operations. They must never be exposed
// to VMs, and must never be used on extent for which any VM possesses a map
// capability.

// Donate between two siblings without updating mappings.
error_t
memextent_sparse_donate_sibling_nomap(memextent_t *from, memextent_t *to,
				      paddr_t phys, size_t size);

// Donate to or from a child without updating mappings.
error_t
memextent_sparse_donate_child_nomap(memextent_t *me, paddr_t phys, size_t size,
				    bool reverse);
#endif
