// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>

#define timer_t hyp_timer_t
#include <hyptypes.h>
#undef timer_t

#define register_t std_register_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef register_t

#include <compiler.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <range_map.h>
#include <trace.h>
#include <util.h>

#include "event_handlers.h"
#include "string_util.h"
#include "trace_helpers.h"

range_map_t	range_map;
partition_t	host_partition;
trace_control_t hyp_trace;

void
assert_failed(const char *file, int line, const char *func, const char *err)
{
	printf("Assert failed in %s at %s:%d: %s\n", func, file, line, err);
	exit(-1);
}

void
panic(const char *str)
{
	printf("Panic: %s\n", str);
	exit(-1);
}

size_t
strnlen(const char *str, size_t maxlen);

size_t
util_strnlen(const char *str, size_t maxlen)
{
	return strnlen(str, maxlen);
}

size_t
memscpy(void *s1, size_t s1_size, const void *s2, size_t s2_size)
{
	size_t copy_size = util_min(s1_size, s2_size);
	(void)memcpy(s1, s2, copy_size);
	return copy_size;
}

static void
trace_and_log_init(void)
{
	register_t flags = 0U;

	TRACE_SET_CLASS(flags, ERROR);
	TRACE_SET_CLASS(flags, DEBUG);

	atomic_init(&hyp_trace.enabled_class_flags, flags);
}

void
trigger_trace_log_event(trace_id_t id, trace_action_t action, const char *arg0,
			register_t arg1, register_t arg2, register_t arg3,
			register_t arg4, register_t arg5)
{
	char log[1024];
	(void)snprint(log, util_array_size(log), arg0, arg1, arg2, arg3, arg4,
		      arg5);
	puts(log);
}

partition_t *
object_get_partition_additional(partition_t *partition)
{
	assert(partition != NULL);

	return partition;
}

void
object_put_partition(partition_t *partition)
{
	assert(partition != NULL);
}

partition_t *
partition_get_root(void)
{
	return &host_partition;
}

void_ptr_result_t
partition_alloc(partition_t *partition, size_t bytes, size_t min_alignment)
{
	assert(partition != NULL);
	assert(bytes > 0U);

	void *mem = aligned_alloc(min_alignment, bytes);

	return (mem != NULL) ? void_ptr_result_ok(mem)
			     : void_ptr_result_error(ERROR_NOMEM);
}

void
partition_free(partition_t *partition, void *mem, size_t bytes)
{
	assert(partition != NULL);
	assert(bytes > 0U);

	free(mem);
}

void
preempt_disable(void)
{
}

void
preempt_enable(void)
{
}

void
rcu_read_start(void)
{
}

void
rcu_read_finish(void)
{
}

void
rcu_enqueue(rcu_entry_t *rcu_entry, rcu_update_class_t rcu_update_class)
{
	assert(rcu_update_class == RCU_UPDATE_CLASS_RANGE_MAP_FREE_LEVEL);

	(void)range_map_gpt_handle_rcu_free_level(rcu_entry);
}

cpu_index_t
cpulocal_check_index(cpu_index_t cpu)
{
	return cpu;
}

cpu_index_t
cpulocal_get_index_unsafe(void)
{
	return 0U;
}

void
trigger_range_map_value_add_offset_event(range_map_type_t   type,
					 range_map_value_t *value,
					 size_t		    offset)
{
	if ((type == RANGE_MAP_TYPE_TEST_A) ||
	    (type == RANGE_MAP_TYPE_TEST_B) ||
	    (type == RANGE_MAP_TYPE_TEST_C)) {
		range_map_tests_add_offset(type, value, offset);
	} else {
		// Nothing to do
	}
}

bool
trigger_range_map_values_equal_event(range_map_type_t type, range_map_value_t x,
				     range_map_value_t y)
{
	bool ret;

	if ((type == RANGE_MAP_TYPE_TEST_A) ||
	    (type == RANGE_MAP_TYPE_TEST_B) ||
	    (type == RANGE_MAP_TYPE_TEST_C)) {
		ret = range_map_tests_values_equal(x, y);
	} else if (RANGE_MAP_TYPE_EMPTY) {
		ret = range_map_gpt_handle_empty_values_equal();
	} else {
		ret = false;
	}

	return ret;
}

error_t
trigger_range_map_walk_callback_event(range_map_callback_t callback,
				      range_map_entry_t entry, size_t base,
				      size_t size, range_map_arg_t arg)
{
	error_t ret;

	if (callback == RANGE_MAP_CALLBACK_RESERVED) {
		range_map_gpt_handle_reserved_callback();
	} else if (callback == RANGE_MAP_CALLBACK_TEST) {
		ret = range_map_tests_callback(entry, base, size, arg);
	} else {
		ret = ERROR_ARGUMENT_INVALID;
	}

	return ret;
}

int
main(void)
{
	trace_and_log_init();

	range_map_gpt_handle_tests_init();

	range_map_gpt_handle_tests_start();

	printf("SUCCESS: All GPT tests completed successfully!\n");

	return 0;
}
