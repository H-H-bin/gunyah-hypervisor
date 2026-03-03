#!/usr/bin/env python3
# coding: utf-8
#
# Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause

"""
Run as a part of gitlab CI, after Parasoft reports have been generated.

This script converts the Parasoft XML-format report to a Code Climate
compatible json file, that gitlab code quality can interpret.
"""

import xml.etree.ElementTree as ET
import json
import argparse
import sys
import os
import re

argparser = argparse.ArgumentParser(
    description="Convert Parasoft XML to Code Climate JSON")
argparser.add_argument('input', type=argparse.FileType('r'), nargs='?',
                       default=sys.stdin, help="the Parasoft XML input")
argparser.add_argument('--output', '-o', type=argparse.FileType('w'),
                       default=sys.stdout, help="the Code Climate JSON output")
args = argparser.parse_args()

tree = ET.parse(args.input)

parasoft_viols = tree.findall(".//StdViol") + tree.findall(".//FlowViol")

cc_viols = []

severity_map = {
    1: "blocker",
    2: "critical",
    3: "major",
    4: "minor",
    5: "info",
}

deviation_map = {
    # Deviation because the behaviour proscribed by the rule is exactly the
    # intended behaviour of assert(), assert_safety() and assert_debug():
    # it prints the unexpanded expression.
    'MISRAC2012-RULE_20_12-a': [
        (None, re.compile(
            r"parameter of potential macro 'assert(|_safety|_debug)'"
        )),
    ],
    # False positives due to __c11 builtins taking int memory order arguments
    # instead of enum in the Clang implementation.
    'MISRAC2012-RULE_10_3-b': [
        (None, re.compile(r"number '1'.*'essentially Enum'.*"
                          r"'__c11_atomic_(thread|signal)_fence'.*"
                          r"'essentially signed'")),
        (None, re.compile(r"number '2'.*'essentially Enum'.*"
                          r"'__c11_atomic_load'.*'essentially signed'")),
        (None, re.compile(r"number '3'.*'essentially Enum'.*"
                          r"'__c11_atomic_(store'|exchange'|fetch_).*"
                          r"'essentially signed'")),
        (None, re.compile(r"number '[45]'.*'essentially Enum'.*"
                          r"'__c11_atomic_compare_exchange_(strong|weak)'.*"
                          r"'essentially signed'")),
    ],
    # False positives with unknown cause: the return value of assert_if_const()
    # is always used, to determine whether to call assert_failed()
    'MISRAC2012-RULE_17_7-b': [
        (None, re.compile(r'"assert_if_const"')),
    ],
    'MISRAC2012-RULE_8_7-a': [
        # The could-be-static advisory rule is impractical to enforce for
        # generated accessors, since the type system has no information about
        # which accessors are used.
        (re.compile(r'^build/.*/accessors\.c$'), None),
        # The smccc module specifically has events that are only triggered by
        # handlers for other events.
        (re.compile(r'^build/.*/events/src/smccc\.c$'), None),
        # The object module has type-specific APIs that are only used directly
        # for some specific object types, and otherwise are called only by the
        # type-generic APIs defined in the same file.
        (re.compile(r'^build/.*/objects/.*\.c$'), None),
    ],
    'MISRAC2012-RULE_14_3-ac': [
        # Invariant expressions are expected and unavoidable in generated
        # event triggers because it is not possible to remove error result
        # types from handlers that never return errors.
        (re.compile(r'^build/.*/events/src/.*\.c$'), None),
        # Invariant expressions in assertions that can be statically
        # determined to true are covered by the approved deviation from this
        # rule for safety / sanity checks.
        (None, re.compile(r'\"assert.*always evaluates to true')),
    ],
    'MISRAC2012-RULE_8_13-a': [
        # Could-be-const pointers are expected and unavoidable in generated
        # event triggers because the object may or may not be modified
        # depending on the handlers and the module configuration. The const
        # qualifier is used to specify whether the handlers are allowed to
        # modify the objects, rather than whether they actually do.
        (re.compile(r'^build/.*/events/src/.*\.c$'), None),
        # False positives due to the could-be-const check not understanding
        # that an inline assembly output constraint that dereferences a
        # pointer is a write to that pointer.
        (re.compile(r'^build/.*/accessors\.c$'),
         re.compile(r'parameter "b1"')),
    ],
    # The generated type-generic object and accessor functions terminate
    # non-empty default clauses with a _Noreturn function, panic(),
    # to indicate that the object type or SMC call is invalid.
    # There is an approved deviation for this, and in any case these rules
    # are downgraded to advisory in generated code.
    'MISRAC2012-RULE_16_1-d': [
        (re.compile(r'^build/.*/objects/.*\.c$'), None),
        (re.compile(r'^build/.*/accessors\.c$'), None),
    ],
    'MISRAC2012-RULE_16_3-b': [
        (re.compile(r'^build/.*/objects/.*\.c$'), None),
        (re.compile(r'^build/.*/accessors\.c$'), None),
    ],
    # False positive due to a builtin sizeof variant that does not evaluate its
    # argument, so there is no uninitialised use.
    'MISRAC2012-RULE_9_1-a': [
        (None, re.compile(r'passed to "__builtin_object_size"')),
    ],
    'MISRAC2012-RULE_1_3-b': [
        (None, re.compile(r'passed to "__builtin_object_size"')),
    ],
    # Deviation because casting a pointer to _Atomic to a pointer that can't be
    # dereferenced at all (const void *) is reasonably safe, and is needed for
    # certain builtin functions where the compiler knows the real underlying
    # object type anyway (e.g. __builtin_object_size) or where the object type
    # does not matter (e.g. __builtin_prefetch).
    'MISRAC2012-RULE_11_8-a': [
        (None, re.compile(r"to the 'const void \*' type which removes the "
                          r"'_Atomic'")),
    ],
    # Compliance with rule 21.25 would have a significant performance impact.
    # All existing uses have been thoroughly analysed and tested, so we will
    # seek a project-wide deviation for this rule.
    'MISRAC2012-RULE_21_25-a': [
        (None, None),
    ],
    # A cast from a void or object pointer to uintptr_t is well-defined, so
    # the rationales of rules 11.4 and 11.6 are not applicable in these cases.
    # Casts in the other direction or to other types are potentially unsafe,
    # and might be used to circumvent rule 11.3, so they need specific
    # deviations. Note that Parasoft's messages show the underlying type, not
    # the typedef name, so we must assume that uintptr_t is unsigned long or
    # unsigned long long.
    'MISRAC2012-RULE_11_4-a': [
        (None, re.compile(
            r"converted to integral type 'unsigned (long ?)long'")),
    ],
    'MISRAC2012-RULE_11_6-a': [
        (None, re.compile(
            r"converted to arithmetic type 'unsigned (long ?)long'")),
    ],
    # Rule 8.5 is downgraded to Advisory in generated code, and we can't
    # comply with it easily for event handler prototypes which are duplicated
    # in the calling module's generated events.c and the handling module's
    # private generated event_handlers.h. Both prototypes are generated from
    # the same Python dictionary entry, so they are always consistent.
    'MISRAC2012-RULE_8_5-a': [
        (re.compile(r'^build/.*/events/'), None),
    ],
    # Rule 20.5 is downgraded to Readability (i.e. no need to even document a
    # deviation) for generated code.
    'MISRAC2012-RULE_20_5-a': [
        (re.compile(r'^build/'), None),
    ],
    # False positive due to a pointer-to-array type being misinterpreted as
    # incomplete when it is the type of the RHS of an assignment, but complete
    # when it is the LHS of an assignment. This is easily identified using two
    # criteria: (a) the origin type name of the alleged conversion contains
    # the string "(*)[", and (b) the destination type name is identical to the
    # origin (i.e. no conversion is actually happening).
    'MISRAC2012-RULE_11_2-a': [
        (None, re.compile(
            r'Pointer to incomplete type \'(.*\(\*\)\[.*)\' '
            r'should not be converted to type \'\1\'')),
    ],
    # Approved deviation for Directive 4.10 warnings because we have an
    # alternative mechanism for avoiding multiple inclusion that satisfies the
    # requirements of the directive (but not the check, which expects guards).
    'MISRAC2012-DIR_4_10-a': [
        (None, None),
    ],
}


