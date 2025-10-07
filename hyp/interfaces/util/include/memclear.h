// © 2024 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// A secure memory zero for large aligned buffers.
//
// Requires the pointer s and size to each be naturally aligned to 1024 bytes.
//
// Guaranteed not to be optimized out
void
memclear(void *s, size_t size);

// A secure memory zero with cache clean for large buffers.
//
// Requires the pointer s and size to each be naturally aligned to 1024 bytes.
//
// Guaranteed not to be optimized out
void
memclear_and_clean(void *s, size_t size);
