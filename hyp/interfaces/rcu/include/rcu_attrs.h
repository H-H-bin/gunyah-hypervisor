// © 2022 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifdef __EVENTS_DSL__
#define require_rcu_read require_read(rcu_read)
#else
#define ACQUIRE_RCU_READ ACQUIRE_READ(rcu_read)
#define RELEASE_RCU_READ RELEASE_READ(rcu_read)
#define REQUIRE_RCU_READ REQUIRE_READ(rcu_read)
#define EXCLUDE_RCU_READ EXCLUDE_READ(rcu_read)
#endif
