// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "string_util.h"

static bool
test_string_util();

bool
test_string_util()
{
	char	      test[128];
	size_result_t ret_val;
	bool	      ret = true;

	// decimal
	ret_val = snprint(test, 128, "this is {:d} {:d}", 10, 20, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#if 0
	// float
	ret_val = snprint(test, 128, "this is {:f} {:f}", 10.9, 20.1, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#endif

	// binary
	ret_val = snprint(test, 128, "this is {:b}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#if 0
	// e
	ret_val = snprint(test, 128, "this is {:e}", 89000.65, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// e
	ret_val = snprint(test, 128, "this is {:e}", 89000.5321, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#endif

	// x
	ret_val = snprint(test, 128, "this is {:x}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#if 0
	// precise
	ret_val = snprint(test, 128, "this is {:.5f}", 89.532235, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}
#endif

	// minwidth
	ret_val = snprint(test, 128, "this is {:20d}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// zero padding
	ret_val = snprint(test, 128, "this is {:020x}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// alternative form
	ret_val = snprint(test, 128, "this is {:#x}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// sign
	ret_val = snprint(test, 128, "this is {:+d}", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// alignment
	ret_val = snprint(test, 128, "this is {:=>30d} aa", 89, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:=<30d} aa", 90, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:=^30d} aa", 91, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:=>30d} aa", -92, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:==30d} aa", -93, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:==#30x} aa", -94, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:==+#30x} aa", -95, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:==+#030x} aa", 96, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	ret_val = snprint(test, 128, "this is {:+#030x} aa", 96, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// Invalid align
	ret_val = snprint(test, 128, "this is {:.=>30d} aa", -96, 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s] - should have failed\n", test);
		ret = false;
		goto out;
	} else {
		printf("got %d\n", ret_val.e);
	}

	// string
	ret_val = snprint(test, 128, "this is \"{:-^20s}\" hahah",
			  (register_t) "another", 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// maxwidth
	ret_val = snprint(test, 128, "this is \"{:-^20.3s}\" hahah",
			  (register_t) "another", 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

	// failure test case
	ret_val = snprint(test, 10, "this is\"{:-^20.3s}\" hahah",
			  (register_t) "another", 0, 0, 0, 0);
	if (ret_val.e == OK) {
		printf("the result [%s]\n", test);
	} else if (ret_val.e == ERROR_STRING_TRUNCATED) {
		printf("truncated result [%s]\n", test);
	} else {
		printf("failed to do snprint\n");
		ret = false;
		goto out;
	}

out:
	return ret;
}

int
main(void)
{
	int ret = 0;

	if (!test_string_util()) {
		printf("failed to string util test\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}
