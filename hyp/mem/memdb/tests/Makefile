# Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause

# Host tests for memdb module
# Based on successful patterns from range_map_gpt and range_tree_avl tests

CC=clang

CFLAGS = --target=x86_64-linux-gnu -m64 -mcx16 -std=gnu18 -O1 -g -Wall -Werror \
	 -Wno-gcc-compat -Wno-gnu-alignof-expression -ffunction-sections \
	 -fdata-sections -fno-omit-frame-pointer
# disabled address sanitizer since the test code leaks lots of memory by not cleaning up the memdb
#	 -fdata-sections -fsanitize=address -fno-omit-frame-pointer

# Include paths for memdb dependencies
INCLUDES = -I../../../interfaces/memdb/include \
	   -I$(GUNYAH_BUILD)/include \
	   -I$(GUNYAH_BUILD)/events/include \
	   -I$(GUNYAH_BUILD)/objects/include \
	   -I../../../interfaces/allocator/include \
	   -I../../../interfaces/partition/include \
	   -I../../../interfaces/util/include \
	   -I../../../interfaces/object/include \
	   -I../../../interfaces/rcu/include \
	   -I../../../interfaces/preempt/include \
	   -I../../../interfaces/cpulocal/include \
	   -I../../../interfaces/log/include \
	   -I../../../interfaces/trace/include \
	   -I../../../misc/log_standard/include \
	   -I../../../arch/armv8/include \
	   -I../include \
	   -DHYP_STANDALONE_TEST -DUNIT_TESTS

# Macro definitions for host testing
INCLUDES += -imacros ../../../interfaces/util/include/attributes.h \
	    -imacros ../../../core/spinlock_ticket/include/spinlock_attrs.h \
	    -imacros ../../../core/preempt/include/preempt_attrs.h \
	    -imacros ../../../interfaces/rcu/include/rcu_attrs.h

# MEMDB implementation selection
# Options: stub, bitmap
# Note: bitmap implementation require additional dependencies and may not
# compile in host test environment. Use stub for basic testing.
MEMDB_IMPL=stub

# Validate implementation choice
ifeq ($(MEMDB_IMPL),bitmap)
    $(info Building with bitmap implementation - may require additional dependencies)
else ifeq ($(MEMDB_IMPL),stub)
    $(info Building with stub implementation - recommended for host testing)
else
    $(error Invalid MEMDB_IMPL: $(MEMDB_IMPL). Valid options: stub, bitmap)
endif

# Source files based on implementation
ifeq ($(MEMDB_IMPL),bitmap)
    MEMDB_SRC = ../../memdb_bitmap/src/memdb.c
    SPINLOCK_SRC = spinlock_stubs.c
    INCLUDES += -I../../memdb_bitmap/include \
                -I../../../interfaces/bootmem/include \
                -I../../../interfaces/bitmap/include \
                -I../../../interfaces/spinlock/include \
                -I.
else
    MEMDB_SRC = memdb_stubs.c
endif

SOURCES = host_tests.c \
	  $(MEMDB_SRC) \
	  allocator_stubs.c

# Add additional sources for bitmap implementation
ifeq ($(MEMDB_IMPL),bitmap)
    INCLUDES += -I$(GUNYAH_BUILD)/events/memdb_bitmap/include
    SOURCES += $(SPINLOCK_SRC)
    SOURCES += $(GUNYAH_BUILD)/autogen/core/base/src/hypresult.c
    SOURCES += $(GUNYAH_BUILD)/autogen/core/base/src/accessors.c
    SOURCES += bitmap_stubs.c
endif

# Object files
OBJECTS = $(patsubst %.c,%.o,$(notdir $(SOURCES)))

# Target binary
TARGET = memdb_tests

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -Wl,--gc-sections -o $@

# Build rules for different source locations
host_tests.o: host_tests.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

allocator_stubs.o: allocator_stubs.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

ifeq ($(MEMDB_IMPL),bitmap)
memdb.o: ../../memdb_bitmap/src/memdb.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
spinlock_stubs.o: spinlock_stubs.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
hypresult.o: $(GUNYAH_BUILD)/autogen/core/base/src/hypresult.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
accessors.o: $(GUNYAH_BUILD)/autogen/core/base/src/accessors.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
bitmap_stubs.o: bitmap_stubs.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
else
memdb_stubs.o: memdb_stubs.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
endif

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
