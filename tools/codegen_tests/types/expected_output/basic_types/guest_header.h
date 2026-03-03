// Automatically generated. Do not modify.
//
// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HYPTYPES_H_
#define HYPTYPES_H_

#include <limits.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

typedef struct DemoSingleField_s	    DemoSingleField_t;
typedef struct StructBitField_b		    StructBitField_t;
typedef struct interrupt_control_register_b interrupt_control_register_t;

struct DemoSingleField_s {
	uint32_t int_var;
};

// Bitfield: StructBitField <uint64_t x2>
typedef struct StructBitField_b {
	// 31:0      uint32_t test
	// 42:32     uint16_t test1
	// 58:51     uint8_t test5
	uint64_t bf[2];
} StructBitField_t;

#define StructBitField_default()                                               \
	(StructBitField_t)                                                     \
	{                                                                      \
		.bf = { 0x0U, 0x0U }                                           \
	}

#define StructBitField_cast(val_0, val_1)                                      \
	(StructBitField_t)                                                     \
	{                                                                      \
		.bf = {(val_0), (val_1) }                                      \
	}

void
StructBitField_init(StructBitField_t *bit_field);

// Set all unknown/unnamed fields to their expected default values.
// Note, this does NOT clean const named fields to default values.
StructBitField_t
StructBitField_clean(StructBitField_t bit_field);

bool
StructBitField_is_equal(StructBitField_t b1, StructBitField_t b2);

bool
StructBitField_is_empty(StructBitField_t bit_field);

// Check all unknown/unnamed fields have expected default values.
// Note, this does NOT check:
// - whether const named fields have their default values,
// - whether named fields with enumerated types are in-range, or
// - the values of named writeonly fields.
bool
StructBitField_is_clean(StructBitField_t bit_field);

typedef enum color_code_e { RED = 1, BLUE = 2 } color_code_t;

#define COLOR_CODE__MAX BLUE
#define COLOR_CODE__MIN RED

typedef uint32_t count_t;

typedef enum error_code_e {
	ERROR_CODE_ERROR_TIMEOUT	   = -3,
	ERROR_CODE_ERROR_RESOURCE_BUSY	   = -2,
	ERROR_CODE_ERROR_NOT_FOUND	   = -1,
	ERROR_CODE_ERROR_OK		   = 0,
	ERROR_CODE_ERROR_RETRY		   = 1,
	ERROR_CODE_ERROR_INVALID_ARG	   = 2,
	ERROR_CODE_ERROR_FATAL		   = 10,
	ERROR_CODE_ERROR_PERMISSION_DENIED = 11,
	ERROR_CODE_ERROR_OUT_OF_MEMORY	   = 12
} error_code_t;

#define ERROR_CODE__MAX ERROR_CODE_ERROR_OUT_OF_MEMORY
#define ERROR_CODE__MIN ERROR_CODE_ERROR_TIMEOUT

typedef uint32_t index_t;

// Bitfield: interrupt_control_register <uint64_t>
typedef struct interrupt_control_register_b {
	// 7:0       color_code_t interrupt_id
	// 15:8      int16_t priority_level
	// 23:16     const uint16_t handler_index
	// 35:32     uint16_t interrupt_flags
	uint64_t bf[1];
} interrupt_control_register_t;

#define interrupt_control_register_default()                                   \
	(interrupt_control_register_t)                                         \
	{                                                                      \
		.bf = { 0x0U }                                                 \
	}

#define interrupt_control_register_cast(val_0)                                 \
	(interrupt_control_register_t)                                         \
	{                                                                      \
		.bf = {(val_0) }                                               \
	}

uint64_t
interrupt_control_register_raw(interrupt_control_register_t bit_field);

void
interrupt_control_register_init(interrupt_control_register_t *bit_field);

// Set all unknown/unnamed fields to their expected default values.
// Note, this does NOT clean const named fields to default values.
interrupt_control_register_t
interrupt_control_register_clean(interrupt_control_register_t bit_field);

bool
interrupt_control_register_is_equal(interrupt_control_register_t b1,
				    interrupt_control_register_t b2);

bool
interrupt_control_register_is_empty(interrupt_control_register_t bit_field);

