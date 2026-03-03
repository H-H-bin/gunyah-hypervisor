// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier; BSD-3-Clause

#if defined(ARCH_ARM_FEAT_VHE)
#define VHE(X)	X##2 [X!]
#define VHE_V(X, V)	X##2 [V]
#define VHE_T(X, T)	X##2 <T> [X!]
#define VHE_E(X, E)	X##2 E [X!]
#else
#define VHE(X)		X
#define VHE_V(X, V)	X [V]
#define VHE_T(X, T) 	X <T>
#define VHE_E(X, E)	X E
#endif
