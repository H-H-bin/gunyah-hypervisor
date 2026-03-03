#!/usr/bin/env python3
#
# Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause

from Cheetah.Template import Template

import os
import argparse
import itertools
import subprocess
import logging
import sys
import pickle
import shlex


# Determine the location of this script.
__loc__ = os.path.realpath(os.path.join(os.getcwd(),
                                        os.path.dirname(__file__)))

# The typed directory is added to the sys path so that when the pickle is
# loaded it can find the corresponding ast nodes.
typed_path = os.path.join(__loc__, '..', 'typed')
sys.path.append(typed_path)


type_size = dict()


logger = logging.getLogger(__name__)

valid_access_strs = \
    set([''.join(x) for x in itertools.chain.from_iterable(
        itertools.combinations('oOrwRWm', r) for r in range(1, 7))])


class register:
    def __init__(self, name, type_name, features=[], variants=[], access='rw'):
        if access in ['o', 'O']:
            access += 'rw'
        if access not in valid_access_strs:
            logger.error("Invalid access type '%s'", access)
            sys.exit(1)
        self.name = name
        self.type_name = type_name
        self._features = tuple(features)
        self._variants = variants
        self._read = 'r' in access
        self._write = 'w' in access
        self._volatile_read = 'R' in access
        self._barrier_write = 'W' in access
        self._ordered = 'O' in access
        self._non_ordered = 'o' in access or 'O' not in access
        self._barrier_read = 'm' in access

    @property
    def features(self):
        return self._features

    @property
    def variants(self):
        ret = []
        type_name = self.type_name[:-1] if self.type_name.endswith(
            '!') else self.type_name

        for v in self._variants:
            if v.endswith('!'):
                ret.append((v[:-1],
                            type_name if self.type_name.endswith(
                                '!') else v[:-1],
                            type_size[type_name] if self.type_name.endswith(
                                '!') else type_size[v[:-1]]))
            else:
                ret.append(('_'.join((self.name, v)),
                            type_name if self.type_name.endswith(
                                '!') else '_'.join((type_name, v)),
                            type_size[type_name] if self.type_name.endswith(
                                '!') else type_size['_'.join((type_name, v))]))

        if not ret:
            if type_name.startswith('uint'):
                size = int(int(type_name[4:])/8)
            else:
                if type_name not in type_size:
                    raise Exception(
                        "no type: {:s} for register {:s}".format(
                            type_name, self.name))
                size = type_size[type_name]
            ret = [(self.name, type_name, size)]
        return sorted(ret)

    @property
    def is_readable(self):
        return self._read

    @property
    def is_volatile(self):
        return self._volatile_read

    @property
    def is_readable_barrier(self):
        return self._barrier_read

    @property
    def is_writable(self):
        return self._write

    @property
    def is_writeable_barrier(self):
        return self._barrier_write

    @property
    def need_ordered(self):
        return self._ordered

    @property
    def need_non_ordered(self):
        return self._non_ordered


def generate_accessors(template, input, ns):
    registers = {}

    for line in input.splitlines():
        if line.startswith('//'):
            continue
        tokens = line.split(maxsplit=1)
        if not tokens:
            continue
        name = tokens[0]
        if name in registers:
            raise Exception("duplicate register:", name)

        kwargs = {}

        type_name = name
        args = tokens[1] if len(tokens) > 1 else ''

        if args.startswith('+'):
            try:
                features, args = args[1:].split(maxsplit=1)
            except ValueError:
                features = args[1:]
                args = ''
            kwargs['features'] = features.split(',')

        if args.startswith('<'):
            type_name, args = args[1:].split('>', maxsplit=1)
            args = args.strip()

        if args.startswith('['):
            variants, args = args[1:].split(']', maxsplit=1)
            variants = variants.split()
            args = args.strip()
            kwargs['variants'] = variants

        if args:
            kwargs['access'] = args

        registers[name] = register(name, type_name, **kwargs)

    ns['registers'] = [registers[r] for r in sorted(registers.keys())]

    output = str(Template(file=template, searchList=ns))

    return output


def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(message)s",
    )

    args = argparse.ArgumentParser()

    mode_args = args.add_mutually_exclusive_group(required=True)
    mode_args.add_argument('-t', '--template',
                           type=argparse.FileType('r', encoding="utf-8"),
                           help="Template file used to generate output")

    args.add_argument('-o', '--output',
                      type=argparse.FileType('w', encoding="utf-8"),
                      default=sys.stdout, help="Write output to file")
    args.add_argument("-f", "--formatter",
                      help="specify clang-format to format the code")
    args.add_argument("input", metavar='INPUT', nargs='*',
                      help="Input type register file to process",
                      type=argparse.FileType('r', encoding="utf-8"))
    args.add_argument('-p', '--load-pickle',
                      type=argparse.FileType('rb'),
                      help="Load the IR from typed Python pickle")
    options = args.parse_args()

    # Load typed pickle to get the types used for the registers
    ir = pickle.load(options.load_pickle)
    for d in ir.definitions:
        if d.type_name not in type_size:
            if d.category == "bitfield":
                type_size[d.type_name] = d.size

    output = ""

    input = ""
    for f in options.input:
        input += f.read()
        f.close()

    output += generate_accessors(options.template, input, {})

    if options.formatter:
        ret = subprocess.run(shlex.split(options.formatter),
                             input=output.encode("utf-8"),
                             stdout=subprocess.PIPE)
        output = ret.stdout.decode("utf-8")
        if ret.returncode != 0:
            raise Exception("failed to format output:\n ", ret.stderr)

    options.output.write(output)


if __name__ == '__main__':
    main()
