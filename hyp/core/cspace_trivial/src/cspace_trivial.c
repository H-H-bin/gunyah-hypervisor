// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

// Placeholder implementation

#include <hyptypes.h>

#include <cspace.h>

cap_id_result_t
cspace_create_master_cap(cspace_t *cspace, object_ptr_t object,
			 object_type_t type)
{
	cap_id_result_t ret;
	ret.e = OK;
	ret.r = 0U;

	(void)cspace;
	(void)object;
	(void)type;

	return ret;
}

error_t
cspace_configure(cspace_t *cspace, count_t max_caps)
{
	(void)cspace;
	(void)max_caps;

	return OK;
}
