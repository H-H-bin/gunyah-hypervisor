// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>
#include <string.h>

#include <hypcall_def.h>
#include <hypcontainers.h>
#include <hyprights.h>

#include <cspace.h>
#include <cspace_lookup.h>
#include <object.h>
#include <spinlock.h>

#include <events/viommu.h>

hypercall_viommu_bind_streams_result_t
hypercall_viommu_bind_streams(cap_id_t viommu_cap, cap_id_t iommu_cap,
			      viommu_stream_id_t stream_id_start,
			      count_t		 stream_count)
{
	error_t err;
	count_t bound_count = 0U;

	err = trigger_viommu_bind_streams_event(viommu_cap, iommu_cap,
						stream_id_start, stream_count,
						&bound_count);

	return (hypercall_viommu_bind_streams_result_t){ .count = bound_count,
							 .error = err };
}

hypercall_viommu_unbind_streams_result_t
hypercall_viommu_unbind_streams(cap_id_t	   viommu_cap,
				viommu_stream_id_t stream_id_start,
				count_t		   stream_count)
{
	error_t err;
	count_t unbound_count = 0U;

	err = trigger_viommu_unbind_streams_event(viommu_cap, stream_id_start,
						  stream_count, &unbound_count);

	return (hypercall_viommu_unbind_streams_result_t){
		.count = unbound_count, .error = err
	};
}
