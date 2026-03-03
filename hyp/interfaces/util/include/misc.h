// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

//
// Miscellaneous utility functions.
//

// Compare two counts, and return true if the first is before the second,
// assuming that both counts are being actively updated so that the
// difference never exceeds INT32_MAX.
// (This is effectively the same as a signed comparison, but
// performed manually on unsigned values because the behaviour of signed
// overflow is undefined.)
bool
util_is_before(count_t a, count_t b);