def matches_deviation(v):
    rule = v.attrib['rule']
    if rule not in deviation_map:
        return False

    msg = v.attrib['msg']
    path = v.attrib['locFile'].split(os.sep, 2)[2]

    def check_constraint(constraint, value):
        if constraint is None:
            return True
        try:
            return constraint.search(value)
        except AttributeError:
            return constraint == value

    for d_path, d_msg in deviation_map[rule]:
        if check_constraint(d_path, path) and check_constraint(d_msg, msg):
            return True

    return False


cc_viols = [
    ({
        "type": "issue",
        "categories": ["Bug Risk"],
        "severity": ('info' if matches_deviation(v)
                     else severity_map[int(v.attrib['sev'])]),
        "check_name": v.attrib['rule'],
        "description": (v.attrib['msg'] + '. ' +
                        v.attrib['rule.header'] + '. (' +
                        v.attrib['rule'] + ')'),
        "fingerprint": v.attrib['unbViolId'],
        "location": {
            "path": v.attrib['locFile'].split(os.sep, 2)[2],
            "lines": {
                "begin": int(v.attrib['locStartln']),
                "end": int(v.attrib['locEndLn'])
            }
        }
    })
    for v in parasoft_viols]

args.output.write(json.dumps(cc_viols))
args.output.close()