// Check all unknown/unnamed fields have expected default values.
// Note, this does NOT check:
// - whether const named fields have their default values,
// - whether named fields with enumerated types are in-range, or
// - the values of named writeonly fields.
bool
interrupt_control_register_is_clean(interrupt_control_register_t bit_field);

typedef enum result_code_e {
	RESULT_SUCCESS = 1,
	RESULT_FAILURE = 2
} result_code_t;

#define RESULT_CODE__MAX RESULT_FAILURE
#define RESULT_CODE__MIN RESULT_SUCCESS

#include <guest_hypresult.h>

void
StructBitField_set_test(StructBitField_t *bit_field, uint32_t val);

uint32_t
StructBitField_get_test(const StructBitField_t *bit_field);

void
StructBitField_copy_test(StructBitField_t	*bit_field_dst,
			 const StructBitField_t *bit_field_src);

void
StructBitField_set_test1(StructBitField_t *bit_field, uint16_t val);

uint16_t
StructBitField_get_test1(const StructBitField_t *bit_field);

void
StructBitField_copy_test1(StructBitField_t	 *bit_field_dst,
			  const StructBitField_t *bit_field_src);

void
StructBitField_set_test5(StructBitField_t *bit_field, uint8_t val);

uint8_t
StructBitField_get_test5(const StructBitField_t *bit_field);

void
StructBitField_copy_test5(StructBitField_t	 *bit_field_dst,
			  const StructBitField_t *bit_field_src);

color_code_t
color_code_raw_cast(uint32_t val);

#define color_code_cast(val) color_code_raw_cast((uint32_t)(val))

color_code_result_t
color_code_raw_cast_safe(uint32_t val);

#define color_code_cast_safe(val) color_code_raw_cast_safe((uint32_t)(val))

bool
color_code_raw_is_valid(uint32_t val);

#define color_code_is_valid(val) color_code_raw_is_valid((uint32_t)(val))

error_code_t
error_code_raw_cast(int32_t val);

#define error_code_cast(val) error_code_raw_cast((int32_t)(val))

error_code_result_t
error_code_raw_cast_safe(int32_t val);

#define error_code_cast_safe(val) error_code_raw_cast_safe((int32_t)(val))

bool
error_code_raw_is_valid(int32_t val);

#define error_code_is_valid(val) error_code_raw_is_valid((int32_t)(val))

void
interrupt_control_register_set_interrupt_id(
	interrupt_control_register_t *bit_field, color_code_t val);

color_code_t
interrupt_control_register_get_interrupt_id(
	const interrupt_control_register_t *bit_field);

void
interrupt_control_register_copy_interrupt_id(
	interrupt_control_register_t	   *bit_field_dst,
	const interrupt_control_register_t *bit_field_src);

void
interrupt_control_register_set_priority_level(
	interrupt_control_register_t *bit_field, int16_t val);

int16_t
interrupt_control_register_get_priority_level(
	const interrupt_control_register_t *bit_field);

void
interrupt_control_register_copy_priority_level(
	interrupt_control_register_t	   *bit_field_dst,
	const interrupt_control_register_t *bit_field_src);

uint16_t
interrupt_control_register_get_handler_index(
	const interrupt_control_register_t *bit_field);

void
interrupt_control_register_set_interrupt_flags(
	interrupt_control_register_t *bit_field, uint16_t val);

uint16_t
interrupt_control_register_get_interrupt_flags(
	const interrupt_control_register_t *bit_field);

void
interrupt_control_register_copy_interrupt_flags(
	interrupt_control_register_t	   *bit_field_dst,
	const interrupt_control_register_t *bit_field_src);

result_code_t
result_code_raw_cast(uint32_t val);

#define result_code_cast(val) result_code_raw_cast((uint32_t)(val))

result_code_result_t
result_code_raw_cast_safe(uint32_t val);

#define result_code_cast_safe(val) result_code_raw_cast_safe((uint32_t)(val))

bool
result_code_raw_is_valid(uint32_t val);

#define result_code_is_valid(val) result_code_raw_is_valid((uint32_t)(val))
#else
#error multiple include HYPTYPES_H_
#endif
